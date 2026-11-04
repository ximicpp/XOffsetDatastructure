// HFT demo, phase 1: one process, a real Nasdaq TotalView-ITCH window.
//
//   hft_bench --feed F.itch --case prepare --out F.feed [--seconds S]
//   hft_bench --feed F.feed --case state --model xo|std|cista|pool [--books N]
//   hft_bench --feed F.feed --case hot --format raw|fixed|sbe|xo [--readers R]
//   hft_bench --feed F.feed --case join --format FMT [--at FRACTION]
//   hft_bench --case handshake
//
// Every case prints one JSON line with a digest; the digests of one case must
// be equal across models and formats. Built with -DHFT_IR, `--ir PHASE` makes
// callgrind count only that phase (see measure.hpp and run.sh).
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "hft/apply.hpp"
#include "hft/cases.hpp"
#include "hft/itch.hpp"
#include "hft/measure.hpp"
#include "hft/model.hpp"

using namespace hft;

int main(int argc, char** argv) {
    Args a;
    for (int i = 1; i + 1 < argc; i += 2) a.kv[argv[i]] = argv[i + 1];
    const std::string kase = a.get("--case", "state");
    if (kase == "handshake") return run_handshake(a);
    const std::string path = a.get("--feed", "");
    if (path.empty()) {
        std::fprintf(stderr, "--feed is required\n");
        return 2;
    }
    const bool cached = path.size() > 5 && path.compare(path.size() - 5, 5, ".feed") == 0;
    Feed feed = cached ? load_cached_feed(path) : load_feed(path, std::atof(a.get("--seconds", "0").c_str()));
    // --max-updates N: keep the seed and the first N window updates (whole packets).
    if (const auto cut = std::size_t(a.num("--max-updates", 0)); cut && feed.seed_end + cut < feed.updates.size()) {
        std::size_t p = 0;
        while (p + 1 < feed.packet_begin.size() && feed.packet_begin[p + 1] <= feed.seed_end + cut) ++p;
        feed.packet_begin.resize(p + 1);
        feed.updates.resize(feed.packet_begin.back());
    }
    if (kase == "prepare") {
        save_feed(feed, a.get("--out", path + ".feed"));
        std::printf("{\"case\":\"prepare\",\"updates\":%zu,\"seed\":%zu,\"packets\":%zu,\"instruments\":%zu,"
                    "\"itch_messages\":%llu,\"itch_bytes\":%llu,\"window_ns\":%llu}\n",
                    feed.updates.size(), feed.seed_end, feed.packets(), feed.instruments(),
                    (unsigned long long)feed.itch_messages, (unsigned long long)feed.itch_bytes,
                    (unsigned long long)(feed.window_end_ns - feed.window_start_ns));
        return 0;
    }
    if (kase == "state") return run_state(a, feed);
    if (kase == "hot") return run_hot(a, feed);
    if (kase == "join") return run_join(a, feed);
    std::fprintf(stderr, "unknown case %s\n", kase.c_str());
    return 2;
}
