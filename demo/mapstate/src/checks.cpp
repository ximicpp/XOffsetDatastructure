// Map state demo: check bodies (requirements section 7). OWNER: common-library thread.
// Detail strings name the first failure only; callers prefix tick/request.
#include "mapdemo/checks.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>
#include <vector>

#include "mapdemo/rules.hpp"

namespace mapdemo::checks {
namespace {

std::uint32_t bits(float f) noexcept {
    std::uint32_t u;
    std::memcpy(&u, &f, sizeof u);
    return u;
}

bool same(float a, float b) noexcept { return bits(a) == bits(b); }
bool same(Point a, Point b) noexcept { return same(a.x, b.x) && same(a.y, b.y); }
bool same(const Rect& a, const Rect& b) noexcept { return same(a.min, b.min) && same(a.max, b.max); }

bool finite(Point p) noexcept { return std::isfinite(p.x) && std::isfinite(p.y); }
bool inside_scene(Point p) noexcept { return finite(p) && rules::contains(scene_bounds(), p); }
bool behavior_ok(Behavior b) noexcept { return static_cast<std::uint32_t>(b) <= 2u; }

std::string fmt_point(Point p) {
    char buf[96];
    std::snprintf(buf, sizeof buf, "(%.9g,%.9g)", double(p.x), double(p.y));
    return buf;
}

std::string unit_tag(std::size_t index, std::uint32_t id) {
    return "unit[" + std::to_string(index) + "] id " + std::to_string(id) + ": ";
}

Result fail(std::string detail) { return Result{false, std::move(detail)}; }

} // namespace

Result batch_invariants(const UnitStateBatch& batch, std::uint32_t expected_units, std::uint64_t expected_tick) {
    if (batch.tick != expected_tick)
        return fail("tick " + std::to_string(batch.tick) + ", expected " + std::to_string(expected_tick));
    if (batch.reserved0 != 0) return fail("reserved0 " + std::to_string(batch.reserved0) + ", expected 0");
    const auto& units = batch.units;
    if (units.size() != expected_units)
        return fail("unit count " + std::to_string(units.size()) + ", expected " + std::to_string(expected_units));
    for (std::size_t i = 0; i < units.size(); ++i) {
        const UnitState& u = units[i];
        if (u.id != i) return fail(unit_tag(i, u.id) + "id out of order");
        if (u.scene_id != kSceneId) return fail(unit_tag(i, u.id) + "scene_id " + std::to_string(u.scene_id));
        if (!inside_scene(u.pos)) return fail(unit_tag(i, u.id) + "position " + fmt_point(u.pos) + " not finite/in scene");
        if (!(u.speed >= 20.0f && u.speed <= 80.0f)) return fail(unit_tag(i, u.id) + "speed out of 20..80");
        if (u.hp < 50 || u.hp > 100) return fail(unit_tag(i, u.id) + "hp " + std::to_string(u.hp) + " out of 50..100");
        if (!behavior_ok(u.behavior))
            return fail(unit_tag(i, u.id) + "behavior " + std::to_string(std::uint32_t(u.behavior)) + " out of range");
        const std::size_t n = u.path.size();
        if (n < 4 || n > 8) return fail(unit_tag(i, u.id) + "path size " + std::to_string(n) + " out of 4..8");
        if (u.next_waypoint >= n)
            return fail(unit_tag(i, u.id) + "next_waypoint " + std::to_string(u.next_waypoint) + " >= path size");
        for (std::size_t k = 0; k < n; ++k)
            if (!inside_scene(u.path[k]))
                return fail(unit_tag(i, u.id) + "path[" + std::to_string(k) + "] " + fmt_point(u.path[k]) + " not in scene");
        const std::string_view name = u.name.view();
        if (name != rules::short_name(u.id) && name != rules::long_name(u.id))
            return fail(unit_tag(i, u.id) + "unexpected name '" + std::string(name) + "'");
    }
    return Result{};
}

Result compare_with_reference(const UnitStateBatch& batch, const stdm::UnitStateBatch& ref) {
    if (batch.tick != ref.tick)
        return fail("tick " + std::to_string(batch.tick) + ", reference " + std::to_string(ref.tick));
    if (batch.units.size() != ref.units.size())
        return fail("unit count " + std::to_string(batch.units.size()) + ", reference " + std::to_string(ref.units.size()));
    for (std::size_t i = 0; i < ref.units.size(); ++i) {
        const UnitState& a = batch.units[i];
        const stdm::UnitState& r = ref.units[i];
        const std::string tag = unit_tag(i, r.id);
        if (a.id != r.id) return fail(tag + "id " + std::to_string(a.id));
        if (a.scene_id != r.scene_id) return fail(tag + "scene_id differs");
        if (!same(a.pos, r.pos)) return fail(tag + "pos " + fmt_point(a.pos) + " vs reference " + fmt_point(r.pos));
        if (!same(a.speed, r.speed)) return fail(tag + "speed differs");
        if (a.hp != r.hp) return fail(tag + "hp " + std::to_string(a.hp) + " vs reference " + std::to_string(r.hp));
        if (a.behavior != r.behavior) return fail(tag + "behavior differs");
        if (a.next_waypoint != r.next_waypoint)
            return fail(tag + "next_waypoint " + std::to_string(a.next_waypoint) + " vs reference " +
                        std::to_string(r.next_waypoint));
        if (a.name.view() != std::string_view(r.name))
            return fail(tag + "name '" + std::string(a.name.view()) + "' vs reference '" + r.name + "'");
        if (a.path.size() != r.path.size())
            return fail(tag + "path size " + std::to_string(a.path.size()) + " vs reference " + std::to_string(r.path.size()));
        for (std::size_t k = 0; k < r.path.size(); ++k)
            if (!same(a.path[k], r.path[k]))
                return fail(tag + "path[" + std::to_string(k) + "] " + fmt_point(a.path[k]) + " vs reference " +
                            fmt_point(r.path[k]));
    }
    return Result{};
}

void Coverage::observe(const UnitStateBatch& before, const UnitStateBatch& after) {
    const std::size_t n = std::min<std::size_t>(before.units.size(), after.units.size());
    if (stage_.size() < n) stage_.resize(n, 0);
    for (std::size_t i = 0; i < n; ++i) {
        const UnitState& b = before.units[i];
        const UnitState& a = after.units[i];
        if (a.hp != b.hp) ++hp_changes;
        if (a.behavior != b.behavior) ++behavior_changes;
        const std::string_view bn = b.name.view();
        const std::string_view an = a.name.view();
        if (an != bn) {
            if (an.size() > bn.size()) ++name_to_long;
            else if (an.size() < bn.size()) ++name_to_short;
        }
        const std::size_t bs = b.path.size();
        const std::size_t as = a.path.size();
        if (as > bs) {
            ++path_grow;
            if (stage_[i] == 2) grow_shrink_grow = true;
            else stage_[i] = 1;
        } else if (as < bs) {
            ++path_shrink;
            if (stage_[i] == 1) stage_[i] = 2;
        }
    }
}

bool Coverage::complete() const noexcept {
    return name_to_long > 0 && name_to_short > 0 && path_grow > 0 && path_shrink > 0 && hp_changes > 0 &&
           behavior_changes > 0 && grow_shrink_grow;
}

Result compare_snapshot(const ViewSnapshot& snapshot, const ViewRequest& request, const stdm::UnitStateBatch& ref) {
    if (snapshot.request_id != request.request_id) return fail("request_id not echoed");
    if (snapshot.scene_id != request.scene_id) return fail("scene_id not echoed");
    if (!same(snapshot.rect, request.rect)) return fail("rect not echoed");
    if (snapshot.tick != ref.tick)
        return fail("snapshot tick " + std::to_string(snapshot.tick) + ", reference " + std::to_string(ref.tick));

    std::size_t expected = 0, in_scene = 0;
    for (const stdm::UnitState& r : ref.units) {
        if (r.scene_id != request.scene_id) continue;
        ++in_scene;
        if (rules::contains(request.rect, r.pos)) ++expected;
    }
    if (snapshot.units.size() != expected)
        return fail("visible " + std::to_string(snapshot.units.size()) + ", reference " + std::to_string(expected));
#if MAPDEMO_SCHEMA_VERSION >= 2
    if (snapshot.scene_units != in_scene)
        return fail("scene_units " + std::to_string(snapshot.scene_units) + ", reference " + std::to_string(in_scene));
    if (snapshot.reserved0 != 0) return fail("reserved0 " + std::to_string(snapshot.reserved0) + ", expected 0");
#else
    (void)in_scene;
#endif

    std::vector<bool> seen(ref.units.size(), false);
    for (std::size_t i = 0; i < snapshot.units.size(); ++i) {
        const VisibleUnit& v = snapshot.units[i];
        const std::string tag = "visible[" + std::to_string(i) + "] id " + std::to_string(v.id) + ": ";
        if (v.id >= ref.units.size()) return fail(tag + "unknown id");
        if (seen[v.id]) return fail(tag + "duplicate id");
        seen[v.id] = true;
        const stdm::UnitState& r = ref.units[v.id];   // reference ids are 0..N-1 in order
        if (r.scene_id != request.scene_id || !rules::contains(request.rect, r.pos))
            return fail(tag + "reference unit is not in the request rect");
        if (v.hp != r.hp) return fail(tag + "hp differs");
        if (!same(v.pos, r.pos)) return fail(tag + "pos " + fmt_point(v.pos) + " vs reference " + fmt_point(r.pos));
        if (v.behavior != r.behavior) return fail(tag + "behavior differs");
        if (v.name.view() != std::string_view(r.name)) return fail(tag + "name differs");
    }
    // Same count, every element in the expected set, no duplicates: the id sets are equal.
    return Result{};
}

Result snapshot_sanity(const ViewSnapshot& snapshot, std::uint64_t request_id, const Rect& requested) {
    if (snapshot.request_id != request_id)
        return fail("request_id " + std::to_string(snapshot.request_id) + ", expected " + std::to_string(request_id));
    if (!same(snapshot.rect, requested)) return fail("rect not echoed");
#if MAPDEMO_SCHEMA_VERSION >= 2
    if (snapshot.units.size() > snapshot.scene_units)
        return fail("visible " + std::to_string(snapshot.units.size()) + " > scene_units " +
                    std::to_string(snapshot.scene_units));
    if (snapshot.reserved0 != 0) return fail("reserved0 " + std::to_string(snapshot.reserved0) + ", expected 0");
#endif
    std::vector<std::uint32_t> ids;
    ids.reserve(snapshot.units.size());
    for (std::size_t i = 0; i < snapshot.units.size(); ++i) {
        const VisibleUnit& v = snapshot.units[i];
        const std::string tag = "visible[" + std::to_string(i) + "] id " + std::to_string(v.id) + ": ";
        if (!finite(v.pos)) return fail(tag + "position not finite");
        if (!rules::contains(requested, v.pos)) return fail(tag + "position " + fmt_point(v.pos) + " outside the rect");
        if (!behavior_ok(v.behavior)) return fail(tag + "behavior out of range");
        ids.push_back(v.id);
    }
    std::sort(ids.begin(), ids.end());
    const auto dup = std::adjacent_find(ids.begin(), ids.end());
    if (dup != ids.end()) return fail("duplicate id " + std::to_string(*dup));
    return Result{};
}

std::uint64_t snapshot_digest(const ViewSnapshot& snapshot) { return snapshot_digest_of(snapshot); }

std::uint64_t wire_digest(std::string_view bytes) noexcept {
    // Little endian is enforced by the library, so word reads mean the same
    // bytes on every supported target.
    constexpr std::uint64_t kMul = 0x9E3779B97F4A7C15ull;
    std::uint64_t h = 0xcbf29ce484222325ull;
    const char* p = bytes.data();
    std::size_t n = bytes.size();
    for (; n >= 8; p += 8, n -= 8) {
        std::uint64_t w;
        std::memcpy(&w, p, sizeof w);
        h = (h ^ w) * kMul;
        h ^= h >> 29;
    }
    for (; n != 0; ++p, --n) {
        h = (h ^ std::uint8_t(*p)) * kMul;
        h ^= h >> 29;
    }
    h = (h ^ std::uint64_t(bytes.size())) * kMul;
    return h ^ (h >> 32);
}

bool in_buffer(const XBuffer& buffer, const void* p, std::size_t bytes) noexcept {
    const auto base = reinterpret_cast<std::uintptr_t>(buffer.get_address());
    const auto size = static_cast<std::uintptr_t>(buffer.get_size());
    const auto at = reinterpret_cast<std::uintptr_t>(p);
    if (base == 0 || at < base) return false;
    const std::uintptr_t off = at - base;
    return off <= size && bytes <= size - off;
}

} // namespace mapdemo::checks
