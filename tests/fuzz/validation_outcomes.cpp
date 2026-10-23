// Validation outcome dump for differential runs across library versions:
// build it against two checkouts (only public entry points and
// v2_fuzz_core.hpp are used) and compare the outputs. Each case is one
// mutated wire; its outcome is the result of typed load, in-place reload, raw
// validation and verified receive, each "ok" or the exact error message (raw
// and receive only for wires whose framing the oracle accepts).
//
// usage: validation_outcomes <cases> <seed>             digest per 10000 cases
//        validation_outcomes <cases> <seed> <from> <to> one line per case in [from, to)
// The pool mixes the fuzz core's wires (dense and sparse, with holes) with
// wires of 30-46 KiB, so payloads on both sides of 32 KiB are validated.
#include "v2_fuzz_core.hpp"

#include <map>

namespace {
std::uint64_t fnv(std::string_view s, std::uint64_t h = 0xcbf29ce484222325ull) {
    for (unsigned char c : s) h = (h ^ c) * 0x100000001b3ull;
    return h;
}
std::string outcome(bool accepted, const std::string& error) { return accepted ? "ok" : error; }
} // namespace

int main(int argc, char** argv) {
    const std::size_t cases = argc > 1 ? std::stoull(argv[1]) : 100000;
    const std::uint64_t seed = argc > 2 ? std::stoull(argv[2]) : 20261022;
    const std::size_t from = argc > 4 ? std::stoull(argv[3]) : cases, to = argc > 4 ? std::stoull(argv[4]) : 0;
    std::mt19937_64 rng(seed);
    auto pool = seed_pool(rng, 64);
    for (int i = 0; i < 12; ++i) {
        auto w = sized_wire(rng, 15000 + std::size_t(rng() % 8000));
        check_golden(w, "large " + std::to_string(i));
        pool.push_back(std::move(w));
    }
    std::map<std::string, std::size_t> histogram;
    std::uint64_t block = 0xcbf29ce484222325ull, inputs = block;
    for (std::size_t c = 0; c < cases; ++c) {
        const auto& base = pool[rng() % pool.size()];
        const auto heads = oracle(base, true).heads;
        auto w = base;
        for (int k = 1 + int(rng() % 3); k > 0; --k) mutate_wire(w, rng, heads);
        std::string terr, lerr, rerr, verr;
        const bool typed = bool(typed_load(w, &terr));
        const bool reload = reload_accepts(w, &lerr);
        const auto o = oracle(w);
        std::string line = std::to_string(c) + " " + std::to_string(fnv(w)) + " T:" + outcome(typed, terr) +
                           " L:" + outcome(reload, lerr);
        if (o.ok || o.stage == Oracle::Stage::Payload) {
            const bool raw = raw_accepts(w, &rerr);
            const bool recv = receive_accepts(w, &verr);
            line += " R:" + outcome(raw, rerr) + " V:" + outcome(recv, verr);
        }
        ++histogram[outcome(typed, terr)];
        block = fnv(line, block);
        inputs = fnv(w, inputs);
        if (c >= from && c < to) std::cout << line << "\n";
        if ((c + 1) % 10000 == 0 || c + 1 == cases) {
            if (to == 0) std::cout << "cases " << c + 1 << " outcomes " << block << " inputs " << inputs << "\n";
            block = 0xcbf29ce484222325ull;
        }
    }
    if (to == 0)
        for (const auto& [message, count] : histogram) std::cout << "typed " << count << " " << message << "\n";
    return 0;
}
