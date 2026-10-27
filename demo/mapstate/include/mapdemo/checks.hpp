// Map state demo: automatic checks (requirements section 7).
// FROZEN INTERFACE (demo/mapstate/INTERFACES.md). Bodies in src/checks.cpp
// and checks_impl.hpp, owned by the common-library thread. Check code never
// runs inside a timed region of the measured paths.
#pragma once
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
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
// public fields, no duplicates; empty rect -> empty array.
Result compare_snapshot(const ViewSnapshot& snapshot, const ViewRequest& request, const stdm::UnitStateBatch& ref);

// Client: request id and rect echoed, every unit inside the rect, no duplicate
// ids, finite positions, behavior in range.
Result snapshot_sanity(const ViewSnapshot& snapshot, std::uint64_t request_id, const Rect& requested);

// FNV-1a over request_id, tick, scene_id, rect bits and every unit's
// id, hp, pos bits, behavior, name, in array order. A and the client log it
// per request id; tools/summarize.py compares the two logs.
std::uint64_t snapshot_digest(const ViewSnapshot& snapshot);

// Digest of a whole wire (or any byte string): 64-bit words mixed in order,
// then the tail bytes and the length. A logs it per step (a_steps.csv) so
// runs built with other compilers, on other platforms or with the other
// --buffers mode can be compared tick by tick; equal digests = same bytes.
std::uint64_t wire_digest(std::string_view bytes) noexcept;

// True when [p, p + bytes) lies inside the live payload of `buffer`
// (direct-read evidence on the client).
bool in_buffer(const XBuffer& buffer, const void* p, std::size_t bytes) noexcept;

} // namespace mapdemo::checks
