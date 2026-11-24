#ifndef X_OFFSET_DUMP_HPP
#define X_OFFSET_DUMP_HPP

// Generic JSON diagnostics for XOffset records and wire headers.
// Works under both schema backends (C++20/23 XOFFSET_RECORD and C++26
// reflection) and in builds without exceptions. Details: docs/USER_GUIDE.md.

#include "xoffsetdatastructure.hpp"

#include <array>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

namespace XOffsetDatastructure {

template <typename T> class XView;

struct XDumpOptions {
    std::uint8_t indent = 0;            // 0: compact single-line JSON; >0: spaces per nesting level
    std::size_t max_elements = 0;       // 0: unlimited; >0: cap sequence/map entries per container
    std::size_t max_string_bytes = 0;   // 0: unlimited; >0: cap emitted string length in bytes
};

namespace dump_detail {

inline void append_indent(std::string& out, const XDumpOptions& opts, std::size_t depth) {
    if (opts.indent == 0) return;
    out.push_back('\n');
    out.append(depth * static_cast<std::size_t>(opts.indent), ' ');
}

inline void append_hex_u64(std::string& out, std::uint64_t value) {
    static constexpr char hex[] = "0123456789abcdef";
    char buf[18];
    buf[0] = '0';
    buf[1] = 'x';
    for (int i = 0; i < 16; ++i) {
        buf[2 + i] = hex[(value >> ((15 - i) * 4)) & 0xFu];
    }
    out.append(buf, sizeof(buf));
}

inline void append_escaped_string(std::string& out, std::string_view text, std::size_t max_bytes) {
    static constexpr char hex[] = "0123456789abcdef";
    const bool truncated = max_bytes != 0 && text.size() > max_bytes;
    std::size_t cut = truncated ? max_bytes : text.size();
    // Avoid splitting a multi-byte UTF-8 sequence when truncating.
    if (truncated && cut < text.size() && (static_cast<unsigned char>(text[cut]) & 0xC0u) == 0x80u) {
        while (cut > 0 && (static_cast<unsigned char>(text[cut - 1]) & 0xC0u) == 0x80u) --cut;
        if (cut > 0 && (static_cast<unsigned char>(text[cut - 1]) & 0xC0u) == 0xC0u) --cut;
    }
    const std::string_view shown = text.substr(0, cut);
    out.push_back('"');
    for (unsigned char ch : shown) {
        switch (ch) {
            case '"': out.append("\\\""); break;
            case '\\': out.append("\\\\"); break;
            case '\b': out.append("\\b"); break;
            case '\f': out.append("\\f"); break;
            case '\n': out.append("\\n"); break;
            case '\r': out.append("\\r"); break;
            case '\t': out.append("\\t"); break;
            default:
                if (ch < 0x20u) {
                    char esc[6] = {'\\', 'u', '0', '0', hex[(ch >> 4) & 0xFu], hex[ch & 0xFu]};
                    out.append(esc, sizeof(esc));
                } else {
                    out.push_back(static_cast<char>(ch));
                }
                break;
        }
    }
    if (truncated) out.append("...");
    out.push_back('"');
}

template <typename Int>
inline void append_integer(std::string& out, Int value) {
    char buf[32];
    if constexpr (std::is_signed_v<Int>) {
        const auto v = static_cast<long long>(value);
        auto [ptr, ec] = std::to_chars(buf, buf + sizeof(buf), v);
        if (ec == std::errc{}) out.append(buf, static_cast<std::size_t>(ptr - buf));
    } else {
        const auto v = static_cast<unsigned long long>(value);
        auto [ptr, ec] = std::to_chars(buf, buf + sizeof(buf), v);
        if (ec == std::errc{}) out.append(buf, static_cast<std::size_t>(ptr - buf));
    }
}

template <typename Float>
inline void append_float(std::string& out, Float value) {
    if (!std::isfinite(value)) {
        out.append("null");
        return;
    }
    if (value == Float(0)) {
        out.push_back('0');
        return;
    }
    char buf[64];
#if XOFFSET_DETAIL_TO_CHARS_FLOAT && defined(__cpp_lib_to_chars) && __cpp_lib_to_chars >= 201611L
    auto [ptr, ec] = std::to_chars(buf, buf + sizeof(buf), value);
    if (ec == std::errc{}) {
        out.append(buf, static_cast<std::size_t>(ptr - buf));
        return;
    }
#endif
    const int n = std::is_same_v<Float, float>
        ? std::snprintf(buf, sizeof(buf), "%.9g", static_cast<double>(value))
        : std::snprintf(buf, sizeof(buf), "%.17g", static_cast<double>(value));
    if (n > 0) out.append(buf, static_cast<std::size_t>(n));
    else out.append("null");
}

template <typename T>
struct is_std_array : std::false_type {};
template <typename T, std::size_t N>
struct is_std_array<std::array<T, N>> : std::true_type {};

template <typename T>
struct is_xvector : std::false_type {};
template <typename T>
struct is_xvector<XVector<T>> : std::true_type {};

template <typename T>
struct is_xset : std::false_type {};
template <typename T>
struct is_xset<XSet<T>> : std::true_type {};

template <typename T>
struct is_xmap : std::false_type {};
template <typename K, typename V>
struct is_xmap<XMap<K, V>> : std::true_type {};

template <typename T>
struct is_xview : std::false_type {};
template <typename T>
struct is_xview<XView<T>> : std::true_type {};

template <typename K>
constexpr bool map_key_is_object_key() noexcept {
    using Raw = std::remove_cvref_t<K>;
    return std::is_same_v<Raw, XString> || std::is_same_v<Raw, bool> ||
           std::is_same_v<Raw, std::byte> || std::is_integral_v<Raw> || std::is_enum_v<Raw>;
}

template <typename K>
inline void append_map_key(std::string& out, const K& key, const XDumpOptions& opts) {
    using Raw = std::remove_cvref_t<K>;
    if constexpr (std::is_same_v<Raw, XString>) {
        append_escaped_string(out, key.view(), opts.max_string_bytes);
    } else if constexpr (std::is_same_v<Raw, bool>) {
        out.append(key ? "\"true\"" : "\"false\"");
    } else if constexpr (std::is_same_v<Raw, std::byte>) {
        out.push_back('"');
        append_integer(out, std::to_integer<unsigned>(key));
        out.push_back('"');
    } else if constexpr (std::is_enum_v<Raw>) {
        out.push_back('"');
        append_integer(out, static_cast<std::underlying_type_t<Raw>>(key));
        out.push_back('"');
    } else {
        out.push_back('"');
        append_integer(out, key);
        out.push_back('"');
    }
}

template <typename T>
void dump_value(std::string& out, const T& value, const XDumpOptions& opts, std::size_t depth);

template <typename Range>
void dump_sequence(std::string& out, const Range& seq, std::size_t count, const XDumpOptions& opts,
                   std::size_t depth) {
    out.push_back('[');
    if (count == 0) {
        out.push_back(']');
        return;
    }
    const std::size_t limit = (opts.max_elements != 0 && count > opts.max_elements) ? opts.max_elements : count;
    std::size_t idx = 0;
    for (const auto& elem : seq) {
        if (idx >= limit) break;
        if (idx > 0) out.push_back(',');
        append_indent(out, opts, depth + 1);
        dump_value(out, elem, opts, depth + 1);
        ++idx;
    }
    if (count > limit) {
        if (limit > 0) out.push_back(',');
        append_indent(out, opts, depth + 1);
        out.append("\"... (");
        append_integer(out, count - limit);
        out.append(" more)\"");
    }
    append_indent(out, opts, depth);
    out.push_back(']');
}

template <typename T>
void dump_record_fields(std::string& out, const T& value, const XDumpOptions& opts, std::size_t depth, bool& first) {
    constexpr std::size_t bases = schema::base_count_v<T>;
    constexpr std::size_t members = schema::member_count_v<T>;
    [&]<std::size_t... Bs>(std::index_sequence<Bs...>) {
        (dump_record_fields(out, static_cast<const schema::base_t<T, Bs>&>(value), opts, depth, first), ...);
    }(std::make_index_sequence<bases>{});
    [&]<std::size_t... Ms>(std::index_sequence<Ms...>) {
        (([&] {
            using M = schema::member_t<T, Ms>;
            if (!first) out.push_back(',');
            first = false;
            append_indent(out, opts, depth + 1);
            if constexpr (!M::name.empty()) {
                append_escaped_string(out, M::name, 0);
            } else {
                out.append("\"field_");
                append_integer(out, Ms);
                out.push_back('"');
            }
            out.append(opts.indent ? ": " : ":");
            dump_value(out, M::get(value), opts, depth + 1);
        }()), ...);
    }(std::make_index_sequence<members>{});
}

template <typename T>
void dump_value(std::string& out, const T& value, const XDumpOptions& opts, std::size_t depth) {
    using Raw = std::remove_cvref_t<T>;
    if constexpr (is_xview<Raw>::value) {
        dump_value(out, value.root(), opts, depth);
    } else if constexpr (std::is_same_v<Raw, bool>) {
        out.append(value ? "true" : "false");
    } else if constexpr (std::is_same_v<Raw, std::byte>) {
        append_integer(out, std::to_integer<unsigned>(value));
    } else if constexpr (std::is_integral_v<Raw>) {
        append_integer(out, value);
    } else if constexpr (std::is_floating_point_v<Raw>) {
        append_float(out, value);
    } else if constexpr (std::is_enum_v<Raw>) {
        append_integer(out, static_cast<std::underlying_type_t<Raw>>(value));
    } else if constexpr (std::is_same_v<Raw, XString>) {
        append_escaped_string(out, value.view(), opts.max_string_bytes);
    } else if constexpr (is_xvector<Raw>::value || is_xset<Raw>::value) {
        dump_sequence(out, value, value.size(), opts, depth);
    } else if constexpr (is_std_array<Raw>::value) {
        dump_sequence(out, value, value.size(), opts, depth);
    } else if constexpr (std::is_array_v<Raw>) {
        constexpr std::size_t n = std::extent_v<Raw>;
        dump_sequence(out, value, n, opts, depth);
    } else if constexpr (is_xmap<Raw>::value) {
        using K = typename Raw::key_type;
        const std::size_t count = value.size();
        const std::size_t limit = (opts.max_elements != 0 && count > opts.max_elements) ? opts.max_elements : count;
        if constexpr (map_key_is_object_key<K>()) {
            out.push_back('{');
            if (count == 0) {
                out.push_back('}');
                return;
            }
            std::size_t idx = 0;
            for (const auto& entry : value) {
                if (idx >= limit) break;
                if (idx > 0) out.push_back(',');
                append_indent(out, opts, depth + 1);
                append_map_key(out, entry.first, opts);
                out.append(opts.indent ? ": " : ":");
                dump_value(out, entry.second, opts, depth + 1);
                ++idx;
            }
            if (count > limit) {
                if (limit > 0) out.push_back(',');
                append_indent(out, opts, depth + 1);
                out.append(opts.indent ? "\"__truncated__\": " : "\"__truncated__\":");
                append_integer(out, count - limit);
            }
            append_indent(out, opts, depth);
            out.push_back('}');
        } else {
            out.push_back('[');
            if (count == 0) {
                out.push_back(']');
                return;
            }
            std::size_t idx = 0;
            for (const auto& entry : value) {
                if (idx >= limit) break;
                if (idx > 0) out.push_back(',');
                append_indent(out, opts, depth + 1);
                out.push_back('[');
                dump_value(out, entry.first, opts, depth + 2);
                out.append(opts.indent ? ", " : ",");
                dump_value(out, entry.second, opts, depth + 2);
                out.push_back(']');
                ++idx;
            }
            if (count > limit) {
                if (limit > 0) out.push_back(',');
                append_indent(out, opts, depth + 1);
                out.append("\"... (");
                append_integer(out, count - limit);
                out.append(" more)\"");
            }
            append_indent(out, opts, depth);
            out.push_back(']');
        }
    } else if constexpr (std::is_class_v<Raw> && !std::is_union_v<Raw>) {
        out.push_back('{');
        bool first = true;
        dump_record_fields(out, value, opts, depth, first);
        if (!first) append_indent(out, opts, depth);
        out.push_back('}');
    } else {
        static_assert(std::is_same_v<Raw, void>, "xoffset_dump.hpp: unsupported type for JSON dump");
    }
}

} // namespace dump_detail

/// Appends the JSON representation of an XOffset value into `out`.
template <typename T>
inline void dump_json_into(std::string& out, const T& value, XDumpOptions options = {}) {
    dump_detail::dump_value(out, value, options, 0);
}

/// Returns the JSON representation of an XOffset value (works with XView<T> via *view).
template <typename T>
inline std::string to_json(const T& value, XDumpOptions options = {}) {
    std::string out;
    dump_json_into(out, value, options);
    return out;
}

/// Summarizes the 40-byte wire header of any byte span as JSON without touching the payload.
inline std::string dump_wire_header_json(XByteView wire, XDumpOptions options = {}) {
    const std::span<const std::byte> bytes = wire;
    std::string out;
    const char* sep = options.indent ? ": " : ":";
    auto field = [&](std::string_view name, bool first) {
        if (!first) out.push_back(',');
        dump_detail::append_indent(out, options, 1);
        dump_detail::append_escaped_string(out, name, 0);
        out.append(sep);
    };

    out.push_back('{');
    field("wire_bytes", true);
    dump_detail::append_integer(out, bytes.size());

    if (bytes.size() < sizeof(XWireHeader)) {
        field("truncated", false);
        out.append("true");
        dump_detail::append_indent(out, options, 0);
        out.push_back('}');
        return out;
    }

    XWireHeader h{};
    std::memcpy(&h, bytes.data(), sizeof(h));
    const bool magic_ok = h.magic == XWIRE_MAGIC;
    const unsigned major = static_cast<unsigned>((h.magic >> 16) & 0xFFu);
    const unsigned minor = static_cast<unsigned>((h.magic >> 24) & 0xFFu);

    field("valid_magic", false);
    out.append(magic_ok ? "true" : "false");
    field("format_major", false);
    dump_detail::append_integer(out, major);
    field("format_minor", false);
    dump_detail::append_integer(out, minor);
    field("header_bytes", false);
    dump_detail::append_integer(out, h.header_bytes);
    field("schema_hash", false);
    out.push_back('"');
    dump_detail::append_hex_u64(out, h.schema_hash);
    out.push_back('"');
    field("root_type_id", false);
    out.push_back('"');
    dump_detail::append_hex_u64(out, h.root_type_id);
    out.push_back('"');
    field("used_bytes", false);
    dump_detail::append_integer(out, h.used_bytes);
    field("extent_count", false);
    dump_detail::append_integer(out, h.extent_count);
    field("free_dir", false);
    dump_detail::append_integer(out, h.free_dir);
    field("free_bytes", false);
    dump_detail::append_integer(out, h.free_bytes);
    field("free_ratio", false);
    const double ratio = h.used_bytes != 0 ? static_cast<double>(h.free_bytes) / static_cast<double>(h.used_bytes) : 0.0;
    dump_detail::append_float(out, ratio);

    dump_detail::append_indent(out, options, 0);
    out.push_back('}');
    return out;
}

} // namespace XOffsetDatastructure

#endif // X_OFFSET_DUMP_HPP
