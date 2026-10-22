// SLG prototype: protobuf over the std model.
// decode = ParseFromArray + copy into std types; encode = copy into messages + SerializeToString.
// This is the common "protobuf at the boundary, domain objects inside" pattern.
#pragma once
#include <cstdint>
#include <span>
#include <stdexcept>
#include <string>
#include <type_traits>
#include "slg.pb.h"
#include "slg/model_std.hpp"
#include "runner.hpp"

namespace slg::pbconv {

// Forward declarations so the container helpers below find every overload.
#define SLG_PB_DECL(T) inline void to(pb::T&, const stdm::T&); inline void from(stdm::T&, const pb::T&);
SLG_PB_DECL(Buff) SLG_PB_DECL(Skill) SLG_PB_DECL(Troop) SLG_PB_DECL(Event) SLG_PB_DECL(Tile)
SLG_PB_DECL(Hero) SLG_PB_DECL(Army) SLG_PB_DECL(City) SLG_PB_DECL(PlayerState) SLG_PB_DECL(Report)
SLG_PB_DECL(BattleMsg) SLG_PB_DECL(March) SLG_PB_DECL(ShardState)
#undef SLG_PB_DECL

inline void to(pb::Buff& d, const stdm::Buff& s) { d.set_id(s.id); d.set_stacks(s.stacks); d.set_expire_ms(s.expire_ms); }
inline void to(pb::Skill& d, const stdm::Skill& s) { d.set_id(s.id); d.set_level(s.level); }
inline void to(pb::Troop& d, const stdm::Troop& s) { d.set_type(s.type); d.set_count(s.count); d.set_wounded(s.wounded); }
inline void to(pb::Event& d, const stdm::Event& s) {
    d.set_round(s.round); d.set_hero_id(s.hero_id); d.set_skill_id(s.skill_id); d.set_target(s.target); d.set_damage(s.damage);
}
inline void to(pb::Tile& d, const stdm::Tile& s) { d.set_x(s.x); d.set_y(s.y); d.set_terrain(s.terrain); d.set_level(s.level); d.set_owner(s.owner); }
template <class R, class V> void to_rep(R* r, const V& v) { r->Reserve(int(v.size())); for (const auto& e : v) to(*r->Add(), e); }
template <class M, class S> void to_map(M* m, const S& s) { for (const auto& kv : s) (*m)[kv.first] = kv.second; }
inline void to(pb::Hero& d, const stdm::Hero& s) {
    d.set_id(s.id); d.set_level(s.level); d.set_star(s.star); d.set_name(s.name);
    to_rep(d.mutable_skills(), s.skills); to_rep(d.mutable_buffs(), s.buffs);
}
inline void to(pb::Army& d, const stdm::Army& s) {
    d.set_army_id(s.army_id); d.set_owner_id(s.owner_id); d.set_x(s.x); d.set_y(s.y);
    to_rep(d.mutable_heroes(), s.heroes); to_rep(d.mutable_troops(), s.troops); to_rep(d.mutable_buffs(), s.buffs);
}
inline void to(pb::City& d, const stdm::City& s) {
    d.set_city_id(s.city_id); d.set_level(s.level); to_map(d.mutable_buildings(), s.buildings); to_map(d.mutable_resources(), s.resources);
}
inline void to(pb::PlayerState& d, const stdm::PlayerState& s) {
    d.set_player_id(s.player_id); d.set_name(s.name); to(*d.mutable_city(), s.city);
    to_rep(d.mutable_armies(), s.armies); to_rep(d.mutable_heroes(), s.heroes);
    to_map(d.mutable_research(), s.research); to_map(d.mutable_items(), s.items);
}
inline void to(pb::Report& d, const stdm::Report& s) { d.set_battle_id(s.battle_id); d.set_summary(s.summary); to_rep(d.mutable_events(), s.events); }
inline void to(pb::BattleMsg& d, const stdm::BattleMsg& s) {
    d.set_battle_id(s.battle_id); d.set_seed(s.seed); d.set_winner(s.winner);
    to(*d.mutable_attacker(), s.attacker); to(*d.mutable_defender(), s.defender); to(*d.mutable_report(), s.report);
}
inline void to(pb::March& d, const stdm::March& s) {
    d.set_march_id(s.march_id); d.set_owner(s.owner); d.set_from_x(s.from_x); d.set_from_y(s.from_y);
    d.set_to_x(s.to_x); d.set_to_y(s.to_y); d.set_eta(s.eta); to_rep(d.mutable_troops(), s.troops);
}
inline void to(pb::ShardState& d, const stdm::ShardState& s) {
    d.set_shard_id(s.shard_id); d.set_tick(s.tick); d.set_width(s.width);
    to_rep(d.mutable_tiles(), s.tiles); to_rep(d.mutable_cities(), s.cities); to_rep(d.mutable_marches(), s.marches);
}

inline void from(stdm::Buff& d, const pb::Buff& s) { d.id = s.id(); d.stacks = s.stacks(); d.expire_ms = s.expire_ms(); }
inline void from(stdm::Skill& d, const pb::Skill& s) { d.id = s.id(); d.level = s.level(); }
inline void from(stdm::Troop& d, const pb::Troop& s) { d.type = s.type(); d.count = s.count(); d.wounded = s.wounded(); }
inline void from(stdm::Event& d, const pb::Event& s) {
    d.round = s.round(); d.hero_id = s.hero_id(); d.skill_id = s.skill_id(); d.target = s.target(); d.damage = s.damage();
}
inline void from(stdm::Tile& d, const pb::Tile& s) { d.x = s.x(); d.y = s.y(); d.terrain = s.terrain(); d.level = s.level(); d.owner = s.owner(); }
template <class V, class R> void from_rep(V& v, const R& r) { v.clear(); v.reserve(std::size_t(r.size())); for (const auto& e : r) from(v.emplace_back(), e); }
template <class S, class M> void from_map(S& s, const M& m) { s.clear(); for (const auto& kv : m) s[kv.first] = kv.second; }
inline void from(stdm::Hero& d, const pb::Hero& s) {
    d.id = s.id(); d.level = s.level(); d.star = s.star(); d.name = s.name();
    from_rep(d.skills, s.skills()); from_rep(d.buffs, s.buffs());
}
inline void from(stdm::Army& d, const pb::Army& s) {
    d.army_id = s.army_id(); d.owner_id = s.owner_id(); d.x = s.x(); d.y = s.y();
    from_rep(d.heroes, s.heroes()); from_rep(d.troops, s.troops()); from_rep(d.buffs, s.buffs());
}
inline void from(stdm::City& d, const pb::City& s) {
    d.city_id = s.city_id(); d.level = s.level(); from_map(d.buildings, s.buildings()); from_map(d.resources, s.resources());
}
inline void from(stdm::PlayerState& d, const pb::PlayerState& s) {
    d.player_id = s.player_id(); d.name = s.name(); from(d.city, s.city());
    from_rep(d.armies, s.armies()); from_rep(d.heroes, s.heroes());
    from_map(d.research, s.research()); from_map(d.items, s.items());
}
inline void from(stdm::Report& d, const pb::Report& s) { d.battle_id = s.battle_id(); d.summary = s.summary(); from_rep(d.events, s.events()); }
inline void from(stdm::BattleMsg& d, const pb::BattleMsg& s) {
    d.battle_id = s.battle_id(); d.seed = s.seed(); d.winner = s.winner();
    from(d.attacker, s.attacker()); from(d.defender, s.defender()); from(d.report, s.report());
}
inline void from(stdm::March& d, const pb::March& s) {
    d.march_id = s.march_id(); d.owner = s.owner(); d.from_x = s.from_x(); d.from_y = s.from_y();
    d.to_x = s.to_x(); d.to_y = s.to_y(); d.eta = s.eta(); from_rep(d.troops, s.troops());
}
inline void from(stdm::ShardState& d, const pb::ShardState& s) {
    d.shard_id = s.shard_id(); d.tick = s.tick(); d.width = s.width();
    from_rep(d.tiles, s.tiles()); from_rep(d.cities, s.cities()); from_rep(d.marches, s.marches());
}

template <class T> struct msg_of;
template <> struct msg_of<stdm::PlayerState> { using type = pb::PlayerState; };
template <> struct msg_of<stdm::BattleMsg> { using type = pb::BattleMsg; };
template <> struct msg_of<stdm::ShardState> { using type = pb::ShardState; };
template <> struct msg_of<stdm::Report> { using type = pb::Report; };

} // namespace slg::pbconv

namespace slg {

struct ProtobufFormat {
    static constexpr const char* name = "protobuf";
    using Wire = std::string;
    static std::span<const uint8_t> bytes(const Wire& w) { return {reinterpret_cast<const uint8_t*>(w.data()), w.size()}; }

    template <class T> static Wire encode(const T& v) {
        typename pbconv::msg_of<T>::type m;
        pbconv::to(m, v);
        return m.SerializeAsString();
    }
    static Wire encode_battle_request(int64_t id, int64_t seed, const stdm::Army& a, const stdm::Army& d) {
        pb::BattleMsg m;
        m.set_battle_id(id); m.set_seed(seed); m.set_winner(0);
        pbconv::to(*m.mutable_attacker(), a); pbconv::to(*m.mutable_defender(), d);
        m.mutable_report();
        return m.SerializeAsString();
    }
    // Reuse protocol: one encode and one decode message per type (Clear()
    // keeps their allocated sub-messages) and one output string per slot.
    struct Reuse {
        template <class M> struct Pair { M enc, dec; };
        Pair<pb::BattleMsg> battle;
        Pair<pb::PlayerState> player;
        Pair<pb::ShardState> shard;
        Pair<pb::Report> report;
        std::string slot[kSlots];
        template <class T> auto& pair() {
            if constexpr (std::is_same_v<T, stdm::BattleMsg>) return battle;
            else if constexpr (std::is_same_v<T, stdm::PlayerState>) return player;
            else if constexpr (std::is_same_v<T, stdm::ShardState>) return shard;
            else return report;
        }
        template <class T> std::span<const uint8_t> encode(int s, const T& v) {
            auto& m = pair<T>().enc;
            m.Clear();
            pbconv::to(m, v);
            m.SerializeToString(&slot[s]);
            return bytes(slot[s]);
        }
        std::span<const uint8_t> encode_battle_request(int s, int64_t id, int64_t seed, const stdm::Army& a, const stdm::Army& d) {
            auto& m = battle.enc;
            m.Clear();
            m.set_battle_id(id); m.set_seed(seed); m.set_winner(0);
            pbconv::to(*m.mutable_attacker(), a); pbconv::to(*m.mutable_defender(), d);
            m.mutable_report();
            m.SerializeToString(&slot[s]);
            return bytes(slot[s]);
        }
        template <class T> void decode(std::span<const uint8_t> in, T& v) {
            auto& m = pair<T>().dec;
            if (!m.ParseFromArray(in.data(), int(in.size()))) throw std::runtime_error("protobuf: parse failed");
            pbconv::from(v, m);
        }
    };

    template <class T> static void decode(std::span<const uint8_t> in, T& v) {
        typename pbconv::msg_of<T>::type m;
        if (!m.ParseFromArray(in.data(), int(in.size()))) throw std::runtime_error("protobuf: parse failed");
        pbconv::from(v, m);
    }
};

} // namespace slg
