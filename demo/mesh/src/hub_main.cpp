// Device mesh demo: the hub (xo_hub). OWNER: network thread
// (demo/mesh/INTERFACES.md section 6; the loop lives in src/hub/).
//
//   xo_hub --run LABEL --out DIR [--bind 0.0.0.0] [--port 47801] [--http-port 47802]
//          [--expect N] [--wait-s 120] [--plan pairs,chain,evil]
//          [--types all|Document,Order,...] [--scales small,medium] [--save-modes verified,compact]
//          [--seed 1] [--max-inflight 8] [--max-per-device 2] [--job-timeout-s 60]
//          [--advertise HOST] [--linger] [--no-archive]
//
// Exit 0 only when every planned job passed; 1 otherwise; 2 on a usage error.
#include <cstdio>
#include <cstdlib>
#include <string>
#include <string_view>
#include <vector>
#include "hub/hub.hpp"

namespace {

using namespace xmesh;

void usage() {
    std::fprintf(stderr,
                 "usage: xo_hub --run LABEL --out DIR [--bind 0.0.0.0] [--port 47801] [--http-port 47802]\n"
                 "              [--expect N] [--wait-s 120] [--plan pairs,chain,evil]\n"
                 "              [--types all|Document,Order,...] [--scales small,medium]\n"
                 "              [--save-modes verified,compact] [--seed 1] [--max-inflight 8]\n"
                 "              [--max-per-device 2] [--job-timeout-s 60] [--advertise HOST]\n"
                 "              [--linger] [--no-archive]\n"
                 "  --http-port 0 turns the dashboard off; --advertise sets the host in the join link.\n");
}

std::vector<std::string_view> split(std::string_view s) {
    std::vector<std::string_view> out;
    while (!s.empty()) {
        const std::size_t c = s.find(',');
        if (c != 0) out.push_back(s.substr(0, c));
        if (c == std::string_view::npos) break;
        s.remove_prefix(c + 1);
    }
    return out;
}

bool parse_int(const char* s, long long lo, long long hi, long long& out) {
    if (s == nullptr || *s == '\0') return false;
    char* end = nullptr;
    const long long v = std::strtoll(s, &end, 10);
    if (*end != '\0' || v < lo || v > hi) return false;
    out = v;
    return true;
}

template <class E, class Parse>
bool parse_list(const char* text, std::vector<E>& out, Parse parse) {
    out.clear();
    for (std::string_view name : split(text)) {
        const std::optional<E> e = parse(name);
        if (!e) {
            std::fprintf(stderr, "error: unknown name '%.*s'\n", int(name.size()), name.data());
            return false;
        }
        out.push_back(*e);
    }
    return !out.empty();
}

int parse_args(int argc, char** argv, hub::Options& o) {
    bool have_out = false;
    for (int i = 1; i < argc; ++i) {
        const std::string_view a = argv[i];
        const char* v = i + 1 < argc ? argv[i + 1] : nullptr;
        long long n = 0;
        auto need = [&]() {
            if (v == nullptr) return false;
            ++i;
            return true;
        };
        if (a == "--run") {
            if (!need()) return 2;
            o.run = v;
        } else if (a == "--out") {
            if (!need()) return 2;
            o.out_dir = v;
            have_out = true;
        } else if (a == "--bind") {
            if (!need()) return 2;
            o.bind = v;
        } else if (a == "--port") {
            if (!need() || !parse_int(v, 1, 65535, n)) return 2;
            o.port = std::uint16_t(n);
        } else if (a == "--http-port") {
            if (!need() || !parse_int(v, 0, 65535, n)) return 2;
            o.http_port = std::uint16_t(n);
        } else if (a == "--expect") {
            if (!need() || !parse_int(v, 0, 1000, n)) return 2;
            o.expect = int(n);
        } else if (a == "--wait-s") {
            if (!need() || !parse_int(v, 0, 7 * 24 * 3600, n)) return 2;
            o.wait_s = int(n);
        } else if (a == "--plan") {
            if (!need()) return 2;
            o.plan_pairs = o.plan_chain = o.plan_evil = false;
            for (std::string_view k : split(v)) {
                if (k == "pairs") o.plan_pairs = true;
                else if (k == "chain") o.plan_chain = true;
                else if (k == "evil") o.plan_evil = true;
                else return 2;
            }
        } else if (a == "--types") {
            if (!need()) return 2;
            if (std::string_view(v) == "all") o.types.assign(kAllTypes.begin(), kAllTypes.end());
            else if (!parse_list<TypeId>(v, o.types, parse_type)) return 2;
        } else if (a == "--scales") {
            if (!need() || !parse_list<Scale>(v, o.scales, parse_scale)) return 2;
        } else if (a == "--save-modes") {
            if (!need() || !parse_list<SaveMode>(v, o.save_modes, parse_save_mode)) return 2;
        } else if (a == "--seed") {
            if (!need() || !parse_int(v, 0, (1ll << 62), n)) return 2;
            o.seed = std::uint64_t(n);
        } else if (a == "--max-inflight") {
            if (!need() || !parse_int(v, 1, 4096, n)) return 2;
            o.max_inflight = int(n);
        } else if (a == "--max-per-device") {
            if (!need() || !parse_int(v, 1, 4096, n)) return 2;
            o.max_per_device = int(n);
        } else if (a == "--job-timeout-s") {
            if (!need() || !parse_int(v, 1, 24 * 3600, n)) return 2;
            o.job_timeout_s = int(n);
        } else if (a == "--advertise") {
            if (!need()) return 2;
            o.advertise = v;
        } else if (a == "--linger") {
            o.linger = true;
        } else if (a == "--no-archive") {
            o.archive = false;
        } else if (a == "-h" || a == "--help") {
            usage();
            std::exit(0);
        } else {
            std::fprintf(stderr, "error: unknown argument %s\n", argv[i]);
            return 2;
        }
    }
    if (!have_out) {
        std::fprintf(stderr, "error: --out DIR is required\n");
        return 2;
    }
    return 0;
}

} // namespace

int main(int argc, char** argv) {
    hub::Options opt;
    if (parse_args(argc, argv, opt) != 0) {
        usage();
        return 2;
    }
    return hub::run(opt);
}
