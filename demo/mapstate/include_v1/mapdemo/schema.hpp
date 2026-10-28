// Map state demo: schema version 1, for the old-version client
// (mapdemo_client_v1). FROZEN: this is the release before ViewSnapshot
// gained scene_units, and stays as it was released.
//
// The record types are the frozen snapshot that A lists in its lineage
// (mapdemo/schema_v1.hpp, written by src/schema_freeze.cpp, with field lists
// and pins), used under the names the client sources expect; the constants
// below are those of that release. The mapdemo_client_v1 targets put this
// directory before include/, so `#include "mapdemo/schema.hpp"` in the
// common and client sources finds this file; everything else is shared.
#pragma once
#include <cstddef>
#include <cstdint>
#include "mapdemo/schema_v1.hpp"

#define MAPDEMO_SCHEMA_VERSION 1

namespace mapdemo {
using namespace XOffsetDatastructure;

using mapdemo_v1::Point;
using mapdemo_v1::Rect;
using mapdemo_v1::Behavior;
using mapdemo_v1::SceneData;
using mapdemo_v1::SiteData;
using mapdemo_v1::MapCatalog;
using mapdemo_v1::UnitState;
using mapdemo_v1::UnitStateBatch;
using mapdemo_v1::ViewRequest;
using mapdemo_v1::VisibleUnit;
using mapdemo_v1::ViewSnapshot;
using mapdemo_v1::UnitEvent;

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

static_assert(sizeof(mapdemo::ViewSnapshot) == 48);
