// Map state demo: deterministic generation and simulation rules.
// The SIGNATURES are a frozen interface (demo/mapstate/INTERFACES.md); the
// bodies are owned by the common-library thread. Until then they are minimal
// stubs so servers and client can be built and wired end to end.
// Every template must work on both mapdemo::UnitStateBatch (XOffset) and
// mapdemo::stdm::UnitStateBatch (std): use only size(), operator[],
// begin(), insert(pos, value), erase(pos), push_back, reserve, emplace_back,
// and assignment of names from std::string_view.
#pragma once
#include <cstdint>
#include <string>
#include <string_view>
#include "mapdemo/schema.hpp"

namespace mapdemo::rules {

// splitmix64-based hash of (seed, id, salt).
std::uint64_t hash(std::uint64_t seed, std::uint64_t id, std::uint64_t salt);

// Half-open containment: min inclusive, max exclusive.
inline bool contains(const Rect& r, Point p) {
    return r.min.x <= p.x && p.x < r.max.x && r.min.y <= p.y && p.y < r.max.y;
}
// Finite, min <= max on both axes, and inside `scene` (zero area is legal).
bool rect_is_legal(const Rect& r, const Rect& scene);

std::string short_name(std::uint32_t id);   // "U%05u": <= 10 bytes, stored inline
std::string long_name(std::uint32_t id);    // short name + "-" + marker, >= 16 bytes, arena block

// Fills a MapCatalog root: 1 scene (id kSceneId, bounds scene_bounds()),
// kSiteCount sites. Deterministic from seed.
void build_catalog(MapCatalog& catalog, std::uint64_t seed);

// Initial unit: path point 0 is the start, next_waypoint = 1, 4 integer
// waypoints in [0, kMaxCoord], hp 100, Patrol, short name, speed 20..80.
template <class Unit>
void init_unit(Unit& u, std::uint32_t id, std::uint64_t seed);

// tick = 0, units 0..count-1 (reserve(count) first).
template <class Batch>
void init_batch(Batch& batch, std::uint32_t count, std::uint64_t seed);

// One simulation step: move every unit (array order), then events when
// (tick + 1) % kEventPeriodTicks == 0 for id % kEventIdModulo == 0, then ++tick.
template <class Batch>
void step_batch(Batch& batch, std::uint64_t seed);

} // namespace mapdemo::rules

#include "mapdemo/rules_impl.hpp"
