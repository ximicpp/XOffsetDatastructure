#ifndef X_OFFSET_SCHEMA_FREEZE_HPP
#define X_OFFSET_SCHEMA_FREEZE_HPP

// Schema freeze tool and schema lock check (server side, schema evolution E4).
//
// A project links one small program against its schema:
//
//   #include "game_schema.hpp"
//   #include "xoffset_schema_freeze.hpp"
//   int main(int argc, char** argv) {
//       return XOffsetDatastructure::schema_freeze_main<game::Player, game::Chat>(argc, argv);
//   }
//
// and runs it in two ways:
//
//   schema_freeze freeze <label> <namespace> <out-header> <lock-file>
//       At a release: writes the current version of every root type as a
//       snapshot header (namespace <namespace>, e.g. game_v12) and appends
//       the version to the lock file. A record or enum that did not change
//       since the previous snapshot is not copied: the new snapshot refers to
//       the previous one (using Item = game_v11::Item;), so a lineage never
//       holds two copies of one type. Each root gets its schema name and an
//       XOFFSET_PIN_SCHEMA line, so editing a frozen type fails the build.
//       Each new record also gets its XOFFSET_RECORD line, so a client built
//       without reflection (C++20 field lists) can include the snapshot.
//       Then list the snapshot type in XOFFSET_SCHEMA_LINEAGE.
//
//   schema_freeze check <lock-file>
//       In CTest, on every build. For each root type, against the versions
//       in the lock file:
//       - the current layout equals a frozen version's but the field names
//         differ (and no XOFFSET_SCHEMA_NAME_ALIAS accepts them): fail; two
//         builds would read the same bytes under different names;
//       - the layout changed since the last frozen version and that version
//         is not in the root type's XOFFSET_SCHEMA_LINEAGE: fail; peers on
//         the last release would be rejected.
//
// Paths in the lock file and the #include lines of snapshots are written as
// given on the command line; run the tool from the directory the project's
// include paths start at.
//
// Lock file (text, one line per item, tab-separated, oldest version first):
//   version <label> <namespace> <header>
//   root    <schema name> <snapshot type> <schema_hash> <name_digest> <wire signature>
//   record  <snapshot type> <name> <wire signature> <name signature>
//   enum    <snapshot type> <name> <definition>

#include "xoffset_evolution.hpp"

#include <cctype>
#include <cstdio>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace XOffsetDatastructure {
    namespace detail::freeze {
        namespace ev = detail::evolve;

        struct LockVersion {
            std::string label, ns, header;
        };
        struct LockRoot {
            std::string version, schema_name, type;
            std::uint64_t schema_hash = 0, name_digest = 0;
            std::string signature;
        };
        struct LockItem {            // record or enum of a snapshot
            std::string version, kind, type, name, a, b;
        };
        struct Lock {
            std::vector<LockVersion> versions;
            std::vector<LockRoot> roots;
            std::vector<LockItem> items;
        };

        inline std::string hex(std::uint64_t v) {
            char buf[19];
            std::snprintf(buf, sizeof buf, "0x%016llx", static_cast<unsigned long long>(v));
            return buf;
        }

        inline std::vector<std::string> split_tabs(const std::string& line) {
            std::vector<std::string> out;
            std::size_t start = 0;
            for (;;) {
                const std::size_t tab = line.find('\t', start);
                out.push_back(line.substr(start, tab == std::string::npos ? std::string::npos : tab - start));
                if (tab == std::string::npos) return out;
                start = tab + 1;
            }
        }

        inline bool read_lock(const std::string& path, Lock& lock, std::string& error) {
            std::ifstream in(path);
            if (!in) return true;    // no lock yet: no versions
            std::string line;
            std::size_t n = 0;
            while (std::getline(in, line)) {
                ++n;
                if (line.empty() || line[0] == '#') continue;
                const auto f = split_tabs(line);
                const std::string current = lock.versions.empty() ? std::string() : lock.versions.back().label;
                if (f[0] == "version" && f.size() == 4) {
                    lock.versions.push_back({f[1], f[2], f[3]});
                } else if (f[0] == "root" && f.size() == 6 && !current.empty()) {
                    lock.roots.push_back({current, f[1], f[2], std::stoull(f[3], nullptr, 16),
                                          std::stoull(f[4], nullptr, 16), f[5]});
                } else if ((f[0] == "record" || f[0] == "enum") && f.size() >= 4 && !current.empty()) {
                    lock.items.push_back({current, f[0], f[1], f[2], f[3], f.size() > 4 ? f[4] : std::string()});
                } else {
                    error = path + ":" + std::to_string(n) + ": not a schema lock line";
                    return false;
                }
            }
            return true;
        }

        // ---- spelling the current schema as C++ ----

        struct Snapshot {
            std::string ns;                       // new snapshot namespace
            const Lock* lock = nullptr;
            const LockVersion* previous = nullptr;
            std::vector<const void*> done;        // emitted types
            std::map<std::string, const void*> names;  // snapshot-local name -> type
            std::string body;                     // definitions, dependencies first
            std::string records;                  // XOFFSET_RECORD lines of the new records, same order
            std::vector<LockItem> items;          // lines for the lock file
            std::string error;
        };

        // A record or enum of the previous snapshot with the same name and
        // the same definition, or "".
        inline std::string reuse(const Snapshot& s, std::string_view kind, std::string_view name,
                                 std::string_view a, std::string_view b) {
            if (!s.previous) return {};
            for (const auto& it : s.lock->items)
                if (it.version == s.previous->label && it.kind == kind && it.name == name && it.a == a && it.b == b)
                    return it.type;
            return {};
        }

        template <typename T>
        std::string local_name() {
            return std::string(schema::identifier_v<T>);
        }

        template <typename T>
        bool claim(Snapshot& s) {
            const void* tag = ev::type_tag<T>();
            for (const void* d : s.done)
                if (d == tag) return false;
            s.done.push_back(tag);
            const std::string name = local_name<T>();
            auto [it, inserted] = s.names.emplace(name, tag);
            if (!inserted && it->second != tag && s.error.empty())
                s.error = "two different types are both named '" + name + "'; a snapshot needs distinct names";
            return true;
        }

        template <typename T>
        std::string spell(Snapshot& s);

        template <typename T>
        void define(Snapshot& s);

        // The library's container templates, spelled with their element types.
        template <typename T>
        struct container {
            static constexpr bool known = false;
        };
#define XOFFSET_FREEZE_CONTAINER_(Tmpl)                                                     \
        template <typename E>                                                               \
        struct container<Tmpl<E>> {                                                         \
            static constexpr bool known = true;                                             \
            static std::string spell(Snapshot& s) {                                         \
                return "XOffsetDatastructure::" #Tmpl "<" + freeze::spell<E>(s) + ">";       \
            }                                                                               \
        };
        XOFFSET_FREEZE_CONTAINER_(XVector)
        XOFFSET_FREEZE_CONTAINER_(XFixedVector)
        XOFFSET_FREEZE_CONTAINER_(XSet)
        XOFFSET_FREEZE_CONTAINER_(XFixedFlatSet)
#undef XOFFSET_FREEZE_CONTAINER_
#define XOFFSET_FREEZE_CONTAINER_(Tmpl)                                                     \
        template <typename K, typename V>                                                   \
        struct container<Tmpl<K, V>> {                                                      \
            static constexpr bool known = true;                                             \
            static std::string spell(Snapshot& s) {                                         \
                return "XOffsetDatastructure::" #Tmpl "<" + freeze::spell<K>(s) + ", " +     \
                       freeze::spell<V>(s) + ">";                                           \
            }                                                                               \
        };
        XOFFSET_FREEZE_CONTAINER_(XMap)
        XOFFSET_FREEZE_CONTAINER_(XFixedFlatMap)
#undef XOFFSET_FREEZE_CONTAINER_

        template <typename E>
        void define_enum(Snapshot& s) {
            if (!claim<E>(s)) return;
            using U = std::underlying_type_t<E>;
            std::string text = "enum class " + local_name<E>() + " : " + spell<U>(s) + " {";
            [&]<std::size_t... Is>(std::index_sequence<Is...>) {
                auto one = [&]<std::size_t I>() {
                    using e = schema::enumerator_t<E, I>;
                    if (I) text += ',';
                    text += ' ';
                    text += e::name;
                    text += " = ";
                    if constexpr (std::is_signed_v<U>)
                        text += std::to_string(static_cast<long long>(static_cast<U>(e::value)));
                    else
                        text += std::to_string(static_cast<unsigned long long>(static_cast<U>(e::value)));
                };
                (one.template operator()<Is>(), ...);
            }(std::make_index_sequence<schema::enumerator_count_v<E>>{});
            text += " };";
            const std::string name = local_name<E>();
            const std::string prev = reuse(s, "enum", name, text, "");
            if (!prev.empty()) {
                s.body += "using " + name + " = " + prev + ";\n";
                s.items.push_back({"", "enum", prev, name, text, ""});
            } else {
                s.body += text + "\n";
                s.items.push_back({"", "enum", s.ns + "::" + name, name, text, ""});
            }
        }

        template <typename R>
        void define_record(Snapshot& s) {
            if (!claim<R>(s)) return;
            // Dependencies first.
            std::vector<std::string> fields;
            [&]<std::size_t... Is>(std::index_sequence<Is...>) {
                auto one = [&]<std::size_t I>() {
                    using F = ev::field_t<R, I>;
                    std::string decl;
                    if constexpr (std::is_array_v<F>) {
                        std::string extents;
                        using B = std::remove_all_extents_t<F>;
                        [&]<typename A>(this auto&& self, std::type_identity<A>) -> void {
                            if constexpr (std::is_array_v<A>) {
                                extents += "[" + std::to_string(std::extent_v<A>) + "]";
                                self(std::type_identity<std::remove_extent_t<A>>{});
                            }
                        }(std::type_identity<F>{});
                        decl = spell<B>(s) + " " + std::string(ev::field_name_v<R, I>) + extents + ";";
                    } else {
                        decl = spell<F>(s) + " " + std::string(ev::field_name_v<R, I>) + ";";
                    }
                    fields.push_back(std::move(decl));
                };
                (one.template operator()<Is>(), ...);
            }(std::make_index_sequence<ev::field_count<R>()>{});
            const std::string name = local_name<R>();
            const std::string sig(wire_abi_signature<R>());
            const std::string names = schema_name_signature<R>();
            const std::string prev = reuse(s, "record", name, sig, names);
            if (!prev.empty()) {
                s.body += "using " + name + " = " + prev + ";\n";
                s.items.push_back({"", "record", prev, name, sig, names});
                return;
            }
            s.body += "struct alignas(" + std::to_string(alignof(R)) + ") " + name + " {\n";
            for (const auto& f : fields) s.body += "    " + f + "\n";
            s.body += "};\n";
            // The field list, so builds without reflection (C++20 clients) can
            // use the snapshot; a C++26 build checks it against reflection.
            s.records += "XOFFSET_RECORD(" + s.ns + "::" + name;
            [&]<std::size_t... Is>(std::index_sequence<Is...>) {
                ((s.records += ", " + std::string(ev::field_name_v<R, Is>)), ...);
            }(std::make_index_sequence<ev::field_count<R>()>{});
            s.records += ")\n";
            s.items.push_back({"", "record", s.ns + "::" + name, name, sig, names});
        }

        template <typename T>
        std::string spell(Snapshot& s) {
            using U = std::remove_cv_t<T>;
            if constexpr (std::is_same_v<U, bool>) return "bool";
            else if constexpr (std::is_same_v<U, char>) return "char";
            else if constexpr (std::is_same_v<U, std::byte>) return "std::byte";
            else if constexpr (std::is_same_v<U, signed char>) return "std::int8_t";
            else if constexpr (std::is_same_v<U, unsigned char>) return "std::uint8_t";
            else if constexpr (std::is_same_v<U, std::int16_t>) return "std::int16_t";
            else if constexpr (std::is_same_v<U, std::uint16_t>) return "std::uint16_t";
            else if constexpr (std::is_same_v<U, std::int32_t>) return "std::int32_t";
            else if constexpr (std::is_same_v<U, std::uint32_t>) return "std::uint32_t";
            else if constexpr (std::is_same_v<U, std::int64_t>) return "std::int64_t";
            else if constexpr (std::is_same_v<U, std::uint64_t>) return "std::uint64_t";
            else if constexpr (std::is_same_v<U, float>) return "float";
            else if constexpr (std::is_same_v<U, double>) return "double";
            else if constexpr (std::is_enum_v<U>) {
                define_enum<U>(s);
                return local_name<U>();
            } else if constexpr (std::is_same_v<U, XString>) return "XOffsetDatastructure::XString";
            else if constexpr (std::is_same_v<U, XFixedString>) return "XOffsetDatastructure::XFixedString";
            else if constexpr (container<U>::known) return container<U>::spell(s);
            else if constexpr (detail::schema_names::is_std_array<U>::value) {
                return "std::array<" + spell<typename U::value_type>(s) + ", " + std::to_string(std::tuple_size_v<U>) + ">";
            } else if constexpr (ev::kind_of<U>() == ev::Kind::record) {
                define_record<U>(s);
                return local_name<U>();
            } else {
                static_assert(sizeof(U) == 0, "schema_freeze: non-admitted wire type");
                return {};
            }
        }

        // ---- commands ----

        template <typename W>
        std::string root_line(const std::string& type) {
            return "root\t" + std::string(schema_name<W>::value) + "\t" + type + "\t" +
                   hex(wire_schema_hash_v<W>()) + "\t" + hex(schema_name_digest_v<W>) + "\t" +
                   std::string(std::string_view(wire_abi_signature<W>())) + "\n";
        }

        inline std::string escape(std::string_view text) {
            std::string out;
            for (char c : text) {
                if (c == '"' || c == '\\') out += '\\';
                out += c;
            }
            return out;
        }

        template <typename... Roots>
        int freeze(const std::string& label, const std::string& ns, const std::string& header,
                   const std::string& lock_path) {
            Lock lock;
            std::string error;
            if (!read_lock(lock_path, lock, error)) {
                std::fprintf(stderr, "schema_freeze: %s\n", error.c_str());
                return 2;
            }
            for (const auto& v : lock.versions)
                if (v.label == label || v.ns == ns) {
                    std::fprintf(stderr, "schema_freeze: version %s / namespace %s is already in %s\n",
                                 label.c_str(), ns.c_str(), lock_path.c_str());
                    return 2;
                }
            Snapshot s;
            s.ns = ns;
            s.lock = &lock;
            s.previous = lock.versions.empty() ? nullptr : &lock.versions.back();
            std::string roots_text, lock_roots;
            auto one = [&]<typename W>() {
                const std::string local = spell<W>(s);
                std::string type = ns + "::" + local;
                for (const auto& it : s.items)
                    if (it.kind == "record" && it.name == local) type = it.type;
                if (type.starts_with(ns + "::")) {
                    roots_text += "XOFFSET_REGISTER_SCHEMA_NAME(" + type + ", \"" +
                                  escape(schema_name<W>::value) + "\")\n";
                    roots_text += "XOFFSET_PIN_SCHEMA(" + type + ", \"" +
                                  escape(std::string_view(wire_abi_signature<W>())) + "\");\n";
                } else {
                    roots_text += "// " + std::string(schema_name<W>::value) + ": unchanged since " + type + "\n";
                }
                lock_roots += root_line<W>(type);
            };
            (one.template operator()<Roots>(), ...);
            if (!s.error.empty()) {
                std::fprintf(stderr, "schema_freeze: %s\n", s.error.c_str());
                return 2;
            }
            std::string guard = "XOFFSET_SNAPSHOT_" + ns;
            for (char& c : guard) c = (c >= 'a' && c <= 'z') ? char(c - 32) : (std::isalnum(static_cast<unsigned char>(c)) ? c : '_');
            std::string out;
            out += "// Generated by tools/schema_freeze (xoffset_schema_freeze.hpp): the frozen schema of\n";
            out += "// version " + label + ". Do not edit: the pins below fail the build if this changes.\n";
            out += "#ifndef " + guard + "\n#define " + guard + "\n\n";
            out += "#include \"xoffsetdatastructure.hpp\"\n";
            if (s.previous) out += "#include \"" + s.previous->header + "\"\n";
            out += "\n#include <cstddef>\n#include <cstdint>\n\n";
            out += "namespace " + ns + " {\n" + s.body + "} // namespace " + ns + "\n\n";
            if (!s.records.empty()) out += s.records + "\n";
            out += roots_text;
            out += "\n#endif\n";
            {
                std::ofstream h(header, std::ios::binary | std::ios::trunc);
                if (!(h << out)) {
                    std::fprintf(stderr, "schema_freeze: cannot write %s\n", header.c_str());
                    return 2;
                }
            }
            std::ofstream l(lock_path, std::ios::binary | std::ios::app);
            if (lock.versions.empty())
                l << "# XOffset schema lock (tools/schema_freeze). Released versions, oldest first. Do not edit.\n";
            l << "version\t" << label << "\t" << ns << "\t" << header << "\n" << lock_roots;
            for (const auto& it : s.items) l << it.kind << "\t" << it.type << "\t" << it.name << "\t" << it.a
                                             << (it.kind == "record" ? "\t" + it.b : std::string()) << "\n";
            if (!l) {
                std::fprintf(stderr, "schema_freeze: cannot write %s\n", lock_path.c_str());
                return 2;
            }
            std::printf("schema_freeze: wrote %s and version %s to %s\n", header.c_str(), label.c_str(),
                        lock_path.c_str());
            return 0;
        }

        template <typename W>
        bool check_root(const Lock& lock, std::string& report) {
            const std::string name(schema_name<W>::value);
            const LockRoot* last = nullptr;
            bool ok = true;
            for (const auto& r : lock.roots) {
                if (r.schema_name != name) continue;
                last = &r;
                if (r.schema_hash == wire_schema_hash_v<W>() && !ev::name_digest_matches<W>(r.name_digest)) {
                    report += "FAIL " + name + ": same layout as frozen " + r.type +
                              " but other field names (current " + schema_name_signature<W>() +
                              "); a rename keeps the layout, so old peers would read the bytes under the new "
                              "names. Revert the rename, or register XOFFSET_SCHEMA_NAME_ALIAS(Current, " +
                              hex(r.name_digest) + ") when the bytes keep their meaning.\n";
                    ok = false;
                }
            }
            if (!last) {
                report += "note " + name + ": no frozen version yet\n";
                return ok;
            }
            if (last->schema_hash != wire_schema_hash_v<W>() && ev::find_version<W>(last->schema_hash) == ev::npos) {
                report += "FAIL " + name + ": the layout changed since " + last->version + " (" + last->type +
                          "), which is not in XOFFSET_SCHEMA_LINEAGE of the current type; peers on " +
                          last->version + " would be rejected.\n";
                ok = false;
            }
            if (ok) {
                report += "ok   " + name + ": " +
                          (last->schema_hash == wire_schema_hash_v<W>() ? "equals " + last->version
                                                                         : "next version after " + last->version) +
                          "\n";
            }
            return ok;
        }

        template <typename... Roots>
        int check(const std::string& lock_path) {
            Lock lock;
            std::string error;
            if (!read_lock(lock_path, lock, error)) {
                std::fprintf(stderr, "schema_freeze: %s\n", error.c_str());
                return 2;
            }
            std::string report;
            const bool ok = (check_root<Roots>(lock, report) & ...);
            std::fputs(report.c_str(), ok ? stdout : stderr);
            return ok ? 0 : 1;
        }
    } // namespace detail::freeze

    // The snapshot text of the current Roots (what `freeze` writes), for tests.
    template <typename... Roots>
    std::string schema_snapshot_body(std::string_view ns) {
        detail::freeze::Snapshot s;
        detail::freeze::Lock empty;
        s.ns = std::string(ns);
        s.lock = &empty;
        (detail::freeze::spell<Roots>(s), ...);
        return s.body;
    }

    template <typename... Roots>
    int schema_freeze_main(int argc, char** argv) {
        const std::string cmd = argc > 1 ? argv[1] : "";
        if (cmd == "freeze" && argc == 6) return detail::freeze::freeze<Roots...>(argv[2], argv[3], argv[4], argv[5]);
        if (cmd == "check" && argc == 3) return detail::freeze::check<Roots...>(argv[2]);
        std::fprintf(stderr,
                     "usage: %s freeze <label> <namespace> <out-header> <lock-file>\n"
                     "       %s check <lock-file>\n",
                     argc ? argv[0] : "schema_freeze", argc ? argv[0] : "schema_freeze");
        return 2;
    }
} // namespace XOffsetDatastructure

#endif // X_OFFSET_SCHEMA_FREEZE_HPP
