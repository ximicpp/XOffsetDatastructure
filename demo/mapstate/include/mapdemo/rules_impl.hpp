// Map state demo: rule bodies. OWNER: common-library thread.
// STUB: replace with the rules in MAP_DEMO_DESIGN section 3.
#pragma once
#include <cmath>
#include <cstdio>

namespace mapdemo::rules {

inline std::uint64_t hash(std::uint64_t seed, std::uint64_t id, std::uint64_t salt) {
    std::uint64_t z = seed ^ (id * 0x9E3779B97F4A7C15ull) ^ (salt * 0xC2B2AE3D27D4EB4Full);
    z += 0x9e3779b97f4a7c15ull;
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ull;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebull;
    return z ^ (z >> 31);
}

inline bool rect_is_legal(const Rect& r, const Rect& scene) {
    const bool finite = std::isfinite(r.min.x) && std::isfinite(r.min.y) &&
                        std::isfinite(r.max.x) && std::isfinite(r.max.y);
    return finite && r.min.x <= r.max.x && r.min.y <= r.max.y &&
           scene.min.x <= r.min.x && scene.min.y <= r.min.y &&
           r.max.x <= scene.max.x && r.max.y <= scene.max.y;
}

inline std::string short_name(std::uint32_t id) {
    char buf[16];
    const int n = std::snprintf(buf, sizeof buf, "U%05u", id);
    return std::string(buf, std::size_t(n));
}

inline std::string long_name(std::uint32_t id) {
    return short_name(id) + "-route-marker";   // STUB marker
}

inline void build_catalog(MapCatalog& catalog, std::uint64_t) {   // STUB
    auto& s = catalog.scenes.emplace_back();
    s.id = kSceneId;
    s.name = std::string_view("Demo Plains");
    s.bounds = scene_bounds();
}

template <class Unit>
void init_unit(Unit& u, std::uint32_t id, std::uint64_t seed) {   // STUB
    u.id = id;
    u.scene_id = kSceneId;
    u.pos = Point{float(hash(seed, id, 1) % 4096), float(hash(seed, id, 2) % 4096)};
    u.speed = 20.0f;
    u.hp = 100;
    u.behavior = Behavior::Patrol;
    u.next_waypoint = 1;
    u.name = std::string_view(short_name(id));
    for (int k = 0; k < 4; ++k) u.path.push_back(u.pos);
}

template <class Batch>
void init_batch(Batch& batch, std::uint32_t count, std::uint64_t seed) {
    batch.tick = 0;
    batch.units.reserve(count);
    for (std::uint32_t i = 0; i < count; ++i) init_unit(batch.units.emplace_back(), i, seed);
}

template <class Batch>
void step_batch(Batch& batch, std::uint64_t) {   // STUB: tick only
    batch.tick += 1;
}

} // namespace mapdemo::rules
