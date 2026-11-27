// Game server demo: the schema (shared with examples/game_server) and helpers
// every process uses.
#ifndef GS_GAME_HPP
#define GS_GAME_HPP

#include "game_server/game_schema.hpp"  // examples/game_server: Player, City, Alliance, Command, ConfigTable
#include "gs/protocol.hpp"

#include <cstdint>
#include <string>

namespace gs {

using namespace XOffsetDatastructure;

inline XBuffer::MaximumCapacity grow_to(std::size_t bytes) {
    return XBuffer::maximum_capacity(bytes).with_growth(XGrowPolicy::Automatic);
}

// Redis keys: one value per entity, holding its saved arena bytes.
inline std::string player_key(std::uint64_t id) { return "gs:player:" + std::to_string(id); }
inline std::string city_key(std::uint64_t id) { return "gs:city:" + std::to_string(id); }
inline std::string alliance_key(std::uint64_t id) { return "gs:alliance:" + std::to_string(id); }
// The directory: which logic node owns a player that moved off its home node.
inline std::string route_key(std::uint64_t id) { return "gs:route:" + std::to_string(id); }

inline std::uint32_t home_node(std::uint64_t player_id, std::uint32_t nodes) { return std::uint32_t(player_id % nodes); }
inline std::uint32_t alliance_node(std::uint64_t alliance_id, std::uint32_t nodes) {
    return std::uint32_t(alliance_id % nodes);
}

// The static configuration table, as the offline tool would build it.
inline std::string build_config() {
    auto t = TypedXBuffer<game::ConfigTable>::create(512, grow_to(kConfigLimit));
    t.root().version() = 1;
    t.edit(
        [](game::ConfigTable& c) {
            auto add = [&](game::Building b, std::int32_t max_level, std::int64_t gold) {
                game::BuildingConfig& e = c.buildings.emplace_back();
                e.building = std::uint32_t(b);
                e.max_level = max_level;
                e.gold_per_level = gold;
            };
            add(game::Building::town_hall, 25, 200);
            add(game::Building::farm, kFarmMaxLevel, kFarmGoldPerLevel);
            add(game::Building::barracks, 20, 150);
        },
        256);
    return t.save_compact();
}

}  // namespace gs

#endif
