// SLG prototype: FlatBuffers over the std model.
// decode = Verifier + copy into std types (equivalent to UnPack); encode = build from std types.
// read_report_direct reads a report in place without unpacking (scenario D control).
#pragma once
#include <cstdint>
#include <span>
#include <stdexcept>
#include <vector>
#include <flatbuffers/flatbuffers.h>
#include "slg_generated.h"
#include "slg/generic.hpp"
#include "slg/model_std.hpp"
#include "runner.hpp"

namespace slg::fbconv {
using flatbuffers::FlatBufferBuilder;
using flatbuffers::Offset;

template <class F, class V, class Conv> auto structs(FlatBufferBuilder& b, const V& v, Conv&& c) {
    std::vector<F> tmp;
    tmp.reserve(v.size());
    for (const auto& e : v) tmp.push_back(c(e));
    return b.CreateVectorOfStructs(tmp.data(), tmp.size());
}
inline auto buffs(FlatBufferBuilder& b, const std::vector<stdm::Buff>& v) {
    return structs<fb::Buff>(b, v, [](const stdm::Buff& x) { return fb::Buff(x.id, x.stacks, x.expire_ms); });
}
inline auto troops(FlatBufferBuilder& b, const std::vector<stdm::Troop>& v) {
    return structs<fb::Troop>(b, v, [](const stdm::Troop& x) { return fb::Troop(x.type, x.count, x.wounded); });
}
inline auto kv32(FlatBufferBuilder& b, const std::map<int32_t, int32_t>& m) {
    return structs<fb::KV32>(b, m, [](const auto& kv) { return fb::KV32(kv.first, kv.second); });
}
inline auto kv64(FlatBufferBuilder& b, const std::map<int32_t, int64_t>& m) {
    return structs<fb::KV64>(b, m, [](const auto& kv) { return fb::KV64(kv.first, kv.second); });
}
inline Offset<fb::Hero> build(FlatBufferBuilder& b, const stdm::Hero& h) {
    auto name = b.CreateString(h.name);
    auto sk = structs<fb::Skill>(b, h.skills, [](const stdm::Skill& x) { return fb::Skill(x.id, x.level); });
    auto bf = buffs(b, h.buffs);
    return fb::CreateHero(b, h.id, h.level, h.star, name, sk, bf);
}
inline Offset<fb::Army> build(FlatBufferBuilder& b, const stdm::Army& a);
inline Offset<fb::City> build(FlatBufferBuilder& b, const stdm::City& c);
inline Offset<fb::March> build(FlatBufferBuilder& b, const stdm::March& m);
template <class V> auto tables(FlatBufferBuilder& b, const V& v) {
    using O = decltype(build(b, v[0]));
    std::vector<O> tmp;
    tmp.reserve(v.size());
    for (const auto& e : v) tmp.push_back(build(b, e));
    return b.CreateVector(tmp);
}
inline Offset<fb::Army> build(FlatBufferBuilder& b, const stdm::Army& a) {
    auto h = tables(b, a.heroes);
    auto t = troops(b, a.troops);
    auto bf = buffs(b, a.buffs);
    return fb::CreateArmy(b, a.army_id, a.owner_id, a.x, a.y, h, t, bf);
}
inline Offset<fb::City> build(FlatBufferBuilder& b, const stdm::City& c) {
    auto bl = kv32(b, c.buildings);
    auto rs = kv64(b, c.resources);
    return fb::CreateCity(b, c.city_id, c.level, bl, rs);
}
inline Offset<fb::PlayerState> build(FlatBufferBuilder& b, const stdm::PlayerState& p) {
    auto name = b.CreateString(p.name);
    auto city = build(b, p.city);
    auto ar = tables(b, p.armies);
    auto he = tables(b, p.heroes);
    auto rs = kv32(b, p.research);
    auto it = kv64(b, p.items);
    return fb::CreatePlayerState(b, p.player_id, name, city, ar, he, rs, it);
}
inline Offset<fb::Report> build(FlatBufferBuilder& b, const stdm::Report& r) {
    auto s = b.CreateString(r.summary);
    auto ev = structs<fb::Event>(b, r.events, [](const stdm::Event& e) { return fb::Event(e.round, e.hero_id, e.skill_id, e.target, e.damage); });
    return fb::CreateReport(b, r.battle_id, s, ev);
}
inline Offset<fb::BattleMsg> build_battle(FlatBufferBuilder& b, int64_t id, int64_t seed, int32_t winner,
                                          const stdm::Army& a, const stdm::Army& d, const stdm::Report& r) {
    auto oa = build(b, a);
    auto od = build(b, d);
    auto orp = build(b, r);
    return fb::CreateBattleMsg(b, id, seed, winner, oa, od, orp);
}
inline Offset<fb::BattleMsg> build(FlatBufferBuilder& b, const stdm::BattleMsg& m) {
    return build_battle(b, m.battle_id, m.seed, m.winner, m.attacker, m.defender, m.report);
}
inline Offset<fb::March> build(FlatBufferBuilder& b, const stdm::March& m) {
    auto t = troops(b, m.troops);
    return fb::CreateMarch(b, m.march_id, m.owner, m.from_x, m.from_y, m.to_x, m.to_y, m.eta, t);
}
inline Offset<fb::ShardState> build(FlatBufferBuilder& b, const stdm::ShardState& s) {
    auto tl = structs<fb::Tile>(b, s.tiles, [](const stdm::Tile& t) { return fb::Tile(t.x, t.y, t.terrain, t.level, t.owner); });
    auto ci = tables(b, s.cities);
    auto ma = tables(b, s.marches);
    return fb::CreateShardState(b, s.shard_id, s.tick, s.width, tl, ci, ma);
}

template <class V, class FV, class Conv> void unpack_vec(V& out, const FV* in, Conv&& c) {
    out.clear();
    if (!in) return;
    out.reserve(in->size());
    for (auto it = in->begin(); it != in->end(); ++it) c(out.emplace_back(), *it);
}
template <class M, class FV> void unpack_map(M& out, const FV* in) {
    out.clear();
    if (!in) return;
    for (auto it = in->begin(); it != in->end(); ++it) out[it->k()] = it->v();
}
inline void unpack_buffs(std::vector<stdm::Buff>& o, const flatbuffers::Vector<const fb::Buff*>* in) {
    unpack_vec(o, in, [](stdm::Buff& d, const fb::Buff* s) { d.id = s->id(); d.stacks = s->stacks(); d.expire_ms = s->expire_ms(); });
}
inline void unpack_troops(std::vector<stdm::Troop>& o, const flatbuffers::Vector<const fb::Troop*>* in) {
    unpack_vec(o, in, [](stdm::Troop& d, const fb::Troop* s) { d.type = s->type(); d.count = s->count(); d.wounded = s->wounded(); });
}
inline void unpack(stdm::Hero& d, const fb::Hero* s) {
    d.id = s->id(); d.level = s->level(); d.star = s->star();
    d.name = s->name() ? s->name()->str() : std::string();
    unpack_vec(d.skills, s->skills(), [](stdm::Skill& x, const fb::Skill* y) { x.id = y->id(); x.level = y->level(); });
    unpack_buffs(d.buffs, s->buffs());
}
inline void unpack(stdm::Army& d, const fb::Army* s) {
    d.army_id = s->army_id(); d.owner_id = s->owner_id(); d.x = s->x(); d.y = s->y();
    unpack_vec(d.heroes, s->heroes(), [](stdm::Hero& x, const fb::Hero* y) { unpack(x, y); });
    unpack_troops(d.troops, s->troops());
    unpack_buffs(d.buffs, s->buffs());
}
inline void unpack(stdm::City& d, const fb::City* s) {
    d.city_id = s->city_id(); d.level = s->level();
    unpack_map(d.buildings, s->buildings()); unpack_map(d.resources, s->resources());
}
inline void unpack(stdm::PlayerState& d, const fb::PlayerState* s) {
    d.player_id = s->player_id(); d.name = s->name() ? s->name()->str() : std::string();
    if (s->city()) unpack(d.city, s->city());
    unpack_vec(d.armies, s->armies(), [](stdm::Army& x, const fb::Army* y) { unpack(x, y); });
    unpack_vec(d.heroes, s->heroes(), [](stdm::Hero& x, const fb::Hero* y) { unpack(x, y); });
    unpack_map(d.research, s->research()); unpack_map(d.items, s->items());
}
inline void unpack(stdm::Report& d, const fb::Report* s) {
    d.battle_id = s->battle_id(); d.summary = s->summary() ? s->summary()->str() : std::string();
    unpack_vec(d.events, s->events(), [](stdm::Event& x, const fb::Event* y) {
        x.round = y->round(); x.hero_id = y->hero_id(); x.skill_id = y->skill_id(); x.target = y->target(); x.damage = y->damage();
    });
}
inline void unpack(stdm::BattleMsg& d, const fb::BattleMsg* s) {
    d.battle_id = s->battle_id(); d.seed = s->seed(); d.winner = s->winner();
    if (s->attacker()) unpack(d.attacker, s->attacker());
    if (s->defender()) unpack(d.defender, s->defender());
    if (s->report()) unpack(d.report, s->report()); else d.report = {};
}
inline void unpack(stdm::March& d, const fb::March* s) {
    d.march_id = s->march_id(); d.owner = s->owner(); d.from_x = s->from_x(); d.from_y = s->from_y();
    d.to_x = s->to_x(); d.to_y = s->to_y(); d.eta = s->eta(); unpack_troops(d.troops, s->troops());
}
inline void unpack(stdm::ShardState& d, const fb::ShardState* s) {
    d.shard_id = s->shard_id(); d.tick = s->tick(); d.width = s->width();
    unpack_vec(d.tiles, s->tiles(), [](stdm::Tile& x, const fb::Tile* y) {
        x.x = y->x(); x.y = y->y(); x.terrain = y->terrain(); x.level = y->level(); x.owner = y->owner();
    });
    unpack_vec(d.cities, s->cities(), [](stdm::City& x, const fb::City* y) { unpack(x, y); });
    unpack_vec(d.marches, s->marches(), [](stdm::March& x, const fb::March* y) { unpack(x, y); });
}

template <class T> struct table_of;
template <> struct table_of<stdm::PlayerState> { using type = fb::PlayerState; };
template <> struct table_of<stdm::BattleMsg> { using type = fb::BattleMsg; };
template <> struct table_of<stdm::ShardState> { using type = fb::ShardState; };
template <> struct table_of<stdm::Report> { using type = fb::Report; };

} // namespace slg::fbconv

namespace slg {

struct FlatBuffersFormat {
    static constexpr const char* name = "flatbuffers";
    using Wire = flatbuffers::DetachedBuffer;
    static std::span<const uint8_t> bytes(const Wire& w) { return {w.data(), w.size()}; }

    template <class T> static Wire encode(const T& v) {
        flatbuffers::FlatBufferBuilder b(1024);
        b.Finish(fbconv::build(b, v));
        return b.Release();
    }
    static Wire encode_battle_request(int64_t id, int64_t seed, const stdm::Army& a, const stdm::Army& d) {
        static const stdm::Report empty{};
        flatbuffers::FlatBufferBuilder b(1024);
        b.Finish(fbconv::build_battle(b, id, seed, 0, a, d, empty));
        return b.Release();
    }
    // Reuse protocol: one builder per slot, Clear()ed per message.
    struct Reuse {
        flatbuffers::FlatBufferBuilder slot[kSlots];
        template <class T> std::span<const uint8_t> encode(int s, const T& v) {
            auto& b = slot[s];
            b.Clear();
            b.Finish(fbconv::build(b, v));
            return {b.GetBufferPointer(), b.GetSize()};
        }
        std::span<const uint8_t> encode_battle_request(int s, int64_t id, int64_t seed, const stdm::Army& a, const stdm::Army& d) {
            static const stdm::Report empty{};
            auto& b = slot[s];
            b.Clear();
            b.Finish(fbconv::build_battle(b, id, seed, 0, a, d, empty));
            return {b.GetBufferPointer(), b.GetSize()};
        }
        template <class T> void decode(std::span<const uint8_t> in, T& v) { FlatBuffersFormat::decode(in, v); }
    };

    template <class T> static const typename fbconv::table_of<T>::type* verified_root(std::span<const uint8_t> in) {
        using F = typename fbconv::table_of<T>::type;
        // Default verifier limits are too small for a large shard; raise depth/table limits.
        flatbuffers::Verifier v(in.data(), in.size(), 64, 100000000);
        if (!v.VerifyBuffer<F>(nullptr)) throw std::runtime_error("flatbuffers: verification failed");
        return flatbuffers::GetRoot<F>(in.data());
    }
    template <class T> static void decode(std::span<const uint8_t> in, T& v) { fbconv::unpack(v, verified_root<T>(in)); }

    // Scenario D control: verify, then query the report in place (no unpack).
    static uint64_t read_report_direct(std::span<const uint8_t> in) {
        const fb::Report* r = verified_root<stdm::Report>(in);
        uint64_t even_damage = 0, skill_hits = 0;
        if (const auto* ev = r->events())
            for (auto it = ev->begin(); it != ev->end(); ++it) {
                if ((it->hero_id() & 1) == 0) even_damage += uint64_t(it->damage());
                if (it->skill_id() % 7 == 0) ++skill_hits;
            }
        return even_damage * 1000003ull + skill_hits * 31ull + (r->summary() ? r->summary()->size() : 0);
    }
};

} // namespace slg
