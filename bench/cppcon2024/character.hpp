#pragma once

#include "xoffsetdatastructure.hpp"
#include "common.hpp"

#include <array>
#include <cstring>
#include <span>
#include <type_traits>

namespace xo {
using namespace XOffsetDatastructure;
struct Skill { uint32_t id, level; };
struct Item { uint32_t id; uint32_t number; uint64_t uuid; uint64_t timestamp; };
struct Position { float x, y, z; };
struct Equip { uint32_t id; uint32_t number; uint64_t uuid; uint64_t timestamp; uint32_t level; XVector<float> attributes; };
struct Character {
  uint64_t id; XString name; uint32_t level; float healthpoint, manapoint, speed; Position pos;
  XMap<uint64_t, Item> items; XVector<Position> path; XVector<float> attributes; XVector<Equip> equips; XVector<Skill> skills;
};
}  // namespace xo
XOFFSET_RECORD(xo::Skill, id, level)
XOFFSET_RECORD(xo::Item, id, number, uuid, timestamp)
XOFFSET_RECORD(xo::Position, x, y, z)
XOFFSET_RECORD(xo::Equip, id, number, uuid, timestamp, level, attributes)
XOFFSET_RECORD(xo::Character, id, name, level, healthpoint, manapoint, speed, pos, items, path, attributes, equips, skills)
XOFFSET_REGISTER_SCHEMA_NAME(xo::Character, "bench.cppcon2024.Character")

namespace xo {
// Most equipment has at most eight attributes. Larger equipment keeps
// the same semantics through the arena-backed overflow vector.
struct InlineAttributes {
    std::array<float, 8> values{};
    XVector<float> overflow;
    uint32_t count = 0;

    void assign(std::span<const float> attrs) {
        if (attrs.size() > values.size()) {
            overflow.assign_range(attrs);
            if (overflow.size() != attrs.size()) return; // a fail-soft allocation failed
            values.fill(0);
        } else {
            if (!attrs.empty()) std::memmove(values.data(), attrs.data(), attrs.size_bytes());
            if (!overflow.empty()) overflow.clear();
            std::fill(values.begin() + attrs.size(), values.end(), 0);
        }
        count = static_cast<uint32_t>(attrs.size());
    }

    const float* begin() const {
        if (count <= values.size()) return values.data();
        if (count != overflow.size()) throw std::runtime_error("invalid equipment attribute count");
        return overflow.data();
    }
    const float* end() const { return begin() + count; }
    uint32_t size() const { return count; }
};

struct InlineEquip {
    uint32_t id, number;
    uint64_t uuid, timestamp;
    uint32_t level;
    InlineAttributes attributes;
};


struct CharacterInline {
    uint64_t id;
    XString name;
    uint32_t level;
    float healthpoint, manapoint, speed;
    Position pos;
    XMap<uint64_t, Item> items;
    XVector<Position> path;
    XVector<float> attributes;
    XVector<InlineEquip> equips;
    XVector<Skill> skills;
};
}  // namespace xo

XOFFSET_RECORD(xo::InlineAttributes, values, overflow, count)
XOFFSET_RECORD(xo::InlineEquip, id, number, uuid, timestamp, level, attributes)
XOFFSET_RECORD(xo::CharacterInline, id, name, level, healthpoint, manapoint, speed,
               pos, items, path, attributes, equips, skills)
XOFFSET_REGISTER_SCHEMA_NAME(xo::CharacterInline, "bench.cppcon2024.CharacterInlineSorted")

namespace xo {
template<class C>
inline void fill(C& r, const cc24::stdm::Character& c) {
  r.id = c.id; r.name = c.name.c_str(); r.level = c.level; r.healthpoint = c.healthpoint; r.manapoint = c.manapoint; r.speed = c.speed;
  r.pos = {c.pos.x, c.pos.y, c.pos.z};
  for (const auto& [k, it] : c.items)
    r.items[k] = Item{it.id, it.number, it.uuid, it.timestamp};
  for (const auto& p : c.path) r.path.push_back(xo::Position{p.x, p.y, p.z});
  for (float a : c.attributes) r.attributes.push_back(a);
  for (const auto& e : c.equips) {
    auto& x = r.equips.emplace_back(); x.id = e.id; x.number = e.number; x.uuid = e.uuid; x.timestamp = e.timestamp; x.level = e.level;
    if constexpr (std::is_same_v<C, xo::Character>) {
      for (float a : e.attributes) x.attributes.push_back(a);
    } else {
      x.attributes.assign(e.attributes);
    }
  }
  for (const auto& s : c.skills) r.skills.push_back(xo::Skill{s.id, s.level});
}
}  // namespace xo
