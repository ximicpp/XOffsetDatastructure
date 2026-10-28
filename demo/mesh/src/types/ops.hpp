// Device mesh demo: the deterministic random stream, generated text, and
// container operations with one spelling for both models (XOffset and
// std::). OWNER: types thread.
//
// Determinism: every draw is its own statement (the order in which a
// compiler evaluates function arguments is unspecified), only integer
// arithmetic, and floats come from integers by an exact conversion.
#pragma once
#include <cstdint>
#include <cstdio>
#include <iterator>
#include <map>
#include <set>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>
#include "model.hpp"
#include "xmesh/protocol.hpp"

namespace xmesh::ops {

// splitmix64.
struct Rng {
    std::uint64_t s = 0;
    std::uint64_t next() noexcept {
        std::uint64_t z = (s += 0x9e3779b97f4a7c15ull);
        z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ull;
        z = (z ^ (z >> 27)) * 0x94d049bb133111ebull;
        return z ^ (z >> 31);
    }
    // 0 .. n-1 (0 when n == 0).
    std::uint32_t below(std::uint32_t n) noexcept { return n == 0 ? 0 : std::uint32_t(next() % n); }
    // lo .. hi inclusive.
    std::int64_t range(std::int64_t lo, std::int64_t hi) noexcept {
        return lo + std::int64_t(next() % std::uint64_t(hi - lo + 1));
    }
    bool chance(std::uint32_t num, std::uint32_t den) noexcept { return below(den) < num; }
};

// The stream of one purpose (make or a mutation) of one job.
inline Rng stream(std::uint64_t purpose, std::uint16_t type, std::uint64_t seed, std::uint32_t hop,
                  std::string_view device) noexcept {
    std::uint64_t h = protocol::fnv1a64(&purpose, sizeof purpose);
    h = protocol::fnv1a64(&type, sizeof type, h);
    h = protocol::fnv1a64(&seed, sizeof seed, h);
    h = protocol::fnv1a64(&hop, sizeof hop, h);
    h = protocol::fnv1a64(device.data(), device.size(), h);
    return Rng{h};
}

// Words of generated text, a few of them not ASCII (bytes are bytes on
// every device; only the lengths and the order of keys matter).
inline constexpr std::string_view kWords[] = {
    "the", "data", "moves", "between", "devices", "without", "any", "parsing", "step", "and", "each",
    "receiver", "checks", "changes", "sends", "it", "on", "offset", "arena", "wire", "byte", "copy",
    "server", "client", "phone", "window", "linux", "tablet", "note", "draft", "review", "update",
    "数据", "设备", "互传", "café", "naïve", "Grüße", "один", "δ", "🙂", "order", "sensor", "scene", "chat",
};
inline constexpr std::uint32_t kWordCount = sizeof kWords / sizeof kWords[0];

// Text of about `min_len`..`max_len` bytes (whole words; may run a word past max).
inline std::string text(Rng& r, std::uint32_t min_len, std::uint32_t max_len) {
    const std::uint32_t target = std::uint32_t(r.range(min_len, max_len));
    std::string out;
    while (out.size() < target) {
        if (!out.empty()) out += ' ';
        out += kWords[r.below(kWordCount)];
    }
    return out;
}

inline std::string hex(std::uint64_t v, int digits) {
    char buf[24];
    std::snprintf(buf, sizeof buf, "%0*llx", digits, static_cast<unsigned long long>(v));
    return buf;
}
inline std::string dec(std::uint64_t v, int digits) {
    char buf[24];
    std::snprintf(buf, sizeof buf, "%0*llu", digits, static_cast<unsigned long long>(v));
    return buf;
}

// ---- strings --------------------------------------------------------------------
inline void set(XString& s, std::string_view v) { s = v; }
inline void set(std::string& s, std::string_view v) { s.assign(v.data(), v.size()); }
inline std::string_view view(const XString& s) noexcept { return s.view(); }
inline std::string_view view(const std::string& s) noexcept { return s; }

// ---- vectors --------------------------------------------------------------------
template <class V>
auto& push(V& v) { return v.emplace_back(); }
template <class V>
void erase_at(V& v, std::size_t i) { v.erase(v.begin() + std::ptrdiff_t(i)); }
// Scalars and records without containers only.
template <class V, class T>
void insert_at(V& v, std::size_t i, const T& x) { v.insert(v.begin() + std::ptrdiff_t(i), x); }
template <class T, class P>
void erase_if(XVector<T>& v, P pred) { (void)v.stable_erase_if(pred); }
template <class T, class P>
void erase_if(std::vector<T>& v, P pred) { (void)std::erase_if(v, pred); }
template <class V>
void set_bytes(V& v, const std::vector<std::uint8_t>& bytes) { v.assign(bytes.begin(), bytes.end()); }

// ---- maps -----------------------------------------------------------------------
template <class V>
V& get(XMap<XString, V>& m, std::string_view k) { return m.try_emplace(k).first->second; }
template <class V>
V& get(std::map<std::string, V, std::less<>>& m, std::string_view k) { return m.try_emplace(std::string(k)).first->second; }
template <class V>
V& get(XMap<std::uint64_t, V>& m, std::uint64_t k) { return m.try_emplace(k).first->second; }
template <class V>
V& get(std::map<std::uint64_t, V>& m, std::uint64_t k) { return m.try_emplace(k).first->second; }

template <class V>
void erase_key(XMap<XString, V>& m, std::string_view k) { (void)m.erase(k); }
template <class V>
void erase_key(std::map<std::string, V, std::less<>>& m, std::string_view k) { (void)m.erase(std::string(k)); }
template <class V>
void erase_key(XMap<std::uint64_t, V>& m, std::uint64_t k) { (void)m.erase(k); }
template <class V>
void erase_key(std::map<std::uint64_t, V>& m, std::uint64_t k) { (void)m.erase(k); }

// ---- sets -----------------------------------------------------------------------
inline void add(XSet<XString>& s, std::string_view v) { (void)s.insert(v); }
inline void add(model::Set<std::string>& s, std::string_view v) { (void)s.emplace(v); }
template <class T>
void add(XSet<T>& s, T v) { (void)s.insert(v); }
template <class T>
void add(model::Set<T>& s, T v) { (void)s.insert(v); }

inline void remove(XSet<XString>& s, std::string_view v) { (void)s.erase(v); }
inline void remove(model::Set<std::string>& s, std::string_view v) { (void)s.erase(std::string(v)); }
template <class T>
void remove(XSet<T>& s, T v) { (void)s.erase(v); }
template <class T>
void remove(model::Set<T>& s, T v) { (void)s.erase(v); }

// ---- ordered containers by position -------------------------------------------------
template <class C>
auto nth(C& c, std::size_t i) {
    auto it = c.begin();
    std::advance(it, std::ptrdiff_t(i));
    return it;
}
// A key by value (strings as std::string, so the container may change).
inline std::string key_of(const XString& s) { return std::string(s.view()); }
inline std::string key_of(const std::string& s) { return s; }
template <class K>
    requires std::is_arithmetic_v<K> || std::is_enum_v<K>
K key_of(K k) { return k; }

} // namespace xmesh::ops
