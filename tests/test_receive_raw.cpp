// Source-only tests; author has not compiled or executed this target.
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
struct EnumRepresentations {
    Nonfixed plain{};Negative mixed{};Zero zero{};EmptyEnum empty{};
    FixedEnum fixed{};ScopedEnum scoped{};
    xo::XVector<Nonfixed> values;
    xo::XMap<Nonfixed,std::uint32_t> keys;
};
} // namespace model

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
XOFFSET_REGISTER_SCHEMA_NAME(model::EnumRepresentations,"four-path.raw.enum-representations.v2")

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
static_assert(!rd::receive_raw_supported<model::Bits>());
static_assert(!rd::receive_raw_supported<model::Overlap>());
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
static_assert(rd::raw_enum_bounds<model::Nonfixed>().low==0&&rd::raw_enum_bounds<model::Nonfixed>().high==7);
static_assert(rd::raw_enum_bounds<model::Negative>().low==-4&&rd::raw_enum_bounds<model::Negative>().high==3);
static_assert(rd::raw_enum_bounds<model::Zero>().high==1&&rd::raw_enum_bounds<model::EmptyEnum>().high==1);
static_assert(rd::raw_enum_bounds<model::Wide>().low==(std::numeric_limits<std::underlying_type_t<model::Wide>>::min)());
static_assert(rd::raw_enum_bounds<model::UnsignedWide>().high==(std::numeric_limits<std::underlying_type_t<model::UnsignedWide>>::max)());
static_assert(rd::raw_needs_validation<model::Nonfixed>()&&!rd::raw_needs_validation<model::FixedEnum>());
static_assert(rd::raw_needs_validation<model::EnumRepresentations>());
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
struct Wire {
    std::string bytes;
    xo::XWireHeaderV1 header() const {
        return rd::raw_wire_pod<xo::XWireHeaderV1>(bytes.data());
    }
    const char* payload() const { return bytes.data() + sizeof(xo::XWireHeaderV1); }
    char* payload() { return bytes.data() + sizeof(xo::XWireHeaderV1); }
    std::size_t extent() const { return bytes.size() - sizeof(xo::XWireHeaderV1); }
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
        const auto value = std::int64_t(head + 4) + d.data_delta;
        need(value >= 0 && std::uint64_t(value) < extent(), "test relative geometry");
        return std::size_t(value);
    }
    xo::XBufferCore owner() const {
        // This component fixture deliberately uses the existing copied owner.
        // It tests RAW verification/activation, not the token's no-copy seam.
        return xo::XBufferCore(payload(), extent(), capacity());
    }
};
template<class T> Wire wire_of(xo::XBuffer& source) {
    return {source.save_verified<T>()};
}
template<class T>
auto admit(xo::XBufferCore& owner, const Wire& wire,
           std::pmr::memory_resource* resource = std::pmr::new_delete_resource()) {
    const auto header = wire.header();
    return rr::validate<T>(owner.data(), owner.get_size(),
        header.root_offset, header.allocator_offset, resource);
}
Wire demo() {
    auto source = xo::XBuffer::create<model::Snapshot>(65536, capacity());
    auto& snapshot = source.root<model::Snapshot>();
    snapshot.epoch = 123;
    snapshot.records.resize(2);
    for (std::uint32_t i = 0; i < 2; ++i) {
        auto& r = snapshot.records[i];
        r.id = 10 + i; r.revision = i; r.score = -std::int64_t{100} + i;
        r.name = "original";
        r.samples.push_back(i); r.samples.push_back(i + 20);
        r.payload.push_back(std::uint8_t(i + 4));
        r.attrs.resize(1);
        r.attrs[0].key = "attrs";
        r.attrs[0].value.push_back(std::uint8_t(i + 1));
    }
    // Ensure a real free-list range and a second scratch index exist.
    snapshot.records[0].name = "a substantially longer replacement name";
    return wire_of<model::Snapshot>(source);
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
    try { (void)admit<T>(owner, wire); }
    catch (const xo::XException& error) {
        caught = std::string_view(error.what()) == expected;
    }
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
    for (long point = 0; point < 2; ++point) {
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
    const auto expect = [&](rd::RawPayloadHeader altered, std::uint64_t wire_root,
                            std::uint64_t wire_allocator, const char* message) {
        auto owner = wire.owner();
        std::memcpy(owner.data(), &altered, sizeof(altered));
        const auto before = owner.copy_bytes();
        CountingResource resource;
        bool caught = false;
        try {
            (void)rr::validate<model::Snapshot>(owner.data(), owner.get_size(),
                wire_root, wire_allocator, &resource);
        } catch (const xo::XException& error) {
            caught = std::string_view(error.what()) == message;
        }
        need(caught, "raw metadata error order differs");
        need(resource.attempts == 0 && owner.copy_bytes() == before,
             "metadata rejection allocated scratch or wrote payload");
    };
    auto bad = arena;
    bad.allocator_offset = 0;
    expect(bad, wire_header.root_offset + 8, wire_header.allocator_offset,
           "load_verified<T>(): root offset mismatch");
    expect(bad, wire_header.root_offset, wire_header.allocator_offset,
           "load_verified<T>(): allocator state not found");
    bad = arena;
    bad.root_offset = wire.extent() + 8;
    expect(bad, wire_header.root_offset + 8, wire_header.allocator_offset,
           "load_verified<T>(): root not found");
    bad = arena;
    bad.flags = 1;
    expect(bad, wire_header.root_offset + 8, wire_header.allocator_offset,
           "load_verified<T>(): root offset mismatch");
    expect(bad, wire_header.root_offset, wire_header.allocator_offset + 8,
           "load_verified<T>(): allocator offset mismatch");
    expect(bad, wire_header.root_offset, wire_header.allocator_offset,
           "wire validation: unsupported arena header flags");
    bad = arena;
    bad.used_end -= 8;
    expect(bad, wire_header.root_offset + 8, wire_header.allocator_offset,
           "load_verified<T>(): non-normalized payload extent");
}
void bad_bool_and_terminator() {
    auto source = xo::XBuffer::create<model::Flag>(65536, capacity());
    source.root<model::Flag>().flag = true;
    source.root<model::Flag>().label = "valid";
    auto flag = wire_of<model::Flag>(source);
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

    auto strings = xo::XBuffer::create<model::Sets>(65536, capacity());
    strings.root<model::Sets>().names.insert("a");
    strings.root<model::Sets>().names.insert("b");
    auto set = wire_of<model::Sets>(strings);
    const auto set_head = std::size_t(set.header().root_offset) + offsetof(model::Sets, names);
    set.put<char>(set.target(set.target(set_head)), 'z');
    rejects<model::Sets>(set, "wire validation: set keys not strictly ordered");
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
    need(owned && free && ctx.owned_ranges.size() == 1 && ctx.free_ranges.size() == 1,
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
         structural.allocator_offset == wire.header().allocator_offset &&
         structural.used_bytes == owner.get_size(), "Core wrapper metadata differs");
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
        wire.header().root_offset,wire.header().allocator_offset);}
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
    // Legal values absent from the enumerator list MUST remain accepted.
    r.plain=static_cast<model::Nonfixed>(3);r.mixed=static_cast<model::Negative>(3);
    r.fixed=static_cast<model::FixedEnum>(0xffffffffu);
    r.scoped=static_cast<model::ScopedEnum>(0x7fffffffu);
    r.values.push_back(static_cast<model::Nonfixed>(2));r.values.push_back(static_cast<model::Nonfixed>(5));
    r.keys.emplace(static_cast<model::Nonfixed>(1),11);r.keys.emplace(static_cast<model::Nonfixed>(2),22);
    const auto wire=wire_of<T>(source);const auto root=std::size_t(wire.header().root_offset);
    {auto owner=wire.owner();auto proof=admit<T>(owner,wire);const auto before=owner.copy_bytes();
     (void)std::move(proof).activate();
     const auto* active=std::launder(reinterpret_cast<const T*>(owner.data()+root));
     need(before==owner.copy_bytes()&&static_cast<std::underlying_type_t<model::Nonfixed>>(active->plain)==3&&
          static_cast<std::uint32_t>(active->fixed)==0xffffffffu&&
          static_cast<std::uint32_t>(active->scoped)==0x7fffffffu,"valid nonenumerator/fixed enum value rejected or changed");}
    {auto valid=wire;valid.put<std::underlying_type_t<model::Negative>>(root+offsetof(T,mixed),-4);
     auto owner=valid.owner();auto proof=admit<T>(owner,valid);(void)std::move(proof).activate();
     need(static_cast<std::underlying_type_t<model::Negative>>(
          std::launder(reinterpret_cast<const T*>(owner.data()+root))->mixed)==-4,
          "nonfixed minimum hypothetical-width value rejected");}
    {auto bad=wire;bad.put<std::underlying_type_t<model::Nonfixed>>(root+offsetof(T,plain),0x7fffffff);
     rejects<T>(bad,"wire validation: invalid enum representation");}
    for(auto invalid:{-5,4}){auto bad=wire;bad.put<std::underlying_type_t<model::Negative>>(root+offsetof(T,mixed),invalid);
     rejects<T>(bad,"wire validation: invalid enum representation");}
    {auto valid=wire;valid.put<std::underlying_type_t<model::Zero>>(root+offsetof(T,zero),1);
     valid.put<std::underlying_type_t<model::EmptyEnum>>(root+offsetof(T,empty),1);
     auto owner=valid.owner();auto proof=admit<T>(owner,valid);(void)std::move(proof).activate();
     const auto* active=std::launder(reinterpret_cast<const T*>(owner.data()+root));
     need(static_cast<std::underlying_type_t<model::Zero>>(active->zero)==1&&
          static_cast<std::underlying_type_t<model::EmptyEnum>>(active->empty)==1,
          "fixed SDK one-bit empty/all-zero enum ABI value rejected");}
    {auto bad=wire;bad.put<std::underlying_type_t<model::Zero>>(root+offsetof(T,zero),2);
     rejects<T>(bad,"wire validation: invalid enum representation");}
    {auto bad=wire;bad.put<std::underlying_type_t<model::EmptyEnum>>(root+offsetof(T,empty),2);
     rejects<T>(bad,"wire validation: invalid enum representation");}
    {auto bad=wire;bad.put<std::underlying_type_t<model::Nonfixed>>(bad.target(root+offsetof(T,values)),0x7fffffff);
     rejects<T>(bad,"wire validation: invalid enum representation");}
    {auto bad=wire;using Pair=xo::XKeyValue<model::Nonfixed,std::uint32_t>;
     bad.put<std::underlying_type_t<model::Nonfixed>>(bad.target(root+offsetof(T,keys))+offsetof(Pair,first),0x7fffffff);
     rejects<T>(bad,"wire validation: invalid enum representation");}
}
} // namespace
int main() {
    try {
        demo_activation_and_oom();
        metadata_error_order();
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
