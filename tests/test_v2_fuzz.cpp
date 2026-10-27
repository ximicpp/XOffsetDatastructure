// V2 validator differential + mutation fuzz (seeded, runs in CTest). The
// oracle, acceptors and mutators live in fuzz/v2_fuzz_core.hpp, shared with the
// libFuzzer target fuzz_v2_wire.
// Golden check: for every generated wire, load -> save is byte-identical.
//
// usage: test_v2_fuzz [cases] [seed]
//        test_v2_fuzz --replay <seed> <case>   (rerun one case of a run alone)
//        test_v2_fuzz --write-corpus <dir>     (seed corpus for fuzz_v2_wire)
//
// The golden pool and the page boundary wires depend on the seed only. Each
// case draws from its own stream case_rng(seed, case), so a failing case
// replays without the cases before it. Cases are numbered 0..cases-1; the
// page boundary cases are b0..b1504 whatever `cases` is. A replay starts with
// a fresh reload_verified target buffer, while the full run reuses one buffer
// for every case (see v2_fuzz_core.hpp); a failure that depends on that
// buffer's history reproduces only in the full run.
// XOFFSET_FUZZ_DUMP_DIR=<dir>: a failing case writes its input wire and report
// there before aborting.
#include "fuzz/v2_fuzz_core.hpp"

#include <charconv>

static_assert(kInlineBitmapExtent == xo::detail::WireGranuleIndex::inline_extent);
// check_clone() in exercise() is compiled only when the library has clone().
static_assert(requires(Buffer& b) { b.clone(); b.clone_into(b); });
// check_composed() likewise only when it has save_compact_into with assignments.
#if !defined(XOFFSET_SAVE_COMPOSED_API)
#error "test_v2_fuzz needs save_compact_into with assignments (check_composed)"
#endif

namespace {
constexpr int kBoundaryMutants = 301; // per page boundary wire, the first unmutated
constexpr std::uint64_t kBoundaryStream = std::uint64_t(1) << 63;

struct Inputs {
    std::vector<std::string> pool, boundary;
    std::vector<std::vector<std::size_t>> pool_heads, boundary_heads;
};

Inputs make_inputs(std::uint64_t seed) {
    g_fail.replay = "test_v2_fuzz 0 " + std::to_string(seed);
    std::mt19937_64 rng(seed);
    Inputs in;
    in.pool = seed_pool(rng, 64);
    in.boundary = page_boundary_wires(rng);
    for (const auto& w : in.pool) in.pool_heads.push_back(oracle(w, true).heads);
    for (const auto& w : in.boundary) in.boundary_heads.push_back(oracle(w, true).heads);
    return in;
}

// The core builds its dense and zero-run forms by byte operations; they must
// be the library's own dense and zero_runs emissions of the loaded buffer.
// Returns how many pool wires the library sends with skipped zero runs.
std::size_t check_library_emissions(const std::vector<std::string>& pool) {
    std::size_t elided = 0;
    for (std::size_t i = 0; i < pool.size(); ++i) {
        auto b = typed_load(pool[i]);
        if (!b) fail("pool wire " + std::to_string(i) + " rejected");
        const auto dense = b->save_verified(xo::XSaveOptions{.emission = xo::XWireEmission::dense});
        if (dense != dense_of(b->save_verified())) fail("pool wire " + std::to_string(i) + ": dense emission differs");
        const auto zero = b->save_verified(xo::XSaveOptions{.emission = xo::XWireEmission::zero_runs});
        if (zero != elide_zero_runs(dense)) fail("pool wire " + std::to_string(i) + ": zero_runs emission differs");
        elided += zero != dense;
    }
    return elided;
}

struct Counts { std::size_t accepted = 0, header_rejects = 0, payload_rejects = 0; };

// Every acceptor's outcome ("ok" or the exact error) for one input.
std::string outcomes(const std::string& w) {
    std::string terr, lerr, rerr, verr;
    std::string line = std::string("T:") + (typed_load(w, &terr) ? "ok" : terr) +
                       " L:" + (reload_accepts(w, &lerr) ? "ok" : lerr);
    const auto o = oracle(w);
    if (o.ok || o.stage == Oracle::Stage::Payload) {
        line += std::string(" R:") + (raw_accepts(w, &rerr) ? "ok" : rerr);
        line += std::string(" V:") + (receive_accepts(w, &verr) ? "ok" : verr);
    }
    return line;
}

// The cursor path is a shortcut of the bitmaps: with it off, every outcome
// and every first error must be the same.
std::size_t cursor_compared = 0;
void compare_cursor_off(const std::string& w, const std::string& id) {
    const auto on = outcomes(w);
    xo::detail::wire_validation_cursor = false;
    const auto off = outcomes(w);
    xo::detail::wire_validation_cursor = true;
    if (on != off) fail("case " + id + ": cursor on {" + on + "} off {" + off + "}");
    ++cursor_compared;
}

// Case `id` (a number, or b<k> for page boundary case k).
Outcome run_case(const Inputs& in, std::uint64_t seed, const std::string& id) {
    const bool boundary = id.front() == 'b';
    std::uint64_t k = 0;
    const char* first = id.data() + (boundary ? 1 : 0);
    const char* last = id.data() + id.size();
    if (first == last || std::from_chars(first, last, k).ptr != last) fail("bad case id '" + id + "'");
    std::string w;
    auto rng = case_rng(seed, boundary ? kBoundaryStream | k : k);
    if (boundary) {
        if (k >= in.boundary.size() * kBoundaryMutants) fail("no page boundary case " + id);
        const auto base = std::size_t(k / kBoundaryMutants);
        const int m = int(k % kBoundaryMutants);
        w = in.boundary[base];
        for (int n = m == 0 ? 0 : 1 + int(rng() % 3); n > 0; --n) mutate_wire(w, rng, in.boundary_heads[base]);
    } else {
        const auto base = std::size_t(rng() % in.pool.size());
        w = in.pool[base];
        for (int n = 1 + int(rng() % 3); n > 0; --n) mutate_wire(w, rng, in.pool_heads[base]);
    }
    g_fail.wire = &w;
    g_fail.tag = "v2fuzz-s" + std::to_string(seed) + "-c" + id;
    g_fail.replay = "test_v2_fuzz --replay " + std::to_string(seed) + " " + id;
    compare_cursor_off(w, id);
    const auto outcome = check_case(w, rng, id);
    g_fail.wire = nullptr;
    return outcome;
}

void count(Counts& c, Outcome o) {
    switch (o) {
    case Outcome::Accepted: ++c.accepted; break;
    case Outcome::HeaderReject: ++c.header_rejects; break;
    case Outcome::PayloadReject: ++c.payload_rejects; break;
    }
}
} // namespace

int main(int argc, char** argv) {
    if (argc == 3 && std::string_view(argv[1]) == "--write-corpus") {
        std::cout << "test_v2_fuzz: wrote " << write_v2_seed_corpus(argv[2]) << " seed wires to " << argv[2] << "\n";
        return 0;
    }
    if (argc == 4 && std::string_view(argv[1]) == "--replay") {
        const std::uint64_t seed = std::stoull(argv[2]);
        const auto in = make_inputs(seed);
        const auto o = run_case(in, seed, argv[3]);
        std::cout << "test_v2_fuzz: replay seed=" << seed << " case=" << argv[3] << " outcome="
                  << (o == Outcome::Accepted ? "accepted" : o == Outcome::HeaderReject ? "header_reject" : "payload_reject")
                  << " inconsistencies=0\n";
        return 0;
    }
    const std::size_t cases = argc > 1 ? std::stoul(argv[1]) : 4000;
    const std::uint64_t seed = argc > 2 ? std::stoull(argv[2]) : 20261021;
    const auto in = make_inputs(seed);
    std::size_t sparse = 0;
    for (const auto& w : in.pool) {
        xo::XWireHeaderV2 h; std::memcpy(&h, w.data(), kHeader);
        sparse += h.extent_count != 0;
    }
    // V1 input is rejected explicitly.
    {
        auto v1 = in.pool.front();
        std::memcpy(v1.data(), "XOFFV1\0\0", 8);
        std::string error;
        if (typed_load(v1, &error) || error.find("wire format 1 is not supported") == std::string::npos)
            fail("V1 magic not rejected as format version 1: " + error);
    }
    Counts c;
    for (std::size_t i = 0; i < cases; ++i) count(c, run_case(in, seed, std::to_string(i)));
    // Both sides of the inline bitmap limit, each also mutated.
    const std::size_t boundary_cases = in.boundary.size() * kBoundaryMutants;
    for (std::size_t k = 0; k < boundary_cases; ++k) count(c, run_case(in, seed, "b" + std::to_string(k)));
    if (sparse == 0) fail("no sparse wire in the pool");
    if (check_library_emissions(in.pool) == 0) fail("no pool wire is sent with zero runs skipped");
    if (inflated_comparisons == 0) fail("no sparse wire was compared with its dense payload");
    if (composed_comparisons == 0) fail("no composed wire was compared with assign + save_compact");
    std::cout << "test_v2_fuzz: seed=" << seed << " golden=" << in.pool.size() << " sparse=" << sparse << " cases=" << cases
              << " boundary_cases=" << boundary_cases << " accepted=" << c.accepted
              << " header_rejects=" << c.header_rejects << " payload_rejects=" << c.payload_rejects
              << " sparse_vs_dense=" << inflated_comparisons << " composed=" << composed_comparisons
              << " cursor_off_compared=" << cursor_compared
              << " inconsistencies=0\n";
    return 0;
}
