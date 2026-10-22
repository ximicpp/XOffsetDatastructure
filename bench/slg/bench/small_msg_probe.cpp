// Small-message cost probe for the SLG prototype (scenarios A and D).
// Diagnostic only: splits the per-message cost of XOffset V2 into VM setup,
// owner registry, validation scratch, validation proper and teardown, using
// the prototype's real BattleMsg / Report wires. Full validation is kept in
// every load variant (load_verified / load_verified_with_resource).
//
//   small_msg_probe [--iters 20000] [--rounds 15]
//   small_msg_probe --a-loop SECONDS   (pooled scenario-A pipeline in a loop, for `sample`)
//   small_msg_probe --reuse-loop SECONDS [--players N]
//                                        (SLG_REUSE=1 scenario A + D pipeline in a
//                                        loop with per-phase timing, for `sample`)
//
// Prints one table: median and min ns per operation over the rounds, and
// minor page faults per operation. Variants are interleaved, in a rotated
// order per round. Requires P2996 Clang (same build as slg_bench).
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <memory_resource>
#include <optional>
#include <string>
#include <vector>
#include <sys/mman.h>
#include <sys/resource.h>
#include <unistd.h>
#include "slg/generic.hpp"
#include "slg/model_xoffset.hpp"
#include "slg/trace.hpp"

using namespace slg;
using namespace XOffsetDatastructure;
using Clock = std::chrono::steady_clock;

namespace {

constexpr std::size_t kBattleInitial = 16 * 1024;
constexpr std::size_t kReportInitial = 4 * 1024;
constexpr std::size_t kCap256 = std::size_t(256) << 20;

XBufferPool& pool() {
    static XBufferPool p(XBufferPool::Limits{16, 16 * kCap256, std::size_t(256) << 20});
    return p;
}
XBuffer::MaximumCapacity cap256() {
    return XBuffer::maximum_capacity(kCap256).with_growth(XGrowPolicy::Automatic);
}
XBuffer::MaximumCapacity cap256_pool() { return cap256().with_pool(pool()); }
// D in the prototype calls load_verified without a capacity: default maximum.
XBuffer::MaximumCapacity cap_default_pool() {
    return XBuffer::maximum_capacity(XBuffer::DEFAULT_MAXIMUM_CAPACITY).with_pool(pool());
}

long minor_faults() {
    rusage u{};
    getrusage(RUSAGE_SELF, &u);
    return u.ru_minflt;
}

struct Result { std::vector<double> ns; std::vector<double> flt; };
struct Variant { std::string name; std::function<void()> op; Result r; };

volatile uint64_t g_sink = 0;

// Scratch for the monotonic variants: one reusable buffer, released per load.
alignas(64) std::array<std::byte, 1 << 20> g_scratch;

template <class T>
XBuffer load_mono(const std::string& w, std::optional<XBuffer::MaximumCapacity> cap) {
    std::pmr::monotonic_buffer_resource mono(g_scratch.data(), g_scratch.size(),
                                             std::pmr::new_delete_resource());
    return XBuffer::load_verified_with_resource<T>(
        std::span<const std::byte>(reinterpret_cast<const std::byte*>(w.data()), w.size()), cap, &mono);
}

// Raw OS cost of the V2 owner lifecycle without XOffset: reserve cap + one
// host prefix page (PROT_NONE), enable the prefix, enable and fill the payload
// pages, unmap. Mirrors VirtualMemoryStorage's fresh (unpooled) path.
void raw_vm(std::size_t cap, const std::string& payload) {
    const std::size_t page = std::size_t(::sysconf(_SC_PAGESIZE));
    void* m = ::mmap(nullptr, cap + page, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (m == MAP_FAILED) std::abort();
    char* base = static_cast<char*>(m);
    if (::mprotect(base, page, PROT_READ | PROT_WRITE) != 0) std::abort();
    *reinterpret_cast<void**>(base + page - sizeof(void*)) = base;
    const std::size_t len = ((payload.size() + page - 1) / page) * page;
    if (::mprotect(base + page, len, PROT_READ | PROT_WRITE) != 0) std::abort();
    std::memcpy(base + page, payload.data(), payload.size());
    g_sink += uint64_t(base[page]);
    ::munmap(m, cap + page);
}

} // namespace

int main(int argc, char** argv) {
    int iters = 20000, rounds = 15;
    double a_loop = 0, reuse_loop = 0;
    int players = 2;
    for (int i = 1; i + 1 < argc; i += 2) {
        if (!std::strcmp(argv[i], "--iters")) iters = std::atoi(argv[i + 1]);
        else if (!std::strcmp(argv[i], "--rounds")) rounds = std::atoi(argv[i + 1]);
        else if (!std::strcmp(argv[i], "--a-loop")) a_loop = std::atof(argv[i + 1]);
        else if (!std::strcmp(argv[i], "--reuse-loop")) reuse_loop = std::atof(argv[i + 1]);
        else if (!std::strcmp(argv[i], "--players")) players = std::max(2, std::atoi(argv[i + 1]));
    }

    // Real messages from the M-scale world (seed 20261021, players 1 and 2).
    const uint64_t seed = 20261021;
    const stdm::PlayerState p1 = make_player(seed, 1), p2 = make_player(seed, 2);
    std::string req, res, rep;
    stdm::Report report;
    {
        XBuffer b = XBuffer::create<xm::BattleMsg>(kBattleInitial, cap256());
        auto& msg = b.root<xm::BattleMsg>();
        msg.battle_id = 7; msg.seed = 12345; msg.winner = 0;
        copy_army(msg.attacker, p1.armies[0]);
        copy_army(msg.defender, p2.armies[0]);
        req = b.save_verified<xm::BattleMsg>();
        XBuffer m = XBuffer::load_verified<xm::BattleMsg>(req, cap256());
        simulate_battle(m.root<xm::BattleMsg>());
        res = m.save_verified<xm::BattleMsg>();
        copy_report(report, m.root<xm::BattleMsg>().report);
        XBuffer r = XBuffer::create<xm::Report>(kReportInitial, cap256());
        copy_report(r.root<xm::Report>(), report);
        rep = r.save_verified<xm::Report>();
    }
    std::printf("page %ld B; wire req %zu B, res %zu B, report %zu B\n",
                ::sysconf(_SC_PAGESIZE), req.size(), res.size(), rep.size());

    if (a_loop > 0) {
        // One battle exactly as XOffsetRunner scenario A with SLG_XOFFSET_POOL=1,
        // except apply (which writes into the scene server's players).
        const auto deadline = Clock::now() + std::chrono::duration<double>(a_loop);
        uint64_t n = 0;
        const auto t0 = Clock::now();
        while (Clock::now() < deadline) {
            for (int k = 0; k < 1000; ++k, ++n) {
                std::string q, s2;
                std::optional<XBuffer> m, r;
                {
                    XBuffer b = XBuffer::create<xm::BattleMsg>(kBattleInitial, cap256_pool());
                    auto& msg = b.root<xm::BattleMsg>();
                    msg.battle_id = int64_t(n); msg.seed = int64_t(n * 2654435761u); msg.winner = 0;
                    copy_army(msg.attacker, p1.armies[0]);
                    copy_army(msg.defender, p2.armies[0]);
                    q = b.save_verified<xm::BattleMsg>();
                }
                m.emplace(XBuffer::load_verified<xm::BattleMsg>(q, cap256_pool()));
                simulate_battle(m->root<xm::BattleMsg>());
                s2 = m->save_verified<xm::BattleMsg>();
                r.emplace(XBuffer::load_verified<xm::BattleMsg>(s2, cap256_pool()));
                g_sink += uint64_t(r->root<xm::BattleMsg>().winner);
                m.reset(); r.reset();
            }
        }
        const double ns = double(std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now() - t0).count());
        std::printf("a-loop: %llu battles, %.1f ns per battle (pooled)\n", (unsigned long long)n, ns / double(n));
        return 0;
    }

    if (reuse_loop > 0) {
        // Scenario A and D exactly as XOffsetRunner::run_cycle_reuse: long-lived
        // buffers, reset/save_verified_into/reload_verified, apply into the
        // scene server's player arenas. apply_std applies the same result into
        // std::vector armies (destination-side container cost, reference only).
        // --players N rotates the battles over N players (as the SLG scene
        // server does), each XOffset player in its own buffer and each std
        // player a heap object, to expose cache/TLB effects of apply and copy.
        std::vector<XBuffer> pls;
        std::vector<stdm::PlayerState> sps;
        for (int i = 0; i < players; ++i) {
            const stdm::PlayerState src = make_player(seed, i + 1);
            pls.push_back(XBuffer::create<xm::PlayerState>(64 * 1024, cap256()));
            copy_player(pls.back().root<xm::PlayerState>(), src);
            sps.push_back(src);
        }
        XBuffer a_req = XBuffer::create<xm::BattleMsg>(kBattleInitial, cap256_pool());
        XBuffer a_m = XBuffer::create<xm::BattleMsg>(kBattleInitial, cap256_pool());
        XBuffer a_r = XBuffer::create<xm::BattleMsg>(kBattleInitial, cap256_pool());
        XBuffer d = XBuffer::create<xm::Report>(kReportInitial, cap256_pool());
        std::string q, r2, copy;
        enum { kReset, kCopy, kSaveReq, kReloadReq, kSim, kSaveRes, kReloadRes, kApply, kApplyStd,
               kMemcpyReq, kReloadRep, kQueryRep, kPhases };
        const char* names[kPhases] = {"A.reset", "A.copy_army x2", "A.save_into(req)", "A.reload(req)",
                                      "A.simulate", "A.save_into(res)", "A.reload(res)", "A.apply",
                                      "A.apply_std (ref)", "memcpy(req) (ref)", "D.reload(report)", "D.query"};
        double acc[kPhases] = {};
        auto lap = [](Clock::time_point& t) {
            const auto now = Clock::now();
            const double ns = double(std::chrono::duration_cast<std::chrono::nanoseconds>(now - t).count());
            t = now;
            return ns;
        };
        const auto deadline = Clock::now() + std::chrono::duration<double>(reuse_loop);
        uint64_t n = 0;
        while (Clock::now() < deadline) {
            for (int k = 0; k < 1000; ++k, ++n) {
                const auto ia = std::size_t(n % uint64_t(players));
                const auto id = std::size_t((n * 7 + 1) % uint64_t(players));
                auto& att = pls[ia].root<xm::PlayerState>().armies[0];
                auto& def = pls[id].root<xm::PlayerState>().armies[0];
                auto t = Clock::now();
                auto& msg = a_req.reset<xm::BattleMsg>();
                acc[kReset] += lap(t);
                msg.battle_id = int64_t(n); msg.seed = int64_t(n * 2654435761u); msg.winner = 0;
                copy_army(msg.attacker, att);
                copy_army(msg.defender, def);
                acc[kCopy] += lap(t);
                a_req.save_verified_into<xm::BattleMsg>(q);
                acc[kSaveReq] += lap(t);
                a_m.reload_verified<xm::BattleMsg>(q);
                acc[kReloadReq] += lap(t);
                simulate_battle(a_m.root<xm::BattleMsg>());
                acc[kSim] += lap(t);
                a_m.save_verified_into<xm::BattleMsg>(r2);
                acc[kSaveRes] += lap(t);
                a_r.reload_verified<xm::BattleMsg>(r2);
                acc[kReloadRes] += lap(t);
                const auto& out = a_r.root<xm::BattleMsg>();
                apply_battle_army(att, out.attacker);
                apply_battle_army(def, out.defender);
                acc[kApply] += lap(t);
                apply_battle_army(sps[ia].armies[0], out.attacker);
                apply_battle_army(sps[id].armies[0], out.defender);
                acc[kApplyStd] += lap(t);
                copy.assign(q);
                acc[kMemcpyReq] += lap(t);
                d.reload_verified<xm::Report>(rep);
                acc[kReloadRep] += lap(t);
                g_sink += query_report(d.root<xm::Report>());
                acc[kQueryRep] += lap(t);
                g_sink += uint64_t(copy[17]);
            }
        }
        std::printf("reuse-loop: %llu iterations, %d players; wire req %zu B, res %zu B, report %zu B\n",
                    (unsigned long long)n, players, q.size(), r2.size(), rep.size());
        std::printf("%-22s %10s\n", "phase", "ns/iter");
        for (int i = 0; i < kPhases; ++i) std::printf("%-22s %10.1f\n", names[i], acc[i] / double(n));
        return 0;
    }

    XBuffer loaded_res = XBuffer::load_verified<xm::BattleMsg>(res, cap256());
    XBuffer loaded_rep = XBuffer::load_verified<xm::Report>(rep);

    std::vector<Variant> v;
    auto add = [&](std::string n, std::function<void()> f) { v.push_back({std::move(n), std::move(f), {}}); };

    // --- baselines without XOffset
    add("base.malloc_copy_free(req)", [&] {
        char* p = static_cast<char*>(std::malloc(req.size()));
        std::memcpy(p, req.data(), req.size()); g_sink += uint64_t(p[17]); std::free(p);
    });
    add("raw_vm.256MiB(req)", [&] { raw_vm(kCap256, req); });
    add("raw_vm.64MiB(report)", [&] { raw_vm(XBuffer::DEFAULT_MAXIMUM_CAPACITY, rep); });
    add("raw_vm.1MiB(report)", [&] { raw_vm(std::size_t(1) << 20, rep); });

    // --- A: owner creation and teardown (no content)
    add("A.create_destroy", [&] { XBuffer b = XBuffer::create<xm::BattleMsg>(kBattleInitial, cap256()); g_sink += uint64_t(b.maximum_capacity()); });
    add("A.create_destroy.pool", [&] { XBuffer b = XBuffer::create<xm::BattleMsg>(kBattleInitial, cap256_pool()); g_sink += uint64_t(b.maximum_capacity()); });
    // --- A: req_encode as in the prototype (create + copy two armies + save + destroy)
    auto encode = [&](XBuffer::MaximumCapacity c) {
        XBuffer b = XBuffer::create<xm::BattleMsg>(kBattleInitial, c);
        auto& msg = b.root<xm::BattleMsg>();
        msg.battle_id = 7; msg.seed = 12345; msg.winner = 0;
        copy_army(msg.attacker, p1.armies[0]);
        copy_army(msg.defender, p2.armies[0]);
        std::string w = b.save_verified<xm::BattleMsg>();
        g_sink += w.size();
    };
    add("A.req_encode", [&] { encode(cap256()); });
    add("A.req_encode.pool", [&] { encode(cap256_pool()); });
    add("A.save_only(res)", [&] { std::string w = loaded_res.save_verified<xm::BattleMsg>(); g_sink += w.size(); });
    // --- A: load (= req_decode/res_decode) + destroy (= cleanup)
    add("A.load_destroy", [&] { XBuffer m = XBuffer::load_verified<xm::BattleMsg>(req, cap256()); g_sink += uint64_t(m.maximum_capacity()); });
    add("A.load_destroy.pool", [&] { XBuffer m = XBuffer::load_verified<xm::BattleMsg>(req, cap256_pool()); g_sink += uint64_t(m.maximum_capacity()); });
    add("A.load_destroy.mono", [&] { XBuffer m = load_mono<xm::BattleMsg>(req, cap256()); g_sink += uint64_t(m.maximum_capacity()); });
    add("A.load_destroy.pool+mono", [&] { XBuffer m = load_mono<xm::BattleMsg>(req, cap256_pool()); g_sink += uint64_t(m.maximum_capacity()); });

    // --- D: read as in the prototype (load default cap + query + destroy)
    add("D.read", [&] { XBuffer b = XBuffer::load_verified<xm::Report>(rep); g_sink += query_report(b.root<xm::Report>()); });
    add("D.read.pool", [&] { XBuffer b = XBuffer::load_verified<xm::Report>(rep, cap_default_pool()); g_sink += query_report(b.root<xm::Report>()); });
    add("D.read.mono", [&] { XBuffer b = load_mono<xm::Report>(rep, std::nullopt); g_sink += query_report(b.root<xm::Report>()); });
    add("D.read.pool+mono", [&] { XBuffer b = load_mono<xm::Report>(rep, cap_default_pool()); g_sink += query_report(b.root<xm::Report>()); });
    add("D.query_only", [&] { g_sink += query_report(loaded_rep.root<xm::Report>()); });

    // Split timing for the unpooled paths: load vs destroy, measured per call.
    double split_load_a = 0, split_destroy_a = 0, split_load_d = 0, split_destroy_d = 0;
    {
        std::vector<double> la, da, ld, dd;
        for (int r = 0; r < rounds; ++r) {
            int64_t l = 0, d = 0, l2 = 0, d2 = 0;
            for (int i = 0; i < iters; ++i) {
                auto t0 = Clock::now();
                std::optional<XBuffer> m; m.emplace(XBuffer::load_verified<xm::BattleMsg>(req, cap256()));
                auto t1 = Clock::now(); m.reset(); auto t2 = Clock::now();
                std::optional<XBuffer> b; b.emplace(XBuffer::load_verified<xm::Report>(rep));
                auto t3 = Clock::now(); b.reset(); auto t4 = Clock::now();
                l += (t1 - t0).count(); d += (t2 - t1).count(); l2 += (t3 - t2).count(); d2 += (t4 - t3).count();
            }
            la.push_back(double(l) / iters); da.push_back(double(d) / iters);
            ld.push_back(double(l2) / iters); dd.push_back(double(d2) / iters);
        }
        auto med = [](std::vector<double> x) { std::sort(x.begin(), x.end()); return x[x.size() / 2]; };
        split_load_a = med(la); split_destroy_a = med(da); split_load_d = med(ld); split_destroy_d = med(dd);
    }

    for (auto& x : v) for (int i = 0; i < 200; ++i) x.op();  // warm-up, fills the pool
    for (int r = 0; r < rounds; ++r) {
        for (std::size_t k = 0; k < v.size(); ++k) {
            auto& x = v[(k + std::size_t(r)) % v.size()];
            const long f0 = minor_faults();
            const auto t0 = Clock::now();
            for (int i = 0; i < iters; ++i) x.op();
            const auto t1 = Clock::now();
            x.r.ns.push_back(double(std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count()) / iters);
            x.r.flt.push_back(double(minor_faults() - f0) / iters);
        }
    }

    std::printf("\niters %d x rounds %d; ns per op (median / min), minor faults per op (median)\n", iters, rounds);
    std::printf("%-30s %10s %10s %8s\n", "variant", "median_ns", "min_ns", "faults");
    for (auto& x : v) {
        auto ns = x.r.ns, fl = x.r.flt;
        std::sort(ns.begin(), ns.end()); std::sort(fl.begin(), fl.end());
        std::printf("%-30s %10.1f %10.1f %8.2f\n", x.name.c_str(), ns[ns.size() / 2], ns.front(), fl[fl.size() / 2]);
    }
    std::printf("\nsplit (unpooled, per call incl. ~2 clock reads):\n");
    std::printf("  A load_verified(req, 256MiB) %8.1f ns   destroy %8.1f ns\n", split_load_a, split_destroy_a);
    std::printf("  D load_verified(report)      %8.1f ns   destroy %8.1f ns\n", split_load_d, split_destroy_d);
    const auto ps = pool().stats();
    std::printf("\npool: hits %llu misses %llu cached %zu\n", (unsigned long long)ps.hits,
                (unsigned long long)ps.misses, ps.cached_blocks);
    std::printf("sink %llu\n", (unsigned long long)g_sink);
    return 0;
}
