// H1 section 5.4: string lengths in the SLG trace. libstdc++ keeps strings of up
// to 15 bytes inline, libc++ up to 22, so a baseline that decodes into std::string
// allocates for 16-22 byte strings under GCC only. This counts, for every message
// the std-model formats encode (cereal path, per-message protocol), the lengths of
// all std::string fields. The trace is deterministic, so the counts do not depend
// on the compiler.
//
//   slg_strlen [--scale M] [--cycles 41] [--seed 20261021]
#include <cstdio>
#include <cstdlib>
#include <map>
#include <string>
#include "runner.hpp"
#include "std_runner.hpp"
#include "cereal_format.hpp"

using namespace slg;

namespace {

std::map<std::string, std::map<std::size_t, unsigned long long>> g_hist;  // message type -> length -> count

void count(const char* t, const std::string& s) { ++g_hist[t][s.size()]; }
void walk(const char* t, const stdm::Hero& h) { count(t, h.name); }
void walk(const char* t, const stdm::Army& a) { for (const auto& h : a.heroes) walk(t, h); }
void walk(const char* t, const stdm::Report& r) { count(t, r.summary); }
void walk(const char* t, const stdm::BattleMsg& m) { walk(t, m.attacker); walk(t, m.defender); walk(t, m.report); }
void walk(const char* t, const stdm::PlayerState& p) {
    count(t, p.name);
    for (const auto& a : p.armies) walk(t, a);
    for (const auto& h : p.heroes) walk(t, h);
}
void walk(const char*, const stdm::ShardState&) {}

template <class T> constexpr const char* type_name() {
    if constexpr (std::is_same_v<T, stdm::BattleMsg>) return "BattleMsg";
    else if constexpr (std::is_same_v<T, stdm::PlayerState>) return "PlayerState";
    else if constexpr (std::is_same_v<T, stdm::ShardState>) return "ShardState";
    else if constexpr (std::is_same_v<T, stdm::Report>) return "Report";
    else return "other";
}

struct CountingCereal : CerealFormat {
    static constexpr const char* name = "cereal_strlen";
    template <class T> static Wire encode(const T& v) {
        walk(type_name<T>(), v);
        return CerealFormat::encode(v);
    }
    static Wire encode_battle_request(int64_t id, int64_t seed, const stdm::Army& a, const stdm::Army& d) {
        walk("BattleRequest", a);
        walk("BattleRequest", d);
        return CerealFormat::encode_battle_request(id, seed, a, d);
    }
};

} // namespace

int main(int argc, char** argv) {
    std::string scale = "M";
    int cycles = 41;
    uint64_t seed = 20261021;
    for (int i = 1; i + 1 < argc; i += 2) {
        std::string a = argv[i];
        if (a == "--scale") scale = argv[i + 1];
        else if (a == "--cycles") cycles = std::atoi(argv[i + 1]);
        else if (a == "--seed") seed = std::strtoull(argv[i + 1], nullptr, 10);
        else { std::fprintf(stderr, "unknown argument %s\n", a.c_str()); return 2; }
    }
    unsetenv("SLG_REUSE");  // the reuse protocol bypasses Format::encode
    const Scale* sc = find_scale(scale);
    if (!sc) { std::fprintf(stderr, "unknown scale\n"); return 2; }
    StdRunner<CountingCereal> r;
    r.setup(*sc, seed);
    for (int c = 0; c < cycles; ++c) {
        CycleStats st;
        r.run_cycle(c, st, nullptr);
    }
    std::printf("# string lengths of std::string fields encoded per message type, scale %s, %d cycles, seed %llu\n",
                sc->name, cycles, static_cast<unsigned long long>(seed));
    std::printf("type\tlen<=15\tlen16-22\tlen>=23\ttotal\n");
    for (const auto& [t, h] : g_hist) {
        unsigned long long a = 0, b = 0, c = 0;
        for (const auto& [len, n] : h) (len <= 15 ? a : len <= 22 ? b : c) += n;
        std::printf("%s\t%llu\t%llu\t%llu\t%llu\n", t.c_str(), a, b, c, a + b + c);
    }
    std::printf("\ntype\tlen\tcount\n");
    for (const auto& [t, h] : g_hist)
        for (const auto& [len, n] : h) std::printf("%s\t%zu\t%llu\n", t.c_str(), len, n);
    return 0;
}
