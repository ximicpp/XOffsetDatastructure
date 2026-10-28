// Map state demo: the wire schema shared by server A, server B and the client.
// FROZEN INTERFACE (design doc demo/mapstate/INTERFACES.md). Changing a type
// here changes every wire; coordinate with all demo threads first.
#pragma once
#include <cstddef>
#include <cstdint>
#include "xoffsetdatastructure.hpp"

namespace mapdemo {
using namespace XOffsetDatastructure;

struct Point { float x{}; float y{}; };
struct Rect { Point min; Point max; };   // half-open: min inclusive, max exclusive
enum class Behavior : std::uint32_t { Patrol = 0, Hold = 1, Alert = 2 };

// Container heads (XString, XVector) are 12 bytes aligned to 4 (wire format
// 2.1). Records without an 8-byte member are therefore 4-aligned, and every
// record below is laid out without implicit padding (checked at the end of
// this file); a reserved field fills the gap where one would remain.
struct SceneData {
    std::uint32_t id{};
    XString name;
    Rect bounds;
};

struct SiteData {
    std::uint32_t id{};
    std::uint32_t scene_id{};
    Point pos;
    XString name;
};

struct MapCatalog {
    XVector<SceneData> scenes;
    XVector<SiteData> sites;
};

struct UnitState {
    std::uint32_t id{};
    std::uint32_t scene_id{};
    Point pos;
    float speed{};                  // scene units per second
    std::int32_t hp{};
    Behavior behavior{};
    std::uint32_t next_waypoint{};
    XString name;
    XVector<Point> path;
};

struct UnitStateBatch {
    std::uint64_t tick{};
    std::uint32_t reserved0{};      // 0; fills the 8-byte record alignment
    XVector<UnitState> units;
};

struct ViewRequest {
    std::uint64_t request_id{};
    std::uint32_t scene_id{};
    std::uint32_t reserved0{};      // 0
    Rect rect;
};

struct VisibleUnit {
    std::uint32_t id{};
    std::int32_t hp{};
    Point pos;
    Behavior behavior{};
    XString name;
};

struct ViewSnapshot {
    std::uint64_t request_id{};
    std::uint64_t tick{};
    std::uint32_t scene_id{};
    Rect rect;
    XVector<VisibleUnit> units;
};

// A unit right after one of its events (rules: every kEventPeriodTicks ticks,
// the units with id % kEventIdModulo == 0), pushed to clients that subscribe
// to the event stream. Small messages (about 120-200 bytes): the demo's
// small-message traffic for the zstd dictionary and batching options
// (codec.hpp). Added 10-28; the four types above and Hello are unchanged.
struct UnitEvent {
    std::uint64_t tick{};
    std::uint32_t unit_id{};
    std::int32_t hp{};
    Point pos;
    Behavior behavior{};
    std::uint32_t next_waypoint{};
    XString name;
    XVector<Point> path;
};

// ---- fixed defaults (requirements section 5) --------------------------------
inline constexpr std::uint32_t kSceneId = 1;
inline constexpr float kSceneSize = 4096.0f;      // scene bounds [0, 4096) x [0, 4096)
inline constexpr float kMaxCoord = 4095.0f;       // waypoints and positions stay in [0, 4095]
inline constexpr std::uint32_t kSiteCount = 16;
inline constexpr std::uint32_t kDefaultUnits = 10000;
inline constexpr std::uint64_t kDefaultSeed = 1;
inline constexpr float kStepSeconds = 0.05f;      // 20 Hz target
inline constexpr std::uint32_t kEventPeriodTicks = 20;
inline constexpr std::uint32_t kEventIdModulo = 100;
inline constexpr float kViewWidth = 1024.0f;
inline constexpr float kViewHeight = 768.0f;
inline constexpr double kViewRequestHz = 10.0;

inline constexpr std::size_t kCapBatch = std::size_t(64) << 20;
inline constexpr std::size_t kCapSnapshot = std::size_t(16) << 20;
inline constexpr std::size_t kCapCatalog = std::size_t(1) << 20;
inline constexpr std::size_t kCapViewRequest = std::size_t(4) << 10;
inline constexpr std::size_t kCapEvent = std::size_t(4) << 10;           // one UnitEvent wire
inline constexpr std::size_t kMaxEventsPerFrame = 128;                   // A splits larger event ticks

inline Rect scene_bounds() { return Rect{{0.0f, 0.0f}, {kSceneSize, kSceneSize}}; }
// Legal, non-empty-area rect that is empty by construction (positions <= kMaxCoord).
inline Rect empty_probe_rect() { return Rect{{0.0f, 4095.5f}, {kSceneSize, kSceneSize}}; }

// Receive/build limits. Every XBuffer of a type uses its type's cap; growth is
// automatic up to the cap (B grows the received batch in place).
inline XBuffer::MaximumCapacity cap_batch() {
    return XBuffer::maximum_capacity(kCapBatch).with_growth(XGrowPolicy::Automatic);
}
inline XBuffer::MaximumCapacity cap_snapshot() {
    return XBuffer::maximum_capacity(kCapSnapshot).with_growth(XGrowPolicy::Automatic);
}
inline XBuffer::MaximumCapacity cap_catalog() {
    return XBuffer::maximum_capacity(kCapCatalog).with_growth(XGrowPolicy::Automatic);
}
inline XBuffer::MaximumCapacity cap_view_request() {
    return XBuffer::maximum_capacity(kCapViewRequest).with_growth(XGrowPolicy::Automatic);
}
inline XBuffer::MaximumCapacity cap_event() {
    return XBuffer::maximum_capacity(kCapEvent).with_growth(XGrowPolicy::Automatic);
}

} // namespace mapdemo

// Field lists, in declaration order. The C++20/23 client builds (field-list
// backend: MinGW, MSVC, Android NDK, Apple clang) read the schema from these
// lines; the C++26 build checks them against reflection, so both backends
// describe the same records and compute the same wire_schema_hash_v (the
// Hello handshake compares it). XOFFSET_RECORD also declares each type an
// ordinary record (no bases, unions, bit-fields or [[no_unique_address]]), so
// the library may move record elements with one memmove and rebuild only
// their inline heads.
XOFFSET_RECORD(mapdemo::Point, x, y)
XOFFSET_RECORD(mapdemo::Rect, min, max)
XOFFSET_RECORD(mapdemo::SceneData, id, name, bounds)
XOFFSET_RECORD(mapdemo::SiteData, id, scene_id, pos, name)
XOFFSET_RECORD(mapdemo::MapCatalog, scenes, sites)
XOFFSET_RECORD(mapdemo::UnitState, id, scene_id, pos, speed, hp, behavior, next_waypoint, name, path)
XOFFSET_RECORD(mapdemo::UnitStateBatch, tick, reserved0, units)
XOFFSET_RECORD(mapdemo::ViewRequest, request_id, scene_id, reserved0, rect)
XOFFSET_RECORD(mapdemo::VisibleUnit, id, hp, pos, behavior, name)
XOFFSET_RECORD(mapdemo::ViewSnapshot, request_id, tick, scene_id, rect, units)
XOFFSET_RECORD(mapdemo::UnitEvent, tick, unit_id, hp, pos, behavior, next_waypoint, name, path)

XOFFSET_REGISTER_SCHEMA_NAME(mapdemo::MapCatalog, "mapdemo.MapCatalog.v1")
XOFFSET_REGISTER_SCHEMA_NAME(mapdemo::UnitStateBatch, "mapdemo.UnitStateBatch.v1")
XOFFSET_REGISTER_SCHEMA_NAME(mapdemo::ViewRequest, "mapdemo.ViewRequest.v1")
XOFFSET_REGISTER_SCHEMA_NAME(mapdemo::ViewSnapshot, "mapdemo.ViewSnapshot.v1")
XOFFSET_REGISTER_SCHEMA_NAME(mapdemo::UnitEvent, "mapdemo.UnitEvent.v1")

XOFFSET_REQUIRE_NO_PADDING(mapdemo::SceneData);
XOFFSET_REQUIRE_NO_PADDING(mapdemo::SiteData);
XOFFSET_REQUIRE_NO_PADDING(mapdemo::MapCatalog);
XOFFSET_REQUIRE_NO_PADDING(mapdemo::UnitState);
XOFFSET_REQUIRE_NO_PADDING(mapdemo::UnitStateBatch);
XOFFSET_REQUIRE_NO_PADDING(mapdemo::ViewRequest);
XOFFSET_REQUIRE_NO_PADDING(mapdemo::VisibleUnit);
XOFFSET_REQUIRE_NO_PADDING(mapdemo::ViewSnapshot);
XOFFSET_REQUIRE_NO_PADDING(mapdemo::UnitEvent);
static_assert(sizeof(mapdemo::UnitState) == 56);
static_assert(sizeof(mapdemo::VisibleUnit) == 32);
static_assert(sizeof(mapdemo::ViewSnapshot) == 48);
static_assert(sizeof(mapdemo::UnitStateBatch) == 24);
static_assert(sizeof(mapdemo::ViewRequest) == 32);
static_assert(sizeof(mapdemo::UnitEvent) == 56);
static_assert(XOffsetDatastructure::detail::shift_relocate_supported<mapdemo::UnitState>());
