// Map state demo: two ViewSnapshot schemas no release has, for the handshake
// refusal cases (client --evil, mapdemo_selftest). Test types only; A never
// lists them. Additive header (10-28); OWNER: common-library thread.
//
// RenamedViewSnapshot has the layout of version 1 under other field names
// (VisibleUnit::hp is called health): the bytes are the same, their meaning
// is not, so A answers reject_names. FutureViewSnapshot is a version A does
// not know (version 2 plus one field): reject_unknown.
#pragma once
#include <cstdint>
#include "mapdemo/schema.hpp"

namespace mapdemo::probe {
using namespace XOffsetDatastructure;

struct RenamedVisibleUnit {
    std::uint32_t id{};
    std::int32_t health{};
    Point pos;
    Behavior behavior{};
    XString name;
};

struct RenamedViewSnapshot {
    std::uint64_t request_id{};
    std::uint64_t tick{};
    std::uint32_t scene_id{};
    Rect rect;
    XVector<RenamedVisibleUnit> units;
};

struct FutureViewSnapshot {
    std::uint64_t request_id{};
    std::uint64_t tick{};
    std::uint32_t scene_id{};
    Rect rect;
    XVector<VisibleUnit> units;
    std::uint32_t scene_units{};
    std::uint32_t reserved0{};
    std::uint64_t server_time_us{};
};

} // namespace mapdemo::probe

XOFFSET_RECORD(mapdemo::probe::RenamedVisibleUnit, id, health, pos, behavior, name)
XOFFSET_RECORD(mapdemo::probe::RenamedViewSnapshot, request_id, tick, scene_id, rect, units)
XOFFSET_RECORD(mapdemo::probe::FutureViewSnapshot, request_id, tick, scene_id, rect, units, scene_units, reserved0,
               server_time_us)
XOFFSET_REGISTER_SCHEMA_NAME(mapdemo::probe::RenamedViewSnapshot, "mapdemo.ViewSnapshot.v1")
XOFFSET_REGISTER_SCHEMA_NAME(mapdemo::probe::FutureViewSnapshot, "mapdemo.ViewSnapshot.v1")
