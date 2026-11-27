// The fresh-buffer fast paths of 2.0.0-preview.3 against the general paths
// they skip:
//   - load: a dense, slot-sized image of T into a new buffer
//     (XReceiver::load with admit, XBuffer / TypedXBuffer load_verified and
//     try_load_verified) takes one slot and one copy. The buffer it builds
//     matches what the general path builds (storage kind, maximum, growth
//     policy, image bytes, free table) and behaves the same afterwards
//     (identical mutations give identical images). Injected Reserve and
//     Prepare failures, capacity limits and a graph that fails verification
//     after the copy all fail cleanly and give the slot back.
//   - save_verified<T>(): a clean buffer below the automatic compaction
//     threshold is copied as is. Every emission and buffer state gives the
//     bytes save_verified_into (an independent implementation) gives.
//   - typed root access: the fast save path trusts root_is<T>() to imply
//     typed root access (as save_verified_into and the root lookups do).
//     Every state change that drops or restores typed access is walked
//     here; a buffer without typed access is refused by every save. The
//     invariant itself is asserted inside the library in builds without
//     NDEBUG (every test program).
// Built with XOFFSET_FORCE_FULL_CHECK=0 so that admit is the real admit path
// (test assertions undefine NDEBUG). No exception of its own: also built as
// test_fresh_fast_paths_noexc.
#define XOFFSET_FORCE_FULL_CHECK 0
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <span>
#include <string>
#include <vector>
#include "../xoffset_receiver.hpp"
using namespace XOffsetDatastructure;

struct FpItem {
    std::uint32_t id{};
    XString name;
    XVector<std::uint16_t> marks;
};
XOFFSET_RECORD_DERIVED(FpItem, (), id, name, marks)

struct FpRoot {
    std::uint64_t seq{};
    XString title;
    XVector<std::uint32_t> values;
    XVector<FpItem> items;
    XMap<std::uint32_t, XString> names;
    bool flag{};
};
XOFFSET_RECORD_DERIVED(FpRoot, (), seq, title, values, items, names, flag)
XOFFSET_REGISTER_SCHEMA_NAME(FpRoot, "test.FreshFastPathsRoot")

namespace {
int failures = 0;
void need(bool ok, const char* what) {
    if (!ok) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        ++failures;
    }
}

struct Rng {
    std::uint64_t s;
    std::uint64_t next() {
        s += 0x9e3779b97f4a7c15ull;
        std::uint64_t z = s;
        z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ull;
        z = (z ^ (z >> 27)) * 0x94d049bb133111ebull;
        return z ^ (z >> 31);
    }
    std::uint32_t below(std::uint32_t n) { return n ? std::uint32_t(next() % n) : 0; }
};

constexpr std::size_t kSourceMax = std::size_t(1) << 24;
auto source_cap() { return XBuffer::maximum_capacity(kSourceMax).with_growth(XGrowPolicy::Automatic); }

// A random root (inline and heap strings, vectors, nested records, a map)
// with holes in its free table when `holes` (erased elements, retired
// blocks of up to `retire_max` bytes).
XBuffer make_source(std::uint64_t seed, std::size_t scale, bool holes, std::uint32_t retire_max = 800) {
    Rng rng{seed};
    auto b = XBuffer::create<FpRoot>(4096, source_cap());
    auto& r = b.root<FpRoot>();
    r.seq = rng.next();
    r.title = std::string(rng.below(60), char('a' + rng.below(26))).c_str();
    r.flag = rng.below(2) != 0;
    const auto values = rng.below(std::uint32_t(scale) + 1);
    for (std::uint32_t i = 0; i < values; ++i) r.values.push_back(std::uint32_t(rng.next()));
    const auto items = rng.below(std::uint32_t(scale / 8) + 2);
    for (std::uint32_t i = 0; i < items; ++i) {
        auto& item = r.items.emplace_back();
        item.id = i;
        item.name = std::string(rng.below(40), char('A' + rng.below(26))).c_str();
        const auto marks = rng.below(20);
        for (std::uint32_t k = 0; k < marks; ++k) item.marks.push_back(std::uint16_t(rng.next()));
    }
    const auto names = rng.below(std::uint32_t(scale / 16) + 2);
    for (std::uint32_t k = 0; k < names; ++k)
        r.names.emplace(std::uint32_t(rng.next()), std::string(rng.below(50), 'n').c_str());
    if (holes) {
        if (r.items.size() > 2) r.items.erase(r.items.begin() + 1);
        if (!r.names.empty()) r.names.erase(r.names.begin()->first);
        const auto retired = 1 + rng.below(6);
        for (std::uint32_t i = 0; i < retired; ++i) {
            const auto bytes = std::uint32_t(8 * (1 + rng.below(retire_max / 8)));
            auto* p = b.arena()->allocate_aligned(bytes, 8);
            std::memset(p, 0xab, bytes);
            b.arena()->deallocate(p, bytes);
        }
    }
    return b;
}

std::size_t used(const XBufferCore& b) { return static_cast<std::size_t>(b.arena()->used_end); }

bool image_is(const XBufferCore& b, const std::string& wire) {
    return used(b) == wire.size() && std::memcmp(b.data(), wire.data(), wire.size()) == 0;
}

bool same_image(const XBufferCore& a, const XBufferCore& b) {
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

// The same edits on every buffer: reuse of free blocks, growth at the tail,
// a heap string, an erase.
void mutate(XBuffer& b, std::uint64_t seed) {
    Rng rng{seed};
    auto& r = b.root<FpRoot>();
    r.seq += 1;
    r.title = "a title long enough to need a heap block";
    for (int i = 0; i < 24; ++i) r.values.push_back(std::uint32_t(rng.next()));
    auto& item = r.items.emplace_back();
    item.id = 999;
    item.name = "an item name longer than the inline limit";
    for (int i = 0; i < 9; ++i) item.marks.push_back(std::uint16_t(i));
    r.names.emplace(0xfeedu, "fresh");
    if (r.items.size() > 3) r.items.erase(r.items.begin());
}

std::string dense_wire(XBuffer& b) {
    return b.save_verified<FpRoot>(XSaveOptions{.emission = XWireEmission::dense});
}

// The storage the general path plans for a fresh load of `size` bytes
// under `cap` (XBufferCore::plan_storage), for caps the fast path takes.
XStorageKind planned_kind(std::size_t cap) {
    return cap <= XBuffer::SLOT_MAX ? XStorageKind::PinnedSlot : XStorageKind::MovableSlot;
}

enum class Entry { receiver, xbuffer_load_verified, xbuffer_try_load_verified, typed_load_verified, typed_try_load_verified };
constexpr Entry kEntries[] = {Entry::receiver, Entry::xbuffer_load_verified, Entry::xbuffer_try_load_verified,
                              Entry::typed_load_verified, Entry::typed_try_load_verified};

#if XOFFSET_NO_EXCEPTIONS
// Without exceptions the throwing entries end in the fatal handler on a
// failure: only the successful loads use them (see load_equivalence).
bool try_entry_fallback(Entry entry, const std::string& wire, XBuffer::MaximumCapacity cap, XBuffer& out,
                        XErrc& code) {
    (void)code;
    if (entry == Entry::xbuffer_load_verified) out = XBuffer::load_verified<FpRoot>(wire, cap);
    else out = TypedXBuffer<FpRoot>::load_verified(wire, cap);
    return true;
}
#endif

// Loads `wire` through `entry` under `cap`; `out` receives the buffer when
// the load succeeds. Entries that throw are used only with exceptions on.
bool load(Entry entry, const std::string& wire, XBuffer::MaximumCapacity cap, XBuffer& out, XErrc& code) {
    code = XErrc::ok;
    switch (entry) {
    case Entry::receiver: {
        XReceiver<XLink::server_peer> rx(cap);
        auto loaded = rx.load<FpRoot>(wire);
        if (!loaded) { code = loaded.code(); return false; }
        out = std::move(*loaded);
        return true;
    }
    case Entry::xbuffer_try_load_verified: {
        auto loaded = XBuffer::try_load_verified<FpRoot>(wire, cap);
        if (!loaded) { code = loaded.code(); return false; }
        out = std::move(*loaded);
        return true;
    }
    case Entry::typed_try_load_verified: {
        auto loaded = TypedXBuffer<FpRoot>::try_load_verified(wire, cap);
        if (!loaded) { code = loaded.code(); return false; }
        out = std::move(*loaded);
        return true;
    }
#if !XOFFSET_NO_EXCEPTIONS
    case Entry::xbuffer_load_verified:
        try {
            out = XBuffer::load_verified<FpRoot>(wire, cap);
            return true;
        } catch (const XException& e) {
            code = e.code();
            return false;
        }
    case Entry::typed_load_verified:
        try {
            out = TypedXBuffer<FpRoot>::load_verified(wire, cap);
            return true;
        } catch (const XException& e) {
            code = e.code();
            return false;
        }
#else
    case Entry::xbuffer_load_verified:
    case Entry::typed_load_verified:
        return try_entry_fallback(entry, wire, cap, out, code);
#endif
    }
    return false;
}


bool entry_may_fail(Entry entry) {
#if XOFFSET_NO_EXCEPTIONS
    return entry != Entry::xbuffer_load_verified && entry != Entry::typed_load_verified;
#else
    (void)entry;
    return true;
#endif
}

// 1. A fresh fast load builds the buffer the general path builds.
void load_equivalence() {
    const std::size_t caps[] = {std::size_t(1) << 16, XBuffer::SLOT_MAX, std::size_t(1) << 22};
    for (std::uint64_t seed = 1; seed <= 24; ++seed) {
        const std::size_t scale = seed % 3 == 0 ? 1200 : 40 * seed;
        auto source = make_source(seed, scale, seed % 2 == 0);
        const std::string wire = dense_wire(source);
        need(wire.size() <= XBuffer::SLOT_MAX, "source wire fits a slot");
        // The general path: Automatic growth (a VM reservation above
        // SLOT_MAX) and Pinned storage never take the fast path.
        XBuffer general_auto, general_pinned;
        XErrc code{};
        need(load(Entry::xbuffer_try_load_verified, wire,
                  XBuffer::maximum_capacity(std::size_t(1) << 22).with_growth(XGrowPolicy::Automatic), general_auto,
                  code),
             "general load (Automatic growth)");
        need(load(Entry::xbuffer_try_load_verified, wire,
                  XBuffer::maximum_capacity(std::size_t(1) << 22).with_storage(XStorage::Pinned), general_pinned, code),
             "general load (Pinned)");
        need(general_auto.storage_kind() == XStorageKind::Vm && general_pinned.storage_kind() == XStorageKind::Vm,
             "general loads are on VM storage");
        need(image_is(general_auto, wire) && same_free_table(general_auto, source), "general load image");
        for (std::size_t cap : caps) {
            if (wire.size() + 8192 > cap) continue; // room for mutate()
            for (Entry entry : kEntries) {
                XBuffer fast;
                if (!load(entry, wire, XBuffer::maximum_capacity(cap), fast, code)) {
                    need(false, "fast load succeeds");
                    continue;
                }
                need(fast.storage_kind() == planned_kind(cap), "fast load: storage kind as planned");
                need(fast.maximum_capacity() == cap, "fast load: maximum capacity");
                need(fast.growth_policy() == XGrowPolicy::Explicit, "fast load: growth policy");
                need(image_is(fast, wire), "fast load: image is the wire");
                need(same_free_table(fast, general_auto), "fast load: free table as the general load");
                need(fast.ok(), "fast load: no recorded failure");
                // Round trip through both save paths.
                need(fast.save_verified<FpRoot>(XSaveOptions{.emission = XWireEmission::dense}) == wire,
                     "fast load: save_verified round trip");
                // The same edits on the fast load, the general load and the
                // source give the same image and free table.
                auto general = general_auto.clone();
                auto reference = source.clone();
                // Explicit growth: room is asked for up front, the same on all.
                need(fast.grow(8192) && general.grow(8192) && reference.grow(8192), "grow before the edits");
                mutate(fast, seed);
                mutate(general, seed);
                mutate(reference, seed);
                need(same_image(fast, general) && same_free_table(fast, general),
                     "fast and general load behave the same after edits");
                need(same_image(fast, reference) && same_free_table(fast, reference),
                     "fast load and source behave the same after edits");
                std::string a, b;
                fast.save_verified_into<FpRoot>(a);
                general.save_verified_into<FpRoot>(b);
                need(a == b && fast.save_verified<FpRoot>() == a, "fast and general load emit the same wire");
            }
        }
    }
}

// 2. Failures inside the fast load: injected storage failures, a graph that
// fails verification after the copy, and limits. Each leaves no slot behind.
void load_failures() {
    auto source = make_source(77, 300, true);
    const std::string wire = dense_wire(source);
    const auto cap = XBuffer::maximum_capacity(std::size_t(1) << 16);

    // A bool member that is neither 0 nor 1: the header and size admit it,
    // verification rejects it after the slot copy; admit takes it as is.
    std::string bad_bool = wire;
    bad_bool[40 + offsetof(FpRoot, flag)] = char(7);

    const auto carved_before = XBuffer::slot_storage_stats().carved_bytes;
    for (int round = 0; round < 400; ++round) {
        for (Entry entry : kEntries) {
            if (!entry_may_fail(entry)) continue;
            XBuffer out;
            XErrc code{};
            detail::clear_vm_failures();
            detail::fail_next_vm_operation(detail::VmOperation::Reserve);
            need(!load(entry, wire, cap, out, code) && code == XErrc::out_of_storage, "Reserve failure reported");
            detail::clear_vm_failures();
            detail::fail_next_vm_operation(detail::VmOperation::Prepare);
            need(!load(entry, wire, cap, out, code) && code == XErrc::out_of_storage, "Prepare failure reported");
            detail::clear_vm_failures();
            if (entry != Entry::receiver) {
                need(!load(entry, bad_bool, cap, out, code) && code != XErrc::ok,
                     "verification failure after the slot copy reported");
            } else {
                need(load(entry, bad_bool, cap, out, code), "admit takes the image unchecked");
            }
            // The next load on the same thread succeeds.
            XBuffer good;
            need(load(entry, wire, cap, good, code) && image_is(good, wire), "load after a failure");
        }
    }
    // A leaked slot per failure would carve 400 * 5 * 3 new slots.
    const auto carved_after = XBuffer::slot_storage_stats().carved_bytes;
    need(carved_after - carved_before <= (std::size_t(1) << 22), "failed fast loads give their slots back");

    for (Entry entry : kEntries) {
        if (!entry_may_fail(entry)) continue;
        XBuffer out;
        XErrc code{};
        // Over the maximum: the general path refuses it.
        need(!load(entry, wire, XBuffer::maximum_capacity(wire.size() - 8), out, code) &&
                 code == XErrc::capacity_exceeded,
             "wire over the maximum refused");
        // Truncated and misaligned wires never pass the image compare.
        const std::string truncated = wire.substr(0, wire.size() - 8);
        need(!load(entry, truncated, cap, out, code) && code != XErrc::ok, "truncated wire refused");
        const std::string ragged = wire + std::string(3, '\0');
        need(!load(entry, ragged, cap, out, code) && code != XErrc::ok, "ragged wire refused");
    }
}

// 3. save_verified<T>() (fast path when it applies) emits what
// save_verified_into emits, for every emission and buffer state.
void save_equivalence() {
    const XSaveOptions options[] = {XSaveOptions{}, XSaveOptions{.emission = XWireEmission::dense},
                                    XSaveOptions{.emission = XWireEmission::free_runs},
                                    XSaveOptions{.emission = XWireEmission::zero_runs}};
    std::vector<XBuffer> buffers;
    for (std::uint64_t seed = 1; seed <= 12; ++seed) {
        buffers.push_back(make_source(seed, 60 * seed, false));        // no free bytes
        buffers.push_back(make_source(seed + 100, 60 * seed, true, 48)); // a few free bytes (< 512)
        buffers.push_back(make_source(seed + 200, 60 * seed, true, 6000)); // many free bytes
    }
    // At least wire_spare_scan_min used bytes: the root's spare decides.
    for (std::uint64_t seed = 1; seed <= 4; ++seed) {
        auto b = make_source(seed + 300, 6000, false);
        b.root<FpRoot>().values.reserve(b.root<FpRoot>().values.size() * (seed + 1));
        buffers.push_back(std::move(b));
    }
    // Loaded buffers (a fast verified load, an admit load).
    {
        auto s = make_source(400, 500, true);
        const auto w = dense_wire(s);
        auto loaded = XBuffer::try_load_verified<FpRoot>(w, XBuffer::maximum_capacity(std::size_t(1) << 16));
        need(bool(loaded), "verified load for save");
        if (loaded) buffers.push_back(std::move(*loaded));
        XReceiver<XLink::server_peer> rx(XBuffer::maximum_capacity(std::size_t(1) << 16));
        auto admitted = rx.load<FpRoot>(w);
        need(bool(admitted), "admit load for save");
        if (admitted) buffers.push_back(std::move(*admitted));
    }
    std::size_t checked = 0;
    for (auto& b : buffers) {
        for (const auto& option : options) {
            std::string into;
            b.save_verified_into<FpRoot>(into, option);
            const std::string direct = b.save_verified<FpRoot>(option);
            auto tried = b.try_save_verified<FpRoot>(option);
            need(direct == into, "save_verified == save_verified_into");
            need(tried && *tried == into, "try_save_verified == save_verified_into");
            // The wire loads back to the same content.
            auto back = XBuffer::try_load_verified<FpRoot>(direct, source_cap());
            need(back && back->root<FpRoot>().seq == b.root<FpRoot>().seq &&
                     back->root<FpRoot>().values.size() == b.root<FpRoot>().values.size(),
                 "saved wire loads back");
            ++checked;
        }
    }
    need(checked == buffers.size() * 4, "every buffer and emission checked");
}

// 4. Typed root access across the state changes that drop or restore it.
// A buffer without typed access is refused (no_root) by every save; with
// typed access every save path emits the same bytes.
bool refused(XBuffer& b) {
    auto tried = b.try_save_verified<FpRoot>();
    std::string out;
    auto status = b.try_save_verified_into<FpRoot>(out);
    bool ok = !tried && tried.code() == XErrc::no_root && !status && status.code() == XErrc::no_root;
#if !XOFFSET_NO_EXCEPTIONS
    try {
        (void)b.save_verified<FpRoot>();
        ok = false;
    } catch (const XException& e) {
        ok = ok && e.code() == XErrc::no_root;
    }
#endif
    return ok;
}

bool saves_agree(XBuffer& b) {
    std::string into;
    b.save_verified_into<FpRoot>(into);
    auto tried = b.try_save_verified<FpRoot>();
    return tried && *tried == into && b.save_verified<FpRoot>() == into && !into.empty();
}

void typed_access_states() {
    auto source = make_source(500, 200, true);
    const std::string wire = dense_wire(source);
    const auto bytes = std::span<const std::byte>(reinterpret_cast<const std::byte*>(wire.data()), wire.size());

    // Unverified loads: no typed access.
    auto unverified = XBuffer::load_unverified(bytes);
    need(refused(unverified), "load_unverified: refused");
    XBuffer constructed(wire.data(), wire.size());
    need(refused(constructed), "XBuffer(data, size): refused");
    XBuffer constructed_cap(wire.data(), wire.size(), XBuffer::maximum_capacity(std::size_t(1) << 16));
    need(refused(constructed_cap), "XBuffer(data, size, cap): refused");
    auto typed_unverified = TypedXBuffer<FpRoot>::load_unverified(bytes);
    need(refused(typed_unverified), "TypedXBuffer::load_unverified: refused");

    // A clone keeps the source's access.
    auto clone_unverified = unverified.clone();
    need(refused(clone_unverified), "clone of an unverified buffer: refused");
    auto clone_typed = source.clone();
    need(saves_agree(clone_typed), "clone of a typed buffer: saves agree");

    // Move and swap carry the access with the content.
    auto moved_into = XBuffer::load_unverified(bytes);
    auto typed = source.clone();
    moved_into = std::move(typed);
    need(saves_agree(moved_into), "typed content moved over an unverified buffer: saves agree");
    auto left = XBuffer::load_unverified(bytes);
    auto right = source.clone();
    std::swap(left, right);
    need(saves_agree(left) && refused(right), "swap carries typed access with the content");
    auto unverified_moved_over = source.clone();
    unverified_moved_over = XBuffer::load_unverified(bytes);
    need(refused(unverified_moved_over), "unverified content moved over a typed buffer: refused");

    // A verified reload restores typed access; a failed one leaves none.
    auto reloaded = XBuffer::load_unverified(bytes);
    need(bool(reloaded.try_reload_verified<FpRoot>(wire)), "verified reload of an unverified buffer");
    need(saves_agree(reloaded) && image_is(reloaded, wire), "verified reload: saves agree");
    std::string bad_bool = wire;
    bad_bool[40 + offsetof(FpRoot, flag)] = char(7);
    auto failed_reload = source.clone();
    need(!failed_reload.try_reload_verified<FpRoot>(bad_bool), "failed verified reload reported");
    {
        auto tried = failed_reload.try_save_verified<FpRoot>();
        need(!tried, "after a failed reload: nothing to save");
    }

    // A move target takes the access with the content.
    auto donor = source.clone();
    auto taker = std::move(donor);
    need(saves_agree(taker), "move target: saves agree");
    // The moved-from buffer is empty: every save refuses it (no_root)
    // instead of resolving an owner it no longer has.
    need(refused(donor), "moved-from buffer: refused");
    {
        auto compact = donor.try_save_compact<FpRoot>();
        need(!compact && compact.code() == XErrc::no_root, "moved-from buffer: compact save refused");
    }
    // Given content again, it saves as any buffer.
    donor = source.clone();
    need(saves_agree(donor), "moved-from buffer given content again: saves agree");

    // A fresh typed buffer and the fast loads (section 1) have access.
    auto fresh = XBuffer::create<FpRoot>(4096, source_cap());
    need(saves_agree(fresh), "fresh buffer: saves agree");
}

#if !XOFFSET_NO_EXCEPTIONS
// 5. A recorded failure: the fast save path refuses as the general one does.
void failed_emit() {
    auto b = XBuffer::create<FpRoot>(256, XBuffer::maximum_capacity(4096));
    auto values = b.ref<FpRoot>().field<&FpRoot::values>();
    bool failed = false;
    for (int i = 0; i < 4096 && !failed; ++i) failed = !values.push_back(std::uint32_t(i));
    need(failed && !b.ok(), "a proxy push_back past the maximum records a failure");
    XErrc direct{}, into{};
    try { (void)b.save_verified<FpRoot>(); } catch (const XException& e) { direct = e.code(); }
    try { std::string out; b.save_verified_into<FpRoot>(out); } catch (const XException& e) { into = e.code(); }
    need(direct != XErrc::ok && direct == into, "save_verified refuses a recorded failure as save_verified_into does");
    b.clear_error();
    need(saves_agree(b), "after clear_error: saves agree");
}
#endif
} // namespace

int main() {
    load_equivalence();
    load_failures();
    save_equivalence();
    typed_access_states();
#if !XOFFSET_NO_EXCEPTIONS
    failed_emit();
#endif
    if (failures) {
        std::fprintf(stderr, "test_fresh_fast_paths: %d failure(s)\n", failures);
        return 1;
    }
    std::printf("test_fresh_fast_paths: all checks passed\n");
    return 0;
}
