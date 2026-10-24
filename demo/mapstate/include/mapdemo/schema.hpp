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

struct alignas(8) SceneData {
    std::uint32_t id{};
    std::uint32_t reserved0{};
    XString name;
    Rect bounds;
};

struct alignas(8) SiteData {
    std::uint32_t id{};
    std::uint32_t scene_id{};
    Point pos;
    XString name;
};

struct alignas(8) MapCatalog {
    XVector<SceneData> scenes;
    XVector<SiteData> sites;
};

struct alignas(8) UnitState {
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

struct alignas(8) UnitStateBatch {
    std::uint64_t tick{};
    XVector<UnitState> units;
};

struct alignas(8) ViewRequest {
    std::uint64_t request_id{};
    std::uint32_t scene_id{};
    std::uint32_t reserved0{};
    Rect rect;
};

struct alignas(8) VisibleUnit {
    std::uint32_t id{};
    std::int32_t hp{};
    Point pos;
    Behavior behavior{};
    std::uint32_t reserved0{};
    XString name;
};

struct alignas(8) ViewSnapshot {
    std::uint64_t request_id{};
    std::uint64_t tick{};
    std::uint32_t scene_id{};
    std::uint32_t reserved0{};
    Rect rect;
    XVector<VisibleUnit> units;
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

} // namespace mapdemo

// Ordinary records (docs/USER_GUIDE.md section 3.1): every type above has
// plain non-static members only (no bases, unions, bit-fields or
// [[no_unique_address]]), so the library may move record elements with one
// memmove and rebuild only their inline heads. Wire bytes are unchanged.
namespace XOffsetDatastructure {
template<> struct wire_record_layout<mapdemo::Point> { static constexpr bool ordinary_members = true; };
template<> struct wire_record_layout<mapdemo::Rect> { static constexpr bool ordinary_members = true; };
template<> struct wire_record_layout<mapdemo::SceneData> { static constexpr bool ordinary_members = true; };
template<> struct wire_record_layout<mapdemo::SiteData> { static constexpr bool ordinary_members = true; };
template<> struct wire_record_layout<mapdemo::MapCatalog> { static constexpr bool ordinary_members = true; };
template<> struct wire_record_layout<mapdemo::UnitState> { static constexpr bool ordinary_members = true; };
template<> struct wire_record_layout<mapdemo::UnitStateBatch> { static constexpr bool ordinary_members = true; };
template<> struct wire_record_layout<mapdemo::ViewRequest> { static constexpr bool ordinary_members = true; };
template<> struct wire_record_layout<mapdemo::VisibleUnit> { static constexpr bool ordinary_members = true; };
template<> struct wire_record_layout<mapdemo::ViewSnapshot> { static constexpr bool ordinary_members = true; };
} // namespace XOffsetDatastructure

XOFFSET_REGISTER_SCHEMA_NAME(mapdemo::MapCatalog, "mapdemo.MapCatalog.v1")
XOFFSET_REGISTER_SCHEMA_NAME(mapdemo::UnitStateBatch, "mapdemo.UnitStateBatch.v1")
XOFFSET_REGISTER_SCHEMA_NAME(mapdemo::ViewRequest, "mapdemo.ViewRequest.v1")
XOFFSET_REGISTER_SCHEMA_NAME(mapdemo::ViewSnapshot, "mapdemo.ViewSnapshot.v1")

XOFFSET_REQUIRE_NO_PADDING(mapdemo::SceneData);
XOFFSET_REQUIRE_NO_PADDING(mapdemo::SiteData);
XOFFSET_REQUIRE_NO_PADDING(mapdemo::MapCatalog);
XOFFSET_REQUIRE_NO_PADDING(mapdemo::UnitState);
XOFFSET_REQUIRE_NO_PADDING(mapdemo::UnitStateBatch);
XOFFSET_REQUIRE_NO_PADDING(mapdemo::ViewRequest);
XOFFSET_REQUIRE_NO_PADDING(mapdemo::VisibleUnit);
XOFFSET_REQUIRE_NO_PADDING(mapdemo::ViewSnapshot);
static_assert(sizeof(mapdemo::UnitState) == 64);
static_assert(sizeof(mapdemo::VisibleUnit) == 40);
static_assert(sizeof(mapdemo::ViewRequest) == 32);
