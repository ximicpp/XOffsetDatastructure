// Spare watch (XBufferCore): capacity left inside live blocks counts with the
// free bytes for the automatic emission. A vector grown in place leaves no
// free block; shrunk to a quarter of its capacity or below (with at least
// 1 KiB of capacity) it registers its head, and the next automatic send goes
// compact when the free bytes alone keep the image and free plus watched
// spare meet the rule (XBufferCore::watched_spare_bytes).
//   - every shrink in place (pop_back, erase, map erase, resize, clear,
//     stable_erase_if, a shorter assign, POD copy assignment) after growth:
//     the compact wire through every entry, and the same contents received;
//   - clear and refill, and oscillation around half of the capacity: the
//     image, with the bytes free_runs sends;
//   - the watch follows move, swap and clone; reload and reset drop it;
//   - stale entries (a head moved by its outer vector, its element destroyed,
//     its block reused): no crash, and every wire passes the checked load
//     with the source's contents.
#include "xoffsetdatastructure.hpp"

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <random>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace xo = XOffsetDatastructure;

namespace sw {
struct alignas(8) Cell {
    std::uint64_t a{};
    std::uint64_t b{};
};
struct alignas(8) Rec {
    std::uint64_t id{};
    xo::XVector<std::uint64_t> data;
};
struct Root {
    std::uint64_t id{};
    xo::XVector<std::uint64_t> values;
    xo::XVector<Cell> cells;
    xo::XVector<Cell> few;
    xo::XVector<Rec> records;
    xo::XMap<std::uint64_t, std::uint64_t> map;
};
} // namespace sw
XOFFSET_RECORD_DERIVED(sw::Cell, (), a, b)
XOFFSET_RECORD_DERIVED(sw::Rec, (), id, data)
XOFFSET_RECORD_DERIVED(sw::Root, (), id, values, cells, few, records, map)
XOFFSET_ORDINARY_RECORDS(sw::Cell, sw::Rec, sw::Root)
XOFFSET_REGISTER_SCHEMA_NAME(sw::Root, "test.spare_watch.root")

namespace {
using Buffer = xo::TypedXBuffer<sw::Root>;

[[noreturn]] void fail(const std::string& message) {
    std::cerr << "test_spare_watch FAILED: " << message << "\n";
    std::exit(1);
}
void need(bool ok, const std::string& what) { if (!ok) fail(what); }

constexpr xo::XSaveOptions kRuns{.emission = xo::XWireEmission::free_runs};
constexpr xo::XSaveOptions kDense{.emission = xo::XWireEmission::dense};
constexpr xo::XSaveOptions kElide{.emission = xo::XWireEmission::zero_runs};

Buffer fresh() {
    auto b = Buffer::create(1u << 14, xo::XBuffer::maximum_capacity(1u << 22).with_growth(xo::XGrowPolicy::Automatic));
    b.plain_root().id = 42;
    return b;
}

xo::XWireHeader header_of(const std::string& wire) {
    need(wire.size() >= sizeof(xo::XWireHeader), "wire too small");
    xo::XWireHeader h{};
    std::memcpy(&h, wire.data(), sizeof(h));
    return h;
}

// What the rule picks from the image header alone.
bool free_bytes_prefer_compact(Buffer& b) {
    const auto h = header_of(b.save_verified(kDense));
    return xo::detail::wire_prefers_compact(h.used_bytes, h.free_bytes);
}

bool same(const sw::Root& x, const sw::Root& y) {
    if (x.id != y.id || x.values.size() != y.values.size() || x.cells.size() != y.cells.size() ||
        x.few.size() != y.few.size() || x.records.size() != y.records.size() || x.map.size() != y.map.size())
        return false;
    for (std::size_t i = 0; i < x.values.size(); ++i)
        if (x.values[i] != y.values[i]) return false;
    for (std::size_t i = 0; i < x.cells.size(); ++i)
        if (x.cells[i].a != y.cells[i].a || x.cells[i].b != y.cells[i].b) return false;
    for (std::size_t i = 0; i < x.few.size(); ++i)
        if (x.few[i].a != y.few[i].a || x.few[i].b != y.few[i].b) return false;
    for (std::size_t i = 0; i < x.records.size(); ++i) {
        const auto& r = x.records[i];
        const auto& s = y.records[i];
        if (r.id != s.id || r.data.size() != s.data.size()) return false;
        for (std::size_t j = 0; j < r.data.size(); ++j)
            if (r.data[j] != s.data[j]) return false;
    }
    auto i = x.map.begin();
    auto j = y.map.begin();
    for (; i != x.map.end(); ++i, ++j)
        if (i->first != j->first || i->second != j->second) return false;
    return true;
}

// The wire loads through the full check with b's contents.
void received_same(Buffer& b, const std::string& wire, const std::string& where) {
    auto loaded = Buffer::load(wire);
    need(same(loaded.plain_root(), b.plain_root()), where + ": the received contents differ");
}

// The default emission of `b` through every entry and output shape equals
// `expected`.
void check_entries(Buffer& b, const std::string& expected, const std::string& where) {
    need(b.save_verified() == expected, where + ": save_verified");
    need(b.XBuffer::save_verified<sw::Root>() == expected, where + ": XBuffer::save_verified");
    std::string out;
    b.save_verified_into(out);
    need(out == expected, where + ": save_verified_into (empty output)");
    b.save_verified_into(out);
    need(out == expected, where + ": save_verified_into (reused output)");
    std::string short_out = "previous";
    short_out.reserve(expected.size() - 1);
    short_out.assign(short_out.capacity(), char(0x5a));
    b.save_verified_into(short_out);
    need(short_out == expected, where + ": save_verified_into (output one byte short)");
    std::string exact(expected.size(), char(0x5a));
    exact.shrink_to_fit();
    b.save_verified_into(exact);
    need(exact == expected, where + ": save_verified_into (exactly sized output)");
}

// The free bytes alone keep the image, the watched spare turns it compact.
void expect_spare_compact(Buffer& b, const std::string& where) {
    need(!free_bytes_prefer_compact(b), where + ": the free bytes alone already prefer compact");
    need(b.spare_watched(), where + ": no head registered");
    const auto compact = b.save_compact();
    const auto h = header_of(compact);
    need(h.extent_count == 0 && h.free_dir == 0 && h.free_bytes == 0 && compact.size() == h.used_bytes,
         where + ": not a compact wire");
    need(compact.size() * 2 < b.save_verified(kDense).size(), where + ": the compact wire is not much smaller");
    check_entries(b, compact, where);
    received_same(b, compact, where);
    // The explicit forms still send the image.
    const auto dense = b.save_verified(kDense);
    need(header_of(dense).used_bytes == dense.size(), where + ": dense emission is not the image");
    need(Buffer::load(b.save_verified(kRuns)).save_verified(kDense) == dense, where + ": free-run wire");
    need(Buffer::load(b.save_verified(kElide)).save_verified(kDense) == dense, where + ": zero-run wire");
}

// The default sends the image, as free_runs does.
void expect_image(Buffer& b, const std::string& where) {
    need(!free_bytes_prefer_compact(b), where + ": fixture expected below the compact threshold");
    const auto runs = b.save_verified(kRuns);
    check_entries(b, runs, where);
    received_same(b, runs, where);
}

// values grown in place by push_back to 1024 elements (8 KiB, no free block).
void grow_values(Buffer& b, std::uint64_t n = 1024) {
    auto& v = b.plain_root().values;
    for (std::uint64_t i = 0; i < n; ++i) v.push_back(i * 3 + 1);
}

void check_shrink_paths() {
    {
        auto b = fresh();
        grow_values(b);
        need(b.plain_root().values.capacity() == 1024, "values did not grow to 1024");
        expect_image(b, "grown, before the shrink");
        auto& v = b.plain_root().values;
        while (v.size() > 257) v.pop_back();
        need(!b.spare_watched(), "pop_back: registered above a quarter");
        v.pop_back();
        expect_spare_compact(b, "pop_back");
    }
    {
        auto b = fresh();
        grow_values(b);
        auto& v = b.plain_root().values;
        while (v.size() > 200) v.erase(v.begin() + 7);
        expect_spare_compact(b, "erase");
    }
    {
        auto b = fresh();
        auto& r = b.plain_root().records;
        for (std::uint64_t i = 0; i < 128; ++i) r.emplace_back().id = i;
        need(r.capacity() * sizeof(sw::Rec) >= 1024, "records below 1 KiB of capacity");
        while (r.size() > r.capacity() / 4) r.erase(r.begin() + 1);
        expect_spare_compact(b, "erase of records with heads");
    }
    {
        auto b = fresh();
        auto& m = b.plain_root().map;
        for (std::uint64_t k = 0; k < 512; ++k) m.try_emplace(k, k * k);
        for (std::uint64_t k = 0; k < 512 && m.size() > 100; k += 1) m.erase(k * 5 % 512);
        expect_spare_compact(b, "map erase");
    }
    {
        auto b = fresh();
        grow_values(b);
        b.plain_root().values.resize(100);
        expect_spare_compact(b, "resize (scalar)");
    }
    {
        auto b = fresh();
        auto& c = b.plain_root().cells;
        for (std::uint64_t i = 0; i < 512; ++i) c.push_back(sw::Cell{i, ~i});
        c.resize(60);
        expect_spare_compact(b, "resize (record)");
    }
    {
        auto b = fresh();
        grow_values(b);
        b.plain_root().values.clear();
        expect_spare_compact(b, "clear");
    }
    {
        auto b = fresh();
        grow_values(b);
        const auto removed = b.plain_root().values.stable_erase_if([](std::uint64_t x) { return x % 5 != 0; });
        need(removed > 700, "stable_erase_if removed too few");
        expect_spare_compact(b, "stable_erase_if");
    }
    {
        auto b = fresh();
        grow_values(b);
        const std::uint64_t some[] = {9, 8, 7, 6, 5};
        b.plain_root().values.assign_range(std::span<const std::uint64_t>(some));
        need(b.plain_root().values.capacity() == 1024, "a shorter assign changed the capacity");
        expect_spare_compact(b, "shorter assign_range");
    }
    {
        auto b = fresh();
        grow_values(b);
        const std::uint64_t some[] = {1, 2, 3};
        b.plain_root().values.assign_bytes(some, 3);
        expect_spare_compact(b, "shorter assign_bytes");
    }
    {
        auto b = fresh();
        auto& root = b.plain_root();
        for (std::uint64_t i = 0; i < 512; ++i) root.cells.push_back(sw::Cell{i, i + 1});
        for (std::uint64_t i = 0; i < 20; ++i) root.few.push_back(sw::Cell{100 + i, 200 + i});
        root.cells = root.few;
        need(root.cells.capacity() == 512 && root.cells.size() == 20, "POD copy assignment changed the capacity");
        expect_spare_compact(b, "POD copy assignment");
    }
    {   // A vector inside a record (its head in the records block).
        auto b = fresh();
        auto& r = b.plain_root().records;
        for (std::uint64_t i = 0; i < 4; ++i) r.emplace_back().id = i;
        auto& d = r[2].data;
        for (std::uint64_t i = 0; i < 2048; ++i) d.push_back(i);
        d.resize(300);
        expect_spare_compact(b, "nested vector");
    }
    {   // Several containers, each crossing on its own.
        auto b = fresh();
        auto& root = b.plain_root();
        grow_values(b, 256);
        for (std::uint64_t i = 0; i < 128; ++i) root.cells.push_back(sw::Cell{i, i});
        root.values.resize(64);
        root.cells.resize(32);
        expect_spare_compact(b, "two containers");
    }
}

void check_kept_image() {
    {   // Clear and refill to the same size every frame.
        auto b = fresh();
        grow_values(b);
        const auto before = b.save_verified(kRuns);
        for (int frame = 0; frame < 3; ++frame) {
            auto& v = b.plain_root().values;
            v.clear();
            for (std::uint64_t i = 0; i < 1024; ++i) v.push_back(i * 3 + 1);
            expect_image(b, "clear and refill, frame " + std::to_string(frame));
            need(b.save_verified() == before, "clear and refill: bytes changed");
            need(!b.spare_watched(), "clear and refill: the entry survived the refill");
        }
    }
    {   // Oscillation around half of the capacity never crosses a quarter.
        auto b = fresh();
        grow_values(b);
        auto& v = b.plain_root().values;
        for (int round = 0; round < 20; ++round) {
            while (v.size() > 480) v.pop_back();
            while (v.size() < 560) v.push_back(7);
            need(!b.spare_watched(), "oscillation: registered");
        }
        while (v.size() > 513) v.pop_back();
        expect_image(b, "oscillation around half");
    }
    {   // Refilled above half after the crossing: dropped at the next send.
        auto b = fresh();
        grow_values(b);
        auto& v = b.plain_root().values;
        v.resize(10);
        need(b.spare_watched(), "refill: not registered");
        for (std::uint64_t i = 0; i < 600; ++i) v.push_back(i);
        expect_image(b, "refilled above half");
        need(!b.spare_watched(), "refill: the entry survived the send");
    }
    {   // Below 1 KiB of capacity nothing registers.
        auto b = fresh();
        grow_values(b, 100);
        need(b.plain_root().values.capacity() * 8 >= 1024, "small capacity: fixture");
        b.plain_root().values.resize(5);
        need(b.spare_watched(), "1 KiB of capacity: not registered");
    }
    {
        auto b = fresh();
        grow_values(b, 60);
        need(b.plain_root().values.capacity() * 8 < 1024, "small capacity: fixture");
        b.plain_root().values.resize(5);
        need(!b.spare_watched(), "small capacity: registered");
        expect_image(b, "small capacity");
    }
}

Buffer shrunk() {
    auto b = fresh();
    grow_values(b);
    b.plain_root().values.resize(64);
    need(b.spare_watched(), "fixture: not registered");
    return b;
}

void check_follows_content() {
    {
        auto source = shrunk();
        Buffer moved(std::move(source));
        expect_spare_compact(moved, "move construction");
    }
    {
        auto source = shrunk();
        auto target = fresh();
        grow_values(target, 10);
        target = std::move(source);
        expect_spare_compact(target, "move assignment");
    }
    {
        auto a = shrunk();
        auto c = fresh();
        grow_values(c, 10);
        a.swap(c);
        expect_spare_compact(c, "swap, the shrunk content");
        expect_image(a, "swap, the other content");
        a.swap(c);
        expect_spare_compact(a, "swap back");
        expect_image(c, "swap back, the other content");
    }
    {
        auto a = shrunk();
        auto copy = a.clone();
        expect_spare_compact(copy, "clone");
        expect_spare_compact(a, "clone source");
    }
    {
        auto a = shrunk();
        auto dst = fresh();
        a.clone_into(dst);
        expect_spare_compact(dst, "clone_into");
        auto same_size = a.clone();
        same_size.plain_root().id = 7;
        a.clone_into(same_size);
        expect_spare_compact(same_size, "clone_into an alike buffer");
    }
    {   // A reload replaces the content: its spare is no longer known.
        auto a = shrunk();
        const auto image = a.save_verified(kRuns);
        auto other = shrunk();
        other.reload_verified(image);
        expect_image(other, "reload of an image with spare");
        need(!other.spare_watched(), "reload: the watch survived");
        a.reload_verified(image);
        expect_image(a, "reload of its own image");
    }
    {
        auto a = shrunk();
        a.reset();
        a.plain_root().id = 5;
        grow_values(a, 16);
        expect_image(a, "reset");
        need(!a.spare_watched(), "reset: the watch survived");
    }
}

void check_stale_entries() {
    {   // The registered head moves with its outer vector.
        auto b = fresh();
        auto& r = b.plain_root().records;
        r.reserve(2);
        r.emplace_back().id = 1;
        r.emplace_back().id = 2;
        for (std::uint64_t i = 0; i < 1024; ++i) r[1].data.push_back(i);
        r[1].data.resize(100);
        need(b.spare_watched(), "outer move: not registered");
        for (std::uint64_t i = 3; i < 40; ++i) r.emplace_back().id = i;   // relocates the heads
        const auto wire = b.save_verified();
        received_same(b, wire, "head moved by its outer vector");
        need(wire == b.save_compact() || wire == b.save_verified(kRuns), "head moved: neither wire");
    }
    {   // The registered element is destroyed, then its block reused.
        auto b = fresh();
        auto& root = b.plain_root();
        for (std::uint64_t i = 0; i < 3; ++i) root.records.emplace_back().id = i;
        for (std::uint64_t i = 0; i < 1024; ++i) root.records[2].data.push_back(i);
        root.records[2].data.resize(10);
        need(b.spare_watched(), "destroyed: not registered");
        root.records.pop_back();
        auto wire = b.save_verified();
        received_same(b, wire, "registered element destroyed");
        for (std::uint64_t i = 0; i < 1500; ++i) root.values.push_back(~i);
        for (std::uint64_t i = 0; i < 64; ++i) root.cells.push_back(sw::Cell{i, i});
        wire = b.save_verified();
        received_same(b, wire, "registered block reused");
        need(wire == b.save_compact() || wire == b.save_verified(kRuns), "block reused: neither wire");
    }
    {   // More watched heads than slots, then random edits: every send is
        // one of the two wires and loads with the source's contents. Every
        // 40 steps the buffer reloads its compact wire (no free block, no
        // watch), so both wires and the watch's own decisions recur.
        std::mt19937_64 rng(0x5a17e5);
        auto b = fresh();
        {
            auto& root = b.plain_root();
            for (std::uint64_t i = 0; i < 12; ++i) root.records.emplace_back().id = i;
            for (std::uint64_t i = 0; i < 12; ++i) {
                for (std::uint64_t j = 0; j < 256; ++j) root.records[i].data.push_back(j);
                root.records[i].data.resize(16);
            }
        }
        need(b.spare_watched(), "slots: not registered");
        expect_spare_compact(b, "more heads than slots");
        int compacts = 0, images = 0, by_spare = 0;
        for (int step = 1; step <= 800; ++step) {
            if (step % 40 == 0) b.reload_verified(b.save_compact());
            auto& root = b.plain_root();
            switch (rng() % 8) {
            case 0: root.values.push_back(rng()); break;
            case 1: if (!root.values.empty()) root.values.pop_back(); break;
            case 2: root.values.resize(rng() % 600); break;
            case 3: { auto& d = root.records[rng() % root.records.size()].data;
                      const auto n = rng() % 700;
                      if (n > d.size()) { for (auto k = d.size(); k < n; ++k) d.push_back(k); } else d.resize(n);
                      break; }
            case 4: if (root.records.size() > 2) root.records.erase(root.records.begin() + std::ptrdiff_t(rng() % root.records.size())); break;
            case 5: root.records.emplace_back().id = rng(); break;
            case 6: root.map.try_emplace(rng() % 300, rng()); break;
            default: root.map.erase(rng() % 300); break;
            }
            if (step % 3 != 0) continue;
            const bool by_free = free_bytes_prefer_compact(b);
            const auto wire = b.save_verified();
            const bool compact = wire == b.save_compact();
            const auto where = "random edits, step " + std::to_string(step);
            need(compact || wire == b.save_verified(kRuns), where + ": neither wire");
            need(compact || !by_free, where + ": the free bytes prefer compact, the image went");
            compacts += compact;
            images += !compact;
            by_spare += compact && !by_free;
            received_same(b, wire, where);
            std::string into;
            b.save_verified_into(into);
            need(into == wire, where + ": save_verified_into differs");
        }
        need(compacts > 10 && images > 10 && by_spare > 5,
             "random edits: " + std::to_string(compacts) + " compact (" + std::to_string(by_spare) +
                 " by the watched spare), " + std::to_string(images) + " image");
    }
}
} // namespace

int main() {
    static_assert(xo::XBufferCore::spare_watch_min_bytes == 1024 && xo::XBufferCore::spare_watch_slots == 8);
    check_shrink_paths();
    check_kept_image();
    check_follows_content();
    check_stale_entries();
    std::cout << "{\"status\":\"passed\",\"spare_watch\":true}\n";
    return 0;
}
