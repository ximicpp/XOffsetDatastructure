// Device mesh demo: what a device can do with an object of one of the six
// data types, without knowing which type it is.
// FROZEN INTERFACE (demo/mesh/INTERFACES.md). Bodies live in src/types/,
// owned by the types thread; the hub, the device program and the self-test
// call only what is declared here.
//
// Every function is deterministic in its arguments and gives the same
// result on every device (the 64-bit little-endian targets of
// INTERFACES.md section 1): state 0 and each mutation come from a
// splitmix64 stream of (type, seed, step), use integer arithmetic and exact
// float/double assignments only (the build sets -ffp-contract=off), and do
// the same buffer operations in every build, with exceptions or without
// (a noexc build reserves no more and no less), so the data part a device
// sends is byte-identical to the hub's replay of the same job.
//
// Nothing here throws, in either build: failures come back as an Error.
#pragma once
#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include "xmesh/result.hpp"
#include "xmesh/schema.hpp"

namespace xmesh {

// HopMeta::type. The values are wire values: never renumber.
enum class TypeId : std::uint16_t {
    Document = 1,
    Order = 2,
    SensorSeries = 3,
    KvConfig = 4,
    SceneTree = 5,
    ChatRoom = 6,
};
inline constexpr std::array<TypeId, 6> kAllTypes{TypeId::Document, TypeId::Order, TypeId::SensorSeries,
                                                 TypeId::KvConfig, TypeId::SceneTree, TypeId::ChatRoom};

// HopMeta::scale: how big state 0 is. Targets for the data part of state 0
// (save_verified): Small 1-4 KiB, a typical message; Medium about 64 KiB;
// Large about 1 MiB (SensorSeries about 4 MiB). The self-test prints the
// real sizes.
enum class Scale : std::uint8_t { Small = 0, Medium = 1, Large = 2 };

// HopMeta::save_mode: how a device writes the data part it sends on.
// Verified: save_verified (the arena as it is). Compact: save_compact
// (rebuilt; DEPLOYMENT.md 4.2, rebuild before forwarding).
enum class SaveMode : std::uint8_t { Verified = 0, Compact = 1 };

// HopMeta::evil: a bad data part route[0] sends instead of state 0, to show
// that the first receiver refuses it (protocol.hpp expected_verdict).
enum class EvilCase : std::uint8_t {
    None = 0,
    Truncated = 1,     // the wire cut short
    BadOffset = 2,     // a container's offset moved outside the arena
    SchemaHash = 3,    // one byte of the schema fingerprint flipped
    WrongType = 4,     // state 0 of another type, sent as this type
    TooLarge = 5,      // a valid wire longer than data_limit(type, scale)
    StaleDigest = 6,   // a valid wire with one value changed after the content digest was taken
    ModelDrift = 7,    // a valid wire with one value changed, digest retaken: only the model check sees it
};
inline constexpr std::array<EvilCase, 7> kAllEvilCases{EvilCase::Truncated, EvilCase::BadOffset,
                                                       EvilCase::SchemaHash, EvilCase::WrongType,
                                                       EvilCase::TooLarge, EvilCase::StaleDigest,
                                                       EvilCase::ModelDrift};

// Names: "Document", "small", "verified", "truncated", ...; parse_* also
// accepts any letter case. to_* checks a raw wire value.
const char* type_name(TypeId type) noexcept;
const char* scale_name(Scale scale) noexcept;
const char* save_mode_name(SaveMode mode) noexcept;
const char* evil_name(EvilCase evil) noexcept;
std::optional<TypeId> parse_type(std::string_view name) noexcept;
std::optional<Scale> parse_scale(std::string_view name) noexcept;
std::optional<SaveMode> parse_save_mode(std::string_view name) noexcept;
std::optional<EvilCase> parse_evil(std::string_view name) noexcept;
std::optional<TypeId> to_type(std::uint16_t raw) noexcept;
std::optional<Scale> to_scale(std::uint8_t raw) noexcept;
std::optional<SaveMode> to_save_mode(std::uint8_t raw) noexcept;
std::optional<EvilCase> to_evil(std::uint8_t raw) noexcept;

// The longest data part a receiver accepts before loading it, and the
// maximum_capacity it loads it with. Both cover the object after
// kMaxRoute mutations; data_limit(...) <= protocol.hpp kMaxDataBytes.
std::uint32_t data_limit(TypeId type, Scale scale) noexcept;
std::uint32_t receive_cap(TypeId type, Scale scale) noexcept;

// Longest route of a job (HopMeta::route), route[0] included.
inline constexpr std::uint32_t kMaxRoute = 64;

// One route position: its index in the route and the device's name. The
// mutation a device applies, and the HopMark it appends, depend on both.
struct Step {
    std::uint32_t hop = 0;
    std::string device;
};

// An object of one of the six types in its own XBuffer.
class Object {
public:
    Object() = default;
    Object(TypeId type, XBuffer buffer) noexcept : type_(type), buffer_(std::move(buffer)) {}

    TypeId type() const noexcept { return type_; }
    XBuffer& buffer() noexcept { return buffer_; }
    const XBuffer& buffer() const noexcept { return buffer_; }
    // The root; T must be the root type of type().
    template <class T>
    T& root() { return buffer_.root<T>(); }

private:
    TypeId type_ = TypeId::Document;
    XBuffer buffer_;
};

// State 0 of a job: what route[0] builds from the seed. Its trail is
// {HopMark{0, creator}}.
Result<Object> make_object(TypeId type, std::uint64_t seed, Scale scale, std::string_view creator);

// A received data part: data_limit checked, then try_load_verified of the
// type's root with receive_cap(type, scale).
Result<Object> receive_object(TypeId type, Scale scale, std::string_view wire);

// Value digest: equal for equal values on every device, whatever the arena
// layout (so save_verified and save_compact of one state digest the same).
// FNV-1a 64 (protocol.hpp fnv1a64) over a canonical walk of the fields.
std::uint64_t content_digest(Object& object);

// Mutation step.hop (>= 1) in place, as route[step.hop] applies it after
// receiving: edits, inserts and erases chosen by (type, seed, step), the
// type's counters bumped, and HopMark{step.hop, step.device} appended to
// the trail. Grows the buffer as needed.
Status mutate(Object& object, std::uint64_t seed, const Step& step);

// The reference model: state 0 and mutations 1..upto of `route` rebuilt in
// plain std:: types (no XOffset) and compared with `object` field by field.
// The error names the first field that differs, e.g.
// "Order.lines[2].qty: 3 != 4".
Status check_against_model(Object& object, std::uint64_t seed, Scale scale, std::span<const Step> route,
                           std::uint32_t upto);

// The data part to send on.
Result<std::string> save(Object& object, SaveMode mode);

// One line for logs and the dashboard, e.g.
// "Order 3 lines, total 12990, trail linux-x64 > windows-msvc > ios-sim".
std::string summary(Object& object);

// What route[k] sends, for each k < route.size() - 1, when the whole job
// runs as the devices run it (make, save, receive, mutate, save, ...).
// The hub replays every job with this and compares the reports with it;
// the self-test writes it into the corpus.
struct HopExpect {
    std::uint64_t content_digest = 0;   // of the state route[k] sends
    std::uint64_t wire_digest = 0;      // protocol.hpp wire_digest of the data part
    std::uint32_t bytes = 0;            // data part length
};
Result<std::vector<HopExpect>> replay(TypeId type, std::uint64_t seed, Scale scale, SaveMode mode,
                                      std::span<const Step> route);

// An evil job's hop 0: the bad data part and the content digest route[0]
// claims for it in HopMeta (StaleDigest: the digest of the unchanged state).
struct EvilPart {
    std::string wire;
    std::uint64_t claimed_content_digest = 0;
};
Result<EvilPart> make_evil(TypeId type, std::uint64_t seed, Scale scale, SaveMode mode, std::string_view creator,
                           EvilCase evil);

} // namespace xmesh
