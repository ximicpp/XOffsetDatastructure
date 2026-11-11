// Retired large blocks give their pages back (3.6 memory gate, 2026-11-11).
// A vector that outgrows its block in a long-lived buffer leaves a hole that
// later, larger requests do not reuse. On Linux a hole of at least
// XOFFSET_RETIRE_DISCARD_BYTES has its whole pages discarded (MADV_DONTNEED)
// instead of being cleared with memset: they must read back as zero (M3), stop
// being resident, be reusable, and save/load must see the same content.
// Elsewhere the hole is cleared as before; the zero checks run everywhere.
#include <cstdint>
#include <cstring>
#include <iostream>
#include <vector>
#include "../xoffsetdatastructure.hpp"
#if defined(__linux__)
#include <sys/mman.h>
#include <unistd.h>
#endif
using namespace XOffsetDatastructure;

struct DiscardRoot {
    std::uint64_t id{};
    XVector<std::uint64_t> values;
    XVector<XString> labels;
};
XOFFSET_RECORD_DERIVED(DiscardRoot, (), id, values, labels)
XOFFSET_REGISTER_SCHEMA_NAME(DiscardRoot, "test.DiscardRoot")

namespace {
int failures = 0;
void need(bool ok, const char* what) {
    if (!ok) {
        std::cout << "  FAIL: " << what << "\n";
        ++failures;
    }
}

struct Hole { std::uint32_t offset, bytes; };
std::vector<Hole> holes_at_least(const XBuffer& b, std::uint32_t min_bytes) {
    std::vector<Hole> out;
    b.for_each_free_block([&](auto h) { if (h.bytes >= min_bytes) out.push_back({h.offset, h.bytes}); });
    return out;
}

bool all_zero(const char* p, std::size_t n) {
    for (std::size_t i = 0; i < n; ++i) if (p[i] != 0) return false;
    return true;
}

#if defined(__linux__)
// Whole pages of [p, p + n) that are resident.
std::size_t resident_pages(const char* p, std::size_t n) {
    const auto page = static_cast<std::uintptr_t>(::sysconf(_SC_PAGESIZE));
    const auto a = reinterpret_cast<std::uintptr_t>(p);
    const auto lo = (a + page - 1) & ~(page - 1), hi = (a + n) & ~(page - 1);
    if (hi <= lo) return 0;
    std::vector<unsigned char> v((hi - lo) / page);
    if (::mincore(reinterpret_cast<void*>(lo), hi - lo, v.data()) != 0) return ~std::size_t(0);
    std::size_t r = 0;
    for (auto c : v) r += c & 1u;
    return r;
}
#endif

// Fills `values` with n nonzero words, puts a label after it (so the vector's
// block no longer ends the arena), then grows the vector past its block.
void build_hole(XBuffer& b, std::size_t n) {
    auto& r = b.root<DiscardRoot>();
    r.values.reserve(n);
    for (std::size_t i = 0; i < n; ++i) r.values.push_back(0x5a5a5a5a00000000ull | i);
    r.labels.emplace_back("after the vector");
    r.values.reserve(2 * n);
    r.values.push_back(7);
}

void test_large_hole(std::size_t n, bool expect_discard) {
    std::cout << "large hole, " << n * 8 << " bytes\n";
    XBuffer b = XBuffer::create<DiscardRoot>(1 << 20);
    build_hole(b, n);
    const auto hs = holes_at_least(b, std::uint32_t(n * 8));
    need(hs.size() == 1, "the old vector block is one hole");
    if (hs.empty()) return;
    const char* p = b.data() + hs[0].offset;
#if defined(__linux__)
    const std::size_t res = resident_pages(p, hs[0].bytes);
    if (expect_discard && XOFFSET_RETIRE_DISCARD_BYTES != 0)
        need(res == 0, "the hole's whole pages are no longer resident");
    else
        need(res != 0 && res != ~std::size_t(0), "a small hole keeps its pages (cleared in place)");
#else
    (void)expect_discard;
#endif
    // Read after mincore: a read maps the shared zero page, which mincore counts.
    need(all_zero(p, hs[0].bytes), "the hole reads as zero (M3)");
    // The hole is reusable, and a reused block starts from zero.
    auto& r = b.root<DiscardRoot>();
    const auto before = b.free_block_bytes();
    r.labels.emplace_back(std::string(1000, 'x'));
    need(b.free_block_bytes() < before, "a later allocation reuses the hole");
    need(r.labels.back() == std::string(1000, 'x'), "the reused block holds its new content");
    // The content survives save and load unchanged.
    const auto wire = b.save_verified<DiscardRoot>();
    XBuffer c = XBuffer::load_verified<DiscardRoot>(wire);
    const auto& rc = c.root<DiscardRoot>();
    need(rc.values.size() == n + 1, "loaded size");
    bool same = rc.values.back() == 7;
    for (std::size_t i = 0; i < n && same; ++i) same = rc.values[i] == (0x5a5a5a5a00000000ull | i);
    need(same, "loaded values");
    need(rc.labels.size() == 2 && rc.labels[0] == "after the vector", "loaded labels");
    need(b.save_verified<DiscardRoot>() == wire, "saving again gives the same bytes");
}

// Many rounds of grow / retire / reuse in one buffer (a long string assigned
// sizes from 1 to 60 KiB, a vector growing past its block every few rounds):
// the content stays right and every hole stays zero.
void test_churn() {
    std::cout << "churn\n";
    XBuffer b = XBuffer::create<DiscardRoot>(1 << 20);
    auto& r = b.root<DiscardRoot>();
    r.labels.emplace_back("");
    r.labels.emplace_back("tail");
    for (int round = 0; round < 200; ++round) {
        const std::size_t len = 1024 + std::size_t((round * 7919) % 60) * 1024;
        const char c = char('a' + round % 26);
        r.labels[0] = std::string(len, c);
        if (round % 5 == 0) {
            const std::size_t n = r.values.size() + 600;
            while (r.values.size() < n) r.values.push_back(r.values.size() * 3 + 1);
            r.labels.emplace_back(std::to_string(round));
        }
        need(r.labels[0] == std::string(len, c), "the long string after a round");
        bool ok = true;
        for (std::size_t i = 0; i < r.values.size() && ok; ++i) ok = r.values[i] == i * 3 + 1;
        need(ok, "values after a round");
        for (auto h : holes_at_least(b, 8)) need(all_zero(b.data() + h.offset, h.bytes), "every hole is zero");
        if (failures) return;
    }
    const auto wire = b.save_verified<DiscardRoot>();
    XBuffer c = XBuffer::load_verified<DiscardRoot>(wire);
    need(c.root<DiscardRoot>().values.size() == r.values.size(), "churned buffer loads");
}
} // namespace

int main() {
    test_large_hole(16 * 1024, true);   // 128 KiB block: discarded
    test_large_hole(1024, false);       // 8 KiB block: cleared in place
    test_churn();
    std::cout << (failures ? "FAILED" : "PASSED") << "\n";
    return failures ? 1 : 0;
}
