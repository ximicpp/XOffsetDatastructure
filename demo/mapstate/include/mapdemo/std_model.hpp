// Map state demo: reference model on std containers. Used ONLY by automatic
// checks (server A --check, selftest). Member names match schema.hpp so the
// templates in rules.hpp run unchanged on both models.
// FROZEN INTERFACE (demo/mapstate/INTERFACES.md).
#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include "mapdemo/schema.hpp"

namespace mapdemo::stdm {

using mapdemo::Behavior;
using mapdemo::Point;
using mapdemo::Rect;

struct UnitState {
    std::uint32_t id{};
    std::uint32_t scene_id{};
    Point pos;
    float speed{};
    std::int32_t hp{};
    Behavior behavior{};
    std::uint32_t next_waypoint{};
    std::string name;
    std::vector<Point> path;
};

struct UnitStateBatch {
    std::uint64_t tick{};
    std::vector<UnitState> units;
};

} // namespace mapdemo::stdm
