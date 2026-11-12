// SLG prototype benchmark driver.
//
//   slg_bench --mode verify [--scale S|M|L] [--cycles 2] [--formats a,b,...]
//       Runs every format on the same synthetic world and checks that the game state
//       and the report queries are identical after every cycle. Prints wire sizes.
//   slg_bench --mode bench --out raw.jsonl [--scale M] [--cycles 41] [--seed N] [--formats ...]
//       Interleaves formats in a random order per cycle and writes one JSON line per
//       (cycle, format, metric). Analyse with tools/analyze.py; cycle 0 is warm-up.
//   slg_bench --mode proc --out proc.jsonl [--scale M] [--cycles 11] --formats <one format>
//       Linux only. Runs the cycles untimed, then writes the process's memory from
//       /proc/self (PROC.* rows: RSS, peak RSS, page tables, mappings), before setup
//       ("base") and at the end. Run one format per process: the numbers are the process's.
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>
#include "runner.hpp"
#include "std_runner.hpp"
#include "slg/view.hpp"
#if SLG_WITH_CEREAL
#include "cereal_format.hpp"
#endif
#if SLG_WITH_PROTOBUF
#include "protobuf_format.hpp"
#endif
#if SLG_WITH_FLATBUFFERS
#include "flatbuffers_format.hpp"
#endif
#if SLG_WITH_ZPP_BITS
#include "zpp_bits_format.hpp"
#endif
#if SLG_WITH_CISTA
#include "cista_runner.hpp"
#endif
#if SLG_WITH_XOFFSET
#include "xoffset_runner.hpp"
#endif
#if SLG_WITH_COMPRESSION
#include <lz4.h>
#include <zstd.h>
#endif

using namespace slg;

namespace {

std::unique_ptr<Runner> make_runner(const std::string& n) {
#if SLG_WITH_XOFFSET
    if (n == "xoffset") return std::make_unique<XOffsetRunner>();
    // Trust tier (3.4 P1): whole-wire receives through XReceiver admit; scoring main column.
    if (n == "xoffset_trusted") return std::make_unique<XOffsetTrustedRunner>();
    if (n == "xoffset_wire") return std::make_unique<XOffsetWireRunner>();   // W3 step 0, SLG_XPORT=1 only
    if (n == "xoffset_ideal_recv") return std::make_unique<XOffsetIdealRecvRunner>();   // W3 step 0 diagnostic, SLG_XPORT=1 only
    // Diagnostic formats (not in the default list): the model with wire_record_layout
    // declared, received with reload_verified / with direct receive (SLG_REUSE=1 only).
    if (n == "xoffset_optin") return std::make_unique<XOffsetRunnerT<XoOptinModel, false>>();
    if (n == "xoffset_direct") return std::make_unique<XOffsetRunnerT<XoOptinModel, true>>();
#endif
#if SLG_WITH_CEREAL
    if (n == "cereal") return std::make_unique<StdRunner<CerealFormat>>();
#endif
#if SLG_WITH_PROTOBUF
    if (n == "protobuf") return std::make_unique<StdRunner<ProtobufFormat>>();
#endif
#if SLG_WITH_FLATBUFFERS
    if (n == "flatbuffers") return std::make_unique<StdRunner<FlatBuffersFormat>>();
    if (n == "flatbuffers_noverify") return std::make_unique<StdRunner<FlatBuffersFormatT<false>>>();  // diagnostic only
#endif
#if SLG_WITH_ZPP_BITS
    if (n == "zpp_bits") return std::make_unique<StdRunner<ZppBitsFormat>>();
    // Diagnostic (4.0 W1): the D* rows with dirty-flag change bodies; U* rows as zpp_bits.
    if (n == "zpp_bits_dirty") return std::make_unique<StdRunner<ZppBitsDirtyFormat>>();
#endif
#if SLG_WITH_CISTA
    if (n == "cista") return std::make_unique<CistaRunner<cista::mode::DEEP_CHECK>>();
    if (n == "cista_direct") return std::make_unique<CistaRunner<cista::mode::DEEP_CHECK, true>>();  // diagnostic only, SLG_REUSE=1
    if (n == "cista_unchecked") return std::make_unique<CistaRunner<cista::mode::CAST>>();  // diagnostic only
#endif
    return nullptr;
}

std::string all_formats() {
    std::string s;
    for (const char* n : {"xoffset", "cereal", "protobuf", "flatbuffers", "zpp_bits", "cista"})
        if (make_runner(n)) s += (s.empty() ? "" : ",") + std::string(n);
    return s;
}

std::vector<std::string> split(const std::string& s) {
    std::vector<std::string> out;
    std::stringstream ss(s);
    for (std::string x; std::getline(ss, x, ',');) if (!x.empty()) out.push_back(x);
    return out;
}

struct Compressed { std::size_t zstd = 0, lz4 = 0; };
Compressed compress(const std::string& w) {
    Compressed c;
#if SLG_WITH_COMPRESSION
    std::string z(ZSTD_compressBound(w.size()), '\0');
    c.zstd = ZSTD_compress(z.data(), z.size(), w.data(), w.size(), 3);
    std::string l(std::size_t(LZ4_compressBound(int(w.size()))), '\0');
    c.lz4 = std::size_t(LZ4_compress_default(w.data(), l.data(), int(w.size()), int(l.size())));
#else
    (void)w;
#endif
    return c;
}

void print_sizes(const std::vector<std::unique_ptr<Runner>>& rs, const std::vector<CycleStats>& st,
                 const std::vector<SampleWires>& keep, const Scale& sc) {
    if (demo_cases()) {
        std::printf("\nwire bytes per message, demo case set (average over the last cycle)\n");
        std::printf("%-12s %10s %10s %10s %10s\n", "format", "A.req", "A.res", "B.player", "S.player");
        for (std::size_t i = 0; i < rs.size(); ++i) {
            auto avg = [&](const char* k, int n) { return n ? double(st[i].bytes.at(k)) / n : 0.0; };
            std::printf("%-12s %10.0f %10.0f %10.0f %10.0f\n", rs[i]->name().c_str(), avg("A.req", sc.battles),
                        avg("A.res", sc.battles), avg("B.out", sc.migrants), avg("S.player", sc.migrants));
        }
        std::printf("\nserver -> client, bytes: EB = one viewport pull while browsing (raw / zstd-3),\n"
                    "U = one viewport delta to a static client (raw / zstd-3), rate P/C/S\n");
        std::printf("%-12s %-4s %10s %10s %10s %10s %10s %10s\n", "format", "case", "pull", "pull.zstd", "U.delta", "U.zstd", "pulls", "deliveries");
        for (std::size_t i = 0; i < rs.size(); ++i)
            for (const char* r : {"B", "P", "C", "S"}) {
                const auto& b = st[i].bytes;
                auto get = [&](const std::string& k) { const auto it = b.find(k); return it == b.end() ? 0.0 : double(it->second); };
                const std::string E = std::string("E") + r, U = std::string("U") + r, N = std::string("N") + r;
                const double req = get(E + ".requests"), del = get(U + ".deliveries");
                std::printf("%-12s %-4s %10.0f %10.0f %10.0f %10.0f %10.0f %10.0f\n", rs[i]->name().c_str(), r,
                            req ? get(E + ".window") / req : 0, req ? get(E + ".window_zstd") / req : 0,
                            del ? get(U + ".delta") / del : 0, del ? get(U + ".delta_zstd") / del : 0, req, del);
                if (get(E + ".window_compact") > 0)
                    std::printf("%-12s %-4s %10.0f %10.0f   (save_compact pull / zstd-3)\n", "", "", get(E + ".window_compact") / req,
                                get(E + ".window_compact_zstd") / req);
                // Native delta per message: per static delivery, or per NB (browse) delivery.
                const double nmsg = get(N + ".deliveries") > 0 ? get(N + ".deliveries") : del;
                if (get(N + ".delta") > 0 && nmsg > 0)
                    std::printf("%-12s %-4s %10s %10s %10.0f %10.0f   (native delta / zstd-3)\n", "", "", "", "", get(N + ".delta") / nmsg,
                                get(N + ".delta_zstd") / nmsg);
            }
        (void)keep;
        return;
    }
    std::printf("\nwire bytes per message (average over the last cycle)\n");
    std::printf("%-12s %12s %12s %12s %12s %12s\n", "format", "A.req", "A.res", "B.player", "C.shard", "D.report");
    for (std::size_t i = 0; i < rs.size(); ++i) {
        auto avg = [&](const char* k, int n) { return n ? double(st[i].bytes.at(k)) / n : 0.0; };
        std::printf("%-12s %12.0f %12.0f %12.0f %12.0f %12.0f\n", rs[i]->name().c_str(), avg("A.req", sc.battles),
                    avg("A.res", sc.battles), avg("B.back", sc.migrants), avg("C.snapshot2", 1), avg("D.report", sc.battles));
    }
#if SLG_WITH_COMPRESSION
    std::printf("\ncompressed size of one sample message (zstd -3 / lz4), bytes\n");
    std::printf("%-12s %21s %21s %21s %21s\n", "format", "A.res", "B.player", "C.shard", "D.report");
    for (std::size_t i = 0; i < rs.size(); ++i) {
        std::printf("%-12s", rs[i]->name().c_str());
        for (const std::string* w : {&keep[i].res, &keep[i].player, &keep[i].shard, &keep[i].report}) {
            const auto c = compress(*w);
            std::printf(" %10zu/%10zu", c.zstd, c.lz4);
        }
        std::printf("\n");
    }
#else
    (void)keep;
#endif
}

} // namespace

#if SLG_MEMCOUNT
// --mode mem: every operator new counts its bytes (a header in front of the block
// keeps the size for delete). Only this build pays for it.
#include <atomic>
#include <new>
namespace {
std::atomic<std::int64_t> g_live{0};
constexpr std::size_t kHdr = 16;
void* counted(std::size_t n, std::size_t align) {
    const std::size_t h = align > kHdr ? align : kHdr;
    void* raw = nullptr;
    if (posix_memalign(&raw, h, n + h) != 0 || !raw) throw std::bad_alloc();
    char* p = static_cast<char*>(raw) + h;
    reinterpret_cast<std::size_t*>(p)[-1] = n;
    reinterpret_cast<std::size_t*>(p)[-2] = h;
    g_live.fetch_add(std::int64_t(n), std::memory_order_relaxed);
    return p;
}
void uncounted(void* p) noexcept {
    if (!p) return;
    const std::size_t n = static_cast<std::size_t*>(p)[-1], h = static_cast<std::size_t*>(p)[-2];
    g_live.fetch_sub(std::int64_t(n), std::memory_order_relaxed);
    std::free(static_cast<char*>(p) - h);
}
} // namespace
std::int64_t slg::mem_live() { return g_live.load(std::memory_order_relaxed); }
void* operator new(std::size_t n) { return counted(n, kHdr); }
void* operator new[](std::size_t n) { return counted(n, kHdr); }
void* operator new(std::size_t n, std::align_val_t a) { return counted(n, std::size_t(a)); }
void* operator new[](std::size_t n, std::align_val_t a) { return counted(n, std::size_t(a)); }
void* operator new(std::size_t n, const std::nothrow_t&) noexcept { try { return counted(n, kHdr); } catch (...) { return nullptr; } }
void* operator new[](std::size_t n, const std::nothrow_t&) noexcept { try { return counted(n, kHdr); } catch (...) { return nullptr; } }
void operator delete(void* p) noexcept { uncounted(p); }
void operator delete[](void* p) noexcept { uncounted(p); }
void operator delete(void* p, std::size_t) noexcept { uncounted(p); }
void operator delete[](void* p, std::size_t) noexcept { uncounted(p); }
void operator delete(void* p, std::align_val_t) noexcept { uncounted(p); }
void operator delete[](void* p, std::align_val_t) noexcept { uncounted(p); }
void operator delete(void* p, std::size_t, std::align_val_t) noexcept { uncounted(p); }
void operator delete[](void* p, std::size_t, std::align_val_t) noexcept { uncounted(p); }
#endif

namespace {
// --mode proc: process memory from /proc/self/status (kB fields) and the mapping
// count from /proc/self/maps. Empty on platforms without /proc.
struct ProcMem { long long rss = -1, hwm = -1, pte = -1, anon = -1, vmas = -1; };
ProcMem read_proc_mem() {
    ProcMem m;
    std::ifstream st("/proc/self/status");
    std::string line;
    auto field = [&](const char* key, long long& v) {
        const std::size_t n = std::char_traits<char>::length(key);
        if (line.compare(0, n, key) == 0) v = std::atoll(line.c_str() + n) * 1024;
    };
    while (std::getline(st, line)) {
        field("VmRSS:", m.rss); field("VmHWM:", m.hwm); field("VmPTE:", m.pte); field("RssAnon:", m.anon);
    }
    std::ifstream maps("/proc/self/maps");
    if (maps) { m.vmas = 0; while (std::getline(maps, line)) ++m.vmas; }
    return m;
}
} // namespace

int main(int argc, char** argv) {
    std::string mode = "verify", scale = "S", formats, out;
    int cycles = -1;
    uint64_t seed = 20261021;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        auto next = [&]() -> std::string { if (i + 1 >= argc) { std::cerr << "missing value for " << a << "\n"; std::exit(2); } return argv[++i]; };
        if (a == "--mode") mode = next();
        else if (a == "--scale") scale = next();
        else if (a == "--cycles") cycles = std::stoi(next());
        else if (a == "--seed") seed = std::stoull(next());
        else if (a == "--formats") formats = next();
        else if (a == "--out") out = next();
        else { std::cerr << "unknown argument " << a << "\n"; return 2; }
    }
    const Scale* sc = find_scale(scale);
    if (!sc) { std::cerr << "unknown scale " << scale << "\n"; return 2; }
    if (demo_cases() && !reuse_mode()) { std::cerr << "SLG_CASES=demo needs SLG_REUSE=1\n"; return 2; }
    if (formats.empty()) formats = all_formats();
    if (cycles < 0) cycles = mode == "verify" || mode == "mem" ? 2 : mode == "proc" ? 11 : 41;
    const ProcMem proc_base = mode == "proc" ? read_proc_mem() : ProcMem{};

    std::vector<std::unique_ptr<Runner>> rs;
    for (const auto& n : split(formats)) {
        auto r = make_runner(n);
        if (!r) { std::cerr << "format not built: " << n << " (built: " << all_formats() << ")\n"; return 2; }
        r->setup(*sc, seed);
        rs.push_back(std::move(r));
    }
    if (rs.empty()) { std::cerr << "no formats\n"; return 2; }

    std::ofstream jsonl;
    if (mode == "bench") {
        if (out.empty()) { std::cerr << "--out is required in bench mode\n"; return 2; }
        jsonl.open(out);
        jsonl << "{\"meta\":{\"scale\":\"" << sc->name << "\",\"seed\":" << seed << ",\"cycles\":" << cycles
              << ",\"formats\":\"" << formats << "\",\"cases\":\"" << (demo_cases() ? "demo" : "abcd")
              << "\",\"protocol\":\"" << (reuse_mode() ? "reuse" : "per_message")
              << "\",\"xoffset_pool\":" << (std::getenv("SLG_XOFFSET_POOL") || reuse_mode() ? "true" : "false") << "}}\n";
    } else if (mode == "mem") {
        if (out.empty()) { std::cerr << "--out is required in mem mode\n"; return 2; }
        if (mem_live() < 0) { std::cerr << "--mode mem needs a -DSLG_MEMCOUNT=1 build\n"; return 2; }
        jsonl.open(out);
    } else if (mode == "proc") {
        if (out.empty()) { std::cerr << "--out is required in proc mode\n"; return 2; }
        if (proc_base.rss < 0 || proc_base.pte < 0) { std::cerr << "--mode proc needs /proc/self/status (Linux)\n"; return 2; }
        jsonl.open(out);
    } else if (mode != "verify") {
        std::cerr << "unknown mode " << mode << "\n"; return 2;
    }

    std::vector<std::size_t> order(rs.size());
    for (std::size_t i = 0; i < order.size(); ++i) order[i] = i;
    Rng shuffle_rng(mix_seed(seed, 0x0d3e));
    int failures = 0;
    std::vector<CycleStats> last(rs.size());
    std::vector<SampleWires> keep(rs.size());

    for (int c = 0; c < cycles; ++c) {
#if SLG_IR_COUNT
        ir_cycle() = c;
#endif
        for (std::size_t i = order.size(); i > 1; --i) std::swap(order[i - 1], order[std::size_t(shuffle_rng.below(int(i)))]);
        std::vector<CycleStats> st(rs.size());
        const bool keep_wires = mode == "verify" && c + 1 == cycles;
        for (std::size_t idx : order) {
            try {
                rs[idx]->run_cycle(c, st[idx], keep_wires ? &keep[idx] : nullptr);
            } catch (const std::exception& e) {
                std::cerr << "cycle " << c << " " << rs[idx]->name() << ": " << e.what() << "\n";
                return 1;
            }
        }
        // Correctness gate: identical game state and query results across formats.
        const uint64_t d0 = rs[0]->world_digest();
        for (std::size_t i = 0; i < rs.size(); ++i) {
            const uint64_t d = i == 0 ? d0 : rs[i]->world_digest();
            const bool ok_state = d == d0;
            const bool ok_report = st[i].report_sum == st[0].report_sum;
            const bool ok_direct = st[i].report_direct_sum == 0 || st[i].report_direct_sum == st[i].report_sum;
            if (!ok_state || !ok_report || !ok_direct) {
                ++failures;
                std::cerr << "MISMATCH cycle " << c << " " << rs[i]->name() << (ok_state ? "" : " state")
                          << (ok_report ? "" : " report") << (ok_direct ? "" : " report_direct") << "\n";
            }
            if (mode == "verify")
                std::printf("cycle %d %-12s state=%016llx reports=%llu%s\n", c, rs[i]->name().c_str(),
                            static_cast<unsigned long long>(d), static_cast<unsigned long long>(st[i].report_sum),
                            ok_state && ok_report && ok_direct ? "" : "  <-- MISMATCH");
        }
        if (mode == "bench") {
            for (std::size_t i = 0; i < rs.size(); ++i) {
                for (const auto& [k, v] : st[i].ns)
                    jsonl << "{\"cycle\":" << c << ",\"format\":\"" << rs[i]->name() << "\",\"metric\":\"" << k << "\",\"ns\":" << v << "}\n";
                for (const auto& [k, v] : st[i].bytes)
                    jsonl << "{\"cycle\":" << c << ",\"format\":\"" << rs[i]->name() << "\",\"metric\":\"" << k << "\",\"bytes\":" << v << "}\n";
            }
            std::cerr << "cycle " << c + 1 << "/" << cycles << "\r";
        }
        last = std::move(st);
    }
    if (mode == "mem") {
        for (std::size_t i = 0; i < rs.size(); ++i) {
            CycleStats m;
            rs[i]->memory(m);
            for (const auto& [k, v] : m.bytes)
                jsonl << "{\"format\":\"" << rs[i]->name() << "\",\"metric\":\"" << k << "\",\"bytes\":" << v << "}\n";
        }
    }
    if (mode == "proc") {
        const ProcMem end = read_proc_mem();
        auto row = [&](const char* k, long long v) {
            jsonl << "{\"format\":\"" << formats << "\",\"metric\":\"PROC." << k << "\",\"bytes\":" << v << "}\n";
        };
        row("base_rss", proc_base.rss); row("base_pte", proc_base.pte); row("base_vmas", proc_base.vmas);
        row("rss", end.rss); row("hwm", end.hwm); row("pte", end.pte); row("rss_anon", end.anon); row("vmas", end.vmas);
        // Diagnosis: SLG_PROC_SMAPS=<file> keeps a copy of /proc/self/smaps at the end.
        if (const char* f = std::getenv("SLG_PROC_SMAPS")) {
            std::ifstream in("/proc/self/smaps");
            std::ofstream o(f);
            o << in.rdbuf();
        }
    }
    if (mode == "verify") print_sizes(rs, last, keep, *sc);
    if (failures) { std::cerr << failures << " mismatches\n"; return 1; }
    std::cerr << (mode == "verify" ? "verify: all formats agree\n" : "\n" + mode + ": done\n");
    return 0;
}
