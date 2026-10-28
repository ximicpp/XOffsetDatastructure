// Map state demo: automatic checks (requirements section 7).
// FROZEN INTERFACE (demo/mapstate/INTERFACES.md). Bodies in src/checks.cpp
// and checks_impl.hpp, owned by the common-library thread. Check code never
// runs inside a timed region of the measured paths.
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>
#include <vector>
#include "mapdemo/rules.hpp"
#include "mapdemo/schema.hpp"
#include "mapdemo/std_model.hpp"

namespace mapdemo::checks {

struct Result {
    bool ok = true;
    std::string detail;   // first failure, human readable
};

// Server A after each step: count, ids 0..N-1 in order, tick, reserved0 == 0, finite positions
// inside the scene, path size 4..8 with points inside the scene, next < size,
// behavior in 0..2, hp in 50..100.
Result batch_invariants(const UnitStateBatch& batch, std::uint32_t expected_units, std::uint64_t expected_tick);

// Exact (bitwise for floats) field-by-field comparison with the reference model.
Result compare_with_reference(const UnitStateBatch& batch, const stdm::UnitStateBatch& ref);

// Coverage of the modification rules, observed on consecutive batches
// returned by B (before = previous tick, after = new tick).
struct Coverage {
    std::uint64_t name_to_long = 0;
    std::uint64_t name_to_short = 0;
    std::uint64_t path_grow = 0;
    std::uint64_t path_shrink = 0;
    std::uint64_t hp_changes = 0;
    std::uint64_t behavior_changes = 0;
    bool grow_shrink_grow = false;   // some unit grew, then shrank, then grew again
    void observe(const UnitStateBatch& before, const UnitStateBatch& after);
    bool complete() const noexcept;
private:
    std::vector<std::uint8_t> stage_;   // per unit index: 0, 1 grew, 2 grew then shrank (additive, 10-22)
};

// Server A: snapshot built from the batch of tick T versus the reference model
// at the same tick filtered by the same scene and rect: same id set, same
// public fields, no duplicates; empty rect -> empty array; scene_units the
// reference's unit count in the scene (schema version 2).
Result compare_snapshot(const ViewSnapshot& snapshot, const ViewRequest& request, const stdm::UnitStateBatch& ref);

// Client: request id and rect echoed, every unit inside the rect, no duplicate
// ids, finite positions, behavior in range; schema version 2: no more units
// than scene_units, reserved0 == 0.
Result snapshot_sanity(const ViewSnapshot& snapshot, std::uint64_t request_id, const Rect& requested);

// FNV-1a over request_id, tick, scene_id, rect bits and every unit's
// id, hp, pos bits, behavior, name, in array order. A and the client log it
// per request id; tools/summarize.py compares the two logs. These are the
// fields of both schema versions (version 2 added scene_units, which is not
// in the digest), so A's digest of a snapshot it converts for a version-1
// client equals that client's digest of what it received.
std::uint64_t snapshot_digest(const ViewSnapshot& snapshot);

// The same digest of a ViewSnapshot of any schema version (A digests the
// version-1 snapshots it converts with it; snapshot_digest calls it).
template <typename Snapshot>
std::uint64_t snapshot_digest_of(const Snapshot& snapshot);

// A: the business rules for a client's write-back (an Edit), of either
// schema version. load_verified has already proved the structure; these are
// the field values A accepts from a client: scene kSceneId, a legal rect,
// every unit inside it at a finite position, hp 50..100, behavior 0..2, a
// name of 1..kMaxEditName bytes, no duplicate ids; schema version 2: no
// more units than scene_units, reserved0 == 0.
inline constexpr std::size_t kMaxEditName = 64;
template <typename Snapshot>
Result edit_sanity_of(const Snapshot& snapshot);

// Digest of a whole wire (or any byte string): 64-bit words mixed in order,
// then the tail bytes and the length. A logs it per step (a_steps.csv) so
// runs built with other compilers, on other platforms or with the other
// --buffers mode can be compared tick by tick; equal digests = same bytes.
std::uint64_t wire_digest(std::string_view bytes) noexcept;

// True when [p, p + bytes) lies inside the live payload of `buffer`
// (direct-read evidence on the client).
bool in_buffer(const XBuffer& buffer, const void* p, std::size_t bytes) noexcept;

namespace detail {
// FNV-1a 64.
struct Fnv {
    std::uint64_t h = 0xcbf29ce484222325ull;
    void bytes(const void* p, std::size_t n) noexcept {
        const auto* b = static_cast<const unsigned char*>(p);
        for (std::size_t i = 0; i < n; ++i) {
            h ^= b[i];
            h *= 0x100000001b3ull;
        }
    }
    template <class T>
    void le(T v) noexcept {   // integers, little endian
        unsigned char b[sizeof(T)];
        for (std::size_t i = 0; i < sizeof(T); ++i) b[i] = static_cast<unsigned char>(std::uint64_t(v) >> (8 * i));
        bytes(b, sizeof b);
    }
    void f(float v) noexcept {
        std::uint32_t u;
        std::memcpy(&u, &v, sizeof u);
        le(u);
    }
};
} // namespace detail

template <typename Snapshot>
std::uint64_t snapshot_digest_of(const Snapshot& snapshot) {
    detail::Fnv d;
    d.le(snapshot.request_id);
    d.le(snapshot.tick);
    d.le(snapshot.scene_id);
    d.f(snapshot.rect.min.x);
    d.f(snapshot.rect.min.y);
    d.f(snapshot.rect.max.x);
    d.f(snapshot.rect.max.y);
    d.le(std::uint64_t(snapshot.units.size()));
    for (std::size_t i = 0; i < snapshot.units.size(); ++i) {
        const auto& v = snapshot.units[i];
        d.le(v.id);
        d.le(std::uint32_t(v.hp));
        d.f(v.pos.x);
        d.f(v.pos.y);
        d.le(std::uint32_t(v.behavior));
        const std::string_view name = v.name.view();
        d.le(std::uint32_t(name.size()));
        d.bytes(name.data(), name.size());
    }
    return d.h;
}

template <typename Snapshot>
Result edit_sanity_of(const Snapshot& snapshot) {
    auto fail = [](std::string why) { return Result{false, std::move(why)}; };
    const Rect rect{{snapshot.rect.min.x, snapshot.rect.min.y}, {snapshot.rect.max.x, snapshot.rect.max.y}};
    if (snapshot.scene_id != kSceneId) return fail("scene_id " + std::to_string(snapshot.scene_id));
    if (!rules::rect_is_legal(rect, scene_bounds())) return fail("rect not legal in the scene");
    if constexpr (requires { snapshot.scene_units; }) {
        if (snapshot.units.size() > snapshot.scene_units)
            return fail("units " + std::to_string(snapshot.units.size()) + " > scene_units " +
                        std::to_string(snapshot.scene_units));
        if (snapshot.reserved0 != 0) return fail("reserved0 " + std::to_string(snapshot.reserved0));
    }
    std::vector<std::uint32_t> ids;
    ids.reserve(snapshot.units.size());
    std::size_t i = 0;
    for (const auto& v : snapshot.units) {
        const Point pos{v.pos.x, v.pos.y};
        const std::string tag = "unit[" + std::to_string(i) + "] id " + std::to_string(v.id) + ": ";
        if (!std::isfinite(pos.x) || !std::isfinite(pos.y) || !rules::contains(rect, pos))
            return fail(tag + "position outside the rect");
        if (v.hp < 50 || v.hp > 100) return fail(tag + "hp " + std::to_string(v.hp) + " out of 50..100");
        if (static_cast<std::uint32_t>(v.behavior) > 2u) return fail(tag + "behavior out of range");
        const std::size_t name = v.name.view().size();
        if (name == 0 || name > kMaxEditName) return fail(tag + "name of " + std::to_string(name) + " bytes");
        ids.push_back(v.id);
        ++i;
    }
    std::sort(ids.begin(), ids.end());
    const auto dup = std::adjacent_find(ids.begin(), ids.end());
    if (dup != ids.end()) return fail("duplicate id " + std::to_string(*dup));
    return Result{};
}

} // namespace mapdemo::checks
