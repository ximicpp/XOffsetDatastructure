// SLG prototype: cereal binary archive over the std model.
#pragma once
#include <concepts>
#include <cstdint>
#include <istream>
#include <span>
#include <sstream>
#include <streambuf>
#include <string>
#include <cereal/archives/binary.hpp>
#include <cereal/types/map.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/vector.hpp>
#include "slg/model_std.hpp"
#include "runner.hpp"

namespace slg::stdm {
// Only cereal archives: zpp_bits (zpp_bits_format.hpp) also looks up free serialize()
// functions by ADL and would otherwise pick these up for its own archives.
template <class A>
concept CerealArchive = std::derived_from<A, cereal::detail::OutputArchiveBase> || std::derived_from<A, cereal::detail::InputArchiveBase>;
template <CerealArchive A> void serialize(A& ar, Buff& v) { ar(v.id, v.stacks, v.expire_ms); }
template <CerealArchive A> void serialize(A& ar, Skill& v) { ar(v.id, v.level); }
template <CerealArchive A> void serialize(A& ar, Troop& v) { ar(v.type, v.count, v.wounded); }
template <CerealArchive A> void serialize(A& ar, Hero& v) { ar(v.id, v.level, v.star, v.name, v.skills, v.buffs); }
template <CerealArchive A> void serialize(A& ar, Army& v) { ar(v.army_id, v.owner_id, v.x, v.y, v.heroes, v.troops, v.buffs); }
template <CerealArchive A> void serialize(A& ar, City& v) { ar(v.city_id, v.level, v.buildings, v.resources); }
template <CerealArchive A> void serialize(A& ar, PlayerPatch& v) { ar(v.player_id, v.seq, v.army_index, v.armies, v.items, v.city_changed, v.city); }
template <CerealArchive A> void serialize(A& ar, PlayerState& v) { ar(v.player_id, v.name, v.city, v.armies, v.heroes, v.research, v.items); }
template <CerealArchive A> void serialize(A& ar, Event& v) { ar(v.round, v.hero_id, v.skill_id, v.target, v.damage); }
template <CerealArchive A> void serialize(A& ar, Report& v) { ar(v.battle_id, v.summary, v.events); }
template <CerealArchive A> void serialize(A& ar, BattleMsg& v) { ar(v.battle_id, v.seed, v.winner, v.attacker, v.defender, v.report); }
template <CerealArchive A> void serialize(A& ar, Tile& v) { ar(v.x, v.y, v.terrain, v.level, v.owner); }
template <CerealArchive A> void serialize(A& ar, March& v) { ar(v.march_id, v.owner, v.from_x, v.from_y, v.to_x, v.to_y, v.eta, v.troops); }
template <CerealArchive A> void serialize(A& ar, ShardState& v) { ar(v.shard_id, v.tick, v.width, v.tiles, v.cities, v.marches); }
template <CerealArchive A> void serialize(A& ar, UnitHero& v) { ar(v.id, v.level, v.star, v.skin); }
template <CerealArchive A> void serialize(A& ar, ViewUnit& v) {
    ar(v.id, v.kind, v.owner, v.alliance, v.x, v.y, v.hp, v.max_hp, v.state, v.level, v.faction, v.flags, v.speed, v.attack, v.defense, v.morale, v.load, v.icon, v.skin, v.banner, v.title, v.vip, v.rank, v.slots, v.power, v.state_end, v.shield_end, v.cooldown_end, v.last_update, v.kills, v.in_battle, v.shielded, v.hidden, v.name, v.alliance_tag, v.owner_name, v.commander, v.heroes, v.troops, v.buffs);
}
template <CerealArchive A> void serialize(A& ar, ViewMarch& v) { ar(v.id, v.owner, v.x0, v.y0, v.x1, v.y1, v.t0, v.t1); }
template <CerealArchive A> void serialize(A& ar, ViewSnapshot& v) { ar(v.request_id, v.tick, v.x0, v.y0, v.x1, v.y1, v.units, v.marches, v.tiles); }
template <CerealArchive A> void serialize(A& ar, ViewDelta& v) { ar(v.request_id, v.tick, v.x0, v.y0, v.x1, v.y1, v.upserts, v.removed, v.marches, v.removed_marches, v.tiles); }
} // namespace slg::stdm

namespace slg {

struct CerealFormat {
    static constexpr const char* name = "cereal";
    using Wire = std::string;
    static std::span<const uint8_t> bytes(const Wire& w) { return {reinterpret_cast<const uint8_t*>(w.data()), w.size()}; }

    template <class T> static Wire encode(const T& v) {
        std::ostringstream os(std::ios::binary);
        { cereal::BinaryOutputArchive ar(os); ar(v); }
        return std::move(os).str();
    }
    // Same byte layout as encode(BattleMsg), written straight from the scene server's armies.
    static Wire encode_battle_request(int64_t id, int64_t seed, const stdm::Army& a, const stdm::Army& d) {
        static const stdm::Report empty{};
        std::ostringstream os(std::ios::binary);
        { cereal::BinaryOutputArchive ar(os); ar(id, seed, int32_t{0}, a, d, empty); }
        return std::move(os).str();
    }
    // Reuse protocol: one output string per slot, appended through a streambuf.
    struct StringSink : std::streambuf {
        explicit StringSink(std::string& s) : out(s) {}
        int_type overflow(int_type c) override {
            if (c != traits_type::eof()) out.push_back(char(c));
            return c;
        }
        std::streamsize xsputn(const char* p, std::streamsize n) override { out.append(p, std::size_t(n)); return n; }
        std::string& out;
    };
    struct Reuse {
        std::string slot[kSlots];
        template <class T> std::span<const uint8_t> encode(int s, const T& v) {
            auto& out = slot[s];
            out.clear();
            StringSink sink(out);
            std::ostream os(&sink);
            { cereal::BinaryOutputArchive ar(os); ar(v); }
            return bytes(out);
        }
        std::span<const uint8_t> encode_battle_request(int s, int64_t id, int64_t seed, const stdm::Army& a, const stdm::Army& d) {
            static const stdm::Report empty{};
            auto& out = slot[s];
            out.clear();
            StringSink sink(out);
            std::ostream os(&sink);
            { cereal::BinaryOutputArchive ar(os); ar(id, seed, int32_t{0}, a, d, empty); }
            return bytes(out);
        }
        // The input stream is reused across messages (fairness review 2026-11-03).
        struct InBuf : std::streambuf {
            void set(std::span<const uint8_t> in) {
                char* b = const_cast<char*>(reinterpret_cast<const char*>(in.data()));
                setg(b, b, b + in.size());
            }
        } ib;
        std::istream is{&ib};
        template <class T> void decode(std::span<const uint8_t> in, T& v) {
            ib.set(in);
            is.clear();
            cereal::BinaryInputArchive ar(is);
            ar(v);
        }
    };

    template <class T> static void decode(std::span<const uint8_t> in, T& v) {
        struct membuf : std::streambuf {
            membuf(const uint8_t* p, std::size_t n) {
                char* b = const_cast<char*>(reinterpret_cast<const char*>(p));
                setg(b, b, b + n);
            }
        } buf(in.data(), in.size());
        std::istream is(&buf);
        cereal::BinaryInputArchive ar(is);
        ar(v);
    }
};

} // namespace slg
