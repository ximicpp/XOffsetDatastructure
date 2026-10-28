// Raw receive component tests (V2 wire: header + payload + hole table).
#include "receive_raw.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <compare>
#include <cstring>
#include <iostream>
#include <limits>
#include <memory_resource>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>

namespace xo = XOffsetDatastructure;
namespace rd = xo::detail;
namespace rr = xo::detail::receive_raw;
namespace model {
struct Attr { xo::XString key; xo::XVector<std::uint8_t> value; };
struct Record {
    std::uint64_t id{};
    std::uint32_t revision{};
    std::int64_t score{};
    xo::XString name;
    xo::XVector<std::uint32_t> samples;
    xo::XVector<std::uint8_t> payload;
    xo::XVector<Attr> attrs;
};
struct Snapshot {
    std::uint32_t schema_version{1};
    std::uint64_t epoch{};
    xo::XVector<Record> records;
};
struct Bools { xo::XVector<bool> values; };
struct Flag { bool flag{}; xo::XString label; };
struct Maps { xo::XMap<std::uint32_t, bool> values; };
struct Sets { xo::XSet<xo::XString> names; };
struct Fixed { xo::XString labels[2]; bool flags[2]; };
struct FixedScalars { std::uint32_t values[2]; bool flags[2]; };
struct Constructor { std::uint32_t value; Constructor() noexcept : value(0) {} };
struct Destructor { std::uint32_t value{}; ~Destructor() noexcept {} };
struct Assignment {
    std::uint32_t value{};
    Assignment& operator=(const Assignment& rhs) noexcept { value = rhs.value; return *this; }
};
struct Volatile { volatile std::uint32_t value; };
struct Const { const std::uint32_t value{}; };
struct Bits { unsigned value : 1; };
struct Empty {};
struct Overlap { [[no_unique_address]] Empty empty; xo::XString value; };
struct UserKey { std::uint32_t value; };
} // namespace model
XOFFSET_RECORD_DERIVED(model::Attr, (), key, value)
XOFFSET_RECORD_DERIVED(model::Record, (), id, revision, score, name, samples, payload, attrs)
XOFFSET_RECORD_DERIVED(model::Snapshot, (), schema_version, epoch, records)
XOFFSET_RECORD_DERIVED(model::Bools, (), values)
XOFFSET_RECORD_DERIVED(model::Flag, (), flag, label)
XOFFSET_RECORD_DERIVED(model::Maps, (), values)
XOFFSET_RECORD_DERIVED(model::Sets, (), names)
XOFFSET_RECORD_DERIVED(model::Fixed, (), labels, flags)
XOFFSET_RECORD_DERIVED(model::FixedScalars, (), values, flags)
XOFFSET_RECORD_DERIVED(model::Destructor, (), value)
XOFFSET_RECORD_DERIVED(model::Assignment, (), value)
XOFFSET_RECORD_DERIVED(model::Volatile, (), value)
XOFFSET_RECORD_DERIVED(model::Const, (), value)
XOFFSET_RECORD_DERIVED(model::UserKey, (), value)
namespace model {
bool operator<(const UserKey& a, const UserKey& b) { return a.value < b.value; }
enum class BuiltinEnum : std::uint32_t { low = 1, high = 2 };
inline std::size_t rvalue_enum_calls{};
enum class RvalueEnum : std::uint32_t { low = 1, high = 2 };
bool operator<(RvalueEnum&&, RvalueEnum&&) noexcept {
    ++rvalue_enum_calls; return false;
}
enum class RvalueSpaceshipEnum : std::uint32_t { low = 1, high = 2 };
std::strong_ordering operator<=>(RvalueSpaceshipEnum&& a, RvalueSpaceshipEnum&& b) noexcept {
    ++rvalue_enum_calls; return std::uint32_t(b) <=> std::uint32_t(a);
}
enum class ThrowingEnum : std::uint32_t { low = 1, high = 2 };
bool operator<(ThrowingEnum, ThrowingEnum) { throw std::runtime_error("enum operator< must not be called"); }
enum class SpaceshipEnum : std::uint32_t { low = 1, high = 2 };
std::strong_ordering operator<=>(SpaceshipEnum a, SpaceshipEnum b) noexcept {
    return std::uint32_t(b) <=> std::uint32_t(a);
}
enum class PolicyEnum : std::uint32_t { low = 1, high = 2 };
std::strong_ordering operator<=>(PolicyEnum a, PolicyEnum b) noexcept {
    return std::uint32_t(b) <=> std::uint32_t(a);
}
struct Enums { xo::XMap<PolicyEnum, bool> values; };
struct DerivedHead : xo::XString { DerivedHead() noexcept = default; };
struct CustomScalar { std::uint32_t value{}; };
struct CustomHolder { xo::XVector<CustomScalar> values; };
struct CanonicalValidateOnly { std::uint32_t value{}; };
enum Nonfixed { nf_zero=0,nf_seven=7 };
enum Negative { negative_low=-3,negative_high=1 };
enum Zero { only_zero=0 };
enum EmptyEnum {};
enum FixedEnum : std::uint32_t { fixed_zero=0,fixed_one=1 };
enum class ScopedEnum : std::uint32_t { zero=0,one=1 };
enum Wide { wide_min=(-9223372036854775807LL-1),wide_max=9223372036854775807LL };
enum UnsignedWide { unsigned_wide_max=18446744073709551615ULL };
// Enums without a fixed underlying type cannot be part of a wire schema (their
// representation is compiler-chosen; tests/portable_schema/key_fail.cpp), so a
// wire carries only fixed enums; raw_enum_bounds of the others is checked below.
struct EnumRepresentations {
    FixedEnum fixed{};ScopedEnum scoped{};
    xo::XVector<FixedEnum> values;
    xo::XMap<ScopedEnum,std::uint32_t> keys;
};
} // namespace model
XOFFSET_RECORD_DERIVED(model::Enums, (), values)
XOFFSET_RECORD_DERIVED(model::CustomScalar, (), value)
XOFFSET_RECORD_DERIVED(model::CustomHolder, (), values)
XOFFSET_RECORD_DERIVED(model::CanonicalValidateOnly, (), value)
XOFFSET_RECORD_DERIVED(model::EnumRepresentations, (), fixed, scoped, values, keys)

namespace XOffsetDatastructure {
// Source attestations: all these declarations above have only ordinary members.
template<> struct wire_record_layout<model::Attr> { static constexpr bool ordinary_members = true; };
template<> struct wire_record_layout<model::Record> { static constexpr bool ordinary_members = true; };
template<> struct wire_record_layout<model::Snapshot> { static constexpr bool ordinary_members = true; };
template<> struct wire_record_layout<model::Bools> { static constexpr bool ordinary_members = true; };
template<> struct wire_record_layout<model::Flag> { static constexpr bool ordinary_members = true; };
template<> struct wire_record_layout<model::Maps> { static constexpr bool ordinary_members = true; };
template<> struct wire_record_layout<model::Sets> { static constexpr bool ordinary_members = true; };
template<> struct wire_record_layout<model::Fixed> { static constexpr bool ordinary_members = true; };
template<> struct wire_record_layout<model::FixedScalars> { static constexpr bool ordinary_members = true; };
// These attestations cannot bypass the independent implicit/cv/bit-field gate.
template<> struct wire_record_layout<model::Constructor> { static constexpr bool ordinary_members = true; };
template<> struct wire_record_layout<model::Destructor> { static constexpr bool ordinary_members = true; };
template<> struct wire_record_layout<model::Assignment> { static constexpr bool ordinary_members = true; };
template<> struct wire_record_layout<model::Volatile> { static constexpr bool ordinary_members = true; };
template<> struct wire_record_layout<model::Const> { static constexpr bool ordinary_members = true; };
template<> struct wire_record_layout<model::Bits> { static constexpr bool ordinary_members = true; };
template<> struct wire_record_layout<model::UserKey> { static constexpr bool ordinary_members = true; };
template<> struct wire_record_layout<model::Enums> { static constexpr bool ordinary_members = true; };
template<> struct wire_record_layout<model::CustomHolder> { static constexpr bool ordinary_members = true; };
template<> struct wire_record_layout<model::EnumRepresentations> { static constexpr bool ordinary_members = true; };
template<> struct wire_activation<model::CustomScalar> {
    static constexpr bool enabled = true;
    static void validate_raw(const char* p,detail::WireValidationContext&) {
        (void)detail::raw_wire_pod<std::uint32_t>(p);
    }
    static void activate(char* p) noexcept {
        const auto value=detail::raw_wire_pod<std::uint32_t>(p);
        auto* object=::new(static_cast<void*>(p)) model::CustomScalar;
        object->value=value;
    }
};
template<> struct wire_activation<model::CanonicalValidateOnly> {
    static constexpr bool enabled=true;
    inline static unsigned canonical_calls{};
    static void validate_raw(const char*,detail::WireValidationContext&) {}
    static void validate_raw(const char*,detail::WireValidationContext&,const detail::CanonicalRawBytes*) {
        ++canonical_calls;
    }
    static void activate(char* p) noexcept {
        const auto value=detail::raw_wire_pod<std::uint32_t>(p);
        auto* object=::new(static_cast<void*>(p)) model::CanonicalValidateOnly;
        object->value=value;
    }
    // Deliberately no canonical activate overload. A canonical Proof must
    // refuse this surface before running validation, never terminate later.
};
// This explicit raw policy reproduces the namespace enum's reverse ordering.
// It never invokes either enum operator on temporary or inactive wire objects.
template<> struct raw_key_order<model::PolicyEnum> {
    static constexpr bool enabled = true;
    inline static std::size_t calls{};
    inline static bool fail{};
    static bool less(const char* a, const char* b, const detail::WireValidationContext&) {
        ++calls;
        if (fail) throw std::runtime_error("explicit enum raw policy failure");
        return detail::raw_wire_pod<std::uint32_t>(a) > detail::raw_wire_pod<std::uint32_t>(b);
    }
};
}
XOFFSET_REGISTER_SCHEMA_NAME(model::Snapshot, "four-path.raw.demo.v1")
XOFFSET_REGISTER_SCHEMA_NAME(model::Bools, "four-path.raw.bools.v1")
XOFFSET_REGISTER_SCHEMA_NAME(model::Flag, "four-path.raw.flag.v1")
XOFFSET_REGISTER_SCHEMA_NAME(model::Maps, "four-path.raw.maps.v1")
XOFFSET_REGISTER_SCHEMA_NAME(model::Sets, "four-path.raw.sets.v1")
XOFFSET_REGISTER_SCHEMA_NAME(model::Fixed, "four-path.raw.fixed.v1")
XOFFSET_REGISTER_SCHEMA_NAME(model::FixedScalars, "four-path.raw.fixed-scalars.v1")
XOFFSET_REGISTER_SCHEMA_NAME(model::Enums, "four-path.raw.enums.v1")
XOFFSET_REGISTER_SCHEMA_NAME(model::CanonicalValidateOnly,"four-path.raw.incomplete-canonical.v2")
XOFFSET_REGISTER_SCHEMA_NAME(model::EnumRepresentations,"four-path.raw.enum-representations.v3")

static_assert(rd::raw_implicit_lifetime<model::Snapshot>(),
              "fixed compiler must support the implicit-lifetime trait/builtin");
static_assert(rd::receive_raw_supported<model::Snapshot>());
static_assert(rd::receive_raw_supported<model::Bools>());
static_assert(rd::receive_raw_supported<model::Flag>());
static_assert(rd::receive_raw_supported<model::Maps>());
static_assert(rd::receive_raw_supported<model::Sets>());
// Historical owning-head C-array is outside this SDK's actual V1 admission.
static_assert(!xo::is_v1_wire_admitted_v<model::Fixed>);
static_assert(!rd::receive_raw_supported<model::Fixed>());
static_assert(xo::is_v1_wire_admitted_v<model::FixedScalars>);
static_assert(rd::receive_raw_supported<model::FixedScalars>());
static_assert(!rd::receive_raw_supported<model::Constructor>());
static_assert(!rd::receive_raw_supported<model::Destructor>());
static_assert(!rd::receive_raw_supported<model::Assignment>());
static_assert(!rd::raw_only_implicit_special_members<model::Assignment>());
static_assert(!rd::receive_raw_supported<model::Volatile>());
static_assert(!rd::receive_raw_supported<model::Const>());
#if XOFFSET_SCHEMA_REFLECTION
static_assert(!rd::receive_raw_supported<model::Bits>());
#endif
#if XOFFSET_SCHEMA_REFLECTION // overlapping ([[no_unique_address]]) members: no field list possible
static_assert(!rd::receive_raw_supported<model::Overlap>());
#endif
static_assert(!rd::receive_raw_supported<xo::XSet<model::UserKey>>());
static_assert(!rd::receive_raw_supported<xo::XSet<float>>());
static_assert(xo::raw_key_order<model::BuiltinEnum>::enabled);
static_assert(xo::raw_key_order<model::RvalueEnum>::enabled);
static_assert(xo::raw_key_order<model::RvalueSpaceshipEnum>::enabled);
static_assert(!xo::raw_key_order<model::ThrowingEnum>::enabled);
static_assert(!xo::raw_key_order<model::SpaceshipEnum>::enabled);
static_assert(!rd::receive_raw_supported<xo::XSet<model::ThrowingEnum>>());
static_assert(!rd::receive_raw_supported<xo::XSet<model::SpaceshipEnum>>());
static_assert(rd::receive_raw_supported<model::Enums>());
static_assert(noexcept(rd::activate_verified_wire<model::Snapshot>(
    static_cast<char*>(nullptr), std::size_t{})));
static_assert(rr::supported_v<model::Snapshot>);
static_assert(!rd::raw_descriptor_layout<model::DerivedHead>());
static_assert(rr::future_supported_v<model::Snapshot>);
static_assert(rr::supported_v<model::CustomScalar>);
static_assert(!rr::future_supported_v<model::CustomScalar>);
static_assert(!rr::future_supported_v<model::CustomHolder>);
static_assert(!rr::future_supported_v<xo::XVector<model::CustomScalar>>);
static_assert(!rr::future_supported_v<model::CustomScalar[2]>);
static_assert(rr::supported_v<model::CanonicalValidateOnly>);
static_assert(!rr::future_supported_v<model::CanonicalValidateOnly>);
static_assert(!rd::raw_enum_fixed_underlying<model::Nonfixed>());
static_assert(!rd::raw_enum_fixed_underlying<model::Negative>());
static_assert(!rd::raw_enum_fixed_underlying<model::Zero>());
static_assert(!rd::raw_enum_fixed_underlying<model::EmptyEnum>());
static_assert(rd::raw_enum_fixed_underlying<model::FixedEnum>());
static_assert(rd::raw_enum_fixed_underlying<model::ScopedEnum>());
#if XOFFSET_SCHEMA_REFLECTION
static_assert(rd::raw_enum_bounds<model::Nonfixed>().low==0&&rd::raw_enum_bounds<model::Nonfixed>().high==7);
static_assert(rd::raw_enum_bounds<model::Negative>().low==-4&&rd::raw_enum_bounds<model::Negative>().high==3);
static_assert(rd::raw_enum_bounds<model::Zero>().high==1&&rd::raw_enum_bounds<model::EmptyEnum>().high==1);
static_assert(rd::raw_enum_bounds<model::Wide>().low==(std::numeric_limits<std::underlying_type_t<model::Wide>>::min)());
static_assert(rd::raw_enum_bounds<model::UnsignedWide>().high==(std::numeric_limits<std::underlying_type_t<model::UnsignedWide>>::max)());
static_assert(rd::raw_needs_validation<model::Nonfixed>()&&!rd::raw_needs_validation<model::FixedEnum>());
#endif
static_assert(!std::is_default_constructible_v<rr::Proof<model::Snapshot>>);
static_assert(!std::is_copy_constructible_v<rr::Proof<model::Snapshot>>);
static_assert(std::is_nothrow_move_constructible_v<rr::Proof<model::Snapshot>>);
static_assert(noexcept(std::declval<rr::Proof<model::Snapshot>&&>().activate()));

namespace {
void need(bool ok, const char* text) {
    if (!ok) throw std::runtime_error(text);
}
auto capacity() {
    return xo::XBuffer::maximum_capacity(4u << 20).with_growth(xo::XGrowPolicy::Automatic);
}
// V2.1 dense wire: [XWireHeaderV2][payload bytes 48..used_bytes][hole table:
// hole_count x 8]. `image` is the payload a receiver builds from it: the
// payload header it writes itself, then the wire's payload bytes.
struct Wire {
    std::string bytes;
    std::string image;
    xo::XWireHeaderV2 header() const {
        return rd::raw_wire_pod<xo::XWireHeaderV2>(bytes.data());
    }
    const char* payload() const { return image.data(); }
    char* payload() { return image.data(); }
    std::size_t extent() const { return std::size_t(header().used_bytes); }
    const char* holes() const {
        return bytes.data() + sizeof(xo::XWireHeaderV2) + extent() - xo::XWIRE_PAYLOAD_BEGIN;
    }
    std::size_t hole_count() const { return std::size_t(header().hole_count); }
    xo::XBufferCore::WireHole hole(std::size_t i) const {
        need(i < hole_count(), "test hole index");
        return rd::raw_wire_pod<xo::XBufferCore::WireHole>(holes() + i * sizeof(xo::XBufferCore::WireHole));
    }
    template<class P> P at(std::size_t offset) const {
        need(offset <= extent() && sizeof(P) <= extent() - offset, "test raw read bounds");
        return rd::raw_wire_pod<P>(payload() + offset);
    }
    template<class P> void put(std::size_t offset, P value) {
        need(offset <= extent() && sizeof(P) <= extent() - offset, "test raw write bounds");
        std::memcpy(payload() + offset, &value, sizeof(value));
    }
    std::size_t target(std::size_t head) const {
        const auto d = at<rd::RawOwningDescriptor>(head);
        const auto value = std::int64_t(head) + d.data_delta;
        need(value >= 0 && std::uint64_t(value) < extent(), "test relative geometry");
        return std::size_t(value);
    }
    xo::XBufferCore owner() const {
        // This component fixture deliberately uses the existing copied owner.
        // It tests RAW verification/activation, not the token's no-copy seam.
        // V2: an owner copied from raw bytes has no known free table until
        // the wire hole table is installed (the Core wrapper requires it).
        xo::XBufferCore core(payload(), extent(), capacity());
        core.load_free_blocks(holes(), hole_count());
        return core;
    }
};
template<class T> Wire wire_of(xo::XBuffer& source) {
    Wire wire{source.save_verified<T>(), {}};
    const auto h = wire.header();
    need(h.extent_count == 0 && wire.bytes.size() == sizeof(xo::XWireHeaderV2) + h.used_bytes -
         xo::XWIRE_PAYLOAD_BEGIN + h.hole_count * sizeof(xo::XBufferCore::WireHole),
         "V2.1 wire size differs from header");
    wire.image.assign(std::size_t(h.used_bytes), '\0');
    xo::XBufferCore::write_payload_header(wire.image.data(), h.root_offset, h.used_bytes);
    std::memcpy(wire.image.data() + xo::XWIRE_PAYLOAD_BEGIN, wire.bytes.data() + sizeof(xo::XWireHeaderV2),
                std::size_t(h.used_bytes) - xo::XWIRE_PAYLOAD_BEGIN);
    return wire;
}
template<class T>
auto admit(xo::XBufferCore& owner, const Wire& wire,
           std::pmr::memory_resource* resource = std::pmr::new_delete_resource()) {
    const auto header = wire.header();
    return rr::validate<T>(owner.data(), owner.get_size(),
        header.root_offset, wire.holes(), wire.hole_count(), resource);
}
Wire demo(bool large = false) {
    auto source = xo::XBuffer::create<model::Snapshot>(65536, capacity());
    auto& snapshot = source.root<model::Snapshot>();
    snapshot.epoch = 123;
    snapshot.records.resize(2);
    for (std::uint32_t i = 0; i < 2; ++i) {
        auto& r = snapshot.records[i];
        r.id = 10 + i; r.revision = i; r.score = -std::int64_t{100} + i;
        // Out-of-line (> 10 bytes): its replacement below retires a block.
        r.name = "original record name";
        r.samples.push_back(i); r.samples.push_back(i + 20);
        r.payload.push_back(std::uint8_t(i + 4));
        r.attrs.resize(1);
        r.attrs[0].key = "attrs";
        r.attrs[0].value.push_back(std::uint8_t(i + 1));
    }
    // Ensure a real hole (V2 hole table entry) and a second scratch index exist.
    snapshot.records[0].name = "a substantially longer replacement name";
    // Above 32 KiB the validator takes its bitmaps from the scratch resource.
    if (large) snapshot.records[1].payload.reserve(40000);
    auto wire = wire_of<model::Snapshot>(source);
    need(wire.hole_count() != 0, "demo wire has no hole table entry");
    return wire;
}
class CountingResource : public std::pmr::memory_resource {
public:
    long fail_at{-1};
    std::size_t attempts{}, failures{}, live{}, peak{}, allocations{}, deallocations{};
private:
    void* do_allocate(std::size_t bytes, std::size_t alignment) override {
        const auto point = attempts++;
        if (fail_at >= 0 && point == std::size_t(fail_at)) {
            ++failures; throw std::bad_alloc();
        }
        auto* p = std::pmr::new_delete_resource()->allocate(bytes, alignment);
        ++allocations; live += bytes; peak = std::max(peak, live);
        return p;
    }
    void do_deallocate(void* p, std::size_t bytes, std::size_t alignment) override {
        ++deallocations; live -= bytes;
        std::pmr::new_delete_resource()->deallocate(p, bytes, alignment);
    }
    bool do_is_equal(const std::pmr::memory_resource& other) const noexcept override {
        return this == &other;
    }
};
template<class T> void rejects(const Wire& wire, const char* expected) {
    auto owner = wire.owner();
    const auto before = owner.copy_bytes();
    bool caught = false;
    std::string actual = "(accepted)";
    try { (void)admit<T>(owner, wire); }
    catch (const xo::XException& error) {
        actual = error.what();
        caught = actual == expected;
    }
    if (!caught) std::cerr << "expected: " << expected << "\nactual:   " << actual << "\n";
    need(caught, "raw first error differs");
    need(owner.copy_bytes() == before, "rejected raw graph changed payload");
}
void demo_activation_and_oom() {
    const auto wire = demo();
    auto owner = wire.owner();
    const auto before = owner.copy_bytes();
    const auto* base = owner.get_address();
    CountingResource resource;
    auto original = admit<model::Snapshot>(owner, wire, &resource);
    need(resource.live == 0 && resource.allocations == resource.deallocations,
         "raw verification scratch did not balance");
    need(owner.copy_bytes() == before, "successful raw verifier wrote payload");
    auto proof = std::move(original);
    need(std::move(original).activate().owning_heads == 0, "moved proof retained activation authority");
    const auto stats = std::move(proof).activate();
    need(stats.owning_heads == 13 && stats.custom_objects == 0, "demo activation head count differs");
    need(owner.get_address() == base && owner.copy_bytes() == before,
         "activation changed base or any payload byte");
    need(std::move(proof).activate().owning_heads == 0, "proof activated twice");
    auto* snapshot = std::launder(reinterpret_cast<model::Snapshot*>(
        owner.data() + std::size_t(wire.header().root_offset)));
    need(snapshot->epoch == 123 && snapshot->records.size() == 2, "active root values differ");
    need(snapshot->records[1].id == 11 && snapshot->records[1].score == -99 &&
         snapshot->records[1].attrs[0].key == "attrs" &&
         snapshot->records[1].samples[1] == 21, "active nested values differ");
    need(resource.allocations == 0, "payload below 32 KiB used the raw scratch resource");
    const auto large = demo(true);
    need(large.header().used_bytes > 32768, "large raw fixture is not above the inline scratch limit");
    for (long point = 0; point < 1; ++point) {
        const auto& wire = large;
        auto failed_owner = wire.owner();
        const auto failed_before = failed_owner.copy_bytes();
        CountingResource fail; fail.fail_at = point;
        bool caught = false;
        try {
            auto admitted = admit<model::Snapshot>(failed_owner, wire, &fail);
            (void)std::move(admitted).activate();
        } catch (const std::bad_alloc&) { caught = true; }
        need(caught && fail.failures == 1 && fail.live == 0, "real raw OOM point not exercised");
        need(failed_owner.copy_bytes() == failed_before, "raw OOM changed activation bytes");
    }
}
void metadata_error_order() {
    const auto wire = demo();
    const auto wire_header = wire.header();
    const auto arena = wire.at<rd::RawPayloadHeader>(0);
    const std::string table(wire.holes(), wire.hole_count() * sizeof(xo::XBufferCore::WireHole));
    const auto expect = [&](rd::RawPayloadHeader altered, std::uint64_t wire_root,
                            const std::string& holes, const char* message,
                            bool before_scratch = true) {
        auto owner = wire.owner();
        std::memcpy(owner.data(), &altered, sizeof(altered));
        const auto before = owner.copy_bytes();
        CountingResource resource;
        bool caught = false;
        try {
            (void)rr::validate<model::Snapshot>(owner.data(), owner.get_size(),
                wire_root, holes.data(), holes.size() / sizeof(xo::XBufferCore::WireHole), &resource);
        } catch (const xo::XException& error) {
            caught = std::string_view(error.what()) == message;
        }
        need(caught, "raw metadata error order differs");
        need((!before_scratch || resource.attempts == 0) && resource.live == 0 &&
             owner.copy_bytes() == before,
             "metadata rejection allocated scratch or wrote payload");
    };
    // V2 payload header: {magic XBUFV2, header_size, flags, root_offset,
    // used_end, reserved0, reserved1}; there is no allocator offset/state.
    auto bad = arena;
    bad.magic[5] = '1'; // the V1 payload magic is not accepted
    expect(bad, wire_header.root_offset + 8, table, "XBufferCore: payload header mismatch");
    bad = arena;
    bad.header_size = 40;
    expect(bad, wire_header.root_offset, table, "XBufferCore: payload header mismatch");
    bad = arena;
    bad.reserved0 = 1;
    expect(bad, wire_header.root_offset + 8, table,
           "load_verified<T>(): root offset mismatch");
    expect(bad, wire_header.root_offset, table,
           "wire validation: unsupported arena header flags");
    bad = arena;
    bad.reserved1 = 1;
    expect(bad, wire_header.root_offset, table,
           "wire validation: unsupported arena header flags");
    bad = arena;
    bad.root_offset = wire.extent() + 8;
    expect(bad, wire_header.root_offset + 8, table,
           "load_verified<T>(): root not found");
    bad = arena;
    bad.flags = 1;
    expect(bad, wire_header.root_offset + 8, table,
           "load_verified<T>(): root offset mismatch");
    expect(bad, wire_header.root_offset, table,
           "wire validation: unsupported arena header flags");
    bad = arena;
    bad.used_end -= 8;
    expect(bad, wire_header.root_offset + 8, table,
           "load_verified<T>(): non-normalized payload extent");
    // Hole table entries are checked after the payload header (whose range
    // registration may use scratch) and before the root/graph walk.
    need(wire.hole_count() != 0, "metadata fixture has no hole table");
    const auto first = wire.hole(0);
    auto with_first = [&](xo::XBufferCore::WireHole hole) {
        std::string altered = table;
        std::memcpy(altered.data(), &hole, sizeof(hole));
        return altered;
    };
    for (const auto hole : {xo::XBufferCore::WireHole{first.offset + 4, first.bytes},
                            xo::XBufferCore::WireHole{first.offset, first.bytes + 4},
                            xo::XBufferCore::WireHole{first.offset, 0},
                            xo::XBufferCore::WireHole{0, 8},
                            xo::XBufferCore::WireHole{std::uint32_t(wire.extent()), 8}})
        expect(arena, wire_header.root_offset, with_first(hole),
               "wire validation failed: invalid hole table entry", false);
}

// V2 exact coverage: header + live blocks + holes tile [0, used_end).
template<class T>
void rejects_holes(const Wire& wire, const std::string& holes, const char* expected) {
    auto owner = wire.owner();
    const auto before = owner.copy_bytes();
    CountingResource resource;
    bool caught = false;
    try {
        (void)rr::validate<T>(owner.data(), owner.get_size(), wire.header().root_offset,
            holes.data(), holes.size() / sizeof(xo::XBufferCore::WireHole), &resource);
    } catch (const xo::XException& error) {
        caught = std::string_view(error.what()) == expected;
    }
    need(caught, "raw hole-table first error differs");
    need(resource.live == 0 && owner.copy_bytes() == before, "rejected hole table leaked scratch or changed payload");
}
void hole_table_exact_coverage() {
    const auto wire = demo();
    using Hole = xo::XBufferCore::WireHole;
    const std::string table(wire.holes(), wire.hole_count() * sizeof(Hole));
    for (std::size_t i = 0; i < wire.hole_count(); ++i) {
        const auto hole = wire.hole(i);
        need(hole.offset % 8 == 0 && hole.bytes % 8 == 0 && hole.bytes != 0 &&
             hole.offset >= sizeof(rd::RawPayloadHeader) && hole.offset + hole.bytes <= wire.extent(),
             "writer emitted an unaligned/out-of-range hole");
    }
    // A dropped hole leaves an uncovered dead range.
    rejects_holes<model::Snapshot>(wire, table.substr(sizeof(Hole)),
        "wire validation failed: payload is not exactly covered by live blocks and holes");
    // A duplicated hole overlaps itself; a hole over the root overlaps a live block.
    rejects_holes<model::Snapshot>(wire, table + table.substr(0, sizeof(Hole)),
        "wire validation detected overlapping free-list ranges");
    {
        std::string over_root = table;
        const Hole root{std::uint32_t(wire.header().root_offset), 8};
        std::memcpy(over_root.data(), &root, sizeof(root));
        rejects_holes<model::Snapshot>(wire, over_root,
            "wire validation detected live/free overlap");
    }
    // Splitting a hole into two exact halves is an equally exact tiling.
    const auto first = wire.hole(0);
    if (first.bytes >= 16) {
        std::string split = table;
        const Hole a{first.offset, 8}, b{first.offset + 8, first.bytes - 8};
        std::memcpy(split.data(), &a, sizeof(a));
        split.append(reinterpret_cast<const char*>(&b), sizeof(b));
        auto owner = wire.owner();
        (void)rr::validate<model::Snapshot>(owner.data(), owner.get_size(), wire.header().root_offset,
            split.data(), split.size() / sizeof(Hole));
    }
    // An unreferenced live-looking block (allocated, never retired) is dead
    // space that V2 rejects at the coverage step: a mutation that drops a
    // vector's ownership leaves its block uncovered.
    auto source = xo::XBuffer::create<model::Bools>(65536, capacity());
    source.root<model::Bools>().values.reserve(16);
    auto bools = wire_of<model::Bools>(source);
    need(bools.hole_count() == 0, "bool fixture unexpectedly has holes");
    const auto head = std::size_t(bools.header().root_offset) + offsetof(model::Bools, values);
    auto descriptor = bools.at<rd::RawOwningDescriptor>(head);
    descriptor.data_delta = 0; descriptor.size = 0; descriptor.capacity = 0;
    bools.put(head, descriptor);
    rejects<model::Bools>(bools,
        "wire validation failed: payload is not exactly covered by live blocks and holes");
}
void bad_bool_and_terminator() {
    auto source = xo::XBuffer::create<model::Flag>(65536, capacity());
    source.root<model::Flag>().flag = true;
    source.root<model::Flag>().label = "valid";
    auto flag = wire_of<model::Flag>(source);
    // I1: "valid" is an inline string (head bytes 0..10, byte 11 = 0x80|len).
    const auto label = std::size_t(flag.header().root_offset) + offsetof(model::Flag, label);
    need(rd::raw_inline_string(flag.at<rd::RawOwningDescriptor>(label)) &&
         rd::raw_inline_string_size(flag.at<rd::RawOwningDescriptor>(label)) == 5,
         "short label fixture is not inline");
    {
        auto padding = flag; padding.put<char>(label + 7, 'p');
        rejects<model::Flag>(padding, "wire validation failed: inline XFixedString has nonzero padding");
        auto terminator = flag; terminator.put<char>(label + 5, 'X');
        rejects<model::Flag>(terminator, "wire validation failed: inline XFixedString has nonzero padding");
        auto length = flag; length.put<unsigned char>(label + 11, 0x80u | 11u);
        rejects<model::Flag>(length, "wire validation failed: inline XFixedString length exceeds 10");
    }
    flag.put<unsigned char>(std::size_t(flag.header().root_offset) + offsetof(model::Flag, flag), 2);
    rejects<model::Flag>(flag, "wire validation: invalid bool representation");
    auto wire = demo();
    const auto records_head = std::size_t(wire.header().root_offset) + offsetof(model::Snapshot, records);
    const auto name = wire.target(records_head) + offsetof(model::Record, name);
    const auto head = wire.at<rd::RawOwningDescriptor>(name);
    wire.put<char>(wire.target(name) + head.size, 'X');
    rejects<model::Snapshot>(wire, "wire validation failed: XFixedString missing terminator");
}
void dirty_capacity_tail_bool() {
    auto source = xo::XBuffer::create<model::Bools>(65536, capacity());
    auto& values = source.root<model::Bools>().values;
    values.reserve(8);
    values.push_back(true); values.push_back(false); values.push_back(true);
    need(values.capacity() > values.size(), "bool tail fixture has no spare capacity");
    auto wire = wire_of<model::Bools>(source);
    const auto head = std::size_t(wire.header().root_offset) + offsetof(model::Bools, values);
    const auto data = wire.target(head);
    const auto descriptor = wire.at<rd::RawOwningDescriptor>(head);
    for (std::size_t i = descriptor.size; i < descriptor.capacity; ++i)
        wire.put<unsigned char>(data + i, 0xA5);
    auto owner = wire.owner();
    const auto before = owner.copy_bytes();
    auto proof = admit<model::Bools>(owner, wire);
    const auto stats = std::move(proof).activate();
    need(stats.owning_heads == 1 && owner.copy_bytes() == before, "bool tail was initialized/read/rejected");
    auto* root = std::launder(reinterpret_cast<model::Bools*>(
        owner.data() + std::size_t(wire.header().root_offset)));
    need(root->values.size() == 3 && root->values[0] && !root->values[1] && root->values[2],
         "live bool values differ");
    wire.put<unsigned char>(data + 1, 0xA5);
    rejects<model::Bools>(wire, "wire validation: invalid bool representation");
}
void full_elements_before_ordering() {
    auto source = xo::XBuffer::create<model::Maps>(65536, capacity());
    source.root<model::Maps>().values.emplace(1u, true);
    source.root<model::Maps>().values.emplace(2u, false);
    auto wire = wire_of<model::Maps>(source);
    const auto head = std::size_t(wire.header().root_offset) + offsetof(model::Maps, values);
    const auto data = wire.target(head);
    using Pair = xo::XKeyValue<std::uint32_t, bool>;
    wire.put<std::uint32_t>(data + offsetof(Pair, first), 3u);
    wire.put<unsigned char>(data + sizeof(Pair) + offsetof(Pair, second), 2);
    rejects<model::Maps>(wire, "wire validation: invalid bool representation");
    wire.put<unsigned char>(data + sizeof(Pair) + offsetof(Pair, second), 0);
    rejects<model::Maps>(wire, "wire validation: map keys not strictly ordered");

    // Out-of-line keys (> 10 bytes): the first key's payload byte is changed.
    auto strings = xo::XBuffer::create<model::Sets>(65536, capacity());
    strings.root<model::Sets>().names.insert("a-out-of-line-key");
    strings.root<model::Sets>().names.insert("b-out-of-line-key");
    auto set = wire_of<model::Sets>(strings);
    const auto set_head = std::size_t(set.header().root_offset) + offsetof(model::Sets, names);
    need(!rd::raw_inline_string(set.at<rd::RawOwningDescriptor>(set.target(set_head))), "long key fixture is inline");
    set.put<char>(set.target(set.target(set_head)), 'z');
    rejects<model::Sets>(set, "wire validation: set keys not strictly ordered");
    // I1 inline keys: the first key's characters live in its head bytes 0..10.
    auto short_strings = xo::XBuffer::create<model::Sets>(65536, capacity());
    short_strings.root<model::Sets>().names.insert("a");
    short_strings.root<model::Sets>().names.insert("b");
    auto short_set = wire_of<model::Sets>(short_strings);
    const auto short_head = std::size_t(short_set.header().root_offset) + offsetof(model::Sets, names);
    const auto first_key = short_set.target(short_head);
    need(rd::raw_inline_string(short_set.at<rd::RawOwningDescriptor>(first_key)), "short key fixture is not inline");
    {
        auto owner = short_set.owner();
        (void)admit<model::Sets>(owner, short_set); // inline keys are admitted
    }
    short_set.put<char>(first_key, 'z');
    rejects<model::Sets>(short_set, "wire validation: set keys not strictly ordered");
}
void explicit_enum_policy() {
    // The old validator sees const key lvalues; rvalue-only operators are
    // inapplicable there and must remain inapplicable in the raw default.
    const auto rvalue_less_ignored = []<class E>() {
        const E a = E::low, b = E::high;
        rd::WireValidationContext context;
        return xo::raw_key_order<E>::less(reinterpret_cast<const char*>(&a),
                                          reinterpret_cast<const char*>(&b), context);
    };
    model::rvalue_enum_calls = 0;
    need(rvalue_less_ignored.template operator()<model::RvalueEnum>() &&
         rvalue_less_ignored.template operator()<model::RvalueSpaceshipEnum>() &&
         model::rvalue_enum_calls == 0, "raw enum comparator changed const-lvalue semantics");
    using Policy = xo::raw_key_order<model::PolicyEnum>;
    auto source = xo::XBuffer::create<model::Enums>(65536, capacity());
    source.root<model::Enums>().values.emplace(model::PolicyEnum::low, false);
    source.root<model::Enums>().values.emplace(model::PolicyEnum::high, true);
    auto wire = wire_of<model::Enums>(source);
    const auto head = std::size_t(wire.header().root_offset) + offsetof(model::Enums, values);
    const auto data = wire.target(head);
    using Pair = xo::XKeyValue<model::PolicyEnum, bool>;
    need(wire.at<std::uint32_t>(data + offsetof(Pair, first)) == 2,
         "enum writer fixture did not use custom reverse order");
    auto owner = wire.owner();
    const auto before = owner.copy_bytes();
    Policy::calls = 0; Policy::fail = false;
    auto proof = admit<model::Enums>(owner, wire);
    need(Policy::calls == 1, "explicit enum raw policy was bypassed");
    need(std::move(proof).activate().owning_heads == 1 && owner.copy_bytes() == before,
         "explicit enum policy activation changed bytes");

    Policy::calls = 0; Policy::fail = true;
    wire.put<unsigned char>(data + sizeof(Pair) + offsetof(Pair, second), 2);
    rejects<model::Enums>(wire, "wire validation: invalid bool representation");
    need(Policy::calls == 0, "enum policy ran before complete value graph admission");
    wire.put<unsigned char>(data + sizeof(Pair) + offsetof(Pair, second), 0);
    auto failed_owner = wire.owner();
    const auto failed_before = failed_owner.copy_bytes();
    bool caught = false;
    try { (void)admit<model::Enums>(failed_owner, wire); }
    catch (const std::runtime_error& e) {
        caught = std::string_view(e.what()) == "explicit enum raw policy failure";
    }
    need(caught && Policy::calls == 1 && failed_owner.copy_bytes() == failed_before,
         "raw enum comparison exception consumed/mutated input");
    Policy::fail = false;
    wire.put<std::uint32_t>(data + offsetof(Pair, first), 0);
    rejects<model::Enums>(wire, "wire validation: map keys not strictly ordered");
}
void paired_owned_priority_and_rejection_state() {
    alignas(8) std::array<char, 512> storage{};
    rd::WireValidationContext ctx{
        .base = storage.data(), .end = storage.data() + storage.size()
    };
    rd::register_owned_range(storage.data() + 200, 16, ctx, "owned fixture");
    rd::register_free_range(storage.data() + 80, 16, ctx, "free fixture");
    bool owned = false, free = false;
    try { rd::register_owned_range(storage.data() + 88, 120, ctx, "incoming"); }
    catch (const xo::XException& e) {
        owned = std::string_view(e.what()) == "wire validation detected overlapping dynamic ranges";
    }
    try { rd::register_free_range(storage.data() + 88, 120, ctx, "incoming"); }
    catch (const xo::XException& e) {
        free = std::string_view(e.what()) == "wire validation detected free-list overlap with live ranges";
    }
    need(owned && free && ctx.owned_bytes == 16 && ctx.free_bytes == 16 &&
         ctx.granules.member(false, 25) && !ctx.granules.member(false, 11) &&
         ctx.granules.member(true, 10) && !ctx.granules.member(true, 12),
         "early free hit obscured later owned hit or rejection committed index");
}
void fixed_arrays() {
    auto source = xo::XBuffer::create<model::FixedScalars>(65536, capacity());
    auto& root = source.root<model::FixedScalars>();
    root.values[0] = 7; root.values[1] = 9;
    root.flags[0] = true; root.flags[1] = false;
    const auto wire = wire_of<model::FixedScalars>(source);
    auto owner = wire.owner();
    const auto before = owner.copy_bytes();
    const auto structural = rd::verify_structural_wire_graph_raw<model::FixedScalars>(owner);
    need(structural.root_offset == wire.header().root_offset &&
         structural.used_bytes == owner.get_size(), "Core wrapper metadata differs");
    // V2: the Core wrapper uses the owner's free table as its hole table and
    // refuses an owner whose table is unknown (raw-copied, never installed).
    {
        xo::XBufferCore unknown(wire.payload(), wire.extent(), capacity());
        bool refused = false;
        try { (void)rd::verify_structural_wire_graph_raw<model::FixedScalars>(unknown); }
        catch (const xo::XException& e) { refused = std::string_view(e.what()) == "wire validation: free table incomplete"; }
        need(refused, "Core wrapper accepted an owner with an unknown free table");
    }
    auto proof = admit<model::FixedScalars>(owner, wire);
    const auto stats = std::move(proof).activate();
    const auto* actual=std::launder(reinterpret_cast<const model::FixedScalars*>(owner.data()+wire.header().root_offset));
    need(stats.owning_heads == 0 && before == owner.copy_bytes() && actual->values[0]==7 &&
         actual->values[1]==9 && actual->flags[0] && !actual->flags[1], "fixed-scalar array recovery differs");
}
void incomplete_canonical_policy(){
    using T=model::CanonicalValidateOnly;
    auto source=xo::XBuffer::create<T>(4096,capacity());source.root<T>().value=7;
    const auto wire=wire_of<T>(source);
    auto legacy=wire.owner();auto proof=admit<T>(legacy,wire);
    need(std::move(proof).activate().custom_objects==1,
         "legacy two-argument initial custom domain was changed");
    auto storage=rd::VirtualMemoryStorage::for_receive(wire.extent(),wire.extent());
    auto* origin=storage.canonical_byte_origin();
    for(std::size_t i=0;i<wire.extent();++i)origin[i]=std::byte(static_cast<unsigned char>(wire.payload()[i]));
    xo::wire_activation<T>::canonical_calls=0;bool caught=false;
    try{(void)rr::validate<T>(origin,wire.extent(),wire.extent(),
        wire.header().root_offset,wire.holes(),wire.hole_count());}
    catch(const xo::XException& e){caught=std::string_view(e.what())=="raw receive: custom canonical validation policy required";}
    need(caught&&xo::wire_activation<T>::canonical_calls==0&&
         std::memcmp(origin,wire.payload(),wire.extent())==0,
         "incomplete canonical policy issued an unactivatable Proof or changed raw bytes");
}
void null_upstream_rejected(){
    const auto wire=demo();auto owner=wire.owner();const auto before=owner.copy_bytes();
    bool rejected=false;
    try{(void)rd::verify_structural_wire_graph_raw<model::Snapshot>(owner,nullptr);}
    catch(const std::invalid_argument& e){rejected=std::string_view(e.what())=="validation scratch: null upstream";}
    need(rejected&&owner.copy_bytes()==before,"explicit null raw upstream was replaced or changed valid input");
}
void enum_representations(){
    using T=model::EnumRepresentations;
    auto source=xo::XBuffer::create<T>(16384,capacity());auto& r=source.root<T>();
    // Fixed enums: every value of the underlying type is valid, listed or not.
    r.fixed=static_cast<model::FixedEnum>(0xffffffffu);
    r.scoped=static_cast<model::ScopedEnum>(0x7fffffffu);
    r.values.push_back(static_cast<model::FixedEnum>(2));r.values.push_back(static_cast<model::FixedEnum>(0x80000000u));
    r.keys.emplace(static_cast<model::ScopedEnum>(1),11);r.keys.emplace(static_cast<model::ScopedEnum>(0xfffffffeu),22);
    const auto wire=wire_of<T>(source);const auto root=std::size_t(wire.header().root_offset);
    auto owner=wire.owner();auto proof=admit<T>(owner,wire);const auto before=owner.copy_bytes();
    (void)std::move(proof).activate();
    const auto* active=std::launder(reinterpret_cast<const T*>(owner.data()+root));
    need(before==owner.copy_bytes()&&static_cast<std::uint32_t>(active->fixed)==0xffffffffu&&
         static_cast<std::uint32_t>(active->scoped)==0x7fffffffu&&
         static_cast<std::uint32_t>(active->values.data()[1])==0x80000000u,
         "valid nonenumerator fixed enum value rejected or changed");
}
} // namespace
int main() {
    try {
        demo_activation_and_oom();
        metadata_error_order();
        hole_table_exact_coverage();
        bad_bool_and_terminator();
        dirty_capacity_tail_bool();
        full_elements_before_ordering();
        explicit_enum_policy();
        paired_owned_priority_and_rejection_state();
        fixed_arrays();
        incomplete_canonical_policy();
        null_upstream_rejected();
        enum_representations();
        std::cout << "receive raw: source tests completed\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
