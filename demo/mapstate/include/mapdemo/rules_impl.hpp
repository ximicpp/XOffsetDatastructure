// Map state demo: rule bodies (MAP_DEMO_DESIGN section 3). OWNER: common-library thread.
//
// Everything depends only on (seed, unit id, tick, event number). The same
// template code runs on the XOffset batch (B) and on the std reference model
// (A --check, selftest); the demo is built with -ffp-contract=off and uses
// float arithmetic only, so both models stay bitwise identical.
#pragma once
#include <cmath>
#include <cstddef>
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

// "U00100-route-marker-01": always >= 16 bytes, so it lives in an arena block.
inline std::string long_name(std::uint32_t id) {
    char buf[48];
    const int n = std::snprintf(buf, sizeof buf, "U%05u-route-marker-%02u", id, (id / kEventIdModulo) % 100u);
    return std::string(buf, std::size_t(n));
}

namespace detail {

// Salt spaces. Event hp uses salt k (the event number) as the design states;
// the other draws use disjoint high salts.
inline constexpr std::uint64_t kSaltStartX = 0x1000;
inline constexpr std::uint64_t kSaltStartY = 0x1001;
inline constexpr std::uint64_t kSaltWaypoint = 0x1010;   // + 2 * k (+1 for y), k = 1..3
inline constexpr std::uint64_t kSaltSpeed = 0x1020;
inline constexpr std::uint64_t kSaltSiteJitter = 0x2000; // + 2 * site (+1 for y)
inline constexpr std::uint64_t kSaltInsert = 0x10000;    // + 2 * k (+1 for y), k = event number

inline constexpr std::uint32_t kInitialPathPoints = 4;
inline constexpr int kWaypointSpread = 256;   // +- spread of new waypoints
inline constexpr int kSiteJitter = 128;
inline constexpr std::uint32_t kPathTri[8] = {0, 1, 2, 3, 4, 3, 2, 1};

inline float clamp_coord(float v) {
    if (v < 0.0f) return 0.0f;
    if (v > kMaxCoord) return kMaxCoord;
    return v;
}

inline int clamp_int_coord(int v) {
    if (v < 0) return 0;
    if (v > int(kMaxCoord)) return int(kMaxCoord);
    return v;
}

// Integer offset in [-spread, +spread].
inline int spread(std::uint64_t h, int spread_) {
    return int(h % std::uint64_t(2 * spread_ + 1)) - spread_;
}

// Integer waypoint near (cx, cy), clamped to [0, kMaxCoord].
inline Point near_point(int cx, int cy, std::uint64_t seed, std::uint32_t id, std::uint64_t salt) {
    const int x = clamp_int_coord(cx + spread(hash(seed, id, salt), kWaypointSpread));
    const int y = clamp_int_coord(cy + spread(hash(seed, id, salt + 1), kWaypointSpread));
    return Point{float(x), float(y)};
}

inline constexpr const char* kSiteNames[kSiteCount] = {
    "Ford",           "Northwatch Keep", "Ash Hollow",    "Brightwater",
    "Cairn",          "Duskmere Abbey",  "Elm Crossing",  "Fallow",
    "Greystone Pass", "Harrowgate",      "Iron Mill",     "Juniper Fields of the West",
    "Kestrel Rock",   "Lowmarsh",        "Moor",          "Old Lighthouse",
};

template <class Unit>
void move_unit(Unit& u) {
    auto& path = u.path;
    const std::size_t size = path.size();
    if (size == 0) return;
    float remaining = u.speed * kStepSeconds;
    std::size_t next = u.next_waypoint;
    std::size_t guard = 0;
    for (;;) {
        const Point target = path[next];
        const float dx = target.x - u.pos.x;
        const float dy = target.y - u.pos.y;
        const float dist = std::sqrt(dx * dx + dy * dy);
        if (dist == 0.0f) {   // duplicate point: skip it
            next = (next + 1) % size;
            if (++guard >= size) break;
            continue;
        }
        if (dist <= remaining) {   // reach and snap, no carry-over
            u.pos = target;
            next = (next + 1) % size;
            break;
        }
        u.pos.x += dx / dist * remaining;
        u.pos.y += dy / dist * remaining;
        break;
    }
    u.pos.x = clamp_coord(u.pos.x);
    u.pos.y = clamp_coord(u.pos.y);
    u.next_waypoint = std::uint32_t(next);
}

// Event k for one unit (id % kEventIdModulo == 0).
template <class Unit>
void apply_event(Unit& u, std::uint64_t seed, std::uint64_t k) {
    u.hp = std::int32_t(50 + hash(seed, u.id, k) % 51);
    u.behavior = Behavior((std::uint32_t(u.behavior) + 1) % 3);
    if (k % 2 == 1) u.name = std::string_view(long_name(u.id));
    else u.name = std::string_view(short_name(u.id));

    auto& path = u.path;
    const std::size_t target = kInitialPathPoints + kPathTri[k % 8];
    std::size_t size = path.size();
    std::size_t next = u.next_waypoint;
    if (size < target) {
        // Insert at `next`: the new point becomes the next target.
        const Point p = near_point(int(u.pos.x), int(u.pos.y), seed, u.id, kSaltInsert + 2 * k);
        path.insert(path.begin() + next, p);
    } else if (size > target) {
        const std::size_t e = (next + 1) % size;
        path.erase(path.begin() + e);
        --size;
        if (e < next) --next;
        if (next >= size) next = 0;
        u.next_waypoint = std::uint32_t(next);
    }
}

} // namespace detail

inline void build_catalog(MapCatalog& catalog, std::uint64_t seed) {
    catalog.scenes.reserve(1);
    auto& s = catalog.scenes.emplace_back();
    s.id = kSceneId;
    s.name = std::string_view("Demo Plains");
    s.bounds = scene_bounds();

    // 4 x 4 grid centres, each jittered by +-kSiteJitter, integer coordinates.
    const int cell = int(kSceneSize) / 4;
    catalog.sites.reserve(kSiteCount);
    for (std::uint32_t i = 0; i < kSiteCount; ++i) {
        const int cx = cell / 2 + cell * int(i % 4);
        const int cy = cell / 2 + cell * int(i / 4);
        auto& site = catalog.sites.emplace_back();
        site.id = i + 1;
        site.scene_id = kSceneId;
        site.pos = Point{
            float(detail::clamp_int_coord(cx + detail::spread(hash(seed, i + 1, detail::kSaltSiteJitter + 2 * i), detail::kSiteJitter))),
            float(detail::clamp_int_coord(cy + detail::spread(hash(seed, i + 1, detail::kSaltSiteJitter + 2 * i + 1), detail::kSiteJitter))),
        };
        site.name = std::string_view(detail::kSiteNames[i]);
    }
}

template <class Unit>
void init_unit(Unit& u, std::uint32_t id, std::uint64_t seed) {
    const int x0 = int(hash(seed, id, detail::kSaltStartX) % 4096);
    const int y0 = int(hash(seed, id, detail::kSaltStartY) % 4096);
    u.id = id;
    u.scene_id = kSceneId;
    u.pos = Point{float(x0), float(y0)};
    u.speed = float(20 + hash(seed, id, detail::kSaltSpeed) % 61);
    u.hp = 100;
    u.behavior = Behavior::Patrol;
    u.next_waypoint = 1;
    u.name = std::string_view(short_name(id));
    u.path.reserve(detail::kInitialPathPoints);
    u.path.push_back(u.pos);
    for (std::uint64_t k = 1; k < detail::kInitialPathPoints; ++k)
        u.path.push_back(detail::near_point(x0, y0, seed, id, detail::kSaltWaypoint + 2 * k));
}

template <class Batch>
void init_batch(Batch& batch, std::uint32_t count, std::uint64_t seed) {
    batch.tick = 0;
    batch.units.reserve(count);
    for (std::uint32_t i = 0; i < count; ++i) init_unit(batch.units.emplace_back(), i, seed);
}

template <class Batch>
void step_batch(Batch& batch, std::uint64_t seed) {
    const std::uint64_t t1 = batch.tick + 1;
    const bool event = t1 % kEventPeriodTicks == 0;
    const std::uint64_t k = t1 / kEventPeriodTicks;
    auto& units = batch.units;
    const std::size_t n = units.size();
    // Per unit: move, then its event. Units are independent, so this equals
    // "move every unit, then apply the events".
    for (std::size_t i = 0; i < n; ++i) {
        auto& u = units[i];
        detail::move_unit(u);
        if (event && u.id % kEventIdModulo == 0) detail::apply_event(u, seed, k);
    }
    batch.tick = t1;
}

} // namespace mapdemo::rules
