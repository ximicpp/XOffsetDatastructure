// Diagnostic probe for scenario C ticks (not a formal measurement).
// P1 ticks:   shard_ticks_middle_erase (the original one-by-one erase) on a restored shard.
// P2 erase:   one middle erase in a 2000-element vector of March, ns per shifted element.
// P3 erase_if: ticks rewritten as one compaction pass per tick (same result, checked by digest).
// Build in the SLG prototype tree; the std-only part also builds without XOffset.
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <vector>
#include "slg/generic.hpp"
#include "slg/model_std.hpp"
#include "slg/trace.hpp"
#ifdef SLG_WITH_XOFFSET
#include "slg/model_xoffset.hpp"
// model_xoffset.hpp opts March into stable_erase_if.
#endif

using namespace slg;
using Clock = std::chrono::steady_clock;

static double ms_since(Clock::time_point t0) {
    return std::chrono::duration<double, std::milli>(Clock::now() - t0).count();
}
static double median(std::vector<double> v) {
    std::sort(v.begin(), v.end());
    return v[v.size() / 2];
}

// Same logic and rng sequence as shard_ticks, but arrivals leave in one stable pass.
template <class S, class Compact> void shard_ticks_compact(S& s, uint64_t seed, int ticks, Compact&& compact) {
    Rng rng(seed);
    for (int t = 0; t < ticks; ++t) {
        s.tick += 1;
        int arrivals = 0;
        for (std::size_t k = s.marches.size(); k-- > 0;) {
            auto& m = s.marches[k];
            if (--m.eta > 0) continue;
            const std::size_t idx = std::size_t(m.to_y) * std::size_t(s.width) + std::size_t(m.to_x);
            if (idx < s.tiles.size()) { s.tiles[idx].owner = m.owner; s.tiles[idx].level = std::min(s.tiles[idx].level + 1, 10); }
            ++arrivals;
        }
        compact(s.marches);
        for (int a = 0; a < arrivals; ++a) {
            auto& m = s.marches.emplace_back();
            m.march_id = int64_t(s.tick) * 100000 + a; m.owner = 1 + rng.below(5000);
            m.from_x = rng.below(s.width); m.from_y = rng.below(s.width);
            m.to_x = rng.below(s.width); m.to_y = rng.below(s.width);
            m.eta = rng.range(1, 30);
            for (int k = 0, n = rng.range(1, 4); k < n; ++k) { auto& tr = m.troops.emplace_back(); tr.type = rng.below(4); tr.count = rng.range(100, 5000); }
        }
        for (auto& c : s.cities) {
            for (auto& kv : c.resources) kv.second += 10 + (kv.first % 3);
            if (rng.below(50) == 0) { c.level += 1; c.buildings[rng.below(40)] += 1; }
        }
    }
}

int main(int argc, char** argv) {
    const int reps = argc > 1 ? std::atoi(argv[1]) : 21;
    const Scale& sc = *find_scale("M");
    const uint64_t seed = 20261021;
    const stdm::ShardState base = make_shard(seed, sc);
    const uint64_t tick_seed = mix_seed(seed, 0xc0 + 1);

    // ---- std ----
    std::vector<double> p1s, p3s, p2s;
    uint64_t dig_std = 0, dig_std3 = 0;
    for (int r = 0; r < reps; ++r) {
        stdm::ShardState a = base, b = base;
        auto t0 = Clock::now(); shard_ticks_middle_erase(a, tick_seed, sc.shard_ticks); p1s.push_back(ms_since(t0));
        t0 = Clock::now();
        shard_ticks_compact(b, tick_seed, sc.shard_ticks, [](auto& v) { std::erase_if(v, [](const stdm::March& m) { return m.eta <= 0; }); });
        p3s.push_back(ms_since(t0));
        Digest g1, g3; dig_shard(g1, a); dig_shard(g3, b); dig_std = g1.h; dig_std3 = g3.h;
        std::vector<stdm::March> v(base.marches.begin(), base.marches.end());
        t0 = Clock::now();
        for (int i = 0; i < 1000; ++i) { v.erase(v.begin() + 1000); v.emplace_back().troops.resize(2); }
        p2s.push_back(ms_since(t0) * 1e6 / (1000.0 * 999.0));
    }
    std::printf("std     P1 ticks %.3f ms  P3 erase_if-ticks %.3f ms  P2 %.2f ns/shifted  digest %s\n",
                median(p1s), median(p3s), median(p2s), dig_std == dig_std3 ? "same" : "DIFFERENT");

#ifdef SLG_WITH_XOFFSET
    using namespace XOffsetDatastructure;
    const auto cap = XBuffer::maximum_capacity(std::size_t(256) << 20).with_growth(XGrowPolicy::Automatic);
    XBuffer src = XBuffer::create<xm::ShardState>(4 * 1024 * 1024, cap);
    copy_shard(src.root<xm::ShardState>(), base);
    const std::string wire = src.save_verified<xm::ShardState>();
    std::vector<double> x1, x3, x2;
    uint64_t dig_x = 0, dig_x3 = 0;
    for (int r = 0; r < reps; ++r) {
        XBuffer a = XBuffer::load_verified<xm::ShardState>(wire, cap);
        XBuffer b = XBuffer::load_verified<xm::ShardState>(wire, cap);
        auto t0 = Clock::now(); shard_ticks_middle_erase(a.root<xm::ShardState>(), tick_seed, sc.shard_ticks); x1.push_back(ms_since(t0));
        t0 = Clock::now();
        shard_ticks_compact(b.root<xm::ShardState>(), tick_seed, sc.shard_ticks,
                            [](auto& v) { v.stable_erase_if([](const xm::March& m) { return m.eta <= 0; }); });
        x3.push_back(ms_since(t0));
        Digest g1, g3; dig_shard(g1, a.root<xm::ShardState>()); dig_shard(g3, b.root<xm::ShardState>());
        dig_x = g1.h; dig_x3 = g3.h;
        XBuffer c = XBuffer::load_verified<xm::ShardState>(wire, cap);
        auto& v = c.root<xm::ShardState>().marches;
        t0 = Clock::now();
        for (int i = 0; i < 1000; ++i) { v.erase(v.begin() + 1000); v.emplace_back().troops.resize(2); }
        x2.push_back(ms_since(t0) * 1e6 / (1000.0 * 999.0));
    }
    std::printf("xoffset P1 ticks %.3f ms  P3 erase_if-ticks %.3f ms  P2 %.2f ns/shifted  digest %s, vs std %s\n",
                median(x1), median(x3), median(x2), dig_x == dig_x3 ? "same" : "DIFFERENT",
                dig_x == dig_std ? "same" : "DIFFERENT");
    // P4: run only the original XOffset ticks for profiling (argv[2] = seconds).
    if (argc > 2) {
        const double secs = std::atof(argv[2]);
        auto t0 = Clock::now(); int n = 0;
        while (ms_since(t0) < secs * 1000) {
            XBuffer a = XBuffer::load_verified<xm::ShardState>(wire, cap);
            shard_ticks_middle_erase(a.root<xm::ShardState>(), tick_seed, sc.shard_ticks); ++n;
        }
        std::printf("P4 profile loop: %d runs\n", n);
    }
#endif
    return 0;
}
