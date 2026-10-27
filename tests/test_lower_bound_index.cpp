// P1 regression: scalar keys take the branch-free lower_bound_index. Every
// index must equal std::lower_bound on the same sorted data (duplicates,
// empty and one-element ranges, keys below/above/between), and XMap/XSet with
// scalar keys must match std::map/std::set after random insert/find/erase.
#include "xoffsetdatastructure.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace xo = XOffsetDatastructure;

namespace lbi {
enum class Kind : std::uint8_t { a = 1, b = 3, c = 7, d = 200 };
struct Root {
    xo::XMap<std::int64_t, std::int64_t> wide;
    xo::XMap<std::int32_t, std::int32_t> narrow;
    xo::XMap<std::uint8_t, std::int32_t> tiny;
    xo::XSet<std::uint64_t> ids;
    xo::XSet<std::int16_t> small;
    xo::XMap<Kind, std::int32_t> kinds;
    xo::XSet<double> reals;
    xo::XMap<xo::XString, std::int32_t> named;  // non-scalar key keeps the old loop
};
} // namespace lbi
XOFFSET_RECORD_DERIVED(lbi::Root, (), wide, narrow, tiny, ids, small, kinds, reals, named)

XOFFSET_REGISTER_SCHEMA_NAME(lbi::Root, "test.lower_bound_index.root")

namespace {
[[noreturn]] void fail(const std::string& message) {
    std::cerr << "test_lower_bound_index FAILED: " << message << "\n";
    std::exit(1);
}

struct Rng {
    std::uint64_t s;
    std::uint64_t next() {
        s = s * 6364136223846793005ull + 1442695040888963407ull;
        return s >> 17;
    }
    std::uint32_t below(std::uint32_t n) { return n ? std::uint32_t(next() % n) : 0; }
};

template <class T> std::uint32_t reference(const std::vector<T>& v, const T& key) {
    return std::uint32_t(std::lower_bound(v.begin(), v.end(), key) - v.begin());
}

template <class T> std::uint32_t tested(const std::vector<T>& v, const T& key) {
    return xo::fixed_detail::lower_bound_index(v.data(), std::uint32_t(v.size()), key,
                                               [](const T& x) -> const T& { return x; });
}

// Every size 0..70 (all halving shapes), dense duplicates, every probe key
// from below the minimum to above the maximum.
template <class T> void exhaustive_sizes(Rng& rng, const char* name) {
    for (std::uint32_t n = 0; n <= 70; ++n) {
        for (int round = 0; round < 8; ++round) {
            std::vector<T> v(n);
            const std::uint32_t spread = round % 2 ? 4 : 1000;  // many duplicates on odd rounds
            for (auto& x : v) x = T(rng.below(spread));
            std::sort(v.begin(), v.end());
            const std::uint32_t top = spread + 2;
            for (std::uint32_t k = 0; k <= top; ++k) {
                const T key = T(k) - T(1);
                if (tested(v, key) != reference(v, key))
                    fail(std::string(name) + ": size " + std::to_string(n) + " key " + std::to_string(k));
            }
            for (const auto& key : v)
                if (tested(v, key) != reference(v, key)) fail(std::string(name) + ": member key");
        }
    }
}

void raw_index() {
    Rng rng{20261022};
    exhaustive_sizes<std::int64_t>(rng, "int64");
    exhaustive_sizes<std::int32_t>(rng, "int32");
    exhaustive_sizes<std::uint32_t>(rng, "uint32");
    exhaustive_sizes<std::uint8_t>(rng, "uint8");
    exhaustive_sizes<double>(rng, "double");
    // Extreme values of a signed key.
    std::vector<std::int64_t> v{INT64_MIN, -1, 0, 1, INT64_MAX};
    for (std::int64_t key : {INT64_MIN, std::int64_t(-2), std::int64_t(0), std::int64_t(2), INT64_MAX})
        if (tested(v, key) != reference(v, key)) fail("int64 extremes");
    // Mixed scalar probe type (int probe into int64 data) compares exactly as before.
    std::vector<std::int64_t> w{-5, -1, 3, 9, 9, 40};
    for (int key = -7; key < 45; ++key)
        if (xo::fixed_detail::lower_bound_index(w.data(), std::uint32_t(w.size()), key,
                [](const std::int64_t& x) -> const std::int64_t& { return x; }) !=
            std::uint32_t(std::lower_bound(w.begin(), w.end(), std::int64_t(key)) - w.begin()))
            fail("mixed int/int64 probe");
}

template <class X, class M> void same_map(const X& x, const M& m, const char* name) {
    if (x.size() != m.size()) fail(std::string(name) + ": size");
    auto it = m.begin();
    for (const auto& e : x) {
        if (!(e.first == it->first) || e.second != it->second) fail(std::string(name) + ": entry");
        ++it;
    }
}

template <class X, class S> void same_set(const X& x, const S& s, const char* name) {
    if (x.size() != s.size()) fail(std::string(name) + ": size");
    auto it = s.begin();
    for (const auto& e : x) {
        if (!(e == *it)) fail(std::string(name) + ": member");
        ++it;
    }
}

auto policy() {
    return xo::XBuffer::maximum_capacity(std::size_t(16) << 20).with_growth(xo::XGrowPolicy::Automatic);
}

void containers() {
    Rng rng{7};
    xo::XBuffer buf = xo::XBuffer::create<lbi::Root>(16 * 1024, policy());
    std::map<std::int64_t, std::int64_t> wide;
    std::map<std::int32_t, std::int32_t> narrow;
    std::map<std::uint8_t, std::int32_t> tiny;
    std::set<std::uint64_t> ids;
    std::set<std::int16_t> small;
    std::map<lbi::Kind, std::int32_t> kinds;
    std::set<double> reals;
    std::map<std::string, std::int32_t> named;
    constexpr lbi::Kind all_kinds[] = {lbi::Kind::a, lbi::Kind::b, lbi::Kind::c, lbi::Kind::d};

    for (int op = 0; op < 20000; ++op) {
        auto& r = buf.root<lbi::Root>();
        const auto pick = rng.below(3);  // 0 insert, 1 erase, 2 find
        const std::int64_t k64 = std::int64_t(rng.below(400)) - 200;
        const std::int32_t k32 = std::int32_t(rng.below(150));
        const std::uint8_t k8 = std::uint8_t(rng.below(256));
        const std::uint64_t ku = rng.next() % 300 * 0x100000001ull;
        const std::int16_t k16 = std::int16_t(std::int32_t(rng.below(80)) - 40);
        const lbi::Kind kk = all_kinds[rng.below(4)];
        const double kd = double(std::int32_t(rng.below(90)) - 45) / 4.0;
        const std::string ks = "k" + std::to_string(rng.below(60));
        const std::int32_t value = std::int32_t(rng.below(1000));
        if (pick == 0) {
            r.wide[k64] = value; wide[k64] = value;
            r.narrow.insert_or_assign(k32, value); narrow.insert_or_assign(k32, value);
            r.tiny.try_emplace(k8, value); tiny.try_emplace(k8, value);
            r.ids.insert(ku); ids.insert(ku);
            r.small.insert(k16); small.insert(k16);
            r.kinds[kk] = value; kinds[kk] = value;
            r.reals.insert(kd); reals.insert(kd);
            r.named[std::string_view(ks)] = value; named[ks] = value;
        } else if (pick == 1) {
            if (r.wide.erase(k64) != wide.erase(k64)) fail("wide erase");
            if (r.narrow.erase(k32) != narrow.erase(k32)) fail("narrow erase");
            if (r.tiny.erase(k8) != tiny.erase(k8)) fail("tiny erase");
            if (r.ids.erase(ku) != ids.erase(ku)) fail("ids erase");
            if (r.small.erase(k16) != small.erase(k16)) fail("small erase");
            if (r.kinds.erase(kk) != kinds.erase(kk)) fail("kinds erase");
            if (r.reals.erase(kd) != reals.erase(kd)) fail("reals erase");
            if (r.named.erase(std::string_view(ks)) != named.erase(ks)) fail("named erase");
        } else {
            if (r.wide.contains(k64) != wide.contains(k64)) fail("wide find");
            if (r.narrow.contains(k32) != narrow.contains(k32)) fail("narrow find");
            if (r.tiny.contains(k8) != tiny.contains(k8)) fail("tiny find");
            if (r.ids.contains(ku) != ids.contains(ku)) fail("ids find");
            if (r.small.contains(k16) != small.contains(k16)) fail("small find");
            if (r.kinds.contains(kk) != kinds.contains(kk)) fail("kinds find");
            if (r.reals.contains(kd) != reals.contains(kd)) fail("reals find");
            if (r.named.contains(std::string_view(ks)) != named.contains(ks)) fail("named find");
            auto it = r.wide.find(k64);
            if (it != r.wide.end() && it->second != wide.at(k64)) fail("wide value");
        }
        if (op % 1999 == 0) {
            std::string wire = buf.save_verified<lbi::Root>();
            buf = xo::XBuffer::load_verified<lbi::Root>(wire, policy());
        }
    }
    const auto& r = buf.root<lbi::Root>();
    same_map(r.wide, wide, "wide");
    same_map(r.narrow, narrow, "narrow");
    same_map(r.tiny, tiny, "tiny");
    same_set(r.ids, ids, "ids");
    same_set(r.small, small, "small");
    same_map(r.kinds, kinds, "kinds");
    same_set(r.reals, reals, "reals");
    if (r.named.size() != named.size()) fail("named size");
    for (const auto& [k, v] : named) {
        auto it = r.named.find(std::string_view(k));
        if (it == r.named.end() || it->second != v) fail("named entry");
    }
}
} // namespace

int main() {
    raw_index();
    containers();
    std::cout << "test_lower_bound_index passed\n";
    return 0;
}
