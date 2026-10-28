#ifndef X_OFFSET_HANDSHAKE_HPP
#define X_OFFSET_HANDSHAKE_HPP

// Schema handshake (both ends; no evolution code).
//
// When a connection opens, each side sends one XSchemaHello listing, per root
// type, the schema it works in: root_type_id and schema_hash (the wire
// fingerprint) plus the field-name digest (xoffset_schema_names.hpp). A server
// also lists the older versions it still converts (kind = known). The side
// that knows both versions answers with an XSchemaHelloAck: same, convert,
// reject_unknown (no such version) or reject_names (the version's layout is
// known but its field names differ: two builds disagree on what the bytes
// mean). A client that gets a rejection asks the player to update instead of
// failing on its first message.
//
// Both messages are ordinary wire 2.1 payloads of fixed library types,
// verified by the same validator as every other message; their schema names
// ("xoffset.hello", "xoffset.hello.ack") never change. The library does not
// frame messages: send the hello as the first message of the connection, in
// whatever framing the application uses (see examples/schema_handshake).
//
// Client side, no exceptions escape:
//   std::string hello = make_schema_hello<Player, Chat>("ios-1.12.0");
//   XSchemaAckResult r = read_schema_ack<Player, Chat>(ack_bytes);
//   if (!r) show_update_prompt(r.message);
//
// Accepting side: accept_schema_hello<Player, Chat>(hello, ack) accepts the
// exact versions only; XPeerSchemas::accept (xoffset_evolution.hpp, servers)
// also accepts the older versions of each root type's lineage and converts.
//
// peek_wire_fingerprint(bytes) reads only the header of any wire (magic and
// version, length, flags) and returns its fingerprint, without throwing. It
// selects which version's load_verified to run; that load still verifies the
// whole wire from the start.

#include "xoffsetdatastructure.hpp"
#include "xoffset_schema_names.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace XOffsetDatastructure {

    // ---- header peek ------------------------------------------------------

    struct XWireFingerprint {
        std::uint64_t root_type_id = 0;
        std::uint64_t schema_hash = 0;
        friend constexpr bool operator==(const XWireFingerprint&, const XWireFingerprint&) = default;
    };

    template <typename T>
    constexpr XWireFingerprint wire_fingerprint_v{wire_root_type_id_v<T>(), wire_schema_hash_v<T>()};

    enum class XPeekStatus : std::uint8_t { ok, bad_magic, unsupported_version, truncated, unsupported_flags };

    struct XWirePeek {
        XPeekStatus status = XPeekStatus::truncated;
        XWireFingerprint fingerprint{};
        explicit operator bool() const noexcept { return status == XPeekStatus::ok; }
    };

    // The checks read_wire_header makes, in the same order, without throwing.
    inline XWirePeek peek_wire_fingerprint(const void* data, std::size_t size) noexcept {
        XWirePeek out;
        const auto* b = static_cast<const unsigned char*>(data);
        if (size < 4 || b[0] != 'X' || b[1] != 'O') { out.status = XPeekStatus::bad_magic; return out; }
        if (b[2] != XWIRE_FORMAT_MAJOR || b[3] != XWIRE_FORMAT_MINOR) {
            out.status = XPeekStatus::unsupported_version;
            return out;
        }
        if (size < sizeof(XWireHeaderV2)) { out.status = XPeekStatus::truncated; return out; }
        XWireHeaderV2 header;
        std::memcpy(&header, data, sizeof(header));
        if (header.flags != XWIRE_FLAGS) { out.status = XPeekStatus::unsupported_flags; return out; }
        out.status = XPeekStatus::ok;
        out.fingerprint = {header.root_type_id, header.schema_hash};
        return out;
    }

    inline XWirePeek peek_wire_fingerprint(std::span<const std::byte> wire) noexcept {
        return peek_wire_fingerprint(wire.data(), wire.size());
    }

    inline XWirePeek peek_wire_fingerprint(std::string_view wire) noexcept {
        return peek_wire_fingerprint(wire.data(), wire.size());
    }

    // ---- handshake messages -------------------------------------------------

    inline constexpr std::uint32_t XSCHEMA_HELLO_WIRE_FORMAT =
        (std::uint32_t(XWIRE_FORMAT_MAJOR) << 8) | XWIRE_FORMAT_MINOR;   // 0x0201
    // Receive limits of a handshake message.
    inline constexpr std::size_t XSCHEMA_HELLO_MAX_BYTES = 64 * 1024;
    inline constexpr std::size_t XSCHEMA_HELLO_MAX_ENTRIES = 1024;
    inline constexpr std::size_t XSCHEMA_HELLO_MAX_LABEL = 256;

    enum class XSchemaRole : std::uint32_t { client = 0, server = 1 };
    enum class XSchemaEntryKind : std::uint32_t { working = 0, known = 1 };

    struct XSchemaEntry {                   // 32 bytes
        std::uint64_t root_type_id;
        std::uint64_t schema_hash;
        std::uint64_t name_digest;
        std::uint32_t kind;                 // XSchemaEntryKind
        std::uint32_t reserved;             // 0
    };

    struct XSchemaHello {
        std::uint32_t wire_format;          // XSCHEMA_HELLO_WIRE_FORMAT
        std::uint32_t role;                 // XSchemaRole
        XVector<XSchemaEntry> entries;
        XString label;                      // for logs only, e.g. "ios-1.12.0"
    };

    enum class XSchemaVerdict : std::uint32_t {
        same = 0,             // the peer works in this version too
        convert = 1,          // the acknowledging side converts both ways
        reject_unknown = 2,   // no such root type or version
        reject_names = 3,     // same layout, other field names
    };

    struct XSchemaAckEntry {                // 24 bytes
        std::uint64_t root_type_id;
        std::uint64_t schema_hash;          // the hello's working version
        std::uint32_t verdict;              // XSchemaVerdict
        std::uint32_t reserved;             // 0
    };

    struct XSchemaHelloAck {
        std::uint32_t wire_format;
        std::uint32_t role;                 // of the acknowledging side
        XVector<XSchemaAckEntry> entries;   // one per working entry of the hello
        XString label;
    };
} // namespace XOffsetDatastructure

XOFFSET_RECORD(XOffsetDatastructure::XSchemaEntry, root_type_id, schema_hash, name_digest, kind, reserved)
XOFFSET_RECORD(XOffsetDatastructure::XSchemaHello, wire_format, role, entries, label)
XOFFSET_RECORD(XOffsetDatastructure::XSchemaAckEntry, root_type_id, schema_hash, verdict, reserved)
XOFFSET_RECORD(XOffsetDatastructure::XSchemaHelloAck, wire_format, role, entries, label)

namespace XOffsetDatastructure {

    inline const char* to_string(XSchemaVerdict v) noexcept {
        switch (v) {
            case XSchemaVerdict::same: return "same";
            case XSchemaVerdict::convert: return "convert";
            case XSchemaVerdict::reject_unknown: return "reject_unknown";
            case XSchemaVerdict::reject_names: return "reject_names";
        }
        return "invalid";
    }

    inline constexpr bool accepted(XSchemaVerdict v) noexcept {
        return v == XSchemaVerdict::same || v == XSchemaVerdict::convert;
    }

    template <typename T>
    constexpr XSchemaEntry schema_entry(XSchemaEntryKind kind = XSchemaEntryKind::working) noexcept {
        return {wire_root_type_id_v<T>(), wire_schema_hash_v<T>(), schema_name_digest_v<T>,
                static_cast<std::uint32_t>(kind), 0};
    }

    // ---- client side --------------------------------------------------------

    // Hello listing the working version of each root type.
    template <typename... Roots>
    std::string make_schema_hello(std::string_view label, XSchemaRole role = XSchemaRole::client);

    struct XSchemaAckResult {
        enum class Status : std::uint8_t {
            ok,          // every root type: same or convert
            malformed,   // not a valid XSchemaHelloAck (message says why)
            rejected,    // a root type was rejected (root_type_id, verdict)
            missing,     // the ack has no entry for a root type
        };
        Status status = Status::malformed;
        std::uint64_t root_type_id = 0;
        XSchemaVerdict verdict = XSchemaVerdict::reject_unknown;
        std::string message;
        explicit operator bool() const noexcept { return status == Status::ok; }
    };

    namespace detail::handshake {
        template <typename Message, typename Fill>
        std::string write(Fill&& fill) {
            auto buffer = TypedXBuffer<Message>::create(
                4096, XBuffer::maximum_capacity(4 * XSCHEMA_HELLO_MAX_BYTES).with_growth(XGrowPolicy::Automatic));
            fill(buffer.root());
            return buffer.save_compact(XCompactOptions{.slack_div = 0});
        }

        inline std::string_view clip_label(std::string_view label) noexcept {
            return label.size() > XSCHEMA_HELLO_MAX_LABEL ? label.substr(0, XSCHEMA_HELLO_MAX_LABEL) : label;
        }

        inline XBuffer::MaximumCapacity receive_limit() noexcept {
            return XBuffer::maximum_capacity(XSCHEMA_HELLO_MAX_BYTES);
        }

        // Loads a handshake message; false (and `error`) when it is not one.
        // Never throws: the loaders that throw are wrapped here.
        template <typename Message>
        bool load(const void* data, std::size_t size, TypedXBuffer<Message>& out, std::string& error) noexcept {
            if (size > XSCHEMA_HELLO_MAX_BYTES) {
                error = "handshake message larger than XSCHEMA_HELLO_MAX_BYTES";
                return false;
            }
#if XOFFSET_NO_EXCEPTIONS
            auto loaded = TypedXBuffer<Message>::try_load_verified(
                std::span<const std::byte>(static_cast<const std::byte*>(data), size), receive_limit());
            if (!loaded) {
                error = std::move(loaded).error().message;
                return false;
            }
            out = std::move(*loaded);
#else
            try {
                out = TypedXBuffer<Message>::load_verified(
                    std::span<const std::byte>(static_cast<const std::byte*>(data), size), receive_limit());
            } catch (const std::exception& e) {
                try { error = e.what(); } catch (...) {}
                return false;
            } catch (...) {
                return false;
            }
#endif
            const Message& m = out.root();
            if (m.wire_format != XSCHEMA_HELLO_WIRE_FORMAT) {
                error = "handshake from another wire format";
                return false;
            }
            if (m.entries.size() > XSCHEMA_HELLO_MAX_ENTRIES || m.label.size() > XSCHEMA_HELLO_MAX_LABEL) {
                error = "handshake message exceeds its entry or label limit";
                return false;
            }
            return true;
        }
    } // namespace detail::handshake

    // ---- accepting side ------------------------------------------------------

    struct XSchemaPeerEntry {
        std::uint64_t root_type_id = 0;
        std::uint64_t schema_hash = 0;      // the peer's working version
        XSchemaVerdict verdict = XSchemaVerdict::reject_unknown;
    };

    struct XSchemaHelloResult {
        bool valid = false;                 // the hello was a valid handshake message
        std::string message;                // why not, when !valid
        XSchemaRole role = XSchemaRole::client;
        std::string label;
        std::vector<XSchemaPeerEntry> entries;  // one per working entry of the hello

        // Valid, and every root type the peer works in is same or convert.
        bool accepted() const noexcept {
            if (!valid) return false;
            for (const auto& e : entries)
                if (!XOffsetDatastructure::accepted(e.verdict)) return false;
            return true;
        }
        const XSchemaPeerEntry* find(std::uint64_t root_type_id) const noexcept {
            for (const auto& e : entries)
                if (e.root_type_id == root_type_id) return &e;
            return nullptr;
        }
    };

    namespace detail::handshake {
        // Verifies a hello, decides each working entry with
        // decide(entry, hello) -> XSchemaVerdict, and writes the ack. A hello
        // that is not valid leaves `ack_out` empty.
        template <typename Decide>
        XSchemaHelloResult accept(const void* data, std::size_t size, std::string& ack_out,
                                  std::string_view label, Decide&& decide) {
            XSchemaHelloResult result;
            ack_out.clear();
            TypedXBuffer<XSchemaHello> buffer;
            if (!load(data, size, buffer, result.message)) {
                if (result.message.empty()) result.message = "not a schema handshake hello";
                return result;
            }
            const XSchemaHello& hello = buffer.root();
            result.valid = true;
            result.role = hello.role == 1 ? XSchemaRole::server : XSchemaRole::client;
            result.label.assign(hello.label.view());
            for (const XSchemaEntry& e : hello.entries) {
                if (e.kind != static_cast<std::uint32_t>(XSchemaEntryKind::working)) continue;
                result.entries.push_back({e.root_type_id, e.schema_hash, decide(e, hello)});
            }
            ack_out = write<XSchemaHelloAck>([&](XSchemaHelloAck& ack) {
                ack.wire_format = XSCHEMA_HELLO_WIRE_FORMAT;
                ack.role = static_cast<std::uint32_t>(XSchemaRole::server);
                ack.entries.reserve(result.entries.size());
                for (const auto& e : result.entries)
                    ack.entries.push_back({e.root_type_id, e.schema_hash, static_cast<std::uint32_t>(e.verdict), 0});
                ack.label.assign(clip_label(label));
            });
            return result;
        }

        // Without schema evolution: only the exact version is accepted.
        template <typename T>
        XSchemaVerdict decide_exact(const XSchemaEntry& e) noexcept {
            if (e.schema_hash != wire_schema_hash_v<T>()) return XSchemaVerdict::reject_unknown;
            return e.name_digest == schema_name_digest_v<T> ? XSchemaVerdict::same : XSchemaVerdict::reject_names;
        }
    } // namespace detail::handshake

    // Accepting side without schema evolution (xoffset_evolution.hpp has
    // XPeerSchemas, which also accepts older versions): each root type the
    // peer works in is `same` when its fingerprint and field-name digest equal
    // this program's, else rejected. Writes the ack into `ack_out` (empty when
    // the hello is not valid). Never throws, except std::bad_alloc.
    template <typename... Roots>
    XSchemaHelloResult accept_schema_hello(const void* data, std::size_t size, std::string& ack_out,
                                           std::string_view label = {}) {
        return detail::handshake::accept(data, size, ack_out, label,
            [](const XSchemaEntry& e, const XSchemaHello&) {
                XSchemaVerdict v = XSchemaVerdict::reject_unknown;
                ((e.root_type_id == wire_root_type_id_v<Roots>()
                      ? (void)(v = detail::handshake::decide_exact<Roots>(e)) : (void)0), ...);
                return v;
            });
    }

    template <typename... Roots>
    XSchemaHelloResult accept_schema_hello(std::string_view hello, std::string& ack_out, std::string_view label = {}) {
        return accept_schema_hello<Roots...>(hello.data(), hello.size(), ack_out, label);
    }

    template <typename... Roots>
    XSchemaHelloResult accept_schema_hello(std::span<const std::byte> hello, std::string& ack_out,
                                           std::string_view label = {}) {
        return accept_schema_hello<Roots...>(hello.data(), hello.size(), ack_out, label);
    }

    template <typename... Roots>
    std::string make_schema_hello(std::string_view label, XSchemaRole role) {
        return detail::handshake::write<XSchemaHello>([&](XSchemaHello& h) {
            h.wire_format = XSCHEMA_HELLO_WIRE_FORMAT;
            h.role = static_cast<std::uint32_t>(role);
            h.entries.reserve(sizeof...(Roots));
            (h.entries.push_back(schema_entry<Roots>()), ...);
            h.label.assign(detail::handshake::clip_label(label));
        });
    }

    // Reads the peer's ack to a hello of make_schema_hello<Roots...>(): ok when
    // every root type is same or convert. Never throws (except std::bad_alloc
    // for the message text).
    template <typename... Roots>
    XSchemaAckResult read_schema_ack(const void* data, std::size_t size) {
        XSchemaAckResult r;
        TypedXBuffer<XSchemaHelloAck> buffer;
        if (!detail::handshake::load(data, size, buffer, r.message)) {
            r.status = XSchemaAckResult::Status::malformed;
            if (r.message.empty()) r.message = "not a schema handshake ack";
            return r;
        }
        const XSchemaHelloAck& ack = buffer.root();
        auto check = [&](const XWireFingerprint fp) -> bool {
            for (const XSchemaAckEntry& e : ack.entries) {
                if (e.root_type_id != fp.root_type_id || e.schema_hash != fp.schema_hash) continue;
                const auto v = static_cast<XSchemaVerdict>(e.verdict);
                if (accepted(v)) return true;
                r.status = XSchemaAckResult::Status::rejected;
                r.root_type_id = fp.root_type_id;
                r.verdict = e.verdict <= 3 ? v : XSchemaVerdict::reject_unknown;
                r.message = std::string("peer rejected a root type: ") + to_string(r.verdict);
                return false;
            }
            r.status = XSchemaAckResult::Status::missing;
            r.root_type_id = fp.root_type_id;
            r.message = "peer's ack has no entry for a root type";
            return false;
        };
        if (!(check(wire_fingerprint_v<Roots>) && ...)) return r;
        r.status = XSchemaAckResult::Status::ok;
        r.message.clear();
        return r;
    }

    template <typename... Roots>
    XSchemaAckResult read_schema_ack(std::string_view ack) {
        return read_schema_ack<Roots...>(ack.data(), ack.size());
    }

    template <typename... Roots>
    XSchemaAckResult read_schema_ack(std::span<const std::byte> ack) {
        return read_schema_ack<Roots...>(ack.data(), ack.size());
    }

} // namespace XOffsetDatastructure

XOFFSET_REGISTER_SCHEMA_NAME(XOffsetDatastructure::XSchemaHello, "xoffset.hello")
XOFFSET_REGISTER_SCHEMA_NAME(XOffsetDatastructure::XSchemaHelloAck, "xoffset.hello.ack")

// The handshake never changes; a different layout would be a new message
// with a new schema name.
XOFFSET_PIN_SCHEMA(XOffsetDatastructure::XSchemaHello, "record[s:32,a:4]{@0:u32,@4:u32,@8:xvector[s:12,a:4]{data:rel32,size:u32,capacity:u32}<record[s:32,a:8]{@0:u64,@8:u64,@16:u64,@24:u32,@28:u32}>,@20:xstring[s:12,a:4]{data:rel32,size:u32,capacity:u32}{inline:10}}");
XOFFSET_PIN_SCHEMA(XOffsetDatastructure::XSchemaHelloAck, "record[s:32,a:4]{@0:u32,@4:u32,@8:xvector[s:12,a:4]{data:rel32,size:u32,capacity:u32}<record[s:24,a:8]{@0:u64,@8:u64,@16:u32,@20:u32}>,@20:xstring[s:12,a:4]{data:rel32,size:u32,capacity:u32}{inline:10}}");
static_assert(XOffsetDatastructure::schema_name_digest_v<XOffsetDatastructure::XSchemaHello> == 0x310402c4ba072bceull,
    "the handshake's field-name digest is frozen");
static_assert(XOffsetDatastructure::schema_name_digest_v<XOffsetDatastructure::XSchemaHelloAck> == 0xf6ebf29df2d597c3ull,
    "the handshake's field-name digest is frozen");

#endif // X_OFFSET_HANDSHAKE_HPP
