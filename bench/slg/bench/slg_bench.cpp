// SLG prototype benchmark driver.
//
//   slg_bench --mode verify [--scale S|M|L] [--cycles 2] [--formats a,b,...]
//       Runs every format on the same synthetic world and checks that the game state
//       and the report queries are identical after every cycle. Prints wire sizes.
//   slg_bench --mode bench --out raw.jsonl [--scale M] [--cycles 41] [--seed N] [--formats ...]
//       Interleaves formats in a random order per cycle and writes one JSON line per
//       (cycle, format, metric). Analyse with tools/analyze.py; cycle 0 is warm-up.
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
    if (formats.empty()) formats = all_formats();
    if (cycles < 0) cycles = mode == "verify" ? 2 : 41;

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
              << ",\"formats\":\"" << formats << "\",\"protocol\":\"" << (reuse_mode() ? "reuse" : "per_message")
              << "\",\"xoffset_pool\":" << (std::getenv("SLG_XOFFSET_POOL") || reuse_mode() ? "true" : "false") << "}}\n";
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
    if (mode == "verify") print_sizes(rs, last, keep, *sc);
    if (failures) { std::cerr << failures << " mismatches\n"; return 1; }
    std::cerr << (mode == "verify" ? "verify: all formats agree\n" : "\nbench: done\n");
    return 0;
}
