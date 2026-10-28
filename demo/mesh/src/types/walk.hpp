// Device mesh demo: the canonical field walk (content digest) and the
// field-by-field comparison of an object with its reference model.
// OWNER: types thread.
//
// Both walk the field lists of model.hpp, so they see values, never the
// arena: two arenas with the same content (save_verified and save_compact
// of one state, or the object and its model) digest the same.
#pragma once
#include <cinttypes>
#include <cstdio>
#include <cstring>
#include <string>
#include <string_view>
#include <type_traits>
#include "model.hpp"
#include "xmesh/protocol.hpp"

namespace xmesh::walk {

template <class T> struct is_str : std::false_type {};
template <> struct is_str<XString> : std::true_type {};
template <> struct is_str<std::string> : std::true_type {};

template <class T> struct is_vec : std::false_type {};
template <class T> struct is_vec<XVector<T>> : std::true_type {};
template <class T, class A> struct is_vec<std::vector<T, A>> : std::true_type {};

template <class T> struct is_set : std::false_type {};
template <class T> struct is_set<XSet<T>> : std::true_type {};
template <class T, class C, class A> struct is_set<std::set<T, C, A>> : std::true_type {};

template <class T> struct is_map : std::false_type {};
template <class K, class V> struct is_map<XMap<K, V>> : std::true_type {};
template <class K, class V, class C, class A> struct is_map<std::map<K, V, C, A>> : std::true_type {};

template <class T> struct is_array : std::false_type {};
template <class T, std::size_t N> struct is_array<std::array<T, N>> : std::true_type {};

inline std::string_view view(const XString& s) noexcept { return s.view(); }
inline std::string_view view(const std::string& s) noexcept { return s; }

// ---- content digest ------------------------------------------------------------

// FNV-1a 64 over: scalars as their little-endian bytes (bool one byte,
// float and double their bits), strings as u32 length + bytes, vectors and
// sets as u32 count + elements, maps as u32 count + (key, value) pairs,
// arrays and records element by element / field by field.
struct Digest {
    std::uint64_t h = protocol::kFnvOffset;

    void bytes(const void* p, std::size_t n) noexcept { h = protocol::fnv1a64(p, n, h); }
    void u32(std::uint32_t v) noexcept { bytes(&v, sizeof v); }

    template <class T>
    void put(const T& v) {
        if constexpr (std::is_same_v<T, bool>) {
            const std::uint8_t b = v ? 1 : 0;
            bytes(&b, 1);
        } else if constexpr (std::is_arithmetic_v<T> || std::is_enum_v<T>) {
            bytes(&v, sizeof v);
        } else if constexpr (is_str<T>::value) {
            const std::string_view s = view(v);
            u32(std::uint32_t(s.size()));
            bytes(s.data(), s.size());
        } else if constexpr (is_array<T>::value) {
            for (const auto& e : v) put(e);
        } else if constexpr (is_vec<T>::value || is_set<T>::value) {
            u32(std::uint32_t(v.size()));
            for (const auto& e : v) put(e);
        } else if constexpr (is_map<T>::value) {
            u32(std::uint32_t(v.size()));
            for (const auto& kv : v) {
                put(kv.first);
                put(kv.second);
            }
        } else {
            fields(v, *this);
        }
    }
    template <class T>
    void operator()(const char*, const T& v) { put(v); }
};

template <class Root>
std::uint64_t digest_of(std::uint16_t type, const Root& root) {
    Digest d;
    d.bytes(&type, sizeof type);
    d.put(root);
    return d.h;
}

// ---- comparison -----------------------------------------------------------------

inline std::string show(std::string_view s) {
    std::string out = "\"";
    const std::size_t n = s.size() < 40 ? s.size() : 40;
    for (std::size_t i = 0; i < n; ++i) {
        const unsigned char c = static_cast<unsigned char>(s[i]);
        if (c >= 0x20 && c < 0x7f && c != '"' && c != '\\') {
            out += char(c);
        } else {
            char hex[8];
            std::snprintf(hex, sizeof hex, "\\x%02x", c);
            out += hex;
        }
    }
    out += s.size() > n ? "\"... (" + std::to_string(s.size()) + " bytes)" : "\"";
    return out;
}

template <class T>
std::string show_scalar(const T& v) {
    char buf[64];
    if constexpr (std::is_same_v<T, bool>) {
        return v ? "true" : "false";
    } else if constexpr (std::is_enum_v<T>) {
        return show_scalar(static_cast<std::underlying_type_t<T>>(v));
    } else if constexpr (std::is_same_v<T, float>) {
        std::uint32_t bits;
        std::memcpy(&bits, &v, sizeof bits);
        std::snprintf(buf, sizeof buf, "%.9g (0x%08" PRIx32 ")", double(v), bits);
    } else if constexpr (std::is_same_v<T, double>) {
        std::uint64_t bits;
        std::memcpy(&bits, &v, sizeof bits);
        std::snprintf(buf, sizeof buf, "%.17g (0x%016" PRIx64 ")", v, bits);
    } else if constexpr (std::is_signed_v<T>) {
        std::snprintf(buf, sizeof buf, "%" PRId64, std::int64_t(v));
    } else {
        std::snprintf(buf, sizeof buf, "%" PRIu64, std::uint64_t(v));
    }
    return buf;
}

template <class T>
std::string show_key(const T& k) {
    if constexpr (is_str<T>::value) return show(view(k));
    else return show_scalar(k);
}

// Compares an object (A, the XOffset side) with its model (B); the first
// difference ends the walk, as "Order.lines[2].qty: 3 != 4".
struct Compare {
    std::string error;
    std::string path;

    void fail(const std::string& what) {
        if (error.empty()) error = path + ": " + what;
    }

    template <class A, class B>
    void at(const std::string& sub, const A& a, const B& b) {
        if (!error.empty()) return;
        const std::size_t keep = path.size();
        path += sub;
        same(a, b);
        path.resize(keep);
    }

    template <class A, class B>
    void same(const A& a, const B& b) {
        if constexpr (std::is_same_v<A, bool> || std::is_integral_v<A> || std::is_enum_v<A>) {
            if (a != b) fail(show_scalar(a) + " != " + show_scalar(b));
        } else if constexpr (std::is_floating_point_v<A>) {
            // Bit equality: the mutations assign exact values.
            if (std::memcmp(&a, &b, sizeof a) != 0) fail(show_scalar(a) + " != " + show_scalar(b));
        } else if constexpr (is_str<A>::value) {
            if (view(a) != view(b)) fail(show(view(a)) + " != " + show(view(b)));
        } else if constexpr (is_array<A>::value) {
            for (std::size_t i = 0; i < a.size() && error.empty(); ++i) at("[" + std::to_string(i) + "]", a[i], b[i]);
        } else if constexpr (is_vec<A>::value) {
            if (a.size() != b.size()) return fail("size " + std::to_string(a.size()) + " != " + std::to_string(b.size()));
            for (std::size_t i = 0; i < a.size() && error.empty(); ++i) at("[" + std::to_string(i) + "]", a[i], b[i]);
        } else if constexpr (is_set<A>::value) {
            if (a.size() != b.size()) return fail("size " + std::to_string(a.size()) + " != " + std::to_string(b.size()));
            auto ib = b.begin();
            std::size_t i = 0;
            for (auto ia = a.begin(); ia != a.end() && error.empty(); ++ia, ++ib, ++i)
                at("{" + std::to_string(i) + "}", *ia, *ib);
        } else if constexpr (is_map<A>::value) {
            if (a.size() != b.size()) return fail("size " + std::to_string(a.size()) + " != " + std::to_string(b.size()));
            auto ib = b.begin();
            for (auto ia = a.begin(); ia != a.end() && error.empty(); ++ia, ++ib) {
                at("{key " + show_key(ib->first) + "}", ia->first, ib->first);
                at("[" + show_key(ib->first) + "]", ia->second, ib->second);
            }
        } else {
            fields2(a, b, *this);
        }
    }

    template <class A, class B>
    void operator()(const char* name, const A& a, const B& b) {
        at(std::string(".") + name, a, b);
    }
};

template <class A, class B>
std::string compare(const char* root_name, const A& a, const B& b) {
    Compare c;
    c.path = root_name;
    c.same(a, b);
    return c.error;
}

} // namespace xmesh::walk
