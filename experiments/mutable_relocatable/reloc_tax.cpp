// X1 + X2 of docs/positioning/MUTABLE_RELOCATABLE.md (section 5).
//
// X1, relocation tax: the same operation on XOffset containers and on std
// containers, inside one process, with no transport at all. Ratio = XOffset / std.
//   micro.*  single container operations
//   slg.*    the shared SLG game logic (bench/slg/include/slg/generic.hpp) run in
//            place on the std model and on the XOffset model
// X2, in-process whole copy: std deep copy against the ways XOffset can copy a
// live buffer today (raw save + load_unverified, save_verified_into +
// reload_verified) and against the clone() / clone_into() prototype on this branch.
//
// Cloud VM numbers: ratios and orders of magnitude only (see the doc).
// Build (GCC 16): see build.sh next to this file.

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <vector>
#if __has_include(<flat_map>)
#include <flat_map>
#endif

#include "xoffsetdatastructure.hpp"
#include "slg/generic.hpp"
#include "slg/model_std.hpp"
#include "slg/model_xoffset.hpp"
#include "slg/trace.hpp"

using namespace XOffsetDatastructure;
using Clock = std::chrono::steady_clock;

namespace {

template <class T> inline void keep(T const& v) { asm volatile("" : : "r,m"(v) : "memory"); }
inline void clobber() { asm volatile("" : : : "memory"); }

// ---- XOffset roots for the micro cases -------------------------------------

struct alignas(8) XIntVec { XVector<std::int32_t> v; };
struct alignas(8) XStrVec { XVector<XString> s; };
struct alignas(8) XIntMap { XMap<std::int32_t, std::int32_t> m; };
struct alignas(8) XHeroVec { XVector<slg::xm::Hero> heroes; };

XBuffer::MaximumCapacity cap() {
    // The SLG runner's setting: Automatic growth, 256 MiB, VM storage.
    return XBuffer::maximum_capacity(std::size_t(256) << 20).with_growth(XGrowPolicy::Automatic);
}

// ---- measurement -----------------------------------------------------------

struct Result {
    std::string name;
    double ops = 1;
    std::vector<double> a, b;  // ns per rep: a = std (or the reference), b = XOffset
};

double median(std::vector<double> v) {
    std::sort(v.begin(), v.end());
    return v[v.size() / 2];
}
double quant(std::vector<double> v, double q) {
    std::sort(v.begin(), v.end());
    return v[std::size_t(q * double(v.size() - 1))];
}

// rep_a / rep_b run one repetition (untimed setup inside) and return the timed ns.
// RELOC_FILTER=substring runs only the matching cases (setup still runs).
bool selected(const char* name) {
    static const char* f = std::getenv("RELOC_FILTER");
    return !f || std::strstr(name, f) != nullptr;
}

Result run(const char* name, double ops, int reps, const std::function<double()>& rep_a,
           const std::function<double()>& rep_b) {
    Result r{name, ops, {}, {}};
    if (!selected(name)) return r;
    for (int i = 0; i < 5; ++i) { rep_a(); rep_b(); }
    for (int i = 0; i < reps; ++i) {
        if (i & 1) { r.b.push_back(rep_b()); r.a.push_back(rep_a()); }
        else       { r.a.push_back(rep_a()); r.b.push_back(rep_b()); }
    }
    return r;
}

template <class F> double timed(F&& f) {
    clobber();
    const auto t0 = Clock::now();
    f();
    clobber();
    const auto t1 = Clock::now();
    return double(std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count());
}

void print_header(const char* a, const char* b) {
    std::printf("%-34s %12s %12s %9s %9s %9s\n", "case", a, b, "ratio", "p10", "p90");
}
void print(const Result& r) {
    if (r.a.empty()) return;
    if (std::getenv("RELOC_DUMP"))
        for (std::size_t i = 0; i < r.a.size(); ++i) std::printf("  %s rep %zu: %.0f %.0f\n", r.name.c_str(), i, r.a[i], r.b[i]);
    std::vector<double> ratio;
    for (std::size_t i = 0; i < r.a.size(); ++i) ratio.push_back(r.b[i] / r.a[i]);
    std::printf("%-34s %12.2f %12.2f %9.3f %9.3f %9.3f\n", r.name.c_str(), median(r.a) / r.ops,
                median(r.b) / r.ops, median(r.b) / median(r.a), quant(ratio, 0.10), quant(ratio, 0.90));
    std::fflush(stdout);
}

int g_fail = 0;
void check(bool ok, const char* what) {
    if (!ok) { std::printf("CHECK FAILED: %s\n", what); ++g_fail; }
}

// ---- X1 micro --------------------------------------------------------------

void micro(int reps) {
    std::printf("\n== X1 micro: ns per operation, ratio = XOffset / std ==\n");
    print_header("std ns/op", "xoffset");
    constexpr int N = 4096;
    slg::Rng rng(42);
    std::vector<std::int32_t> idx(N);
    for (auto& i : idx) i = rng.below(N);

    {   // push_back into a cleared vector (capacity kept)
        std::vector<std::int32_t> sv;
        XBuffer xb = XBuffer::create<XIntVec>(64 * 1024, cap());
        auto* xr = &xb.root<XIntVec>();
        print(run("vec.push_back(cleared)", N, reps,
            [&] { sv.clear(); return timed([&] { for (int i = 0; i < N; ++i) sv.push_back(i); keep(sv.data()); }); },
            [&] { xr->v.clear(); return timed([&] { for (int i = 0; i < N; ++i) xr->v.push_back(i); keep(xr->v.data()); }); }));
    }
    {   // push_back from empty: std constructs a new vector, XOffset resets a reused buffer
        XBuffer xb = XBuffer::create<XIntVec>(64 * 1024, cap());
        print(run("vec.push_back(empty,growth)", N, reps,
            [&] {
                std::optional<std::vector<std::int32_t>> sv;
                const double t = timed([&] { sv.emplace(); for (int i = 0; i < N; ++i) sv->push_back(i); keep(sv->data()); });
                return t;
            },
            [&] {
                return timed([&] { auto& r = xb.reset<XIntVec>(); for (int i = 0; i < N; ++i) r.v.push_back(i); keep(r.v.data()); });
            }));
    }
    {   // iterate and sum
        std::vector<std::int32_t> sv(N);
        for (int i = 0; i < N; ++i) sv[i] = i;
        XBuffer xb = XBuffer::create<XIntVec>(64 * 1024, cap());
        auto& xv = xb.root<XIntVec>().v;
        for (int i = 0; i < N; ++i) xv.push_back(i);
        print(run("vec.iterate_sum", N, reps,
            [&] { std::int64_t s = 0; double t = timed([&] { for (auto x : sv) s += x; keep(s); }); return t; },
            [&] { std::int64_t s = 0; double t = timed([&] { for (auto x : xv) s += x; keep(s); }); return t; }));
        print(run("vec.random_index", N, reps,
            [&] { std::int64_t s = 0; double t = timed([&] { for (auto i : idx) s += sv[std::size_t(i)]; keep(s); }); return t; },
            [&] { std::int64_t s = 0; double t = timed([&] { for (auto i : idx) s += xv[std::size_t(i)]; keep(s); }); return t; }));
    }
    {   // middle insert + middle erase, int, size 1024
        constexpr int M = 1024, K = 64;
        std::vector<std::int32_t> sv(M, 7);
        XBuffer xb = XBuffer::create<XIntVec>(64 * 1024, cap());
        auto& xv = xb.root<XIntVec>().v;
        for (int i = 0; i < M; ++i) xv.push_back(7);
        print(run("vec<int>.mid_insert+erase", K, reps,
            [&] { return timed([&] { for (int k = 0; k < K; ++k) { sv.insert(sv.begin() + M / 2, k); sv.erase(sv.begin() + M / 3); } keep(sv.data()); }); },
            [&] { return timed([&] { for (int k = 0; k < K; ++k) { xv.insert(xv.begin() + M / 2, k); xv.erase(xv.begin() + M / 3); } keep(xv.data()); }); }));
    }
    {   // middle erase + emplace_back, Hero (string + two vectors per element), size 128
        constexpr int M = 128, K = 32;
        std::vector<slg::stdm::Hero> sv;
        for (int i = 0; i < M; ++i) sv.push_back(slg::make_hero(rng, i));
        XBuffer xb = XBuffer::create<XHeroVec>(256 * 1024, cap());
        auto& xv = xb.root<XHeroVec>().heroes;
        for (const auto& h : sv) slg::copy_hero(xv.emplace_back(), h);
        auto fill = [](auto& h, int k) { h.id = k; h.level = 1; h.star = 1; h.name = std::string_view("recruit"); };
        print(run("vec<Hero>.mid_erase+emplace", K, reps,
            [&] { return timed([&] { for (int k = 0; k < K; ++k) { sv.erase(sv.begin() + M / 2); fill(sv.emplace_back(), k); } keep(sv.data()); }); },
            [&] { return timed([&] { for (int k = 0; k < K; ++k) { xv.erase(xv.begin() + M / 2); fill(xv.emplace_back(), k); } keep(xv.data()); }); }));
    }
    {   // string assignment into existing strings: short (inline in both) and long
        constexpr int S = 256;
        std::vector<std::string> shorts, longs;
        for (int i = 0; i < S; ++i) {
            shorts.push_back("s" + std::to_string(100000 + i).substr(0, 7));               // 8 chars
            longs.push_back("long_name_" + std::to_string(i) + std::string(28, 'x'));        // ~40 chars
        }
        std::vector<std::string> ss(S);
        XBuffer xb = XBuffer::create<XStrVec>(256 * 1024, cap());
        auto& xs = xb.root<XStrVec>().s;
        for (int i = 0; i < S; ++i) xs.emplace_back();
        int round = 0;
        print(run("str.assign_short(8B)", S, reps,
            [&] { ++round; return timed([&] { for (int i = 0; i < S; ++i) ss[i] = shorts[(i + round) % S]; keep(ss.data()); }); },
            [&] { return timed([&] { for (int i = 0; i < S; ++i) xs[i] = std::string_view(shorts[(i + round) % S]); keep(xs.data()); }); }));
        print(run("str.assign_long(~40B)", S, reps,
            [&] { ++round; return timed([&] { for (int i = 0; i < S; ++i) ss[i] = longs[(i + round) % S]; keep(ss.data()); }); },
            [&] { return timed([&] { for (int i = 0; i < S; ++i) xs[i] = std::string_view(longs[(i + round) % S]); keep(xs.data()); }); }));
    }
    {   // ordered map: XMap (flat, sorted) against std::map and std::flat_map
        constexpr int K = 512;
        std::vector<std::int32_t> keys(K);
        for (auto& k : keys) k = rng.below(1 << 20);
        std::map<std::int32_t, std::int32_t> sm;
        XBuffer xb = XBuffer::create<XIntMap>(64 * 1024, cap());
        auto& xm = xb.root<XIntMap>().m;
        print(run("map.insert512(vs std::map)", K, reps,
            [&] { sm.clear(); return timed([&] { for (auto k : keys) sm[k] = k; keep(sm.size()); }); },
            [&] { xm.clear(); return timed([&] { for (auto k : keys) xm[k] = k; keep(xm.size()); }); }));
        print(run("map.find512(vs std::map)", K, reps,
            [&] { std::int64_t s = 0; double t = timed([&] { for (auto k : keys) s += sm.find(k)->second; keep(s); }); return t; },
            [&] { std::int64_t s = 0; double t = timed([&] { for (auto k : keys) s += xm.find(k)->second; keep(s); }); return t; }));
        print(run("map.inc_existing(vs std::map)", K, reps,
            [&] { return timed([&] { for (auto k : keys) sm[k] += 1; keep(sm.size()); }); },
            [&] { return timed([&] { for (auto k : keys) xm[k] += 1; keep(xm.size()); }); }));
        print(run("map.erase512(vs std::map)", K, reps,
            [&] { for (auto k : keys) sm[k] = k; return timed([&] { for (auto k : keys) sm.erase(k); keep(sm.size()); }); },
            [&] { for (auto k : keys) xm[k] = k; return timed([&] { for (auto k : keys) xm.erase(k); keep(xm.size()); }); }));
#if defined(__cpp_lib_flat_map)
        std::flat_map<std::int32_t, std::int32_t> fm;
        print(run("map.insert512(vs std::flat_map)", K, reps,
            [&] { fm.clear(); return timed([&] { for (auto k : keys) fm[k] = k; keep(fm.size()); }); },
            [&] { xm.clear(); return timed([&] { for (auto k : keys) xm[k] = k; keep(xm.size()); }); }));
        print(run("map.find512(vs std::flat_map)", K, reps,
            [&] { std::int64_t s = 0; double t = timed([&] { for (auto k : keys) s += fm.find(k)->second; keep(s); }); return t; },
            [&] { std::int64_t s = 0; double t = timed([&] { for (auto k : keys) s += xm.find(k)->second; keep(s); }); return t; }));
        print(run("map.erase512(vs std::flat_map)", K, reps,
            [&] { for (auto k : keys) fm[k] = k; return timed([&] { for (auto k : keys) fm.erase(k); keep(fm.size()); }); },
            [&] { for (auto k : keys) xm[k] = k; return timed([&] { for (auto k : keys) xm.erase(k); keep(xm.size()); }); }));
#else
        std::printf("(std::flat_map not available in this standard library)\n");
#endif
    }
}

// ---- X1 SLG game logic, in place, no transport ------------------------------

void slg_ops(int reps) {
    std::printf("\n== X1 SLG logic in place (no transport): ns per call, ratio = XOffset / std ==\n");
    print_header("std ns", "xoffset");
    const slg::Scale& sc = *slg::find_scale("M");
    constexpr uint64_t seed = 20261022;
    constexpr int P = 64;  // players used as battle sources and migrants
    std::vector<slg::stdm::PlayerState> sp;
    std::vector<XBuffer> xp;
    std::vector<int> armies;
    for (int i = 0; i < P; ++i) {
        sp.push_back(slg::make_player(seed, i + 1));
        xp.push_back(XBuffer::create<slg::xm::PlayerState>(64 * 1024, cap()));
        slg::copy_player(xp.back().root<slg::xm::PlayerState>(), sp.back());
        armies.push_back(int(sp.back().armies.size()));
    }
    slg::Scale small = sc; small.players = P;
    const auto pairs = slg::make_pairings(seed, 1, small, armies);

    // A.build: the battle request built from the two armies (std: copy into a new
    // message; XOffset: reset a reused buffer and copy in place). No encoding.
    {
        XBuffer xb = XBuffer::create<slg::xm::BattleMsg>(16 * 1024, cap());
        std::size_t k = 0;
        print(run("A.build_msg", 1, reps,
            [&] {
                const auto& pr = pairs[k % pairs.size()];
                std::optional<slg::stdm::BattleMsg> m;
                const double t = timed([&] {
                    m.emplace();
                    m->battle_id = pr.battle_id; m->seed = pr.seed;
                    m->attacker = sp[pr.ap].armies[pr.aa];
                    m->defender = sp[pr.dp].armies[pr.da];
                    keep(&*m);
                });
                return t;
            },
            [&] {
                const auto& pr = pairs[k++ % pairs.size()];
                auto& src_a = xp[pr.ap].root<slg::xm::PlayerState>().armies[pr.aa];
                auto& src_d = xp[pr.dp].root<slg::xm::PlayerState>().armies[pr.da];
                return timed([&] {
                    auto& m = xb.reset<slg::xm::BattleMsg>();
                    m.battle_id = pr.battle_id; m.seed = pr.seed;
                    slg::copy_army(m.attacker, src_a);
                    slg::copy_army(m.defender, src_d);
                    keep(&m);
                });
            }));
    }
    {   // The reset part of A.build_msg alone: XOffset reset<T>() of a reused 16 KiB
        // buffer against constructing an empty std message.
        XBuffer xb = XBuffer::create<slg::xm::BattleMsg>(16 * 1024, cap());
        print(run("A.reset_only", 1, reps,
            [&] { std::optional<slg::stdm::BattleMsg> m; return timed([&] { m.emplace(); keep(&*m); }); },
            [&] { return timed([&] { auto& m = xb.reset<slg::xm::BattleMsg>(); keep(&m); }); }));
    }
    // A.simulate and A.apply on the same messages; digests must agree.
    {
        XBuffer xb = XBuffer::create<slg::xm::BattleMsg>(16 * 1024, cap());
        std::size_t ka = 0, kb = 0;
        slg::Digest ga, gb;
        print(run("A.simulate", 1, reps,
            [&] {
                const auto& pr = pairs[ka++ % pairs.size()];
                slg::stdm::BattleMsg m;
                m.battle_id = pr.battle_id; m.seed = pr.seed;
                m.attacker = sp[pr.ap].armies[pr.aa]; m.defender = sp[pr.dp].armies[pr.da];
                const double t = timed([&] { slg::simulate_battle(m); });
                slg::dig_report(ga, m.report);
                return t;
            },
            [&] {
                const auto& pr = pairs[kb++ % pairs.size()];
                auto& m = xb.reset<slg::xm::BattleMsg>();
                m.battle_id = pr.battle_id; m.seed = pr.seed;
                slg::copy_army(m.attacker, sp[pr.ap].armies[pr.aa]);
                slg::copy_army(m.defender, sp[pr.dp].armies[pr.da]);
                const double t = timed([&] { slg::simulate_battle(m); });
                slg::dig_report(gb, m.report);
                return t;
            }));
        check(ga.h == gb.h, "A.simulate digests");
    }
    {   // A.apply: result armies written back into long-lived player state.
        // The first pass over all pairings is reported on its own: the first
        // retired block of a fresh arena creates that owner's free table.
        slg::stdm::BattleMsg sm;
        XBuffer xb = XBuffer::create<slg::xm::BattleMsg>(16 * 1024, cap());
        std::size_t ka = 0, kb = 0;
        auto rep_std = [&] {
            const auto& pr = pairs[ka++ % pairs.size()];
            sm = slg::stdm::BattleMsg{};
            sm.battle_id = pr.battle_id; sm.seed = pr.seed;
            sm.attacker = sp[pr.ap].armies[pr.aa]; sm.defender = sp[pr.dp].armies[pr.da];
            slg::simulate_battle(sm);
            return timed([&] {
                slg::apply_battle_army(sp[pr.ap].armies[pr.aa], sm.attacker);
                slg::apply_battle_army(sp[pr.dp].armies[pr.da], sm.defender);
            });
        };
        auto rep_xo = [&] {
            const auto& pr = pairs[kb++ % pairs.size()];
            auto& m = xb.reset<slg::xm::BattleMsg>();
            auto& pa = xp[pr.ap].root<slg::xm::PlayerState>();
            auto& pd = xp[pr.dp].root<slg::xm::PlayerState>();
            m.battle_id = pr.battle_id; m.seed = pr.seed;
            slg::copy_army(m.attacker, pa.armies[pr.aa]);
            slg::copy_army(m.defender, pd.armies[pr.da]);
            slg::simulate_battle(m);
            return timed([&] {
                slg::apply_battle_army(pa.armies[pr.aa], m.attacker);
                slg::apply_battle_army(pd.armies[pr.da], m.defender);
            });
        };
        Result first{"A.apply(first pass, 512 calls)", 1, {}, {}};
        if (selected(first.name.c_str()))
            for (std::size_t i = 0; i < pairs.size(); ++i) { first.a.push_back(rep_std()); first.b.push_back(rep_xo()); }
        else
            for (std::size_t i = 0; i < pairs.size(); ++i) { rep_std(); rep_xo(); }
        print(first);
        print(run("A.apply(steady)", 1, reps, rep_std, rep_xo));
        slg::Digest ga, gb;
        for (int i = 0; i < P; ++i) { slg::dig_player(ga, sp[i]); slg::dig_player(gb, xp[i].root<slg::xm::PlayerState>()); }
        check(ga.h == gb.h, "A.apply player digests");
    }
    {   // B.season_edits on a migrated player (fresh copy per call)
        XBuffer xb = XBuffer::create<slg::xm::PlayerState>(64 * 1024, cap());
        std::size_t ka = 0, kb = 0;
        slg::Digest ga, gb;
        print(run("B.season_edits(64 ops)", 1, reps,
            [&] {
                const std::size_t i = ka++;
                slg::stdm::PlayerState p = sp[i % P];
                const double t = timed([&] { slg::season_edits(p, slg::mix_seed(seed, i), sc.season_ops); });
                slg::dig_player(ga, p);
                return t;
            },
            [&] {
                const std::size_t i = kb++;
                auto& p = xb.reset<slg::xm::PlayerState>();
                slg::copy_player(p, sp[i % P]);
                const double t = timed([&] { slg::season_edits(p, slg::mix_seed(seed, i), sc.season_ops); });
                slg::dig_player(gb, p);
                return t;
            }));
        check(ga.h == gb.h, "B.season_edits digests");
    }
    {   // C.ticks: one long-lived shard each, 10 ticks per call (stable batch removal)
        slg::stdm::ShardState ss = slg::make_shard(seed, sc);
        XBuffer xb = XBuffer::create<slg::xm::ShardState>(4 * 1024 * 1024, cap());
        slg::copy_shard(xb.root<slg::xm::ShardState>(), ss);
        auto& xs = xb.root<slg::xm::ShardState>();
        std::size_t ka = 0, kb = 0;
        print(run("C.ticks(10)", 1, reps / 4 + 1,
            [&] { const auto i = ka++; return timed([&] { slg::shard_ticks(ss, slg::mix_seed(seed, 777 + i), sc.shard_ticks); }); },
            [&] { const auto i = kb++; return timed([&] { slg::shard_ticks(xs, slg::mix_seed(seed, 777 + i), sc.shard_ticks); }); }));
        slg::Digest ga, gb;
        slg::dig_shard(ga, ss); slg::dig_shard(gb, xs);
        check(ga.h == gb.h, "C.ticks shard digests");
    }
    {   // D.query on one report
        slg::stdm::BattleMsg m;
        m.battle_id = 1; m.seed = 99;
        m.attacker = sp[0].armies[0]; m.defender = sp[1].armies[0];
        slg::simulate_battle(m);
        XBuffer xb = XBuffer::create<slg::xm::Report>(4 * 1024, cap());
        slg::copy_report(xb.root<slg::xm::Report>(), m.report);
        const auto& xr = xb.root<slg::xm::Report>();
        print(run("D.query", 64, reps,
            [&] { uint64_t s = 0; double t = timed([&] { for (int i = 0; i < 64; ++i) { s += slg::query_report(m.report); clobber(); } keep(s); }); return t; },
            [&] { uint64_t s = 0; double t = timed([&] { for (int i = 0; i < 64; ++i) { s += slg::query_report(xr); clobber(); } keep(s); }); return t; }));
    }
}

// ---- X2 in-process whole copy ---------------------------------------------

template <class XT, class ST, class Dig>
void copy_family(const char* label, const ST& src_std, XBuffer& src, int reps, Dig&& dig) {
    const std::size_t used = std::size_t(src.arena()->used_end);
    std::printf("\n-- %s: payload used %zu B, free blocks %zu (%llu B) --\n", label, used,
                src.free_block_count(), (unsigned long long)src.free_block_bytes());
    print_header("std ns", "xoffset");
    char name[96];

    // References.
    std::vector<std::byte> raw(used);
    std::memcpy(raw.data(), src.arena(), used);
    auto ref_construct = [&] {
        std::optional<ST> c;
        const double t = timed([&] { c.emplace(src_std); keep(&*c); });
        return t;
    };
    ST assigned = src_std;
    auto ref_assign = [&] { return timed([&] { assigned = src_std; keep(&assigned); }); };

    std::snprintf(name, sizeof name, "%s.memcpy_floor", label);
    print(run(name, 1, reps, ref_assign, [&] { return timed([&] { std::memcpy(raw.data(), src.arena(), used); keep(raw.data()); }); }));

    // Today: raw save + load_unverified (two copies, free table lost, root<T>() refused).
    std::snprintf(name, sizeof name, "%s.save+load_unverified", label);
    print(run(name, 1, reps, ref_construct, [&] {
        std::optional<XBuffer> c;
        const double t = timed([&] { std::string s = src.save(); c.emplace(XBuffer::load_unverified(s, cap())); keep(&*c); });
        return t;
    }));

    // Today, validated and reusing: save_verified_into + reload_verified.
    {
        std::string wire;
        XBuffer dst = XBuffer::create<XT>(used, cap());
        std::snprintf(name, sizeof name, "%s.save_v_into+reload_v", label);
        print(run(name, 1, reps, ref_assign, [&] {
            return timed([&] { src.template save_verified_into<XT>(wire); dst.template reload_verified<XT>(wire); keep(&dst); });
        }));
    }

    // Prototype: clone() into a new owner, clone_into() a reused owner.
    std::snprintf(name, sizeof name, "%s.clone(new owner)", label);
    print(run(name, 1, reps, ref_construct, [&] {
        std::optional<XBuffer> c;
        const double t = timed([&] { c.emplace(src.clone()); keep(&*c); });
        return t;
    }));
    {
        XBuffer dst = XBuffer::create<XT>(used, cap());
        std::snprintf(name, sizeof name, "%s.clone_into(reused)", label);
        print(run(name, 1, reps, ref_assign, [&] { return timed([&] { src.clone_into(dst); keep(&dst); }); }));

        // Correctness: same content, same free table, typed access kept, both
        // sides keep working independently, and the clone still verifies.
        src.clone_into(dst);
        slg::Digest g0, g1;
        dig(g0, src.root<XT>());
        dig(g1, dst.root<XT>());
        check(g0.h == g1.h, "clone digest");
        check(dst.free_block_count() == src.free_block_count() &&
              dst.free_block_bytes() == src.free_block_bytes(), "clone free table");
        check(dst.free_table_complete() == src.free_table_complete(), "clone free table completeness");
        const std::string w0 = src.template save_verified<XT>();
        const std::string w1 = dst.template save_verified<XT>();
        check(w0 == w1, "clone wire identical");
        XBuffer back = XBuffer::load_verified<XT>(w1, cap());
        slg::Digest g2;
        dig(g2, back.root<XT>());
        check(g2.h == g0.h, "clone round trip");
    }
    std::printf("(reference column: construct = std copy constructor into a new object; "
                "assign = copy assignment into an existing copy)\n");
}

void whole_copy(int reps) {
    std::printf("\n== X2 in-process whole copy: ns per copy, ratio = XOffset / std deep copy ==\n");
    constexpr uint64_t seed = 20261022;
    const slg::Scale& sc = *slg::find_scale("M");

    {   // One player, after some edits so the arena has holes.
        slg::stdm::PlayerState sp = slg::make_player(seed, 3);
        XBuffer xb = XBuffer::create<slg::xm::PlayerState>(64 * 1024, cap());
        slg::copy_player(xb.root<slg::xm::PlayerState>(), sp);
        slg::season_edits(sp, 5, 256);
        slg::season_edits(xb.root<slg::xm::PlayerState>(), 5, 256);
        copy_family<slg::xm::PlayerState>("player", sp, xb, reps,
            [](slg::Digest& g, const auto& p) { slg::dig_player(g, p); });

        // Independence: mutate the clone, the source must not change.
        XBuffer c = xb.clone();
        slg::Digest before, after, cl;
        slg::dig_player(before, xb.root<slg::xm::PlayerState>());
        slg::season_edits(c.root<slg::xm::PlayerState>(), 11, 512);
        slg::dig_player(after, xb.root<slg::xm::PlayerState>());
        slg::dig_player(cl, c.root<slg::xm::PlayerState>());
        check(before.h == after.h && cl.h != before.h, "clone independence");
        slg::stdm::PlayerState sp2 = sp;
        slg::season_edits(sp2, 11, 512);
        slg::Digest sd;
        slg::dig_player(sd, sp2);
        check(sd.h == cl.h, "clone keeps mutating like std");
    }
    {   // The M-scale map shard, after ticks.
        slg::stdm::ShardState ss = slg::make_shard(seed, sc);
        XBuffer xb = XBuffer::create<slg::xm::ShardState>(4 * 1024 * 1024, cap());
        slg::copy_shard(xb.root<slg::xm::ShardState>(), ss);
        slg::shard_ticks(ss, 9, 30);
        slg::shard_ticks(xb.root<slg::xm::ShardState>(), 9, 30);
        copy_family<slg::xm::ShardState>("shard", ss, xb, std::max(11, reps / 10),
            [](slg::Digest& g, const auto& s) { slg::dig_shard(g, s); });
    }
}

} // namespace

int main(int argc, char** argv) {
    const int reps = argc > 1 ? std::atoi(argv[1]) : 201;
    std::printf("reloc_tax: reps=%d (median of paired reps; ratio column = median(xoffset)/median(std), "
                "p10/p90 of per-rep ratios)\n", reps);
    micro(reps);
    slg_ops(reps);
    whole_copy(reps);
    std::printf("\nchecks: %s\n", g_fail ? "FAILED" : "all passed");
    return g_fail ? 1 : 0;
}
