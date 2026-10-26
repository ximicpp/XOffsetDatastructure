// XBuffer::clone() and clone_into(): same-process whole-buffer copies. The
// copy must be independent of its source, carry the exact free table (so both
// allocate identically afterwards), save byte-identically and round-trip;
// clone_into must reuse the destination like reload_verified, refuse a self
// copy without touching the buffer, and leave the destination an empty arena
// on failure. (Clones of fuzzed accepted wires: test_v2_fuzz, exercise().)
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
#include "../xoffsetdatastructure.hpp"
using namespace XOffsetDatastructure;

struct CloneRoot {
    std::uint64_t id{};
    XString name;
    XVector<std::uint32_t> values;
    XVector<XString> tags;
    XMap<std::uint32_t, XString> attrs;
};
XOFFSET_REGISTER_SCHEMA_NAME(CloneRoot, "test.CloneRoot")

struct CloneOtherRoot { std::uint32_t x{}; };
XOFFSET_REGISTER_SCHEMA_NAME(CloneOtherRoot, "test.CloneOtherRoot")

namespace {
void need(bool ok, const char* what) { if (!ok) throw std::runtime_error(what); }
constexpr std::size_t kMax = 1u << 22;
auto cap(XGrowPolicy growth = XGrowPolicy::Automatic) { return XBuffer::maximum_capacity(kMax).with_growth(growth); }

// A root with strings, vectors and a map, plus holes in the free table:
// small and large retired blocks and erased elements.
XBuffer make_source(std::uint64_t id, std::size_t n, XGrowPolicy growth = XGrowPolicy::Automatic) {
    auto b = XBuffer::create<CloneRoot>(1u << 16, cap(growth));
    auto& r = b.root<CloneRoot>();
    r.id = id;
    r.name = "clone-" + std::to_string(id) + std::string(40, 'x');
    for (std::size_t i = 0; i < n; ++i) r.values.push_back(std::uint32_t(id * 1000 + i));
    for (std::size_t i = 0; i < 12; ++i) r.tags.push_back(std::string(20 + i, char('a' + i % 26)).c_str());
    for (std::uint32_t k = 0; k < 9; ++k) r.attrs.emplace(k, std::string(30, char('A' + k)).c_str());
    r.tags.erase(r.tags.begin() + 3);
    r.attrs.erase(4u);
    for (std::uint32_t bytes : {64u, 136u, 6000u}) {
        auto* p = b.arena()->allocate_aligned(bytes, 8);
        std::memset(p, 0xab, bytes);
        b.arena()->deallocate(p, bytes);
    }
    return b;
}

std::size_t used(const XBufferCore& b) { return static_cast<std::size_t>(b.arena()->used_end); }

bool used_bytes_equal(const XBufferCore& a, const XBufferCore& b) {
    return used(a) == used(b) && std::memcmp(a.data(), b.data(), used(a)) == 0;
}

std::vector<std::uint64_t> free_blocks(const XBufferCore& b) {
    std::vector<std::uint64_t> out;
    b.for_each_free_block([&](XBufferCore::WireHole h) { out.push_back(std::uint64_t(h.offset) << 32 | h.bytes); });
    return out;
}

bool same_free_table(const XBufferCore& a, const XBufferCore& b) {
    return a.free_table_complete() == b.free_table_complete() && a.free_block_count() == b.free_block_count() &&
           a.free_block_bytes() == b.free_block_bytes() && free_blocks(a) == free_blocks(b);
}

bool root_refused(XBuffer& b) {
    try { (void)b.root<CloneRoot>(); } catch (const XException&) { return true; }
    return false;
}

// The same edits, applied to two buffers that must stay byte-identical.
void edit(XBuffer& b, std::uint32_t salt) {
    auto& r = b.root<CloneRoot>();
    r.values.push_back(salt);
    r.tags.push_back(std::string(24, char('k' + salt % 8)).c_str());
    r.tags.erase(r.tags.begin());
    r.attrs.emplace(100u + salt, std::string(50, 'z').c_str());
    r.name = std::string(70, char('m' + salt % 8));
}

void clone_is_independent() {
    auto src = make_source(1, 3000);
    const auto wire = src.save_verified<CloneRoot>();
    std::string copy_wire;
    {
        auto c = src.clone();
        need(c.get_address() != src.get_address(), "clone shares the source payload");
        need(c.get_size() == src.get_size(), "clone size differs from the source");
        need(c.maximum_capacity() == src.maximum_capacity(), "clone maximum capacity");
        need(c.growth_policy() == src.growth_policy(), "clone growth policy");
        need(used_bytes_equal(c, src), "clone payload differs from the source");
        need(c.root<CloneRoot>().values.size() == 3000 && c.root<CloneRoot>().id == 1, "clone root content");
        c.root<CloneRoot>().values[0] = 77;
        c.root<CloneRoot>().name = std::string(90, 'q');
        need(src.root<CloneRoot>().values[0] == 1000 && src.save_verified<CloneRoot>() == wire,
             "editing the clone changed the source");
        src.root<CloneRoot>().values[1] = 88;
        need(c.root<CloneRoot>().values[1] == 1001, "editing the source changed the clone");
        copy_wire = c.save_verified<CloneRoot>();
    }
    // The clone owned its payload: destroying it leaves the source intact,
    // and the reverse holds too.
    src.root<CloneRoot>().values[1] = 1001;
    need(src.save_verified<CloneRoot>() == wire, "source after the clone was destroyed");
    XBuffer orphan;
    {
        auto tmp = make_source(2, 10);
        orphan = tmp.clone();
    }
    need(orphan.root<CloneRoot>().id == 2 && orphan.root<CloneRoot>().attrs.size() == 8, "clone outlives its source");
    orphan.root<CloneRoot>().values.push_back(5);
    need(XBuffer::load_verified<CloneRoot>(copy_wire, cap()).root<CloneRoot>().values[0] == 77, "clone wire");
}

void free_table_and_allocation_match() {
    auto src = make_source(3, 500);
    need(src.free_block_count() >= 5 && src.free_table_complete(), "fixture has no holes");
    auto c = src.clone();
    need(same_free_table(c, src), "clone free table differs from the source");
    auto target = XBuffer::create<CloneRoot>(4096, cap());
    src.clone_into(target);
    need(same_free_table(target, src), "clone_into free table differs from the source");
    // Same edits -> same bytes in all three: allocation reuses the same holes.
    for (std::uint32_t round = 0; round < 6; ++round) {
        edit(src, round);
        edit(c, round);
        edit(target, round);
        need(used_bytes_equal(c, src) && used_bytes_equal(target, src), "allocation after clone diverged");
        need(same_free_table(c, src) && same_free_table(target, src), "free table after edits diverged");
    }
    for (std::uint32_t bytes : {64u, 136u, 4096u, 8192u}) {
        auto* a = static_cast<char*>(src.arena()->allocate_aligned(bytes, 8));
        auto* b = static_cast<char*>(c.arena()->allocate_aligned(bytes, 8));
        need(a - src.data() == b - c.data(), "raw allocation offset differs after clone");
    }
    // An incomplete table (load_unverified) stays incomplete in the copy.
    auto raw = XBuffer::load_unverified(src.save(), cap());
    need(!raw.free_table_complete(), "load_unverified fixture has a complete table");
    auto rc = raw.clone();
    need(!rc.free_table_complete(), "clone completed an unknown free table");
    auto rt = XBuffer::create<CloneRoot>(4096, cap());
    raw.clone_into(rt);
    need(!rt.free_table_complete(), "clone_into completed an unknown free table");
}

// The only small hole sits in a later 64-bin word than the request, so the
// search reaches it only through the copied table's nonempty-word summary.
void later_word_hole_is_found_after_clone() {
    auto src = XBuffer::create<CloneRoot>(1u << 16, cap());
    auto* hole = src.arena()->allocate_aligned(2048, 8);
    auto* keep = src.arena()->allocate_aligned(64, 8);
    std::memset(keep, 0x5a, 64);
    src.arena()->deallocate(hole, 2048);
    need(src.free_block_count() == 1, "fixture should have exactly one hole");
    auto c = src.clone();
    auto target = XBuffer::create<CloneRoot>(4096, cap());
    src.clone_into(target);
    auto* a = static_cast<char*>(src.arena()->allocate_aligned(16, 8));
    auto* b = static_cast<char*>(c.arena()->allocate_aligned(16, 8));
    auto* t = static_cast<char*>(target.arena()->allocate_aligned(16, 8));
    need(a == static_cast<char*>(hole), "source did not reuse its later-word hole");
    need(b - c.data() == a - src.data() && t - target.data() == a - src.data(),
         "clone missed a hole in a later bin word");
    need(same_free_table(c, src) && same_free_table(target, src), "free table after split diverged");
}

void wire_identical_and_round_trip() {
    auto src = make_source(4, 2000);
    const auto wire = src.save_verified<CloneRoot>();
    auto c = src.clone();
    need(c.save_verified<CloneRoot>() == wire, "clone save differs from the source save");
    auto loaded = XBuffer::load_verified<CloneRoot>(wire, cap());
    auto lc = loaded.clone();
    need(lc.save_verified<CloneRoot>() == wire, "clone of a loaded buffer is not byte stable");
    need(used_bytes_equal(lc, loaded) && same_free_table(lc, loaded), "clone of a loaded buffer");
    // save -> load of a clone -> clone -> save: still the same bytes.
    auto again = XBuffer::load_verified<CloneRoot>(c.save_verified<CloneRoot>(), cap()).clone();
    need(again.save_verified<CloneRoot>() == wire, "round trip through a clone");
    // Typed access and schema binding come along; untrusted payloads stay untrusted.
    need(c.has_root<CloneRoot>() && !c.has_root<CloneOtherRoot>(), "clone schema binding");
    auto raw = XBuffer::load_unverified(src.save(), cap());
    auto rc = raw.clone();
    need(root_refused(rc), "clone of an unverified payload allowed typed access");
    need(rc.unsafe_root<CloneRoot>().id == 4, "clone of an unverified payload content");
    // An empty arena (no root yet) clones to an empty arena.
    XBuffer empty(4096, cap());
    auto ec = empty.clone();
    need(ec.arena() && ec.arena()->root_offset == 0 && !ec.has_root<CloneRoot>(), "clone of an empty arena");
    ec.make<CloneRoot>()->id = 6;
    need(ec.root<CloneRoot>().id == 6, "make after cloning an empty arena");
}

void clone_into_reuses_destination() {
    auto big = make_source(5, 60000);
    auto small = make_source(6, 3);
    auto dst = XBuffer::create<CloneRoot>(4096, cap());
    const auto* address = dst.get_address();
    auto handle = dst.handle<CloneRoot>();
    big.clone_into(dst);
    need(dst.get_address() == address, "clone_into changed the payload address");
    need(handle->values.size() == 60000, "XHandle did not follow clone_into");
    const auto epoch = dst.epoch();
    small.clone_into(dst);
    need(dst.epoch() == epoch + 1, "clone_into did not advance the epoch by one");
    need(dst.get_address() == address, "clone_into changed the payload address");
    need(handle->values.size() == 3 && handle->id == 6, "XHandle after the second clone_into");
    need(used_bytes_equal(dst, small) && dst.get_size() == small.get_size(), "clone_into payload");
    need(dst.save_verified<CloneRoot>() == small.save_verified<CloneRoot>(), "clone_into save");
    // The big content's bytes do not come back through growth.
    const auto size = dst.get_size();
    need(dst.grow(1u << 20), "grow after clone_into");
    for (std::size_t i = size; i < dst.get_size(); ++i) need(dst.data()[i] == 0, "stale byte after clone_into growth");
    for (std::size_t i = used(dst); i < size; ++i) need(dst.data()[i] == 0, "stale tail byte after clone_into");
    // dst keeps its own maximum capacity and growth policy.
    auto explicit_dst = XBuffer::create<CloneRoot>(4096, cap(XGrowPolicy::Explicit));
    small.clone_into(explicit_dst);
    need(explicit_dst.growth_policy() == XGrowPolicy::Explicit, "clone_into replaced the growth policy");
    // TypedXBuffer forms.
    auto t = TypedXBuffer<CloneRoot>::create(4096, cap());
    t.root().id = 11;
    auto tc = t.clone();
    need(tc.root().id == 11, "TypedXBuffer clone");
    auto td = TypedXBuffer<CloneRoot>::create(4096, cap());
    t.clone_into(td);
    need(td.root().id == 11 && td.save_verified() == t.save_verified(), "TypedXBuffer clone_into");
}

void explicit_growth_keeps_room() {
    // An Explicit buffer has no automatic growth: the clone must keep the
    // source's size so the same mutations fit without grow().
    auto src = make_source(7, 100, XGrowPolicy::Explicit);
    need(src.get_size() > used(src) + 4096, "fixture has no tail room");
    auto c = src.clone();
    need(c.get_size() == src.get_size() && c.growth_policy() == XGrowPolicy::Explicit, "clone size or policy");
    for (std::uint32_t i = 0; i < 200; ++i) {
        src.root<CloneRoot>().values.push_back(i);
        c.root<CloneRoot>().values.push_back(i);
    }
    need(used_bytes_equal(c, src), "Explicit clone diverged");
    // A smaller destination maximum caps the size but still holds the content.
    auto narrow = XBuffer::create<CloneRoot>(
        4096, XBuffer::maximum_capacity(used(src) + 64).with_growth(XGrowPolicy::Explicit));
    src.clone_into(narrow);
    need(narrow.get_size() == used(src) + 64 && used_bytes_equal(narrow, src), "clone_into capped size");
}

void self_copy_is_refused() {
    auto b = make_source(8, 50);
    const auto wire = b.save_verified<CloneRoot>();
    const auto epoch = b.epoch();
    bool threw = false;
    try { b.clone_into(b); } catch (const XException&) { threw = true; }
    need(threw, "clone_into(self) was not refused");
    need(b.epoch() == epoch && b.save_verified<CloneRoot>() == wire, "clone_into(self) changed the buffer");
    auto t = TypedXBuffer<CloneRoot>::create(4096, cap());
    threw = false;
    try { t.clone_into(t); } catch (const XException&) { threw = true; }
    need(threw && t.has_root(), "TypedXBuffer clone_into(self)");
}

void expect_empty_arena(XBuffer& dst, const char* what) {
    need(dst.arena() != nullptr && dst.arena()->root_offset == 0, what);
    need(root_refused(dst) && !dst.has_root<CloneRoot>(), what);
    bool save_refused = false;
    try { (void)dst.save_verified<CloneRoot>(); } catch (const XException&) { save_refused = true; }
    need(save_refused, what);
    dst.reset<CloneRoot>().id = 9;  // and it starts over like a failed reload
    need(dst.root<CloneRoot>().id == 9, what);
}

void failure_leaves_empty_arena() {
    auto src = make_source(9, 20000);
    // Over the destination's maximum: XBadAlloc.
    auto tiny = XBuffer::create<CloneRoot>(4096, XBuffer::maximum_capacity(8192).with_growth(XGrowPolicy::Automatic));
    tiny.root<CloneRoot>().id = 1;
    const auto epoch = tiny.epoch();
    bool too_big = false;
    try { src.clone_into(tiny); } catch (const XBadAlloc&) { too_big = true; }
    need(too_big, "clone_into above maximum_capacity not rejected with XBadAlloc");
    need(tiny.epoch() != epoch, "failed clone_into did not invalidate references");
    expect_empty_arena(tiny, "after an over-capacity clone_into");
    // OS failure while exposing the destination's pages.
    auto dst = XBuffer::create<CloneRoot>(4096, cap());
    detail::fail_next_vm_operation(detail::VmOperation::Prepare);
    bool os_failed = false;
    try { src.clone_into(dst); } catch (const XBadAlloc&) { os_failed = true; }
    detail::clear_vm_failures();
    need(os_failed, "clone_into OS failure not reported as XBadAlloc");
    expect_empty_arena(dst, "after an OS failure in clone_into");
    // A source without an arena: XException, destination emptied.
    XBuffer none;
    auto d2 = XBuffer::create<CloneRoot>(4096, cap());
    bool no_arena = false;
    try { none.clone_into(d2); } catch (const XException&) { no_arena = true; }
    need(no_arena, "clone_into from a buffer without an arena");
    expect_empty_arena(d2, "after clone_into from a buffer without an arena");
    no_arena = false;
    try { (void)none.clone(); } catch (const XException&) { no_arena = true; }
    need(no_arena, "clone() of a buffer without an arena");
    // clone() failures never touch the source.
    const auto wire = src.save_verified<CloneRoot>();
    detail::fail_next_vm_operation(detail::VmOperation::Reserve);
    os_failed = false;
    try { (void)src.clone(); } catch (const XBadAlloc&) { os_failed = true; }
    detail::clear_vm_failures();
    need(os_failed, "clone() OS failure not reported as XBadAlloc");
    need(src.save_verified<CloneRoot>() == wire, "failed clone() changed the source");
    // The source works after all of that.
    auto ok = src.clone();
    need(ok.save_verified<CloneRoot>() == wire, "clone after failures");
}

void pooled_source_clones_through_its_pool() {
    XBufferPool pool(XBufferPool::Limits{4, 4 * kMax, 4 * kMax});
    auto src = XBuffer::create<CloneRoot>(4096, cap().with_pool(pool));
    src.root<CloneRoot>().id = 12;
    { auto warm = XBuffer::create<CloneRoot>(4096, cap().with_pool(pool)); }
    const auto before = pool.stats();
    auto c = src.clone();
    const auto after = pool.stats();
    need(after.hits + after.misses == before.hits + before.misses + 1, "clone did not acquire through the pool");
    need(after.hits == before.hits + 1, "clone did not reuse the pooled mapping");
    need(c.root<CloneRoot>().id == 12, "pooled clone content");
}

// R1 slot storage: clone() keeps the source's kind of storage (a movable slot
// clones onto a movable slot, an address-stable owner onto a pinned one), and
// clone_into() relocates a movable destination slot that is too small.
void clone_keeps_storage_kind() {
    auto movable = make_source(13, 200, XGrowPolicy::Explicit);
    need(movable.storage_kind() == XStorageKind::MovableSlot, "fixture is not a movable slot");
    auto mc = movable.clone();
    need(mc.storage_kind() == XStorageKind::MovableSlot && !mc.address_stable(), "movable slot clone kind");
    need(mc.get_size() == movable.get_size() && used_bytes_equal(mc, movable) && same_free_table(mc, movable),
         "movable slot clone content");

    auto vm = XBuffer::create<CloneRoot>(4096, cap(XGrowPolicy::Explicit).with_storage(XStorage::Pinned));
    vm.root<CloneRoot>().id = 14;
    auto vc = vm.clone();
    need(vc.storage_kind() == XStorageKind::Vm && vc.address_stable(), "pinned VM clone kind");

    auto pinned = XBuffer::create<CloneRoot>(4096, XBuffer::maximum_capacity(1u << 15));
    pinned.root<CloneRoot>().id = 15;
    need(pinned.storage_kind() == XStorageKind::PinnedSlot, "fixture is not a pinned slot");
    auto pc = pinned.clone();
    need(pc.storage_kind() == XStorageKind::PinnedSlot && pc.root<CloneRoot>().id == 15, "pinned slot clone");

    // A small movable destination receives a larger source: it changes
    // storage (epoch advances) and then holds an exact copy.
    auto src = make_source(16, 3000);
    auto dst = XBuffer::create<CloneRoot>(1024, cap(XGrowPolicy::Explicit));
    need(dst.storage_kind() == XStorageKind::MovableSlot && dst.accessible_bytes() < used(src),
         "destination fixture does not force a relocation");
    const auto epoch = dst.epoch();
    src.clone_into(dst);
    need(dst.epoch() != epoch, "clone_into relocation did not advance the epoch");
    need(used_bytes_equal(dst, src) && same_free_table(dst, src), "relocated clone_into content");
    need(dst.save_verified<CloneRoot>() == src.save_verified<CloneRoot>(), "relocated clone_into wire");
    edit(src, 3);
    edit(dst, 3);
    need(used_bytes_equal(dst, src), "relocated clone_into diverged under the same edit");
}
} // namespace

int main() {
    try {
        clone_is_independent();
        free_table_and_allocation_match();
        later_word_hole_is_found_after_clone();
        wire_identical_and_round_trip();
        clone_into_reuses_destination();
        explicit_growth_keeps_room();
        self_copy_is_refused();
        failure_leaves_empty_arena();
        pooled_source_clones_through_its_pool();
        clone_keeps_storage_kind();
        std::cout << "test_clone passed\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "test_clone failed: " << e.what() << '\n';
        return 1;
    }
}
