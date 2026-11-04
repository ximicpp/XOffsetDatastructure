#include "xoffset_image_builder.hpp"
#include <cassert>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <random>

namespace xo = XOffsetDatastructure;
namespace im = xo::image;
namespace host { inline bool deny = false; inline std::size_t calls = 0; }
void* operator new(std::size_t n) {
    ++host::calls;
    if (host::deny) throw std::bad_alloc();
    if (auto p = std::malloc(n ? n : 1)) return p;
    throw std::bad_alloc();
}
void* operator new[](std::size_t n) { return ::operator new(n); }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete[](void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }
void operator delete[](void* p, std::size_t) noexcept { std::free(p); }

struct ImageLeaf { std::uint64_t id{}; xo::XString name; xo::XVector<std::int32_t> values; };
struct ImageRoot {
    std::uint64_t marker{};
    bool enabled{};
    double value{};
    xo::XString label;
    xo::XVector<ImageLeaf> records;
    xo::XVector<bool> flags;
    std::uint32_t fixed[3]{};
    xo::XVector<xo::XVector<std::uint8_t>> nested;
    xo::XVector<std::uint8_t> tail;
};
struct PointerRoot { int* ptr; };
struct VolatileRoot { volatile int n; };
struct BitsRoot { unsigned n : 2; };
struct TinyRoot { std::uint64_t n{}; };
struct DerivedVector : xo::XVector<std::uint32_t> { std::uint32_t extra{}; };
struct DerivedString : xo::XString { std::uint32_t extra{}; };
XOFFSET_REGISTER_SCHEMA_NAME(ImageRoot, "test.DenseImage.v1")
XOFFSET_REGISTER_SCHEMA_NAME(TinyRoot, "test.DenseImageTiny.v1")
namespace XOffsetDatastructure {
template<> struct wire_record_layout<ImageLeaf> { static constexpr bool ordinary_members = true; };
template<> struct wire_record_layout<ImageRoot> { static constexpr bool ordinary_members = true; };
template<> struct wire_record_layout<TinyRoot> { static constexpr bool ordinary_members = true; };
template<> struct wire_record_layout<PointerRoot> { static constexpr bool ordinary_members = true; };
template<> struct wire_record_layout<VolatileRoot> { static constexpr bool ordinary_members = true; };
template<> struct wire_record_layout<BitsRoot> { static constexpr bool ordinary_members = true; };
}
static_assert(im::supported_v<ImageRoot>);
static_assert(im::build_supported_v<ImageRoot>);
static_assert(!im::build_supported_v<xo::XMap<std::uint32_t,std::uint32_t>>);
static_assert(!im::build_supported_v<xo::XSet<std::uint32_t>>);
static_assert(!im::build_supported_v<DerivedVector> && !im::build_supported_v<DerivedString>);
static_assert(!im::supported_v<PointerRoot> && !im::supported_v<VolatileRoot> && !im::supported_v<BitsRoot>);
static_assert(!std::is_constructible_v<xo::PackedWire<ImageRoot>, std::span<const std::byte>>);
static_assert(!std::is_constructible_v<im::RecordView<ImageRoot>, const std::byte*>);
template<class T>concept CanForgeBulk=requires(im::BuildValue<T> value,std::span<const std::uint8_t> bytes){value.template assign<xo::XVector<std::uint8_t>>(bytes);};
static_assert(!CanForgeBulk<ImageRoot> && !CanForgeBulk<xo::XVector<std::uint32_t>>);

template<class F> void rejects(F&& f) {
    bool caught = false; try { f(); } catch (const xo::XException&) { caught = true; }
    assert(caught);
}
auto make(std::size_t extra = 0) {
    auto b = xo::TypedXBuffer<ImageRoot>::create(32768,
        xo::XBuffer::maximum_capacity(4 * 1024 * 1024).with_growth(xo::XGrowPolicy::Automatic));
    auto& r = b.root(); r.marker = 0x123456789abcdef0ULL; r.enabled = true;
    r.value = std::numeric_limits<double>::infinity(); r.label = "short";
    r.records.resize(5);
    for (unsigned i = 0; i < 5; ++i) {
        auto& e = r.records[i]; e.id = i + 91;
        e.name = i % 2 ? "a long non-inline string for the mirror" : "inline";
        e.values.reserve(8); e.values.resize(3);
        for (unsigned j = 0; j < 3; ++j) e.values[j] = int(i * 10 + j) - 20;
    }
    r.flags.reserve(8); r.flags.push_back(true);
    reinterpret_cast<unsigned char*>(r.flags.data())[7] = 2; // unused bool storage must never be read
    r.fixed[0] = 11; r.fixed[1] = 22; r.fixed[2] = 33;
    r.nested.resize(2); r.nested[0].push_back(77);
    r.tail.resize(extra);
    for (std::size_t i = 0; i < extra; ++i) r.tail[i] = std::uint8_t(i);
    r.records[1].name.reserve(256); // a genuine hole and a relocated block
    return b;
}
void check(const im::ImageView<ImageRoot>& image, std::size_t extra = 0) {
    auto r = image.root();
    assert(r.get<0>() == 0x123456789abcdef0ULL && r.get<1>());
    assert(r.get<2>() == std::numeric_limits<double>::infinity());
    assert(r.get<3>() == "short");
    auto records = r.get<4>(); assert(records.size() == 5);
    for (unsigned i = 0; i < 5; ++i) {
        auto e = records[i]; assert(e.get<0>() == i + 91);
        assert(e.get<1>() == (i % 2 ? "a long non-inline string for the mirror" : "inline"));
        auto values = e.get<2>(); assert(values.size() == 3 && values.capacity() == 8);
        for (unsigned j = 0; j < 3; ++j) assert(values[j] == int(i * 10 + j) - 20);
    }
    assert(r.get<5>().size() == 1 && r.get<5>()[0]);
    assert(r.get<6>()[0] == 11 && r.get<6>()[2] == 33);
    assert(r.get<7>()[0][0] == 77 && r.get<7>()[1].size() == 0);
    auto tail = r.get<8>(); assert(tail.size() == extra);
    for (std::size_t i = 0; i < extra; ++i) assert(tail[i] == std::uint8_t(i));
}
int main() {
    const xo::XSaveOptions dense{.emission = xo::XWireEmission::dense};
    for (auto extra : {std::size_t(0), std::size_t(4096), std::size_t(1048576)}) {
        auto b = make(extra);
        auto golden = b.save_verified(dense);
        auto snapshot = xo::freeze_verified(std::move(b));
        assert(!b.has_root());
        std::vector<std::byte> out(golden.size() + 8, std::byte{0x5a});
        xo::emit_dense_into(snapshot, out);
        assert(std::memcmp(out.data(), golden.data(), golden.size()) == 0);
        for (std::size_t i = golden.size(); i < out.size(); ++i) assert(out[i] == std::byte{0x5a});
        rejects([&] { xo::emit_dense_into(snapshot, std::span(out).first(1)); });
        for (auto part : snapshot.segments()) if (!part.empty())
            rejects([&] { xo::emit_dense_into(snapshot, {const_cast<std::byte*>(part.data()), golden.size()}); });
        auto packed = xo::pack_dense(snapshot);
        host::deny = true;
        bool oom = false;
        try { (void)xo::pack_dense(snapshot); } catch (const std::bad_alloc&) { oom = true; }
        host::deny = false; assert(oom && snapshot);
        assert(packed.bytes().size() == golden.size());
        assert(std::memcmp(packed.bytes().data(), golden.data(), golden.size()) == 0);
        rejects([&] { xo::copy_packed_into(packed, {const_cast<std::byte*>(packed.bytes().data()), golden.size()}); });
        im::PreparedImageSlot<ImageRoot> slot(golden.size());
        im::BorrowedImageBuffer<ImageRoot> borrowed(golden.size());
        im::PreparedImageSlot<ImageRoot> tiny(1);
        rejects([&] { (void)im::copy_image_into(packed, tiny); });
        rejects([&] { (void)im::copy_image_into(xo::PackedWire<ImageRoot>{}, slot); });
        const auto allocations = host::calls;
        host::deny = true;
        {
            xo::emit_dense_into(snapshot, out);
            xo::copy_packed_into(packed, out);
            auto image = im::copy_image_into(packed, slot);
            check(image, extra);
            borrowed.reload(packed);
            assert(borrowed.root().get<0>() == 0x123456789abcdef0ULL);
            assert(borrowed.bytes().size() == golden.size());
            assert(std::memcmp(borrowed.bytes().data(),golden.data(),golden.size())==0);
            assert(std::memcmp(image.bytes().data(), golden.data(), golden.size()) == 0);
        }
        host::deny = false;
        assert(host::calls == allocations);
        const auto old_epoch=borrowed.epoch();
        rejects([&]{borrowed.reload(xo::PackedWire<ImageRoot>{});});
        assert(borrowed.epoch()==old_epoch && borrowed.root().get<0>()==0x123456789abcdef0ULL);
        borrowed.reload(packed);assert(borrowed.epoch()==old_epoch+1);
        auto moved_borrow=std::move(borrowed);
        rejects([&]{(void)borrowed.root();});
        rejects([&]{borrowed.reload(packed);});
        borrowed=std::move(moved_borrow);assert(borrowed.root().get<0>()==0x123456789abcdef0ULL);
        {
            auto image = im::copy_image_into(packed, slot);
            rejects([&] { (void)im::copy_image_into(packed, slot); });
            rejects([&] { (void)(image.root().template get<4>()[5]); });
            auto moved = std::move(slot); // view retains storage across slot destruction/move
            slot = std::move(moved);
            check(image, extra);
        }
        snapshot = {}; packed = {}; // independent copied image outlives both source owners
        auto again = make(extra);
        auto p2 = xo::pack_dense(xo::freeze_verified(std::move(again)));
        auto image = im::copy_image_into(p2, slot);
        p2 = {}; check(image, extra);
        // The same dense wire remains accepted by the original owning receiver.
        auto owning = xo::TypedXBuffer<ImageRoot>::load_verified(golden);
        owning.root().marker = 7;
        auto changed = xo::pack_dense(xo::freeze_verified(std::move(owning)));
        im::PreparedImageSlot<ImageRoot> second(changed.bytes().size());
        assert(im::copy_image_into(changed, second).root().get<0>() == 7);
        golden.resize(golden.size() - 1);
        rejects([&] { (void)xo::TypedXBuffer<ImageRoot>::load_verified(golden); });
    }
    // Stale tail never exposed when a reused slot alternates large/small images.
    auto small = xo::pack_dense(xo::freeze_verified(make()));
    auto large = xo::pack_dense(xo::freeze_verified(make(65536)));
    im::PreparedImageSlot<ImageRoot> slot(large.bytes().size());
    { auto v = im::copy_image_into(large, slot); check(v, 65536); }
    { auto v = im::copy_image_into(small, slot); check(v); assert(v.bytes().size() == small.bytes().size()); }
    { auto v = im::copy_image_into(large, slot); check(v, 65536); }
    xo::VerifiedSnapshot<ImageRoot> empty;
    rejects([&] { (void)xo::dense_wire_size(empty); });
    rejects([&] { (void)xo::pack_dense(empty); });
    {
        auto tiny = xo::TypedXBuffer<TinyRoot>::create(4096);
        auto* arena = static_cast<std::byte*>(const_cast<void*>(tiny.get_address()));
        auto frozen = xo::freeze_verified(std::move(tiny));
        assert(xo::dense_wire_size(frozen) == 48);
        rejects([&] { xo::emit_dense_into(frozen, {arena, 48}); });
        assert(frozen.root().n == 0);
    }
    // Fixed seed, valid-graph differential fuzz. Compare every read live value
    // with the owning receiver; no accidental acceptance of arbitrary bytes.
    std::mt19937 random(20261104);
    for (unsigned iteration = 0; iteration < 1000; ++iteration) {
        auto b = make(random() % 257);
        auto& r = b.root(); r.marker = random(); r.enabled = (random() & 1) != 0;
        r.records.resize(random() % 9);
        for (auto& e : r.records) {
            e.id = random(); e.name.assign(std::string(random() % 80, char('a' + random() % 26)));
            e.values.resize(random() % 20);
            for (auto& value : e.values) value = std::int32_t(random());
        }
        auto expected = b.save_verified(dense);
        auto packed = xo::pack_dense(xo::freeze_verified(std::move(b)));
        im::PreparedImageSlot<ImageRoot> target(packed.bytes().size());
        auto actual = im::copy_image_into(packed, target);
        auto native = xo::TypedXBuffer<ImageRoot>::load_verified(expected);
        auto root = actual.root(); auto records = root.get<4>();
        assert(root.get<0>() == native.root().marker && root.get<1>() == native.root().enabled);
        assert(records.size() == native.root().records.size());
        for (std::size_t i = 0; i < records.size(); ++i) {
            auto e = records[i]; const auto& n = native.root().records[i];
            assert(e.get<0>() == n.id && e.get<1>() == n.name.view());
            auto values = e.get<2>(); assert(values.size() == n.values.size());
            for (std::size_t j = 0; j < values.size(); ++j) assert(values[j] == n.values[j]);
        }
    }
    for (unsigned iteration = 0; iteration < 1000; ++iteration) {
        im::ImageBuilder<ImageRoot> builder(65536);
        auto root = builder.root(); const auto marker = random();root.get<0>().set(marker);
        root.get<1>().set(true); root.get<3>().assign("builder");
        auto records = root.get<4>().resize(random()%9);
        for (std::size_t i=0; i<records.size(); ++i) {
            auto e=records[i];e.get<0>().set(i+17);
            e.get<1>().assign(std::string(random()%80,char('a'+random()%26)));
            auto values=e.get<2>().resize(random()%16);
            for (std::size_t j=0; j<values.size(); ++j)values[j].set(std::int32_t(i+j));
        }
        root.get<6>().elements()[2].set(99);
        auto flags=root.get<5>().resize(1);flags[0].set(true);
        const std::array<std::uint8_t,3> bulk{1,2,3};root.get<8>().assign(std::span<const std::uint8_t>(bulk));
        auto nested=root.get<7>().resize(2);nested[0].resize(1)[0].set(77);
        const auto allocations=host::calls;host::deny=true;
        auto wire=std::move(builder).seal();
        host::deny=false;assert(host::calls==allocations);
        rejects([&]{root.get<0>().set(9);});
        auto native=xo::TypedXBuffer<ImageRoot>::load_verified(std::string(reinterpret_cast<const char*>(wire.bytes().data()),wire.bytes().size()));
        assert(native.root().marker==marker && native.root().enabled && native.root().label=="builder");
        assert(native.root().records.size()==records.size() && native.root().fixed[2]==99);
        assert(native.root().flags[0] && native.root().nested[0][0]==77);
        assert(native.root().tail.size()==3 && native.root().tail[2]==3);
        im::ImageView<ImageRoot> keep;
        {im::PreparedImageSlot<ImageRoot> target(wire.bytes().size());keep=im::copy_image_into(wire,target);}
        wire={};assert(keep.root().get<0>()==marker && keep.root().get<4>().size()==records.size());
    }
    {
        im::ImageBuilder<TinyRoot> builder(48);
        builder.root().get<0>().set(42);
        auto packed=std::move(builder).seal();
        auto native=xo::TypedXBuffer<TinyRoot>::create(4096);native.root().n=42;
        const auto golden=native.save_verified(dense);
        assert(golden.size()==packed.bytes().size() && std::memcmp(golden.data(),packed.bytes().data(),golden.size())==0);
        im::ImageBuilder<ImageRoot> limited(256);
        auto value=limited.root().get<4>();rejects([&]{(void)value.resize(1000);});
        (void)std::move(limited).seal(); // failed prepare added no unowned block
    }
    std::cout << "dense export / immutable image proxy contracts passed\n";
}
