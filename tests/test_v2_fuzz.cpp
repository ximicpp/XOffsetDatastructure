// V2 validator differential + mutation fuzz (seeded, runs in CTest). The
// oracle, acceptors and mutators live in fuzz/v2_fuzz_core.hpp, shared with the
// libFuzzer target fuzz_v2_wire.
// Golden check: for every generated wire, load -> save is byte-identical.
//
// usage: test_v2_fuzz [cases] [seed]
//        test_v2_fuzz --write-corpus <dir>   (seed corpus for fuzz_v2_wire)
#include "fuzz/v2_fuzz_core.hpp"

#include <filesystem>

namespace {
// Wires around the validator's inline bitmap limit (payloads up to
// inline_extent keep both granule bitmaps inside the context, larger ones take
// host words). [0, 3): valid wires with used = page - 8, page, page + 8.
// Library wires keep used a multiple of 8, so [3, 5) patch the page-sized wire
// to used = page - 1 (last payload byte dropped) and page + 1 (one zero byte
// appended); they are mutated inputs.
std::vector<std::string> page_boundary_wires(std::mt19937_64& rng) {
    constexpr std::uint64_t page = xo::detail::WireGranuleIndex::inline_extent;
    const auto content = rng;
    rng.discard(1);
    std::vector<std::string> out;
    // Two bytes per value; the rest of the root is well under 1200 bytes.
    for (std::size_t vals = page / 2 - 600; out.size() < 3; ++vals) {
        auto w = sized_wire(content, vals);
        xo::XWireHeaderV2 h; std::memcpy(&h, w.data(), kHeader);
        const auto target = page - 8 + 8 * out.size();
        if (h.used_bytes > page + 8) fail("no library wire with used = " + std::to_string(target));
        if (h.used_bytes != target) continue;
        if (h.extent_count != 0) fail("page boundary wire is sparse");
        check_golden(w, "page boundary used=" + std::to_string(target));
        out.push_back(std::move(w));
    }
    const auto& at_page = out[1];
    xo::XWireHeaderV2 h; std::memcpy(&h, at_page.data(), kHeader);
    auto shorter = at_page;
    shorter.erase(kHeader + page - 1, 1);
    h.used_bytes = page - 1;
    std::memcpy(shorter.data(), &h, kHeader);
    fix_header_sizes(shorter);
    auto longer = at_page;
    longer.insert(kHeader + page, 1, '\0');
    h.used_bytes = page + 1;
    std::memcpy(longer.data(), &h, kHeader);
    fix_header_sizes(longer);
    out.push_back(std::move(shorter));
    out.push_back(std::move(longer));
    return out;
}
} // namespace

int main(int argc, char** argv) {
    if (argc == 3 && std::string_view(argv[1]) == "--write-corpus") {
        // Seed corpus for fuzz_v2_wire: the golden pool, one file per wire.
        std::mt19937_64 rng(20261021);
        std::filesystem::create_directories(argv[2]);
        auto pool = seed_pool(rng, 64);
        for (auto& w : page_boundary_wires(rng)) pool.push_back(std::move(w));
        for (std::size_t i = 0; i < pool.size(); ++i) {
            const auto path = std::string(argv[2]) + "/seed-" + std::to_string(i) + ".wire";
            std::FILE* f = std::fopen(path.c_str(), "wb");
            if (!f || std::fwrite(pool[i].data(), 1, pool[i].size(), f) != pool[i].size() || std::fclose(f) != 0)
                fail("cannot write " + path);
        }
        std::cout << "test_v2_fuzz: wrote " << pool.size() << " seed wires to " << argv[2] << "\n";
        return 0;
    }
    const std::size_t cases = argc > 1 ? std::stoul(argv[1]) : 4000;
    const std::uint64_t seed = argc > 2 ? std::stoull(argv[2]) : 20261021;
    std::mt19937_64 rng(seed);
    std::size_t accepted = 0, header_rejects = 0, payload_rejects = 0, sparse = 0;
    const auto pool = seed_pool(rng, 64);
    for (const auto& w : pool) {
        xo::XWireHeaderV2 h; std::memcpy(&h, w.data(), kHeader);
        sparse += h.extent_count != 0;
    }
    // V1 input is rejected explicitly.
    {
        auto v1 = pool.front();
        std::memcpy(v1.data(), xo::XWIRE_MAGIC_V1, 8);
        std::string error;
        if (typed_load(v1, &error) || error.find("version 1") == std::string::npos)
            fail("V1 magic not rejected as format version 1: " + error);
    }
    for (std::size_t c = 0; c < cases; ++c) {
        const auto& base = pool[rng() % pool.size()];
        const auto heads = oracle(base, true).heads;
        auto w = base;
        const int n = 1 + int(rng() % 3);
        for (int k = 0; k < n; ++k) mutate_wire(w, rng, heads);
        switch (check_case(w, rng, c)) {
        case Outcome::Accepted: ++accepted; break;
        case Outcome::HeaderReject: ++header_rejects; break;
        case Outcome::PayloadReject: ++payload_rejects; break;
        }
    }
    // Both sides of the inline bitmap limit, each also mutated.
    std::size_t boundary_cases = 0;
    for (const auto& base : page_boundary_wires(rng)) {
        const auto heads = oracle(base, true).heads;
        for (int m = 0; m <= 300; ++m) {
            auto w = base;
            for (int k = m == 0 ? 0 : 1 + int(rng() % 3); k > 0; --k) mutate_wire(w, rng, heads);
            switch (check_case(w, rng, cases + boundary_cases++)) {
            case Outcome::Accepted: ++accepted; break;
            case Outcome::HeaderReject: ++header_rejects; break;
            case Outcome::PayloadReject: ++payload_rejects; break;
            }
        }
    }
    if (sparse == 0) fail("no sparse wire in the pool");
    std::cout << "test_v2_fuzz: golden=" << pool.size() << " sparse=" << sparse << " cases=" << cases << " boundary_cases=" << boundary_cases << " accepted=" << accepted
              << " header_rejects=" << header_rejects << " payload_rejects=" << payload_rejects
              << " inconsistencies=0\n";
    return 0;
}
