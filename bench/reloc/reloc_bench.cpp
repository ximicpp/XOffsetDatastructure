// Relocation tax (PERF_GOAL G9): single container operations on XOffset
// containers against the same operations on std containers, in one process,
// with no transport at all. Ratio = XOffset / std, per round.
//
// Origin: experiments/mutable_relocatable/reloc_tax.cpp (X1 micro part) on
// claude/mutable-relocatable-positioning, rewritten for G9:
//   - every timed region is long enough for a 24 MHz timer (Apple M-series);
//   - keyed cases draw new keys every round ("fresh"); the X1 behaviour, the
//     same keys and order every round, is kept as "reused" and reported beside
//     it (a reused order lets the branch predictor learn std::flat_map lookups);
//   - elements with container heads run twice, declared ordinary records
//     (".ord", what a user following the guide writes) and undeclared (".plain");
//   - std::flat_map / std::flat_set are the primary baselines for XMap / XSet,
//     std::map / std::set are reported beside them;
//   - nested object build and reset<T>() are cases of their own.
// Within a round the sides run back to back in rotating order, each after its
// own untimed setup. bench/reloc/tools/analyze.py turns the JSONL into the
// geometric mean and the Bonferroni one-sided 95% upper bound per comparison.
//
// RELOC_IR_COUNT=1 builds the instruction-count variant (see README.md).

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>
#if __has_include(<flat_map>)
#include <flat_map>
#endif
#if __has_include(<flat_set>)
#include <flat_set>
#endif
#if RELOC_IR_COUNT
#include <valgrind/callgrind.h>
#endif

#include "xoffsetdatastructure.hpp"

// ---- record types ----------------------------------------------------------

namespace rb {
struct alignas(8) Skill { int32_t id{}; int32_t level{}; };
struct alignas(8) Buff  { int32_t id{}; int32_t stacks{}; int64_t expire_ms{}; };
struct alignas(8) Troop { int32_t type{}; int32_t count{}; };

namespace sm {  // std model
struct Hero {
    int32_t id{}; int32_t level{}; int32_t star{};
    std::string name;
    std::vector<Skill> skills;
    std::vector<Buff> buffs;
};
struct Army {
    int64_t id{};
    std::string name;
    std::vector<Hero> heroes;
    std::vector<Troop> troops;
};
} // namespace sm

namespace xm {  // XOffset model; HeroOrd/HeroPlain and ArmyOrd/ArmyPlain differ only in the declaration below
using namespace XOffsetDatastructure;
struct alignas(8) HeroOrd {
    int32_t id{}; int32_t level{}; int32_t star{};
    XString name;
    XVector<Skill> skills;
    XVector<Buff> buffs;
};
struct alignas(8) HeroPlain {
    int32_t id{}; int32_t level{}; int32_t star{};
    XString name;
    XVector<Skill> skills;
    XVector<Buff> buffs;
};
struct alignas(8) ArmyOrd   { int64_t id{}; XString name; XVector<HeroOrd> heroes;   XVector<Troop> troops; };
struct alignas(8) ArmyPlain { int64_t id{}; XString name; XVector<HeroPlain> heroes; XVector<Troop> troops; };
struct alignas(8) HeroVecOrd   { XVector<HeroOrd> v;   HeroOrd proto; };
struct alignas(8) HeroVecPlain { XVector<HeroPlain> v; HeroPlain proto; };
struct alignas(8) IntVec { XVector<int32_t> v; };
struct alignas(8) StrVec { XVector<XString> s; };
struct alignas(8) IntMap { XMap<int32_t, int32_t> m; };
struct alignas(8) IntSet { XSet<int32_t> s; };
} // namespace xm
} // namespace rb

// Only the ".ord" records are declared ordinary (all their members are
// ordinary: no bases, bit-fields, unions or [[no_unique_address]]).
#ifdef XOFFSET_ORDINARY_RECORDS
XOFFSET_ORDINARY_RECORDS(rb::xm::HeroOrd, rb::xm::ArmyOrd)
#else
// Libraries without the macro: its two specializations, written out.
#define RELOC_ORDINARY_RECORD(T)                                                       \
    template<> struct XOffsetDatastructure::wire_record_layout<T> {                    \
        static constexpr bool ordinary_members = true;                                 \
    };                                                                                 \
    template<> struct XOffsetDatastructure::StableEraseOrdinaryMembers<T> : std::true_type {};
RELOC_ORDINARY_RECORD(rb::xm::HeroOrd)
RELOC_ORDINARY_RECORD(rb::xm::ArmyOrd)
#undef RELOC_ORDINARY_RECORD
#endif
static_assert(XOffsetDatastructure::StableEraseOrdinaryMembers<rb::xm::HeroOrd>::value &&
              XOffsetDatastructure::wire_record_layout<rb::xm::HeroOrd>::ordinary_members);
static_assert(!XOffsetDatastructure::StableEraseOrdinaryMembers<rb::xm::HeroPlain>::value);

namespace rb {
using namespace XOffsetDatastructure;
using Clock = std::chrono::steady_clock;

template <class T> inline void keep(T const& v) { asm volatile("" : : "r,m"(v) : "memory"); }
inline void clobber() { asm volatile("" : : : "memory"); }

struct Rng {
    uint64_t s;
    explicit Rng(uint64_t seed) : s(seed) {}
    uint64_t next() {
        uint64_t z = (s += 0x9e3779b97f4a7c15ull);
        z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ull;
        z = (z ^ (z >> 27)) * 0x94d049bb133111ebull;
        return z ^ (z >> 31);
    }
    uint32_t below(uint32_t n) { return uint32_t(next() % n); }
};
inline uint64_t mix(uint64_t h, uint64_t v) { return Rng(h ^ (v * 0x9e3779b97f4a7c15ull)).next(); }
inline uint64_t hash_name(std::string_view s) {
    uint64_t h = 1469598103934665603ull;
    for (char c : s) h = (h ^ uint8_t(c)) * 1099511628211ull;
    return h;
}

constexpr uint64_t kSeed = 20261022;

// XOffset buffers: Automatic growth, VM storage, as in the SLG runner; the
// limit is small so that many buffers fit valgrind's address space.
XBuffer::MaximumCapacity cap(std::size_t bytes = std::size_t(16) << 20) {
    return XBuffer::maximum_capacity(bytes).with_growth(XGrowPolicy::Automatic);
}

// ---- instruction counts (CI performance gate) ------------------------------
#if RELOC_IR_COUNT
// Under valgrind --tool=callgrind --collect-atstart=no, callgrind collects only
// inside the XOffset side's timed region of the cases named by RELOC_IR_KEYS
// (comma-separated; an entry matches a case exactly, or as a prefix when it
// ends with '*'), and not in rounds below RELOC_IR_SKIP_ROUNDS (default 0).
// Warm-up rounds are never collected.
inline int& ir_round() { static int r = -1; return r; }
inline bool ir_selected(const char* key) {
    static const int skip = [] { const char* e = std::getenv("RELOC_IR_SKIP_ROUNDS"); return e ? std::atoi(e) : 0; }();
    if (ir_round() < skip) return false;
    static const std::string keys = [] { const char* e = std::getenv("RELOC_IR_KEYS"); return std::string(e ? e : ""); }();
    const std::string_view k(key);
    std::size_t pos = 0;
    while (pos <= keys.size()) {
        std::size_t end = keys.find(',', pos);
        if (end == std::string::npos) end = keys.size();
        const std::string_view item = std::string_view(keys).substr(pos, end - pos);
        if (!item.empty() && item.back() == '*') {
            if (k.starts_with(item.substr(0, item.size() - 1))) return true;
        } else if (!item.empty() && item == k) {
            return true;
        }
        pos = end + 1;
    }
    return false;
}
#endif

// ir_key is the case name on the XOffset side and nullptr on a baseline.
template <class F> double timed(const char* ir_key, F&& f) {
#if RELOC_IR_COUNT
    const bool ir_on = ir_key && ir_selected(ir_key);
    if (ir_on) CALLGRIND_TOGGLE_COLLECT;
#else
    (void)ir_key;
#endif
    clobber();
    const auto t0 = Clock::now();
    f();
    clobber();
    const auto t1 = Clock::now();
#if RELOC_IR_COUNT
    if (ir_on) CALLGRIND_TOGGLE_COLLECT;
#endif
    return double(std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count());
}

// ---- case runner -------------------------------------------------------------

// One side of a case: untimed setup for the round, one timed region, then a
// digest of the result (untimed). All sides of a case must agree on the digest.
struct Side {
    const char* name;   // "xoffset", "std::vector", "std::flat_map", ...
    std::function<double(int round, uint64_t& digest)> rep;
};

struct Options {
    int rounds = 201;
    int warmup = 3;
    std::vector<std::string> filter;  // --cases; empty = all
    std::FILE* out = nullptr;         // JSONL
};
Options g_opt;
int g_fail = 0;
int g_cases = 0;

// --cases entries match a case exactly, or as a prefix when they end with '*'.
bool selected(const std::string& name) {
    if (g_opt.filter.empty()) return true;
    for (const auto& f : g_opt.filter)
        if (f.ends_with('*') ? name.starts_with(std::string_view(f).substr(0, f.size() - 1)) : name == f) return true;
    return false;
}

double median(std::vector<double> v) {
    std::sort(v.begin(), v.end());
    return v[v.size() / 2];
}

// sides[0] is XOffset. primary names the baseline G9 judges (nullptr: the case
// is reported only). prepare(round) runs once per round before the sides.
void run_case(const std::string& name, double ops, const char* primary, std::vector<Side> sides,
              const std::function<void(int)>& prepare = {}) {
    if (!selected(name)) return;
    ++g_cases;
    const std::size_t n = sides.size();
    std::vector<std::vector<double>> ns(n);
    auto one_round = [&](int round, bool record) {
        if (prepare) prepare(round);
        std::vector<double> t(n);
        std::vector<uint64_t> dig(n, 0);
        for (std::size_t j = 0; j < n; ++j) {
            const std::size_t s = (j + std::size_t(round < 0 ? -round : round)) % n;  // rotate the order
            t[s] = sides[s].rep(round, dig[s]);
        }
        for (std::size_t s = 1; s < n; ++s)
            if (dig[s] != dig[0]) {
                std::printf("CHECK FAILED: %s round %d: %s digest %016llx, xoffset %016llx\n", name.c_str(), round,
                            sides[s].name, (unsigned long long)dig[s], (unsigned long long)dig[0]);
                ++g_fail;
            }
        if (!record) return;
        for (std::size_t s = 0; s < n; ++s) ns[s].push_back(t[s]);
        if (g_opt.out) {
            std::fprintf(g_opt.out, "{\"case\":\"%s\",\"round\":%d,\"ops\":%g,\"primary\":%s%s%s,\"ns\":{",
                         name.c_str(), round, ops, primary ? "\"" : "", primary ? primary : "null", primary ? "\"" : "");
            for (std::size_t s = 0; s < n; ++s)
                std::fprintf(g_opt.out, "%s\"%s\":%.0f", s ? "," : "", sides[s].name, t[s]);
            std::fprintf(g_opt.out, "}}\n");
        }
    };
#if RELOC_IR_COUNT
    ir_round() = -1;
#endif
    for (int w = 1; w <= g_opt.warmup; ++w) one_round(-w, false);
    for (int r = 0; r < g_opt.rounds; ++r) {
#if RELOC_IR_COUNT
        ir_round() = r;
#endif
        one_round(r, true);
    }
#if RELOC_IR_COUNT
    ir_round() = -1;
#endif
    // Console summary: ns/op medians and the median of the per-round ratios.
    std::printf("%-30s %-10s %10.2f", name.c_str(), primary ? "G9" : "report", median(ns[0]) / ops);
    for (std::size_t s = 1; s < n; ++s) {
        std::vector<double> ratio;
        for (std::size_t i = 0; i < ns[0].size(); ++i) ratio.push_back(ns[0][i] / ns[s][i]);
        std::printf("   %s %.2f ratio %.3f", sides[s].name, median(ns[s]) / ops, median(ratio));
    }
    std::printf("\n");
    std::fflush(stdout);
}

void skipped(const std::string& name, const char* why) {
    if (!selected(name)) return;
    std::printf("%-30s skipped: %s\n", name.c_str(), why);
    if (g_opt.out) std::fprintf(g_opt.out, "{\"case\":\"%s\",\"skipped\":\"%s\"}\n", name.c_str(), why);
}

// ---- shared fill code (runs unchanged on the std and the XOffset model) ------

constexpr std::string_view kNames[] = {"Ayla", "Brennos", "Caius Varro", "Dagny", "Eirik the Tall", "Fenna",
                                       "Gaius Marius Major", "Hild", "Ingrid Stormborn", "Jorun"};
constexpr std::string_view kArmyNames[] = {"Northern Vanguard", "Iron Wall", "Reserve", "Crimson Lances"};

template <class H> void fill_hero(H& h, int k) {
    h.id = k; h.level = 1 + k % 60; h.star = 1 + k % 6;
    h.name = kNames[std::size_t(k) % std::size(kNames)];
    for (int i = 0; i < 4; ++i) h.skills.push_back(Skill{100 + (k + i) % 37, 1 + i});
    for (int i = 0; i < 2; ++i) h.buffs.push_back(Buff{(k * 7 + i) % 50, 1 + i, int64_t(k) * 1000 + i});
}
template <class A> void fill_army(A& a, int k) {
    a.id = 1000 + k;
    a.name = kArmyNames[std::size_t(k) % std::size(kArmyNames)];
    for (int i = 0; i < 8; ++i) fill_hero(a.heroes.emplace_back(), k * 8 + i);
    for (int i = 0; i < 6; ++i) a.troops.push_back(Troop{i, 100 + (k + i) % 900});
}
template <class H> uint64_t dig_hero(uint64_t g, const H& h) {
    g = mix(g, uint64_t(h.id)); g = mix(g, uint64_t(h.level)); g = mix(g, uint64_t(h.star));
    g = mix(g, hash_name(std::string_view(h.name)));
    for (const auto& s : h.skills) g = mix(g, uint64_t(s.id) << 8 | uint64_t(s.level));
    for (const auto& b : h.buffs) g = mix(g, uint64_t(b.id) ^ uint64_t(b.expire_ms));
    return g;
}
template <class A> uint64_t dig_army(uint64_t g, const A& a) {
    g = mix(g, uint64_t(a.id)); g = mix(g, hash_name(std::string_view(a.name)));
    for (const auto& h : a.heroes) g = dig_hero(g, h);
    for (const auto& t : a.troops) g = mix(g, uint64_t(t.type) << 32 | uint64_t(t.count));
    return g;
}
template <class V> uint64_t dig_ints(const V& v) {
    uint64_t g = v.size();
    for (auto x : v) g = mix(g, uint64_t(uint32_t(x)));
    return g;
}

// ---- XVector<int32_t> ----------------------------------------------------------

void vector_cases() {
    constexpr int N = 4096;
    {   // push_back into a cleared vector, capacity kept: B batches of clear + N push_backs
        constexpr int B = 8;
        std::vector<int32_t> sv;
        XBuffer xb = XBuffer::create<xm::IntVec>(64 * 1024, cap());
        auto& xv = xb.root<xm::IntVec>().v;
        run_case("vec.push_back", double(B) * N, "std::vector", {
            {"xoffset", [&](int, uint64_t& d) {
                const double t = timed("vec.push_back", [&] {
                    for (int b = 0; b < B; ++b) { xv.clear(); for (int i = 0; i < N; ++i) xv.push_back(i ^ b); keep(xv.data()); }
                });
                d = dig_ints(xv); return t; }},
            {"std::vector", [&](int, uint64_t& d) {
                const double t = timed(nullptr, [&] {
                    for (int b = 0; b < B; ++b) { sv.clear(); for (int i = 0; i < N; ++i) sv.push_back(i ^ b); keep(sv.data()); }
                });
                d = dig_ints(sv); return t; }},
        });
    }
    {   // push_back from empty, with growth: R empty vectors (fresh buffers, reset untimed), N each
        constexpr int R = 8;
        std::vector<std::optional<std::vector<int32_t>>> sv(R);
        std::vector<XBuffer> xb;
        for (int i = 0; i < R; ++i) xb.push_back(XBuffer::create<xm::IntVec>(4096, cap()));
        run_case("vec.push_back_grow", double(R) * N, "std::vector", {
            {"xoffset", [&](int, uint64_t& d) {
                std::vector<XVector<int32_t>*> v;
                for (auto& b : xb) v.push_back(&b.reset<xm::IntVec>().v);
                const double t = timed("vec.push_back_grow", [&] {
                    for (auto* p : v) { for (int i = 0; i < N; ++i) p->push_back(i); keep(p->data()); }
                });
                d = 0; for (auto* p : v) d = mix(d, dig_ints(*p)); return t; }},
            {"std::vector", [&](int, uint64_t& d) {
                for (auto& o : sv) o.emplace();
                const double t = timed(nullptr, [&] {
                    for (auto& o : sv) { for (int i = 0; i < N; ++i) o->push_back(i); keep(o->data()); }
                });
                d = 0; for (auto& o : sv) d = mix(d, dig_ints(*o)); return t; }},
        });
    }
    {   // iterate and sum, P passes over N elements
        constexpr int P = 32;
        std::vector<int32_t> sv;
        XBuffer xb = XBuffer::create<xm::IntVec>(64 * 1024, cap());
        auto& xv = xb.root<xm::IntVec>().v;
        for (int i = 0; i < N; ++i) { sv.push_back(i * 3); xv.push_back(i * 3); }
        run_case("vec.iterate", double(P) * N, "std::vector", {
            {"xoffset", [&](int, uint64_t& d) {
                int64_t s = 0;
                const double t = timed("vec.iterate", [&] { for (int p = 0; p < P; ++p) { for (auto x : xv) s += x; keep(s); } });
                d = uint64_t(s); return t; }},
            {"std::vector", [&](int, uint64_t& d) {
                int64_t s = 0;
                const double t = timed(nullptr, [&] { for (int p = 0; p < P; ++p) { for (auto x : sv) s += x; keep(s); } });
                d = uint64_t(s); return t; }},
        });
    }
    constexpr int M = 2048, K = 512;
    {   // middle insert: K inserts at the middle of an M-element vector (refilled untimed)
        std::vector<int32_t> sv;
        XBuffer xb = XBuffer::create<xm::IntVec>(64 * 1024, cap());
        auto& xv = xb.root<xm::IntVec>().v;
        auto refill = [](auto& v) { v.clear(); for (int i = 0; i < M; ++i) v.push_back(i); };
        run_case("vec.mid_insert", K, "std::vector", {
            {"xoffset", [&](int, uint64_t& d) {
                refill(xv);
                const double t = timed("vec.mid_insert", [&] {
                    for (int k = 0; k < K; ++k) xv.insert(xv.begin() + xv.size() / 2, k);
                    keep(xv.data());
                });
                d = dig_ints(xv); return t; }},
            {"std::vector", [&](int, uint64_t& d) {
                refill(sv);
                const double t = timed(nullptr, [&] {
                    for (int k = 0; k < K; ++k) sv.insert(sv.begin() + std::ptrdiff_t(sv.size() / 2), k);
                    keep(sv.data());
                });
                d = dig_ints(sv); return t; }},
        });
    }
    {   // middle erase: K erases at the middle of an (M + K)-element vector (refilled untimed)
        std::vector<int32_t> sv;
        XBuffer xb = XBuffer::create<xm::IntVec>(64 * 1024, cap());
        auto& xv = xb.root<xm::IntVec>().v;
        auto refill = [](auto& v) { v.clear(); for (int i = 0; i < M + K; ++i) v.push_back(i); };
        run_case("vec.mid_erase", K, "std::vector", {
            {"xoffset", [&](int, uint64_t& d) {
                refill(xv);
                const double t = timed("vec.mid_erase", [&] {
                    for (int k = 0; k < K; ++k) xv.erase(xv.begin() + xv.size() / 2);
                    keep(xv.data());
                });
                d = dig_ints(xv); return t; }},
            {"std::vector", [&](int, uint64_t& d) {
                refill(sv);
                const double t = timed(nullptr, [&] {
                    for (int k = 0; k < K; ++k) sv.erase(sv.begin() + std::ptrdiff_t(sv.size() / 2));
                    keep(sv.data());
                });
                d = dig_ints(sv); return t; }},
        });
    }
}

// ---- XVector of elements with container heads (Hero: string + two vectors) ----

template <class XVec>  // xm::HeroVecOrd or xm::HeroVecPlain
void hero_cases(const char* variant, bool primary) {
    const char* base = primary ? "std::vector" : nullptr;
    constexpr int M = 256, K = 64;
    auto name = [&](const char* op) { return std::string("hero.") + op + "." + variant; };
    {   // middle erase: K erases at the middle of M heroes (rebuilt untimed)
        std::vector<sm::Hero> sv;
        XBuffer xb = XBuffer::create<XVec>(256 * 1024, cap());
        const std::string n = name("mid_erase");
        run_case(n, K, base, {
            {"xoffset", [&](int, uint64_t& d) {
                auto& xv = xb.reset<XVec>().v;
                for (int i = 0; i < M; ++i) fill_hero(xv.emplace_back(), i);
                const double t = timed(n.c_str(), [&] {
                    for (int k = 0; k < K; ++k) xv.erase(xv.begin() + xv.size() / 2);
                    keep(xv.data());
                });
                d = 0; for (const auto& h : xv) d = dig_hero(d, h); return t; }},
            {"std::vector", [&](int, uint64_t& d) {
                sv.clear();
                for (int i = 0; i < M; ++i) fill_hero(sv.emplace_back(), i);
                const double t = timed(nullptr, [&] {
                    for (int k = 0; k < K; ++k) sv.erase(sv.begin() + std::ptrdiff_t(sv.size() / 2));
                    keep(sv.data());
                });
                d = 0; for (const auto& h : sv) d = dig_hero(d, h); return t; }},
        });
    }
    {   // middle insert: K copies of one hero inserted at the middle of M heroes (rebuilt untimed)
        std::vector<sm::Hero> sv;
        sm::Hero sproto;
        fill_hero(sproto, 9999);
        XBuffer xb = XBuffer::create<XVec>(256 * 1024, cap());
        const std::string n = name("mid_insert");
        run_case(n, K, base, {
            {"xoffset", [&](int, uint64_t& d) {
                auto& root = xb.reset<XVec>();
                fill_hero(root.proto, 9999);
                auto& xv = root.v;
                xv.reserve(M + K);
                for (int i = 0; i < M; ++i) fill_hero(xv.emplace_back(), i);
                const double t = timed(n.c_str(), [&] {
                    for (int k = 0; k < K; ++k) xv.insert(xv.begin() + xv.size() / 2, root.proto);
                    keep(xv.data());
                });
                d = 0; for (const auto& h : xv) d = dig_hero(d, h); return t; }},
            {"std::vector", [&](int, uint64_t& d) {
                sv.clear();
                sv.reserve(M + K);
                for (int i = 0; i < M; ++i) fill_hero(sv.emplace_back(), i);
                const double t = timed(nullptr, [&] {
                    for (int k = 0; k < K; ++k) sv.insert(sv.begin() + std::ptrdiff_t(sv.size() / 2), sproto);
                    keep(sv.data());
                });
                d = 0; for (const auto& h : sv) d = dig_hero(d, h); return t; }},
        });
    }
    {   // emplace_back from empty, with growth (each growth relocates the heroes): R vectors of M heroes
        constexpr int R = 4;
        std::vector<std::optional<std::vector<sm::Hero>>> sv(R);
        std::vector<XBuffer> xb;
        for (int i = 0; i < R; ++i) xb.push_back(XBuffer::create<XVec>(4096, cap()));
        const std::string n = name("push_back_grow");
        run_case(n, double(R) * M, base, {
            {"xoffset", [&](int, uint64_t& d) {
                std::vector<decltype(XVec::v)*> v;
                for (auto& b : xb) v.push_back(&b.reset<XVec>().v);
                const double t = timed(n.c_str(), [&] {
                    for (auto* p : v) { for (int i = 0; i < M; ++i) fill_hero(p->emplace_back(), i); keep(p->data()); }
                });
                d = 0; for (auto* p : v) for (const auto& h : *p) d = dig_hero(d, h); return t; }},
            {"std::vector", [&](int, uint64_t& d) {
                for (auto& o : sv) o.emplace();
                const double t = timed(nullptr, [&] {
                    for (auto& o : sv) { for (int i = 0; i < M; ++i) fill_hero(o->emplace_back(), i); keep(o->data()); }
                });
                d = 0; for (auto& o : sv) for (const auto& h : *o) d = dig_hero(d, h); return t; }},
        });
    }
}

// ---- XString -----------------------------------------------------------------

// XString has no append() today; the case turns on when the library gains one.
template <class S> concept HasAppend = requires(S& s, std::string_view v) { s.append(v); };

template <class XVec>  // XVector<XString>, a template so that the body is compiled only when used
void append_case(XVec& xs, std::vector<std::string>& ss, int S, int P) {
    auto dig = [](const auto& v) { uint64_t g = 0; for (const auto& s : v) g = mix(g, hash_name(std::string_view(s))); return g; };
    constexpr std::string_view pieces[] = {"alpha::", "beta_::", "gamma::", "delta::", "eps__::", "zeta_::"};
    run_case("str.append", double(P) * S * 6, "std::string", {
        {"xoffset", [&](int r, uint64_t& d) {
            const double t = timed("str.append", [&] {
                for (int p = 0; p < P; ++p) {
                    for (int i = 0; i < S; ++i) {
                        xs[i].clear();
                        for (int j = 0; j < 6; ++j) xs[i].append(pieces[std::size_t(i + j + p + r) % 6]);
                    }
                    keep(xs.data());
                }
            });
            d = dig(xs); return t; }},
        {"std::string", [&](int r, uint64_t& d) {
            const double t = timed(nullptr, [&] {
                for (int p = 0; p < P; ++p) {
                    for (int i = 0; i < S; ++i) {
                        ss[i].clear();
                        for (int j = 0; j < 6; ++j) ss[i].append(pieces[std::size_t(i + j + p + r) % 6]);
                    }
                    keep(ss.data());
                }
            });
            d = dig(ss); return t; }},
    });
}

void string_cases() {
    constexpr int S = 256, P = 32;
    std::vector<std::string> shorts, longs;
    for (int i = 0; i < S; ++i) {
        shorts.push_back("s" + std::to_string(1000000 + i));                     // 8 chars: inline in both
        longs.push_back("long_name_" + std::to_string(1000 + i) + std::string(26, 'x'));  // 40 chars
    }
    std::vector<std::string> ss(S);
    XBuffer xb = XBuffer::create<xm::StrVec>(256 * 1024, cap());
    auto& xs = xb.root<xm::StrVec>().s;
    for (int i = 0; i < S; ++i) xs.emplace_back();
    auto dig = [](const auto& v) { uint64_t g = 0; for (const auto& s : v) g = mix(g, hash_name(std::string_view(s))); return g; };
    // P passes per round, each assigning every string a source picked by (round, pass).
    auto assign_case = [&](const char* n, const std::vector<std::string>& src) {
        run_case(n, double(P) * S, "std::string", {
            {"xoffset", [&](int r, uint64_t& d) {
                const double t = timed(n, [&] {
                    for (int p = 0; p < P; ++p) {
                        const int off = (r & 0xffff) * P + p;
                        for (int i = 0; i < S; ++i) xs[i] = std::string_view(src[std::size_t(i + off) % S]);
                        keep(xs.data());
                    }
                });
                d = dig(xs); return t; }},
            {"std::string", [&](int r, uint64_t& d) {
                const double t = timed(nullptr, [&] {
                    for (int p = 0; p < P; ++p) {
                        const int off = (r & 0xffff) * P + p;
                        for (int i = 0; i < S; ++i) ss[i] = src[std::size_t(i + off) % S];
                        keep(ss.data());
                    }
                });
                d = dig(ss); return t; }},
        });
    };
    assign_case("str.assign_short", shorts);
    assign_case("str.assign_long", longs);
    if constexpr (HasAppend<XString>) {
        append_case(xs, ss, S, P);
    } else {
        skipped("str.append", "XString has no append() in this library");
    }
}

// ---- XMap / XSet ---------------------------------------------------------------

// K distinct keys in insertion order, L lookups and an erase order.
//   fresh:  keys, L random lookups and erase order drawn anew every round.
//   reused: as X1 did, the same keys every round and the lookups are the keys
//           in insertion order, repeated; the predictor can learn the sequence.
struct KeyPlan {
    std::vector<int32_t> keys, lookups, erase_order;
    void make(uint64_t seed, int K, int L, bool fresh) {
        Rng rng(seed);
        std::set<int32_t> seen;
        keys.clear();
        while (int(keys.size()) < K) {
            const int32_t k = int32_t(rng.next() & 0x3fffffff);
            if (seen.insert(k).second) keys.push_back(k);
        }
        lookups.resize(std::size_t(L));
        for (std::size_t i = 0; i < lookups.size(); ++i)
            lookups[i] = fresh ? keys[rng.below(uint32_t(K))] : keys[i % keys.size()];
        erase_order = keys;
        for (std::size_t i = erase_order.size(); i > 1; --i) std::swap(erase_order[i - 1], erase_order[rng.below(uint32_t(i))]);
    }
};

template <class M> void map_fill(M& m, const std::vector<int32_t>& keys) { m.clear(); for (auto k : keys) m[k] = k ^ 0x5a5a; }
template <class M> uint64_t map_dig(const M& m) {
    uint64_t g = m.size();
    for (const auto& kv : m) g = mix(g, uint64_t(uint32_t(kv.first)) << 32 | uint32_t(kv.second));
    return g;
}
template <class S> void set_fill(S& s, const std::vector<int32_t>& keys) { s.clear(); for (auto k : keys) s.insert(k); }
template <class S> uint64_t set_dig(const S& s) {
    uint64_t g = s.size();
    for (auto k : s) g = mix(g, uint64_t(uint32_t(k)));
    return g;
}

#if defined(__cpp_lib_flat_map)
#define RELOC_FLAT_MAP 1
using FlatMap = std::flat_map<int32_t, int32_t>;
#endif
#if defined(__cpp_lib_flat_set)
#define RELOC_FLAT_SET 1
using FlatSet = std::flat_set<int32_t>;
#endif

constexpr int kKeys = 512, kLookups = 4096;
constexpr std::tuple<const char*, int, int> kKeyedOps[] = {{"insert", 0, kKeys}, {"find", 1, kLookups}, {"erase", 2, kKeys}};

void keyed_cases(bool fresh) {
    constexpr int K = kKeys, L = kLookups;
    const char* mode = fresh ? "fresh" : "reused";
    KeyPlan plan;
    int planned_round = 0;
    bool planned = false;
    auto prepare_for = [&](const std::string& n) {
        return [&, n](int round) {
            if (!fresh && planned) return;
            if (fresh && planned && planned_round == round) return;
            plan.make(mix(kSeed ^ hash_name(n), fresh ? uint64_t(round + 1000) : 0), K, L, fresh);
            planned = true; planned_round = round;
        };
    };
    // fresh rounds judge against the flat containers; reused rounds are reported only.
    auto primary = [&](const char* flat) -> const char* { return fresh ? flat : nullptr; };

    {   // maps
        XBuffer xb = XBuffer::create<xm::IntMap>(64 * 1024, cap());
        auto& xmap = xb.root<xm::IntMap>().m;
        std::map<int32_t, int32_t> smap;
#if RELOC_FLAT_MAP
        FlatMap fmap;
#endif
        auto sides_insert = [&](const std::string& n) {
            std::vector<Side> s;
            s.push_back({"xoffset", [&, n](int, uint64_t& d) {
                xmap.clear();
                const double t = timed(n.c_str(), [&] { for (auto k : plan.keys) xmap[k] = k ^ 0x5a5a; keep(xmap.size()); });
                d = map_dig(xmap); return t; }});
#if RELOC_FLAT_MAP
            s.push_back({"std::flat_map", [&](int, uint64_t& d) {
                fmap.clear();
                const double t = timed(nullptr, [&] { for (auto k : plan.keys) fmap[k] = k ^ 0x5a5a; keep(fmap.size()); });
                d = map_dig(fmap); return t; }});
#endif
            s.push_back({"std::map", [&](int, uint64_t& d) {
                smap.clear();
                const double t = timed(nullptr, [&] { for (auto k : plan.keys) smap[k] = k ^ 0x5a5a; keep(smap.size()); });
                d = map_dig(smap); return t; }});
            return s;
        };
        auto sides_find = [&](const std::string& n) {
            std::vector<Side> s;
            s.push_back({"xoffset", [&, n](int, uint64_t& d) {
                map_fill(xmap, plan.keys);
                int64_t sum = 0;
                const double t = timed(n.c_str(), [&] { for (auto k : plan.lookups) sum += xmap.find(k)->second; keep(sum); });
                d = uint64_t(sum); return t; }});
#if RELOC_FLAT_MAP
            s.push_back({"std::flat_map", [&](int, uint64_t& d) {
                map_fill(fmap, plan.keys);
                int64_t sum = 0;
                const double t = timed(nullptr, [&] { for (auto k : plan.lookups) sum += fmap.find(k)->second; keep(sum); });
                d = uint64_t(sum); return t; }});
#endif
            s.push_back({"std::map", [&](int, uint64_t& d) {
                map_fill(smap, plan.keys);
                int64_t sum = 0;
                const double t = timed(nullptr, [&] { for (auto k : plan.lookups) sum += smap.find(k)->second; keep(sum); });
                d = uint64_t(sum); return t; }});
            return s;
        };
        auto sides_erase = [&](const std::string& n) {
            std::vector<Side> s;
            s.push_back({"xoffset", [&, n](int, uint64_t& d) {
                map_fill(xmap, plan.keys);
                const double t = timed(n.c_str(), [&] { for (auto k : plan.erase_order) xmap.erase(k); keep(xmap.size()); });
                d = map_dig(xmap); return t; }});
#if RELOC_FLAT_MAP
            s.push_back({"std::flat_map", [&](int, uint64_t& d) {
                map_fill(fmap, plan.keys);
                const double t = timed(nullptr, [&] { for (auto k : plan.erase_order) fmap.erase(k); keep(fmap.size()); });
                d = map_dig(fmap); return t; }});
#endif
            s.push_back({"std::map", [&](int, uint64_t& d) {
                map_fill(smap, plan.keys);
                const double t = timed(nullptr, [&] { for (auto k : plan.erase_order) smap.erase(k); keep(smap.size()); });
                d = map_dig(smap); return t; }});
            return s;
        };
#if RELOC_FLAT_MAP
        const char* flat = "std::flat_map";
#else
        const char* flat = nullptr;
        skipped(std::string("map.*.") + mode + "/std::flat_map", "std::flat_map not in this standard library");
#endif
        for (const auto& [op, mk, ops] : kKeyedOps) {
            const std::string n = std::string("map.") + op + "." + mode;
            planned = false;
            run_case(n, ops, primary(flat), mk == 0 ? sides_insert(n) : mk == 1 ? sides_find(n) : sides_erase(n), prepare_for(n));
        }
    }
    {   // sets
        XBuffer xb = XBuffer::create<xm::IntSet>(64 * 1024, cap());
        auto& xset = xb.root<xm::IntSet>().s;
        std::set<int32_t> sset;
#if RELOC_FLAT_SET
        FlatSet fset;
#endif
        auto sides_insert = [&](const std::string& n) {
            std::vector<Side> s;
            s.push_back({"xoffset", [&, n](int, uint64_t& d) {
                xset.clear();
                const double t = timed(n.c_str(), [&] { for (auto k : plan.keys) xset.insert(k); keep(xset.size()); });
                d = set_dig(xset); return t; }});
#if RELOC_FLAT_SET
            s.push_back({"std::flat_set", [&](int, uint64_t& d) {
                fset.clear();
                const double t = timed(nullptr, [&] { for (auto k : plan.keys) fset.insert(k); keep(fset.size()); });
                d = set_dig(fset); return t; }});
#endif
            s.push_back({"std::set", [&](int, uint64_t& d) {
                sset.clear();
                const double t = timed(nullptr, [&] { for (auto k : plan.keys) sset.insert(k); keep(sset.size()); });
                d = set_dig(sset); return t; }});
            return s;
        };
        auto sides_find = [&](const std::string& n) {
            std::vector<Side> s;
            s.push_back({"xoffset", [&, n](int, uint64_t& d) {
                set_fill(xset, plan.keys);
                int64_t sum = 0;
                const double t = timed(n.c_str(), [&] { for (auto k : plan.lookups) sum += *xset.find(k); keep(sum); });
                d = uint64_t(sum); return t; }});
#if RELOC_FLAT_SET
            s.push_back({"std::flat_set", [&](int, uint64_t& d) {
                set_fill(fset, plan.keys);
                int64_t sum = 0;
                const double t = timed(nullptr, [&] { for (auto k : plan.lookups) sum += *fset.find(k); keep(sum); });
                d = uint64_t(sum); return t; }});
#endif
            s.push_back({"std::set", [&](int, uint64_t& d) {
                set_fill(sset, plan.keys);
                int64_t sum = 0;
                const double t = timed(nullptr, [&] { for (auto k : plan.lookups) sum += *sset.find(k); keep(sum); });
                d = uint64_t(sum); return t; }});
            return s;
        };
        auto sides_erase = [&](const std::string& n) {
            std::vector<Side> s;
            s.push_back({"xoffset", [&, n](int, uint64_t& d) {
                set_fill(xset, plan.keys);
                const double t = timed(n.c_str(), [&] { for (auto k : plan.erase_order) xset.erase(k); keep(xset.size()); });
                d = set_dig(xset); return t; }});
#if RELOC_FLAT_SET
            s.push_back({"std::flat_set", [&](int, uint64_t& d) {
                set_fill(fset, plan.keys);
                const double t = timed(nullptr, [&] { for (auto k : plan.erase_order) fset.erase(k); keep(fset.size()); });
                d = set_dig(fset); return t; }});
#endif
            s.push_back({"std::set", [&](int, uint64_t& d) {
                set_fill(sset, plan.keys);
                const double t = timed(nullptr, [&] { for (auto k : plan.erase_order) sset.erase(k); keep(sset.size()); });
                d = set_dig(sset); return t; }});
            return s;
        };
#if RELOC_FLAT_SET
        const char* flat = "std::flat_set";
#else
        const char* flat = nullptr;
        skipped(std::string("set.*.") + mode + "/std::flat_set", "std::flat_set not in this standard library");
#endif
        for (const auto& [op, mk, ops] : kKeyedOps) {
            const std::string n = std::string("set.") + op + "." + mode;
            planned = false;
            run_case(n, ops, primary(flat), mk == 0 ? sides_insert(n) : mk == 1 ? sides_find(n) : sides_erase(n), prepare_for(n));
        }
    }
}

// ---- nested object build and reset<T>() ---------------------------------------

template <class XArmy>  // xm::ArmyOrd or xm::ArmyPlain
void nested_cases(const char* variant, bool primary) {
    {   // build R armies (8 heroes, 6 troops) from scratch into empty roots (reset / destroyed untimed)
        constexpr int R = 32;
        std::vector<std::optional<sm::Army>> sa(R);
        std::vector<XBuffer> xb;
        for (int i = 0; i < R; ++i) xb.push_back(XBuffer::create<XArmy>(4096, cap()));
        const std::string n = std::string("nested.build.") + variant;
        run_case(n, R, primary ? "std" : nullptr, {
            {"xoffset", [&](int r, uint64_t& d) {
                std::vector<XArmy*> a;
                for (auto& b : xb) a.push_back(&b.reset<XArmy>());
                const double t = timed(n.c_str(), [&] {
                    for (int i = 0; i < R; ++i) { fill_army(*a[std::size_t(i)], (r & 0xffff) + i); keep(a[std::size_t(i)]); }
                });
                d = 0; for (auto* p : a) d = dig_army(d, *p); return t; }},
            {"std", [&](int r, uint64_t& d) {
                for (auto& o : sa) o.reset();
                const double t = timed(nullptr, [&] {
                    for (int i = 0; i < R; ++i) { fill_army(sa[std::size_t(i)].emplace(), (r & 0xffff) + i); keep(&*sa[std::size_t(i)]); }
                });
                d = 0; for (auto& o : sa) d = dig_army(d, *o); return t; }},
        });
    }
    if (!primary) return;
    {   // reset<T>() of R buffers that each hold a built army (rebuilt untimed), against
        // assigning an empty object to a built std army: both drop the old content
        // (std frees its blocks) and leave an empty root. Constructing an empty std
        // object into empty storage (the X1 comparison) is ~2 ns and not the same
        // operation, so it is not a baseline.
        constexpr int R = 64;
        std::vector<sm::Army> sa(R);
        std::vector<XBuffer> xb;
        for (int i = 0; i < R; ++i) xb.push_back(XBuffer::create<XArmy>(16 * 1024, cap(std::size_t(1) << 20)));
        run_case("reset", R, "std", {
            {"xoffset", [&](int r, uint64_t& d) {
                for (int i = 0; i < R; ++i) fill_army(xb[std::size_t(i)].reset<XArmy>(), (r & 0xffff) + i);
                const double t = timed("reset", [&] { for (auto& b : xb) keep(&b.reset<XArmy>()); });
                d = 0; for (auto& b : xb) d = dig_army(d, b.root<XArmy>()); return t; }},
            {"std", [&](int r, uint64_t& d) {
                for (int i = 0; i < R; ++i) { sa[std::size_t(i)] = sm::Army{}; fill_army(sa[std::size_t(i)], (r & 0xffff) + i); }
                const double t = timed(nullptr, [&] { for (auto& a : sa) { a = sm::Army{}; keep(&a); } });
                d = 0; for (auto& a : sa) d = dig_army(d, a); return t; }},
        });
    }
}

} // namespace rb

int main(int argc, char** argv) {
    using namespace rb;
    std::string mode = "bench", out;
    bool rounds_set = false;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        auto next = [&]() -> std::string { if (i + 1 >= argc) { std::fprintf(stderr, "%s needs a value\n", a.c_str()); std::exit(2); } return argv[++i]; };
        if (a == "--mode") mode = next();
        else if (a == "--rounds") { g_opt.rounds = std::atoi(next().c_str()); rounds_set = true; }
        else if (a == "--warmup") g_opt.warmup = std::atoi(next().c_str());
        else if (a == "--out") out = next();
        else if (a == "--cases") {
            const std::string f = next();
            std::size_t pos = 0;
            while (pos <= f.size()) {
                std::size_t end = f.find(',', pos);
                if (end == std::string::npos) end = f.size();
                if (end > pos) g_opt.filter.push_back(f.substr(pos, end - pos));
                pos = end + 1;
            }
        } else {
            std::fprintf(stderr,
                "usage: reloc_bench [--mode bench|verify] [--rounds N] [--warmup N] [--out raw.jsonl] [--cases a,b]\n");
            return 2;
        }
    }
    if (mode == "verify") { if (!rounds_set) g_opt.rounds = 3; g_opt.warmup = 0; }
    else if (mode != "bench") { std::fprintf(stderr, "unknown mode %s\n", mode.c_str()); return 2; }
    if (!out.empty()) {
        g_opt.out = std::fopen(out.c_str(), "w");
        if (!g_opt.out) { std::perror(out.c_str()); return 2; }
#if defined(__clang__)
        const char* compiler = "clang " __clang_version__;
#else
        const char* compiler = "gcc " __VERSION__;
#endif
        std::fprintf(g_opt.out, "{\"meta\":{\"bench\":\"reloc\",\"version\":1,\"rounds\":%d,\"warmup\":%d,\"compiler\":\"%s\"}}\n",
                     g_opt.rounds, g_opt.warmup, compiler);
    }
    std::printf("reloc_bench: mode=%s rounds=%d warmup=%d (console: median ns/op, median per-round ratio xoffset/baseline)\n",
                mode.c_str(), g_opt.rounds, g_opt.warmup);
    vector_cases();
    hero_cases<xm::HeroVecOrd>("ord", true);
    hero_cases<xm::HeroVecPlain>("plain", false);
    string_cases();
    keyed_cases(true);
    keyed_cases(false);
    nested_cases<xm::ArmyOrd>("ord", true);
    nested_cases<xm::ArmyPlain>("plain", false);
    if (g_opt.out) std::fclose(g_opt.out);
    std::printf("%d cases; checks: %s\n", g_cases, g_fail ? "FAILED" : "all passed");
    return g_fail || g_cases == 0 ? 1 : 0;
}
