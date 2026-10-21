// libFuzzer target for the V2 validators (G5). Each input is one wire. The
// oracle, typed load, raw validation and verified receive must agree, and an
// accepted wire must survive read -> mutate -> save -> load byte-stably. Any
// disagreement aborts (fuzz/v2_fuzz_core.hpp), so libFuzzer keeps the input.
//
// Build: -DXOFFSET_BUILD_FUZZERS=ON with a clang that has -fsanitize=fuzzer.
// Seed corpus: test_v2_fuzz --write-corpus <dir>.
#include "v2_fuzz_core.hpp"

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    const std::string wire(reinterpret_cast<const char*>(data), size);
    // Mutation after acceptance is driven by the input, so a saved crash replays.
    std::uint64_t seed = 0xcbf29ce484222325ull;
    for (std::size_t i = 0; i < size; ++i) seed = (seed ^ data[i]) * 0x100000001b3ull;
    std::mt19937_64 rng(seed);
    (void)check_case(wire, rng, 0);
    return 0;
}
