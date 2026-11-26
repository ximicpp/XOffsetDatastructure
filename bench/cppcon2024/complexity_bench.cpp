// Iso-byte structural complexity benchmark across all 8 CppCon 2024 libraries:
//   - Flat-3KB  : Original 2024 Character (~3.0 KB, ~80% flat POD vectors, 14 containers, depth 2)
//   - Rich-3KB  : Nested game Character (~3.1 KB zpp / ~4.4 KB XOffset, 107 containers, depth 4, L1d-resident)
//   - Flat-12KB : 2024 Character scaled k=4 (~11.5 KB, ~82% flat POD vectors, 38 containers, depth 2)
//   - Rich-12KB : Commercial nested Character (~11.9 KB zpp / ~16.3 KB XOffset, 373 containers, depth 4)
// Compares trusted_reuse, varied_reuse (16 shapes, fits in L2), checked_reuse, fresh, send/recv split, and view.

#include "xoffsetdatastructure.hpp"
#include "xoffset_receiver.hpp"

#define MSGPACK_NO_BOOST 1
#include <msgpack.hpp>
#include "zpp_bits.h"
#include <cista/containers/pair.h>
#include <cista/containers/string.h>
#include <cista/containers/vector.h>
#include <cista/mode.h>
#include <cista/serialization.h>
#include <cista/targets/buf.h>
#include <flatbuffers/flatbuffers.h>
#include <capnp/message.h>
#include <capnp/serialize.h>
#include <zstd.h>

#include "character_generated.h"
#include "character.capnp.h"
#include "character.pb.h"
#include "rich_character_generated.h"
#include "rich_character.capnp.h"
#include "rich_character.pb.h"

#include "common.hpp"
#include "character.hpp"

#include <algorithm>
#include <cstring>
#include <functional>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

using namespace cc24;
using namespace XOffsetDatastructure;

// ======================== Flat Cista models ========================
#define CC24_CISTA_FLAT(NS) \
  namespace cista_##NS { \
  struct Skill { uint32_t id, level; }; \
  struct Item { uint32_t id; uint32_t number; uint64_t uuid; uint64_t timestamp; }; \
  struct Position { float x, y, z; }; \
  struct Equip { uint32_t id; uint32_t number; uint64_t uuid; uint64_t timestamp; uint32_t level; cista::NS::vector<float> attributes; }; \
  struct Character { \
    uint64_t id; cista::NS::string name; uint32_t level; float healthpoint, manapoint, speed; Position pos; \
    cista::NS::vector<cista::pair<uint64_t, Item>> items; cista::NS::vector<Position> path; \
    cista::NS::vector<float> attributes; cista::NS::vector<Equip> equips; cista::NS::vector<Skill> skills; \
  }; }
CC24_CISTA_FLAT(raw)
CC24_CISTA_FLAT(offset)

// ======================== Rich models (std / XOffset / cista) ========================
namespace rich_std {
struct Position { float x, y, z; };
struct Affix { uint32_t attr_id; float value; };
struct Gem { uint32_t id; uint32_t tier; std::string name; std::vector<Affix> bonuses; };
struct RichItem {
  uint32_t id; uint32_t number; uint64_t uuid; uint64_t timestamp;
  std::string name; std::vector<Affix> affixes; std::vector<uint32_t> flags;
};
struct RichEquip {
  uint32_t id; uint32_t number; uint64_t uuid; uint64_t timestamp; uint32_t level;
  std::string name; std::string creator; std::vector<float> attributes; std::vector<Gem> gems;
};
struct SkillEffect { uint32_t effect_id; float magnitude; float duration; std::string tag; };
struct RichSkill { uint32_t id; uint32_t level; uint32_t cooldown_ms; std::string name; std::vector<SkillEffect> effects; };
struct QuestObjective { uint32_t target_id; uint32_t current; uint32_t required; std::string desc; };
struct Quest { uint32_t quest_id; uint32_t status; uint64_t accepted_at; std::string title; std::vector<QuestObjective> objectives; };
struct RichCharacter {
  uint64_t id; std::string name; std::string guild_name; std::string title;
  uint32_t level; float healthpoint, manapoint, speed; Position pos;
  std::vector<std::pair<uint64_t, RichItem>> items;
  std::vector<Position> path;
  std::vector<float> attributes;
  std::vector<RichEquip> equips;
  std::vector<RichSkill> skills;
  std::vector<std::pair<uint32_t, Quest>> quests;
};
}  // namespace rich_std

namespace xo_rich {
using namespace XOffsetDatastructure;
struct Position { float x, y, z; };
struct Affix { uint32_t attr_id; float value; };
struct Gem { uint32_t id; uint32_t tier; XString name; XVector<Affix> bonuses; };
struct RichItem {
  uint32_t id; uint32_t number; uint64_t uuid; uint64_t timestamp;
  XString name; XVector<Affix> affixes; XVector<uint32_t> flags;
};
struct RichEquip {
  uint32_t id; uint32_t number; uint64_t uuid; uint64_t timestamp; uint32_t level;
  XString name; XString creator; XVector<float> attributes; XVector<Gem> gems;
};
struct SkillEffect { uint32_t effect_id; float magnitude; float duration; XString tag; };
struct RichSkill { uint32_t id; uint32_t level; uint32_t cooldown_ms; XString name; XVector<SkillEffect> effects; };
struct QuestObjective { uint32_t target_id; uint32_t current; uint32_t required; XString desc; };
struct Quest { uint32_t quest_id; uint32_t status; uint64_t accepted_at; XString title; XVector<QuestObjective> objectives; };
struct RichCharacter {
  uint64_t id; XString name; XString guild_name; XString title;
  uint32_t level; float healthpoint, manapoint, speed; Position pos;
  XMap<uint64_t, RichItem> items;
  XVector<Position> path;
  XVector<float> attributes;
  XVector<RichEquip> equips;
  XVector<RichSkill> skills;
  XMap<uint32_t, Quest> quests;
};
}  // namespace xo_rich

XOFFSET_RECORD(xo_rich::Position, x, y, z)
XOFFSET_RECORD(xo_rich::Affix, attr_id, value)
XOFFSET_RECORD(xo_rich::Gem, id, tier, name, bonuses)
XOFFSET_RECORD(xo_rich::RichItem, id, number, uuid, timestamp, name, affixes, flags)
XOFFSET_RECORD(xo_rich::RichEquip, id, number, uuid, timestamp, level, name, creator, attributes, gems)
XOFFSET_RECORD(xo_rich::SkillEffect, effect_id, magnitude, duration, tag)
XOFFSET_RECORD(xo_rich::RichSkill, id, level, cooldown_ms, name, effects)
XOFFSET_RECORD(xo_rich::QuestObjective, target_id, current, required, desc)
XOFFSET_RECORD(xo_rich::Quest, quest_id, status, accepted_at, title, objectives)
XOFFSET_RECORD(xo_rich::RichCharacter, id, name, guild_name, title, level, healthpoint, manapoint, speed, pos,
               items, path, attributes, equips, skills, quests)
XOFFSET_REGISTER_SCHEMA_NAME(xo_rich::RichCharacter, "bench.cppcon2024.RichCharacter")

#define CC24_CISTA_RICH(NS) \
  namespace cista_rich_##NS { \
  struct Position { float x, y, z; }; \
  struct Affix { uint32_t attr_id; float value; }; \
  struct Gem { uint32_t id; uint32_t tier; cista::NS::string name; cista::NS::vector<Affix> bonuses; }; \
  struct RichItem { uint32_t id; uint32_t number; uint64_t uuid; uint64_t timestamp; cista::NS::string name; cista::NS::vector<Affix> affixes; cista::NS::vector<uint32_t> flags; }; \
  struct RichEquip { uint32_t id; uint32_t number; uint64_t uuid; uint64_t timestamp; uint32_t level; cista::NS::string name; cista::NS::string creator; cista::NS::vector<float> attributes; cista::NS::vector<Gem> gems; }; \
  struct SkillEffect { uint32_t effect_id; float magnitude; float duration; cista::NS::string tag; }; \
  struct RichSkill { uint32_t id; uint32_t level; uint32_t cooldown_ms; cista::NS::string name; cista::NS::vector<SkillEffect> effects; }; \
  struct QuestObjective { uint32_t target_id; uint32_t current; uint32_t required; cista::NS::string desc; }; \
  struct Quest { uint32_t quest_id; uint32_t status; uint64_t accepted_at; cista::NS::string title; cista::NS::vector<QuestObjective> objectives; }; \
  struct RichCharacter { \
    uint64_t id; cista::NS::string name; cista::NS::string guild_name; cista::NS::string title; \
    uint32_t level; float healthpoint, manapoint, speed; Position pos; \
    cista::NS::vector<cista::pair<uint64_t, RichItem>> items; \
    cista::NS::vector<Position> path; \
    cista::NS::vector<float> attributes; \
    cista::NS::vector<RichEquip> equips; \
    cista::NS::vector<RichSkill> skills; \
    cista::NS::vector<cista::pair<uint32_t, Quest>> quests; \
  }; }
CC24_CISTA_RICH(raw)
CC24_CISTA_RICH(offset)

// ======================== MessagePack bindings ========================
namespace msgpack { MSGPACK_API_VERSION_NAMESPACE(MSGPACK_DEFAULT_API_NS) { namespace adaptor {
#define MP_BIND(T, ...) \
  template <> struct convert<T> { const msgpack::object& operator()(const msgpack::object& _obj, T& _val) const { \
      auto& [__VA_ARGS__] = _val; msgpack::type::make_define_array(__VA_ARGS__).msgpack_unpack(_obj); return _obj; } }; \
  template <> struct pack<T> { template <class S> msgpack::packer<S>& operator()(msgpack::packer<S>& _pk, const T& _val) const { \
      const auto& [__VA_ARGS__] = _val; msgpack::type::make_define_array(__VA_ARGS__).msgpack_pack(_pk); return _pk; } };
MP_BIND(cc24::stdm::Skill, a, b)
MP_BIND(cc24::stdm::Item, a, b, c, d)
MP_BIND(cc24::stdm::Position, a, b, c)
MP_BIND(cc24::stdm::Equip, a, b, c, d, e, f)
MP_BIND(cc24::stdm::Character, a, b, c, d, e, f, g, h, i, j, k, l)

MP_BIND(rich_std::Position, a, b, c)
MP_BIND(rich_std::Affix, a, b)
MP_BIND(rich_std::Gem, a, b, c, d)
MP_BIND(rich_std::RichItem, a, b, c, d, e, f, g)
MP_BIND(rich_std::RichEquip, a, b, c, d, e, f, g, h, i)
MP_BIND(rich_std::SkillEffect, a, b, c, d)
MP_BIND(rich_std::RichSkill, a, b, c, d, e)
MP_BIND(rich_std::QuestObjective, a, b, c, d)
MP_BIND(rich_std::Quest, a, b, c, d, e)
MP_BIND(rich_std::RichCharacter, a, b, c, d, e, f, g, h, i, j, k, l, m, n, o)
}}}

// ======================== Utilities ========================
static volatile uint64_t g_sink = 0;
static bool g_quick = false;
static inline void clobber(const void* p) { asm volatile("" : : "r"(p) : "memory"); }

static size_t zstd3(const void* p, size_t n) {
  std::vector<char> out(ZSTD_compressBound(n));
  size_t z = ZSTD_compress(out.data(), out.size(), p, n, 3);
  if (ZSTD_isError(z)) throw std::runtime_error("zstd");
  return z;
}

static void expect(const char* tag, uint64_t got, uint64_t want) {
  if (got != want) {
    std::fprintf(stderr, "CHECKSUM MISMATCH [%s]: got=%llu want=%llu\n",
                 tag, (unsigned long long)got, (unsigned long long)want);
    std::exit(2);
  }
}

static void put(const char* workload, const char* metric, const char* lib, double v) {
  std::printf("%-10s %-16s %-20s %10.1f\n", workload, metric, lib, v);
  std::fflush(stdout);
}

template <class F>
static double time_op(F&& f, size_t min_iters = 100, int rounds = 7) {
  if (g_quick) { min_iters = 20; rounds = 3; }
  uint64_t acc = 0;
  for (int i = 0; i < 8; ++i) acc += f(size_t(i));
  size_t iters = min_iters;
  while (true) {
    auto t0 = Clock::now();
    for (size_t i = 0; i < iters; ++i) acc += f(i);
    double ms = std::chrono::duration<double, std::milli>(Clock::now() - t0).count();
    const double target_ms = g_quick ? 1.5 : 8.0;
    const size_t cap_iters = g_quick ? 5000 : 100000;
    if (ms >= (g_quick ? 1.0 : 6.0) || iters >= cap_iters) {
      if (ms > 0.05) iters = std::clamp<size_t>(size_t(double(iters) * (target_ms / ms)), min_iters, cap_iters);
      break;
    }
    iters *= 4;
  }
  std::vector<double> samples;
  samples.reserve(size_t(rounds));
  for (int r = 0; r < rounds; ++r) {
    auto t0 = Clock::now();
    for (size_t i = 0; i < iters; ++i) acc += f(i);
    auto t1 = Clock::now();
    samples.push_back(std::chrono::duration<double, std::nano>(t1 - t0).count() / double(iters));
  }
  g_sink = g_sink + acc;
  std::sort(samples.begin(), samples.end());
  return samples[samples.size() / 2];
}

// Cap'n Proto zero-alloc flat writer
static kj::ArrayPtr<const capnp::word> cp_write_flat_reused(capnp::MessageBuilder& msg, std::vector<capnp::word>& out) {
  auto segments = msg.getSegmentsForOutput();
  size_t header_words = (segments.size() + 2) / 2;
  size_t total_words = header_words;
  for (auto s : segments) total_words += s.size();
  out.resize(total_words);
  auto* dst = reinterpret_cast<uint32_t*>(out.data());
  dst[0] = uint32_t(segments.size() - 1);
  for (size_t i = 0; i < segments.size(); ++i) dst[i + 1] = uint32_t(segments[i].size());
  if ((segments.size() & 1) == 0) dst[segments.size() + 1] = 0;
  capnp::word* wp = out.data() + header_words;
  for (auto s : segments) {
    std::memcpy(wp, s.begin(), s.size() * sizeof(capnp::word));
    wp += s.size();
  }
  return kj::ArrayPtr<const capnp::word>(out.data(), total_words);
}

// ======================== Flat Character generators & adapters ========================
static stdm::Character make_flat_scaled(size_t k) {
  Gen r;
  const std::string cs = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
  std::uniform_int_distribution<int> d(0, int(cs.size()) - 1);
  std::string name; for (int i = 0; i < 8; ++i) name += cs[size_t(d(r.g))];
  r.g.seed(0);
  stdm::Character c{};
  c.id = r.u64(); c.name = name; c.level = r.u32();
  c.healthpoint = r.fl(); c.manapoint = r.fl(); c.speed = r.fl();
  c.pos = {r.fl(), r.fl(), r.fl()};
  for (size_t i = 0; i < 64 * k; ++i) {
    uint64_t key = r.u64();
    stdm::Item it{r.u32(), 0, r.u64(), 0}; it.number = r.u32(); it.timestamp = r.u64();
    items_set(c.items, key, it);
  }
  for (size_t i = 0; i < 16 * k; ++i) c.path.push_back({r.fl(), r.fl(), r.fl()});
  for (size_t i = 0; i < 16 * k; ++i) c.attributes.push_back(r.fl());
  for (size_t i = 0; i < 8 * k; ++i) {
    stdm::Equip e{r.u32(), 0, r.u64(), 0, 0, {}};
    e.number = r.u32(); e.timestamp = r.u64(); e.level = r.u32();
    for (int j = 0; j < 8; ++j) e.attributes.push_back(r.fl());
    c.equips.push_back(std::move(e));
  }
  for (size_t i = 0; i < 4 * k; ++i) c.skills.push_back({r.u32(), r.u32()});
  return c;
}

template <class Ch>
static inline uint64_t touch_flat(const Ch& c) {
  return c.id ^ uint64_t(c.items.size()) ^ uint64_t(c.equips.size()) ^
         (c.equips.empty() ? 0ull : fbits(c.equips.back().attributes.back()));
}

static void fill_flat_pb(protobuf_test::Character& m, const stdm::Character& c) {
  m.set_id(c.id); m.set_name(c.name); m.set_level(c.level);
  m.set_healthpoint(c.healthpoint); m.set_manapoint(c.manapoint); m.set_speed(c.speed);
  auto* p = m.mutable_pos(); p->set_x(c.pos.x); p->set_y(c.pos.y); p->set_z(c.pos.z);
  for (const auto& [k, it] : c.items) { auto& x = (*m.mutable_items())[k]; x.set_id(it.id); x.set_uuid(it.uuid); x.set_number(it.number); x.set_timestamp(it.timestamp); }
  for (const auto& q : c.path) { auto* x = m.add_path(); x->set_x(q.x); x->set_y(q.y); x->set_z(q.z); }
  for (float a : c.attributes) m.add_attributes(a);
  for (const auto& e : c.equips) { auto* x = m.add_equips(); x->set_id(e.id); x->set_uuid(e.uuid); x->set_number(e.number); x->set_timestamp(e.timestamp); x->set_level(e.level); for (float a : e.attributes) x->add_attributes(a); }
  for (const auto& s : c.skills) { auto* x = m.add_skills(); x->set_id(s.id); x->set_level(s.level); }
}

static uint64_t checksum_flat_pb(const protobuf_test::Character& m) {
  stdm::Character c{};
  c.id = m.id(); c.name = m.name(); c.level = m.level(); c.healthpoint = m.healthpoint(); c.manapoint = m.manapoint(); c.speed = m.speed();
  c.pos = {m.pos().x(), m.pos().y(), m.pos().z()};
  for (const auto& [k, it] : m.items()) c.items.push_back({k, stdm::Item{it.id(), it.number(), it.uuid(), it.timestamp()}});
  for (const auto& q : m.path()) c.path.push_back({q.x(), q.y(), q.z()});
  for (float a : m.attributes()) c.attributes.push_back(a);
  for (const auto& e : m.equips()) c.equips.push_back(stdm::Equip{e.id(), e.number(), e.uuid(), e.timestamp(), e.level(), {e.attributes().begin(), e.attributes().end()}});
  for (const auto& s : m.skills()) c.skills.push_back({s.id(), s.level()});
  return checksum(c);
}

namespace fbt = flatbuffers_test;
static void fb_flat_encode(flatbuffers::FlatBufferBuilder& b, std::vector<flatbuffers::Offset<fbt::Equip>>& eq, const stdm::Character& c) {
  b.Clear(); eq.clear();
  for (const auto& e : c.equips) {
    auto a = b.CreateVector(e.attributes.data(), e.attributes.size());
    fbt::EquipBuilder eb(b); eb.add_id(e.id); eb.add_uuid(e.uuid); eb.add_number(e.number); eb.add_timestamp(e.timestamp); eb.add_level(e.level); eb.add_attributes(a);
    eq.push_back(eb.Finish());
  }
  auto equips = b.CreateVector(eq);
  auto name = b.CreateString(c.name);
  fbt::Position* pp; auto path = b.CreateUninitializedVectorOfStructs<fbt::Position>(c.path.size(), &pp);
  for (const auto& q : c.path) *pp++ = fbt::Position(q.x, q.y, q.z);
  auto attrs = b.CreateVector(c.attributes.data(), c.attributes.size());
  fbt::ItemsEntry* ip; auto items = b.CreateUninitializedVectorOfStructs<fbt::ItemsEntry>(c.items.size(), &ip);
  for (const auto& [k, it] : c.items) *ip++ = fbt::ItemsEntry(k, fbt::Item(it.id, it.uuid, it.number, it.timestamp));
  fbt::Skill* sp; auto skills = b.CreateUninitializedVectorOfStructs<fbt::Skill>(c.skills.size(), &sp);
  for (const auto& s : c.skills) *sp++ = fbt::Skill(s.id, s.level);
  fbt::CharacterBuilder cb(b);
  cb.add_id(c.id); cb.add_name(name); cb.add_level(c.level); cb.add_healthpoint(c.healthpoint); cb.add_manapoint(c.manapoint); cb.add_speed(c.speed);
  fbt::Position pos(c.pos.x, c.pos.y, c.pos.z); cb.add_pos(&pos);
  cb.add_path(path); cb.add_attributes(attrs); cb.add_items(items); cb.add_equips(equips); cb.add_skills(skills);
  b.Finish(cb.Finish());
}
static void fb_flat_decode(const uint8_t* data, stdm::Character& c) {
  const auto* r = fbt::GetCharacter(data);
  c.id = r->id(); c.name.assign(r->name()->c_str(), r->name()->size()); c.level = r->level();
  c.healthpoint = r->healthpoint(); c.manapoint = r->manapoint(); c.speed = r->speed();
  c.pos = {r->pos()->x(), r->pos()->y(), r->pos()->z()};
  const auto* items = r->items(); c.items.resize(items->size());
  for (flatbuffers::uoffset_t i = 0; i < items->size(); ++i) { const auto* e = items->Get(i); const auto& v = e->value(); c.items[i] = {e->key(), stdm::Item{v.id(), v.number(), v.uuid(), v.timestamp()}}; }
  const auto* path = r->path(); c.path.resize(path->size());
  for (flatbuffers::uoffset_t i = 0; i < path->size(); ++i) { const auto* q = path->Get(i); c.path[i] = {q->x(), q->y(), q->z()}; }
  const auto* attrs = r->attributes(); c.attributes.assign(attrs->begin(), attrs->end());
  const auto* equips = r->equips(); c.equips.resize(equips->size());
  for (flatbuffers::uoffset_t i = 0; i < equips->size(); ++i) {
    const auto* e = equips->Get(i); auto& d = c.equips[i];
    d.id = e->id(); d.number = e->number(); d.uuid = e->uuid(); d.timestamp = e->timestamp(); d.level = e->level();
    d.attributes.assign(e->attributes()->begin(), e->attributes()->end());
  }
  const auto* skills = r->skills(); c.skills.resize(skills->size());
  for (flatbuffers::uoffset_t i = 0; i < skills->size(); ++i) { const auto* s = skills->Get(i); c.skills[i] = {s->id(), s->level()}; }
}

namespace cpt = capnp_character_test;
static void cp_flat_build(capnp::MessageBuilder& msg, const stdm::Character& c) {
  auto r = msg.initRoot<cpt::Character>();
  r.setId(c.id); r.setName(capnp::Text::Reader(c.name.data(), c.name.size())); r.setLevel(c.level);
  r.setHealthpoint(c.healthpoint); r.setManapoint(c.manapoint); r.setSpeed(c.speed);
  auto pos = r.initPos(); pos.setX(c.pos.x); pos.setY(c.pos.y); pos.setZ(c.pos.z);
  auto entries = r.initItems().initEntries(unsigned(c.items.size()));
  for (unsigned i = 0; i < c.items.size(); ++i) {
    auto e = entries[i]; e.setKey(c.items[i].first); auto v = e.initValue(); const auto& it = c.items[i].second;
    v.setId(it.id); v.setUuid(it.uuid); v.setNumber(it.number); v.setTimestamp(it.timestamp);
  }
  auto path = r.initPath(unsigned(c.path.size()));
  for (unsigned i = 0; i < c.path.size(); ++i) { path[i].setX(c.path[i].x); path[i].setY(c.path[i].y); path[i].setZ(c.path[i].z); }
  auto attrs = r.initAttributes(unsigned(c.attributes.size()));
  for (unsigned i = 0; i < c.attributes.size(); ++i) attrs.set(i, c.attributes[i]);
  auto equips = r.initEquips(unsigned(c.equips.size()));
  for (unsigned i = 0; i < c.equips.size(); ++i) {
    const auto& e = c.equips[i]; auto d = equips[i];
    d.setId(e.id); d.setUuid(e.uuid); d.setNumber(e.number); d.setTimestamp(e.timestamp); d.setLevel(e.level);
    auto a = d.initAttributes(unsigned(e.attributes.size())); for (unsigned k = 0; k < e.attributes.size(); ++k) a.set(k, e.attributes[k]);
  }
  auto skills = r.initSkills(unsigned(c.skills.size()));
  for (unsigned i = 0; i < c.skills.size(); ++i) { skills[i].setId(c.skills[i].id); skills[i].setLevel(c.skills[i].level); }
}
static void cp_flat_decode(kj::ArrayPtr<const capnp::word> words, stdm::Character& c) {
  capnp::FlatArrayMessageReader reader(words);
  auto r = reader.getRoot<cpt::Character>();
  c.id = r.getId(); auto n = r.getName(); c.name.assign(n.cStr(), n.size()); c.level = r.getLevel();
  c.healthpoint = r.getHealthpoint(); c.manapoint = r.getManapoint(); c.speed = r.getSpeed();
  auto pos = r.getPos(); c.pos = {pos.getX(), pos.getY(), pos.getZ()};
  auto entries = r.getItems().getEntries(); c.items.resize(entries.size());
  for (unsigned i = 0; i < entries.size(); ++i) { auto e = entries[i]; auto v = e.getValue(); c.items[i] = {e.getKey(), stdm::Item{v.getId(), v.getNumber(), v.getUuid(), v.getTimestamp()}}; }
  auto path = r.getPath(); c.path.resize(path.size());
  for (unsigned i = 0; i < path.size(); ++i) c.path[i] = {path[i].getX(), path[i].getY(), path[i].getZ()};
  auto attrs = r.getAttributes(); c.attributes.resize(attrs.size());
  for (unsigned i = 0; i < attrs.size(); ++i) c.attributes[i] = attrs[i];
  auto equips = r.getEquips(); c.equips.resize(equips.size());
  for (unsigned i = 0; i < equips.size(); ++i) {
    auto e = equips[i]; auto& d = c.equips[i];
    d.id = e.getId(); d.number = e.getNumber(); d.uuid = e.getUuid(); d.timestamp = e.getTimestamp(); d.level = e.getLevel();
    auto a = e.getAttributes(); d.attributes.resize(a.size()); for (unsigned k = 0; k < a.size(); ++k) d.attributes[k] = a[k];
  }
  auto skills = r.getSkills(); c.skills.resize(skills.size());
  for (unsigned i = 0; i < skills.size(); ++i) c.skills[i] = {skills[i].getId(), skills[i].getLevel()};
}
template <class Ch>
static void fill_flat_cista(Ch& r, const stdm::Character& c) {
  r.id = c.id; r.name = c.name; r.level = c.level; r.healthpoint = c.healthpoint; r.manapoint = c.manapoint; r.speed = c.speed;
  r.pos = {c.pos.x, c.pos.y, c.pos.z};
  for (const auto& [k, it] : c.items) { auto& e = r.items.emplace_back(); e.first = k; e.second = {it.id, it.number, it.uuid, it.timestamp}; }
  for (const auto& p : c.path) { r.path.emplace_back(); r.path.back() = {p.x, p.y, p.z}; }
  for (float a : c.attributes) r.attributes.push_back(a);
  for (const auto& e : c.equips) {
    auto& x = r.equips.emplace_back(); x.id = e.id; x.number = e.number; x.uuid = e.uuid; x.timestamp = e.timestamp; x.level = e.level;
    for (float a : e.attributes) x.attributes.push_back(a);
  }
  for (const auto& s : c.skills) { r.skills.emplace_back(); r.skills.back() = {s.id, s.level}; }
}

// ======================== Rich Character generator, checksum & adapters ========================
static std::string make_str(std::mt19937_64& rng, const char* prefix, size_t min_len, size_t max_len) {
  static const char cs[] = "abcdefghijklmnopqrstuvwxyz0123456789_";
  std::string s(prefix);
  size_t target = min_len + (rng() % (max_len - min_len + 1));
  while (s.size() < target) s.push_back(cs[rng() % (sizeof(cs) - 1)]);
  return s;
}

struct RichProfile {
  size_t items, items_d;
  size_t affixes, flags;
  size_t path, path_d;
  size_t attrs, attrs_d;
  size_t equips, equips_d, equip_attrs, gems, bonuses;
  size_t skills, skills_d, effects;
  size_t quests, quests_d, objs;
};

static constexpr RichProfile kRich3KB  {14, 3, 2, 2,  8, 2,  8, 2,  3, 1, 6, 2, 2,  4, 1, 2,  4, 1, 2};
static constexpr RichProfile kRich12KB {48, 8, 3, 3, 16, 4, 16, 4, 10, 2, 8, 3, 2, 10, 2, 3, 14, 3, 3};

static rich_std::RichCharacter make_rich_character(uint64_t seed, const RichProfile& P, bool vary = false) {
  std::mt19937_64 rng(seed);
  std::uniform_real_distribution<float> rf(0.0f, 1000.0f);
  auto pick = [&](size_t base, size_t delta) {
    return (vary && delta > 0) ? (base - delta + size_t(rng() % (2 * delta + 1))) : base;
  };
  rich_std::RichCharacter c{};
  c.id = rng();
  c.name = make_str(rng, "Hero_", 18, 28);
  c.guild_name = make_str(rng, "Guild_", 18, 28);
  c.title = make_str(rng, "Title_", 18, 28);
  c.level = uint32_t(rng());
  c.healthpoint = rf(rng); c.manapoint = rf(rng); c.speed = rf(rng);
  c.pos = {rf(rng), rf(rng), rf(rng)};

  size_t ni = pick(P.items, P.items_d);
  for (size_t i = 0; i < ni; ++i) {
    uint64_t key = rng();
    rich_std::RichItem it{};
    it.id = uint32_t(rng()); it.number = uint32_t(rng()); it.uuid = rng(); it.timestamp = rng();
    it.name = make_str(rng, "Item_", 18, 28);
    size_t na = pick(P.affixes, 1);
    for (size_t j = 0; j < na; ++j) it.affixes.push_back({uint32_t(rng()), rf(rng)});
    size_t nf = pick(P.flags, 1);
    for (size_t j = 0; j < nf; ++j) it.flags.push_back(uint32_t(rng()));
    items_set(c.items, key, it);
  }
  for (size_t i = 0; i < pick(P.path, P.path_d); ++i) c.path.push_back({rf(rng), rf(rng), rf(rng)});
  for (size_t i = 0; i < pick(P.attrs, P.attrs_d); ++i) c.attributes.push_back(rf(rng));

  size_t ne = pick(P.equips, P.equips_d);
  for (size_t i = 0; i < ne; ++i) {
    rich_std::RichEquip e{};
    e.id = uint32_t(rng()); e.number = uint32_t(rng()); e.uuid = rng(); e.timestamp = rng(); e.level = uint32_t(rng());
    e.name = make_str(rng, "Equip_", 18, 28);
    e.creator = make_str(rng, "Smith_", 16, 24);
    for (size_t j = 0; j < pick(P.equip_attrs, 2); ++j) e.attributes.push_back(rf(rng));
    for (size_t g = 0; g < pick(P.gems, 1); ++g) {
      rich_std::Gem gem{};
      gem.id = uint32_t(rng()); gem.tier = uint32_t(rng());
      gem.name = make_str(rng, "Gem_", 16, 24);
      for (size_t b = 0; b < pick(P.bonuses, 1); ++b) gem.bonuses.push_back({uint32_t(rng()), rf(rng)});
      e.gems.push_back(std::move(gem));
    }
    c.equips.push_back(std::move(e));
  }
  size_t ns = pick(P.skills, P.skills_d);
  for (size_t i = 0; i < ns; ++i) {
    rich_std::RichSkill s{};
    s.id = uint32_t(rng()); s.level = uint32_t(rng()); s.cooldown_ms = uint32_t(rng());
    s.name = make_str(rng, "Skill_", 16, 24);
    for (size_t f = 0; f < pick(P.effects, 1); ++f)
      s.effects.push_back({uint32_t(rng()), rf(rng), rf(rng), make_str(rng, "Eff_", 16, 22)});
    c.skills.push_back(std::move(s));
  }
  size_t nq = pick(P.quests, P.quests_d);
  for (size_t i = 0; i < nq; ++i) {
    uint32_t qk = uint32_t(1000 + i * 7 + (rng() % 5));
    rich_std::Quest q{};
    q.quest_id = qk; q.status = uint32_t(rng()); q.accepted_at = rng();
    q.title = make_str(rng, "Quest_", 20, 30);
    for (size_t o = 0; o < pick(P.objs, 1); ++o)
      q.objectives.push_back({uint32_t(rng()), uint32_t(rng()), uint32_t(rng()), make_str(rng, "Obj_", 18, 28)});
    items_set(c.quests, qk, q);
  }
  return c;
}

template <class S>
static inline uint64_t mix_str(uint64_t h, const S& s) {
  auto v = sv(s);
  h = mix(h, v.size());
  for (char ch : v) h = mix(h, uint8_t(ch));
  return h;
}

template <class Ch>
static uint64_t checksum_rich(const Ch& c) {
  uint64_t h = mix(0, c.id);
  h = mix_str(h, c.name); h = mix_str(h, c.guild_name); h = mix_str(h, c.title);
  h = mix(h, c.level); h = mix(h, fbits(c.healthpoint)); h = mix(h, fbits(c.manapoint)); h = mix(h, fbits(c.speed));
  h = mix(h, fbits(c.pos.x)); h = mix(h, fbits(c.pos.y)); h = mix(h, fbits(c.pos.z));
  uint64_t isum = 0;
  for (const auto& kv : c.items) {
    const auto& it = kv.second;
    uint64_t ih = mix(mix(mix(mix(mix(1, kv.first), it.id), it.uuid), it.number), it.timestamp);
    ih = mix_str(ih, it.name);
    ih = mix(ih, it.affixes.size());
    for (const auto& a : it.affixes) ih = mix(mix(ih, a.attr_id), fbits(a.value));
    ih = mix(ih, it.flags.size());
    for (uint32_t fl : it.flags) ih = mix(ih, fl);
    isum += ih;
  }
  h = mix(h, isum); h = mix(h, c.items.size());
  h = mix(h, c.path.size());
  for (const auto& p : c.path) h = mix(mix(mix(h, fbits(p.x)), fbits(p.y)), fbits(p.z));
  h = mix(h, c.attributes.size());
  for (float a : c.attributes) h = mix(h, fbits(a));
  h = mix(h, c.equips.size());
  for (const auto& e : c.equips) {
    h = mix(mix(mix(mix(mix(h, e.id), e.uuid), e.number), e.timestamp), e.level);
    h = mix_str(h, e.name); h = mix_str(h, e.creator);
    h = mix(h, e.attributes.size());
    for (float a : e.attributes) h = mix(h, fbits(a));
    h = mix(h, e.gems.size());
    for (const auto& g : e.gems) {
      h = mix(mix(h, g.id), g.tier); h = mix_str(h, g.name);
      h = mix(h, g.bonuses.size());
      for (const auto& b : g.bonuses) h = mix(mix(h, b.attr_id), fbits(b.value));
    }
  }
  h = mix(h, c.skills.size());
  for (const auto& s : c.skills) {
    h = mix(mix(mix(h, s.id), s.level), s.cooldown_ms); h = mix_str(h, s.name);
    h = mix(h, s.effects.size());
    for (const auto& ef : s.effects) {
      h = mix(mix(mix(h, ef.effect_id), fbits(ef.magnitude)), fbits(ef.duration));
      h = mix_str(h, ef.tag);
    }
  }
  uint64_t qsum = 0;
  for (const auto& kv : c.quests) {
    const auto& q = kv.second;
    uint64_t qh = mix(mix(mix(mix(2, kv.first), q.quest_id), q.status), q.accepted_at);
    qh = mix_str(qh, q.title);
    qh = mix(qh, q.objectives.size());
    for (const auto& o : q.objectives) {
      qh = mix(mix(mix(qh, o.target_id), o.current), o.required);
      qh = mix_str(qh, o.desc);
    }
    qsum += qh;
  }
  h = mix(h, qsum); h = mix(h, c.quests.size());
  return h;
}

template <class Ch>
static inline uint64_t touch_rich(const Ch& c) {
  uint64_t h = c.id ^ uint64_t(c.items.size()) ^ uint64_t(c.quests.size());
  if (!c.equips.empty() && !c.equips.back().gems.empty())
    h ^= uint64_t(sv(c.equips.back().gems.back().name).size());
  return h;
}

static size_t count_rich_containers(const rich_std::RichCharacter& c) {
  size_t n = 9; // name, guild, title, items, path, attrs, equips, skills, quests
  for (const auto& [k, it] : c.items) n += 3; // name, affixes, flags
  for (const auto& e : c.equips) {
    n += 4; // name, creator, attrs, gems
    for (const auto& g : e.gems) n += 2; // name, bonuses
  }
  for (const auto& s : c.skills) {
    n += 2; // name, effects
    n += s.effects.size(); // tag
  }
  for (const auto& [k, q] : c.quests) {
    n += 2; // title, objectives
    n += q.objectives.size(); // desc
  }
  return n;
}

static void fill_xo_rich(xo_rich::RichCharacter& r, const rich_std::RichCharacter& c, bool do_reserve = true) {
  r.id = c.id; r.name = c.name.c_str(); r.guild_name = c.guild_name.c_str(); r.title = c.title.c_str();
  r.level = c.level; r.healthpoint = c.healthpoint; r.manapoint = c.manapoint; r.speed = c.speed;
  r.pos = {c.pos.x, c.pos.y, c.pos.z};
  if (do_reserve) {
    r.items.reserve(c.items.size()); r.path.reserve(c.path.size()); r.attributes.reserve(c.attributes.size());
    r.equips.reserve(c.equips.size()); r.skills.reserve(c.skills.size()); r.quests.reserve(c.quests.size());
  }
  for (const auto& [k, it] : c.items) {
    auto& d = r.items[k];
    d.id = it.id; d.number = it.number; d.uuid = it.uuid; d.timestamp = it.timestamp;
    d.name = it.name.c_str();
    if (do_reserve) { d.affixes.reserve(it.affixes.size()); d.flags.reserve(it.flags.size()); }
    for (const auto& a : it.affixes) d.affixes.push_back({a.attr_id, a.value});
    for (uint32_t fl : it.flags) d.flags.push_back(fl);
  }
  for (const auto& p : c.path) r.path.push_back({p.x, p.y, p.z});
  for (float a : c.attributes) r.attributes.push_back(a);
  for (const auto& e : c.equips) {
    auto& d = r.equips.emplace_back();
    d.id = e.id; d.number = e.number; d.uuid = e.uuid; d.timestamp = e.timestamp; d.level = e.level;
    d.name = e.name.c_str(); d.creator = e.creator.c_str();
    if (do_reserve) { d.attributes.reserve(e.attributes.size()); d.gems.reserve(e.gems.size()); }
    for (float a : e.attributes) d.attributes.push_back(a);
    for (const auto& g : e.gems) {
      auto& dg = d.gems.emplace_back();
      dg.id = g.id; dg.tier = g.tier; dg.name = g.name.c_str();
      if (do_reserve) dg.bonuses.reserve(g.bonuses.size());
      for (const auto& b : g.bonuses) dg.bonuses.push_back({b.attr_id, b.value});
    }
  }
  for (const auto& s : c.skills) {
    auto& ds = r.skills.emplace_back();
    ds.id = s.id; ds.level = s.level; ds.cooldown_ms = s.cooldown_ms; ds.name = s.name.c_str();
    if (do_reserve) ds.effects.reserve(s.effects.size());
    for (const auto& ef : s.effects) {
      auto& def = ds.effects.emplace_back();
      def.effect_id = ef.effect_id; def.magnitude = ef.magnitude; def.duration = ef.duration; def.tag = ef.tag.c_str();
    }
  }
  for (const auto& [k, q] : c.quests) {
    auto& dq = r.quests[k];
    dq.quest_id = q.quest_id; dq.status = q.status; dq.accepted_at = q.accepted_at; dq.title = q.title.c_str();
    if (do_reserve) dq.objectives.reserve(q.objectives.size());
    for (const auto& o : q.objectives) {
      auto& do_ = dq.objectives.emplace_back();
      do_.target_id = o.target_id; do_.current = o.current; do_.required = o.required; do_.desc = o.desc.c_str();
    }
  }
}

template <class Ch>
static void fill_cista_rich(Ch& r, const rich_std::RichCharacter& c) {
  r.id = c.id; r.name = c.name; r.guild_name = c.guild_name; r.title = c.title;
  r.level = c.level; r.healthpoint = c.healthpoint; r.manapoint = c.manapoint; r.speed = c.speed;
  r.pos = {c.pos.x, c.pos.y, c.pos.z};
  for (const auto& [k, it] : c.items) {
    auto& kv = r.items.emplace_back(); kv.first = k; auto& d = kv.second;
    d.id = it.id; d.number = it.number; d.uuid = it.uuid; d.timestamp = it.timestamp; d.name = it.name;
    for (const auto& a : it.affixes) { d.affixes.emplace_back(); d.affixes.back() = {a.attr_id, a.value}; }
    for (uint32_t fl : it.flags) d.flags.push_back(fl);
  }
  for (const auto& p : c.path) { r.path.emplace_back(); r.path.back() = {p.x, p.y, p.z}; }
  for (float a : c.attributes) r.attributes.push_back(a);
  for (const auto& e : c.equips) {
    auto& d = r.equips.emplace_back();
    d.id = e.id; d.number = e.number; d.uuid = e.uuid; d.timestamp = e.timestamp; d.level = e.level;
    d.name = e.name; d.creator = e.creator;
    for (float a : e.attributes) d.attributes.push_back(a);
    for (const auto& g : e.gems) {
      auto& dg = d.gems.emplace_back(); dg.id = g.id; dg.tier = g.tier; dg.name = g.name;
      for (const auto& b : g.bonuses) { dg.bonuses.emplace_back(); dg.bonuses.back() = {b.attr_id, b.value}; }
    }
  }
  for (const auto& s : c.skills) {
    auto& ds = r.skills.emplace_back(); ds.id = s.id; ds.level = s.level; ds.cooldown_ms = s.cooldown_ms; ds.name = s.name;
    for (const auto& ef : s.effects) {
      auto& def = ds.effects.emplace_back(); def.effect_id = ef.effect_id; def.magnitude = ef.magnitude; def.duration = ef.duration; def.tag = ef.tag;
    }
  }
  for (const auto& [k, q] : c.quests) {
    auto& kv = r.quests.emplace_back(); kv.first = k; auto& dq = kv.second;
    dq.quest_id = q.quest_id; dq.status = q.status; dq.accepted_at = q.accepted_at; dq.title = q.title;
    for (const auto& o : q.objectives) {
      auto& do_ = dq.objectives.emplace_back(); do_.target_id = o.target_id; do_.current = o.current; do_.required = o.required; do_.desc = o.desc;
    }
  }
}

namespace fbr = flatbuffers_rich;
struct FbRichScratch {
  std::vector<flatbuffers::Offset<fbr::RichItemEntry>> items;
  std::vector<flatbuffers::Offset<fbr::RichEquip>> equips;
  std::vector<flatbuffers::Offset<fbr::Gem>> gems;
  std::vector<flatbuffers::Offset<fbr::RichSkill>> skills;
  std::vector<flatbuffers::Offset<fbr::SkillEffect>> effects;
  std::vector<flatbuffers::Offset<fbr::QuestEntry>> quests;
  std::vector<flatbuffers::Offset<fbr::QuestObjective>> objs;
};

static void fb_rich_encode(flatbuffers::FlatBufferBuilder& b, FbRichScratch& sc, const rich_std::RichCharacter& c) {
  b.Clear();
  sc.items.clear();
  for (const auto& [k, it] : c.items) {
    auto nm = b.CreateString(it.name);
    fbr::Affix* ap; auto af = b.CreateUninitializedVectorOfStructs<fbr::Affix>(it.affixes.size(), &ap);
    for (const auto& a : it.affixes) *ap++ = fbr::Affix(a.attr_id, a.value);
    auto fl = b.CreateVector(it.flags.data(), it.flags.size());
    fbr::RichItemBuilder ib(b);
    ib.add_id(it.id); ib.add_uuid(it.uuid); ib.add_number(it.number); ib.add_timestamp(it.timestamp);
    ib.add_name(nm); ib.add_affixes(af); ib.add_flags(fl);
    auto iv = ib.Finish();
    fbr::RichItemEntryBuilder eb(b); eb.add_key(k); eb.add_value(iv);
    sc.items.push_back(eb.Finish());
  }
  auto items = b.CreateVector(sc.items);

  sc.equips.clear();
  for (const auto& e : c.equips) {
    sc.gems.clear();
    for (const auto& g : e.gems) {
      auto gn = b.CreateString(g.name);
      fbr::Affix* bp; auto bn = b.CreateUninitializedVectorOfStructs<fbr::Affix>(g.bonuses.size(), &bp);
      for (const auto& x : g.bonuses) *bp++ = fbr::Affix(x.attr_id, x.value);
      fbr::GemBuilder gb(b); gb.add_id(g.id); gb.add_tier(g.tier); gb.add_name(gn); gb.add_bonuses(bn);
      sc.gems.push_back(gb.Finish());
    }
    auto gems = b.CreateVector(sc.gems);
    auto en = b.CreateString(e.name);
    auto ec = b.CreateString(e.creator);
    auto ea = b.CreateVector(e.attributes.data(), e.attributes.size());
    fbr::RichEquipBuilder eb(b);
    eb.add_id(e.id); eb.add_uuid(e.uuid); eb.add_number(e.number); eb.add_timestamp(e.timestamp); eb.add_level(e.level);
    eb.add_name(en); eb.add_creator(ec); eb.add_attributes(ea); eb.add_gems(gems);
    sc.equips.push_back(eb.Finish());
  }
  auto equips = b.CreateVector(sc.equips);

  sc.skills.clear();
  for (const auto& s : c.skills) {
    sc.effects.clear();
    for (const auto& ef : s.effects) {
      auto tg = b.CreateString(ef.tag);
      fbr::SkillEffectBuilder fb(b); fb.add_effect_id(ef.effect_id); fb.add_magnitude(ef.magnitude); fb.add_duration(ef.duration); fb.add_tag(tg);
      sc.effects.push_back(fb.Finish());
    }
    auto efs = b.CreateVector(sc.effects);
    auto sn = b.CreateString(s.name);
    fbr::RichSkillBuilder sb(b); sb.add_id(s.id); sb.add_level(s.level); sb.add_cooldown_ms(s.cooldown_ms); sb.add_name(sn); sb.add_effects(efs);
    sc.skills.push_back(sb.Finish());
  }
  auto skills = b.CreateVector(sc.skills);

  sc.quests.clear();
  for (const auto& [k, q] : c.quests) {
    sc.objs.clear();
    for (const auto& o : q.objectives) {
      auto ds = b.CreateString(o.desc);
      fbr::QuestObjectiveBuilder ob(b); ob.add_target_id(o.target_id); ob.add_current(o.current); ob.add_required(o.required); ob.add_desc(ds);
      sc.objs.push_back(ob.Finish());
    }
    auto objs = b.CreateVector(sc.objs);
    auto qt = b.CreateString(q.title);
    fbr::QuestBuilder qb(b); qb.add_quest_id(q.quest_id); qb.add_status(q.status); qb.add_accepted_at(q.accepted_at); qb.add_title(qt); qb.add_objectives(objs);
    auto qv = qb.Finish();
    fbr::QuestEntryBuilder qeb(b); qeb.add_key(k); qeb.add_value(qv);
    sc.quests.push_back(qeb.Finish());
  }
  auto quests = b.CreateVector(sc.quests);

  auto nm = b.CreateString(c.name);
  auto gn = b.CreateString(c.guild_name);
  auto tl = b.CreateString(c.title);
  fbr::Position* pp; auto path = b.CreateUninitializedVectorOfStructs<fbr::Position>(c.path.size(), &pp);
  for (const auto& p : c.path) *pp++ = fbr::Position(p.x, p.y, p.z);
  auto attrs = b.CreateVector(c.attributes.data(), c.attributes.size());

  fbr::RichCharacterBuilder cb(b);
  cb.add_id(c.id); cb.add_name(nm); cb.add_guild_name(gn); cb.add_title(tl);
  cb.add_level(c.level); cb.add_healthpoint(c.healthpoint); cb.add_manapoint(c.manapoint); cb.add_speed(c.speed);
  fbr::Position pos(c.pos.x, c.pos.y, c.pos.z); cb.add_pos(&pos);
  cb.add_items(items); cb.add_path(path); cb.add_attributes(attrs);
  cb.add_equips(equips); cb.add_skills(skills); cb.add_quests(quests);
  b.Finish(cb.Finish());
}

static void fb_rich_decode(const uint8_t* data, rich_std::RichCharacter& c) {
  const auto* r = fbr::GetRichCharacter(data);
  c.id = r->id();
  c.name.assign(r->name()->c_str(), r->name()->size());
  c.guild_name.assign(r->guild_name()->c_str(), r->guild_name()->size());
  c.title.assign(r->title()->c_str(), r->title()->size());
  c.level = r->level(); c.healthpoint = r->healthpoint(); c.manapoint = r->manapoint(); c.speed = r->speed();
  c.pos = {r->pos()->x(), r->pos()->y(), r->pos()->z()};
  const auto* items = r->items(); c.items.resize(items->size());
  for (flatbuffers::uoffset_t i = 0; i < items->size(); ++i) {
    const auto* e = items->Get(i); const auto* v = e->value();
    c.items[i].first = e->key(); auto& d = c.items[i].second;
    d.id = v->id(); d.number = v->number(); d.uuid = v->uuid(); d.timestamp = v->timestamp();
    d.name.assign(v->name()->c_str(), v->name()->size());
    const auto* af = v->affixes(); d.affixes.resize(af->size());
    for (flatbuffers::uoffset_t j = 0; j < af->size(); ++j) d.affixes[j] = {af->Get(j)->attr_id(), af->Get(j)->value()};
    const auto* fl = v->flags(); d.flags.assign(fl->begin(), fl->end());
  }
  const auto* path = r->path(); c.path.resize(path->size());
  for (flatbuffers::uoffset_t i = 0; i < path->size(); ++i) c.path[i] = {path->Get(i)->x(), path->Get(i)->y(), path->Get(i)->z()};
  const auto* attrs = r->attributes(); c.attributes.assign(attrs->begin(), attrs->end());
  const auto* equips = r->equips(); c.equips.resize(equips->size());
  for (flatbuffers::uoffset_t i = 0; i < equips->size(); ++i) {
    const auto* e = equips->Get(i); auto& d = c.equips[i];
    d.id = e->id(); d.number = e->number(); d.uuid = e->uuid(); d.timestamp = e->timestamp(); d.level = e->level();
    d.name.assign(e->name()->c_str(), e->name()->size());
    d.creator.assign(e->creator()->c_str(), e->creator()->size());
    d.attributes.assign(e->attributes()->begin(), e->attributes()->end());
    const auto* gems = e->gems(); d.gems.resize(gems->size());
    for (flatbuffers::uoffset_t g = 0; g < gems->size(); ++g) {
      const auto* gm = gems->Get(g); auto& dg = d.gems[g];
      dg.id = gm->id(); dg.tier = gm->tier(); dg.name.assign(gm->name()->c_str(), gm->name()->size());
      const auto* bn = gm->bonuses(); dg.bonuses.resize(bn->size());
      for (flatbuffers::uoffset_t b = 0; b < bn->size(); ++b) dg.bonuses[b] = {bn->Get(b)->attr_id(), bn->Get(b)->value()};
    }
  }
  const auto* skills = r->skills(); c.skills.resize(skills->size());
  for (flatbuffers::uoffset_t i = 0; i < skills->size(); ++i) {
    const auto* s = skills->Get(i); auto& ds = c.skills[i];
    ds.id = s->id(); ds.level = s->level(); ds.cooldown_ms = s->cooldown_ms();
    ds.name.assign(s->name()->c_str(), s->name()->size());
    const auto* efs = s->effects(); ds.effects.resize(efs->size());
    for (flatbuffers::uoffset_t f = 0; f < efs->size(); ++f) {
      const auto* ef = efs->Get(f); auto& def = ds.effects[f];
      def.effect_id = ef->effect_id(); def.magnitude = ef->magnitude(); def.duration = ef->duration();
      def.tag.assign(ef->tag()->c_str(), ef->tag()->size());
    }
  }
  const auto* quests = r->quests(); c.quests.resize(quests->size());
  for (flatbuffers::uoffset_t i = 0; i < quests->size(); ++i) {
    const auto* qe = quests->Get(i); const auto* q = qe->value();
    c.quests[i].first = qe->key(); auto& dq = c.quests[i].second;
    dq.quest_id = q->quest_id(); dq.status = q->status(); dq.accepted_at = q->accepted_at();
    dq.title.assign(q->title()->c_str(), q->title()->size());
    const auto* objs = q->objectives(); dq.objectives.resize(objs->size());
    for (flatbuffers::uoffset_t o = 0; o < objs->size(); ++o) {
      const auto* ob = objs->Get(o); auto& do_ = dq.objectives[o];
      do_.target_id = ob->target_id(); do_.current = ob->current(); do_.required = ob->required();
      do_.desc.assign(ob->desc()->c_str(), ob->desc()->size());
    }
  }
}

namespace cpr = capnp_rich_test;
static void cp_rich_build(capnp::MessageBuilder& msg, const rich_std::RichCharacter& c) {
  auto r = msg.initRoot<cpr::RichCharacter>();
  r.setId(c.id);
  r.setName(capnp::Text::Reader(c.name.data(), c.name.size()));
  r.setGuildName(capnp::Text::Reader(c.guild_name.data(), c.guild_name.size()));
  r.setTitle(capnp::Text::Reader(c.title.data(), c.title.size()));
  r.setLevel(c.level); r.setHealthpoint(c.healthpoint); r.setManapoint(c.manapoint); r.setSpeed(c.speed);
  auto pos = r.initPos(); pos.setX(c.pos.x); pos.setY(c.pos.y); pos.setZ(c.pos.z);
  auto items = r.initItems(unsigned(c.items.size()));
  for (unsigned i = 0; i < c.items.size(); ++i) {
    auto e = items[i]; e.setKey(c.items[i].first);
    auto v = e.initValue(); const auto& it = c.items[i].second;
    v.setId(it.id); v.setUuid(it.uuid); v.setNumber(it.number); v.setTimestamp(it.timestamp);
    v.setName(capnp::Text::Reader(it.name.data(), it.name.size()));
    auto af = v.initAffixes(unsigned(it.affixes.size()));
    for (unsigned j = 0; j < it.affixes.size(); ++j) { af[j].setAttrId(it.affixes[j].attr_id); af[j].setValue(it.affixes[j].value); }
    auto fl = v.initFlags(unsigned(it.flags.size()));
    for (unsigned j = 0; j < it.flags.size(); ++j) fl.set(j, it.flags[j]);
  }
  auto path = r.initPath(unsigned(c.path.size()));
  for (unsigned i = 0; i < c.path.size(); ++i) { path[i].setX(c.path[i].x); path[i].setY(c.path[i].y); path[i].setZ(c.path[i].z); }
  auto attrs = r.initAttributes(unsigned(c.attributes.size()));
  for (unsigned i = 0; i < c.attributes.size(); ++i) attrs.set(i, c.attributes[i]);
  auto equips = r.initEquips(unsigned(c.equips.size()));
  for (unsigned i = 0; i < c.equips.size(); ++i) {
    const auto& e = c.equips[i]; auto d = equips[i];
    d.setId(e.id); d.setUuid(e.uuid); d.setNumber(e.number); d.setTimestamp(e.timestamp); d.setLevel(e.level);
    d.setName(capnp::Text::Reader(e.name.data(), e.name.size()));
    d.setCreator(capnp::Text::Reader(e.creator.data(), e.creator.size()));
    auto a = d.initAttributes(unsigned(e.attributes.size()));
    for (unsigned k = 0; k < e.attributes.size(); ++k) a.set(k, e.attributes[k]);
    auto gems = d.initGems(unsigned(e.gems.size()));
    for (unsigned g = 0; g < e.gems.size(); ++g) {
      const auto& gm = e.gems[g]; auto dg = gems[g];
      dg.setId(gm.id); dg.setTier(gm.tier); dg.setName(capnp::Text::Reader(gm.name.data(), gm.name.size()));
      auto bn = dg.initBonuses(unsigned(gm.bonuses.size()));
      for (unsigned b = 0; b < gm.bonuses.size(); ++b) { bn[b].setAttrId(gm.bonuses[b].attr_id); bn[b].setValue(gm.bonuses[b].value); }
    }
  }
  auto skills = r.initSkills(unsigned(c.skills.size()));
  for (unsigned i = 0; i < c.skills.size(); ++i) {
    const auto& s = c.skills[i]; auto ds = skills[i];
    ds.setId(s.id); ds.setLevel(s.level); ds.setCooldownMs(s.cooldown_ms);
    ds.setName(capnp::Text::Reader(s.name.data(), s.name.size()));
    auto efs = ds.initEffects(unsigned(s.effects.size()));
    for (unsigned f = 0; f < s.effects.size(); ++f) {
      const auto& ef = s.effects[f]; auto def = efs[f];
      def.setEffectId(ef.effect_id); def.setMagnitude(ef.magnitude); def.setDuration(ef.duration);
      def.setTag(capnp::Text::Reader(ef.tag.data(), ef.tag.size()));
    }
  }
  auto quests = r.initQuests(unsigned(c.quests.size()));
  for (unsigned i = 0; i < c.quests.size(); ++i) {
    auto qe = quests[i]; qe.setKey(c.quests[i].first);
    auto dq = qe.initValue(); const auto& q = c.quests[i].second;
    dq.setQuestId(q.quest_id); dq.setStatus(q.status); dq.setAcceptedAt(q.accepted_at);
    dq.setTitle(capnp::Text::Reader(q.title.data(), q.title.size()));
    auto objs = dq.initObjectives(unsigned(q.objectives.size()));
    for (unsigned o = 0; o < q.objectives.size(); ++o) {
      const auto& ob = q.objectives[o]; auto do_ = objs[o];
      do_.setTargetId(ob.target_id); do_.setCurrent(ob.current); do_.setRequired(ob.required);
      do_.setDesc(capnp::Text::Reader(ob.desc.data(), ob.desc.size()));
    }
  }
}

static void cp_rich_decode(kj::ArrayPtr<const capnp::word> words, rich_std::RichCharacter& c) {
  capnp::FlatArrayMessageReader reader(words);
  auto r = reader.getRoot<cpr::RichCharacter>();
  c.id = r.getId();
  { auto s = r.getName(); c.name.assign(s.cStr(), s.size()); }
  { auto s = r.getGuildName(); c.guild_name.assign(s.cStr(), s.size()); }
  { auto s = r.getTitle(); c.title.assign(s.cStr(), s.size()); }
  c.level = r.getLevel(); c.healthpoint = r.getHealthpoint(); c.manapoint = r.getManapoint(); c.speed = r.getSpeed();
  { auto p = r.getPos(); c.pos = {p.getX(), p.getY(), p.getZ()}; }
  auto items = r.getItems(); c.items.resize(items.size());
  for (unsigned i = 0; i < items.size(); ++i) {
    auto e = items[i]; auto v = e.getValue();
    c.items[i].first = e.getKey(); auto& d = c.items[i].second;
    d.id = v.getId(); d.number = v.getNumber(); d.uuid = v.getUuid(); d.timestamp = v.getTimestamp();
    { auto s = v.getName(); d.name.assign(s.cStr(), s.size()); }
    auto af = v.getAffixes(); d.affixes.resize(af.size());
    for (unsigned j = 0; j < af.size(); ++j) d.affixes[j] = {af[j].getAttrId(), af[j].getValue()};
    auto fl = v.getFlags(); d.flags.resize(fl.size());
    for (unsigned j = 0; j < fl.size(); ++j) d.flags[j] = fl[j];
  }
  auto path = r.getPath(); c.path.resize(path.size());
  for (unsigned i = 0; i < path.size(); ++i) c.path[i] = {path[i].getX(), path[i].getY(), path[i].getZ()};
  auto attrs = r.getAttributes(); c.attributes.resize(attrs.size());
  for (unsigned i = 0; i < attrs.size(); ++i) c.attributes[i] = attrs[i];
  auto equips = r.getEquips(); c.equips.resize(equips.size());
  for (unsigned i = 0; i < equips.size(); ++i) {
    auto e = equips[i]; auto& d = c.equips[i];
    d.id = e.getId(); d.number = e.getNumber(); d.uuid = e.getUuid(); d.timestamp = e.getTimestamp(); d.level = e.getLevel();
    { auto s = e.getName(); d.name.assign(s.cStr(), s.size()); }
    { auto s = e.getCreator(); d.creator.assign(s.cStr(), s.size()); }
    auto a = e.getAttributes(); d.attributes.resize(a.size());
    for (unsigned k = 0; k < a.size(); ++k) d.attributes[k] = a[k];
    auto gems = e.getGems(); d.gems.resize(gems.size());
    for (unsigned g = 0; g < gems.size(); ++g) {
      auto gm = gems[g]; auto& dg = d.gems[g];
      dg.id = gm.getId(); dg.tier = gm.getTier();
      { auto s = gm.getName(); dg.name.assign(s.cStr(), s.size()); }
      auto bn = gm.getBonuses(); dg.bonuses.resize(bn.size());
      for (unsigned b = 0; b < bn.size(); ++b) dg.bonuses[b] = {bn[b].getAttrId(), bn[b].getValue()};
    }
  }
  auto skills = r.getSkills(); c.skills.resize(skills.size());
  for (unsigned i = 0; i < skills.size(); ++i) {
    auto s = skills[i]; auto& ds = c.skills[i];
    ds.id = s.getId(); ds.level = s.getLevel(); ds.cooldown_ms = s.getCooldownMs();
    { auto nm = s.getName(); ds.name.assign(nm.cStr(), nm.size()); }
    auto efs = s.getEffects(); ds.effects.resize(efs.size());
    for (unsigned f = 0; f < efs.size(); ++f) {
      auto ef = efs[f]; auto& def = ds.effects[f];
      def.effect_id = ef.getEffectId(); def.magnitude = ef.getMagnitude(); def.duration = ef.getDuration();
      { auto tg = ef.getTag(); def.tag.assign(tg.cStr(), tg.size()); }
    }
  }
  auto quests = r.getQuests(); c.quests.resize(quests.size());
  for (unsigned i = 0; i < quests.size(); ++i) {
    auto qe = quests[i]; auto q = qe.getValue();
    c.quests[i].first = qe.getKey(); auto& dq = c.quests[i].second;
    dq.quest_id = q.getQuestId(); dq.status = q.getStatus(); dq.accepted_at = q.getAcceptedAt();
    { auto tl = q.getTitle(); dq.title.assign(tl.cStr(), tl.size()); }
    auto objs = q.getObjectives(); dq.objectives.resize(objs.size());
    for (unsigned o = 0; o < objs.size(); ++o) {
      auto ob = objs[o]; auto& do_ = dq.objectives[o];
      do_.target_id = ob.getTargetId(); do_.current = ob.getCurrent(); do_.required = ob.getRequired();
      { auto ds = ob.getDesc(); do_.desc.assign(ds.cStr(), ds.size()); }
    }
  }
}

static void fill_rich_pb(protobuf_rich::RichCharacter& m, const rich_std::RichCharacter& c) {
  m.set_id(c.id); m.set_name(c.name); m.set_guild_name(c.guild_name); m.set_title(c.title);
  m.set_level(c.level); m.set_healthpoint(c.healthpoint); m.set_manapoint(c.manapoint); m.set_speed(c.speed);
  auto* p = m.mutable_pos(); p->set_x(c.pos.x); p->set_y(c.pos.y); p->set_z(c.pos.z);
  for (const auto& [k, it] : c.items) {
    auto& d = (*m.mutable_items())[k];
    d.set_id(it.id); d.set_uuid(it.uuid); d.set_number(it.number); d.set_timestamp(it.timestamp); d.set_name(it.name);
    for (const auto& a : it.affixes) { auto* x = d.add_affixes(); x->set_attr_id(a.attr_id); x->set_value(a.value); }
    for (uint32_t fl : it.flags) d.add_flags(fl);
  }
  for (const auto& q : c.path) { auto* x = m.add_path(); x->set_x(q.x); x->set_y(q.y); x->set_z(q.z); }
  for (float a : c.attributes) m.add_attributes(a);
  for (const auto& e : c.equips) {
    auto* d = m.add_equips();
    d->set_id(e.id); d->set_uuid(e.uuid); d->set_number(e.number); d->set_timestamp(e.timestamp); d->set_level(e.level);
    d->set_name(e.name); d->set_creator(e.creator);
    for (float a : e.attributes) d->add_attributes(a);
    for (const auto& g : e.gems) {
      auto* dg = d->add_gems(); dg->set_id(g.id); dg->set_tier(g.tier); dg->set_name(g.name);
      for (const auto& b : g.bonuses) { auto* db = dg->add_bonuses(); db->set_attr_id(b.attr_id); db->set_value(b.value); }
    }
  }
  for (const auto& s : c.skills) {
    auto* ds = m.add_skills(); ds->set_id(s.id); ds->set_level(s.level); ds->set_cooldown_ms(s.cooldown_ms); ds->set_name(s.name);
    for (const auto& ef : s.effects) {
      auto* def = ds->add_effects(); def->set_effect_id(ef.effect_id); def->set_magnitude(ef.magnitude); def->set_duration(ef.duration); def->set_tag(ef.tag);
    }
  }
  for (const auto& [k, q] : c.quests) {
    auto& dq = (*m.mutable_quests())[k];
    dq.set_quest_id(q.quest_id); dq.set_status(q.status); dq.set_accepted_at(q.accepted_at); dq.set_title(q.title);
    for (const auto& o : q.objectives) {
      auto* do_ = dq.add_objectives(); do_->set_target_id(o.target_id); do_->set_current(o.current); do_->set_required(o.required); do_->set_desc(o.desc);
    }
  }
}

static uint64_t checksum_rich_pb(const protobuf_rich::RichCharacter& m) {
  rich_std::RichCharacter c{};
  c.id = m.id(); c.name = m.name(); c.guild_name = m.guild_name(); c.title = m.title();
  c.level = m.level(); c.healthpoint = m.healthpoint(); c.manapoint = m.manapoint(); c.speed = m.speed();
  c.pos = {m.pos().x(), m.pos().y(), m.pos().z()};
  for (const auto& [k, it] : m.items()) {
    rich_std::RichItem d{it.id(), it.number(), it.uuid(), it.timestamp(), it.name(), {}, {it.flags().begin(), it.flags().end()}};
    for (const auto& a : it.affixes()) d.affixes.push_back({a.attr_id(), a.value()});
    c.items.push_back({k, std::move(d)});
  }
  for (const auto& q : m.path()) c.path.push_back({q.x(), q.y(), q.z()});
  for (float a : m.attributes()) c.attributes.push_back(a);
  for (const auto& e : m.equips()) {
    rich_std::RichEquip d{e.id(), e.number(), e.uuid(), e.timestamp(), e.level(), e.name(), e.creator(), {e.attributes().begin(), e.attributes().end()}, {}};
    for (const auto& g : e.gems()) {
      rich_std::Gem dg{g.id(), g.tier(), g.name(), {}};
      for (const auto& b : g.bonuses()) dg.bonuses.push_back({b.attr_id(), b.value()});
      d.gems.push_back(std::move(dg));
    }
    c.equips.push_back(std::move(d));
  }
  for (const auto& s : m.skills()) {
    rich_std::RichSkill ds{s.id(), s.level(), s.cooldown_ms(), s.name(), {}};
    for (const auto& ef : s.effects()) ds.effects.push_back({ef.effect_id(), ef.magnitude(), ef.duration(), ef.tag()});
    c.skills.push_back(std::move(ds));
  }
  for (const auto& [k, q] : m.quests()) {
    rich_std::Quest dq{q.quest_id(), q.status(), q.accepted_at(), q.title(), {}};
    for (const auto& o : q.objectives()) dq.objectives.push_back({o.target_id(), o.current(), o.required(), o.desc()});
    c.quests.push_back({k, std::move(dq)});
  }
  return checksum_rich(c);
}

static inline uint64_t touch_rich_pb(const protobuf_rich::RichCharacter& m) {
  uint64_t h = m.id() ^ uint64_t(m.items_size()) ^ uint64_t(m.quests_size());
  if (m.equips_size() > 0 && m.equips(m.equips_size() - 1).gems_size() > 0)
    h ^= uint64_t(m.equips(m.equips_size() - 1).gems(m.equips(m.equips_size() - 1).gems_size() - 1).name().size());
  return h;
}

// ======================== Realistic Gameplay Mutation (Hop & In-Place Tick) ========================
static constexpr const char* kHopTitles[4] = {
    "Title_RaidVictor_Season07",
    "Title_MythicSlayer_Tier09",
    "Title_ArenaChampion_Rank1",
    "Title_WorldBreaker_Epoch4",
};
static constexpr const char* kHopDescs[4] = {
    "Obj_Defeated_Boss_Phase02",
    "Obj_Secured_Citadel_Gate4",
    "Obj_Cleared_Dungeon_Wing3",
    "Obj_Harvested_Core_Shards",
};
static constexpr const char* kHopItemNames[4] = {
    "Item_Mythic_Blade_Drop099",
    "Item_Arcane_Relic_Drop104",
    "Item_Dragon_Scale_Drop211",
    "Item_Astral_Crown_Drop308",
};

static inline void set_rich_str(std::string& s, const char* v) { s = v; }
static inline void set_rich_str(XString& s, const char* v) { s = v; }
template <class S> static inline void set_rich_str(S& s, const char* v) { s.set_owning(v); }

static inline void fill_dropped_item(rich_std::RichItem& d, uint64_t drop_key, uint64_t step) {
  d.id = 9001u + uint32_t(step & 15u);
  d.number = 1u;
  d.uuid = drop_key;
  d.timestamp = 1795660000ull + step;
  d.name = kHopItemNames[step & 3u];
  d.affixes.clear();
  d.affixes.push_back({101u, 42.5f});
  d.affixes.push_back({202u, 18.25f});
  d.flags.clear();
  d.flags.push_back(1u);
  d.flags.push_back(4u);
}

static inline void fill_dropped_item(xo_rich::RichItem& d, uint64_t drop_key, uint64_t step) {
  d.id = 9001u + uint32_t(step & 15u);
  d.number = 1u;
  d.uuid = drop_key;
  d.timestamp = 1795660000ull + step;
  d.name = kHopItemNames[step & 3u];
  d.affixes.clear();
  d.affixes.reserve(2);
  d.affixes.push_back({101u, 42.5f});
  d.affixes.push_back({202u, 18.25f});
  d.flags.clear();
  d.flags.reserve(2);
  d.flags.push_back(1u);
  d.flags.push_back(4u);
}

template <class ItemT>
static inline void fill_dropped_item(ItemT& d, uint64_t drop_key, uint64_t step) {
  d.id = 9001u + uint32_t(step & 15u);
  d.number = 1u;
  d.uuid = drop_key;
  d.timestamp = 1795660000ull + step;
  d.name.set_owning(kHopItemNames[step & 3u]);
  d.affixes.clear();
  d.affixes.emplace_back(); d.affixes.back() = {101u, 42.5f};
  d.affixes.emplace_back(); d.affixes.back() = {202u, 18.25f};
  d.flags.clear();
  d.flags.push_back(1u);
  d.flags.push_back(4u);
}

static inline void rich_items_insert_drop(xo_rich::RichCharacter& c, uint64_t drop_key, uint64_t step) {
  fill_dropped_item(c.items[drop_key], drop_key, step);
}
static inline void rich_items_erase_key(xo_rich::RichCharacter& c, uint64_t key) {
  c.items.erase(key);
}

template <class Ch>
static inline void rich_items_insert_drop(Ch& c, uint64_t drop_key, uint64_t step) {
  auto p = items_find(c.items, drop_key);
  if (p != c.items.end() && p->first == drop_key) {
    fill_dropped_item(p->second, drop_key, step);
    return;
  }
  const auto at = p - c.items.begin();
  c.items.emplace_back();
  p = c.items.begin() + at;
  std::move_backward(p, c.items.end() - 1, c.items.end());
  p->first = drop_key;
  fill_dropped_item(p->second, drop_key, step);
}

template <class Ch>
static inline void rich_items_erase_key(Ch& c, uint64_t key) {
  items_erase(c.items, key);
}

// One realistic server-to-server handoff mutation:
// update scalars & position, overwrite a >SSO title string, advance quest objective + desc,
// upgrade equipment level/attr/gem tier, append a waypoint, loot a new RichItem (with name,
// affixes, flags) into the sorted item map, and consume/erase an existing item.
template <class Ch>
static void mutate_rich_hop(Ch& c, uint64_t step, uint64_t erase_key) {
  c.level += 1u;
  c.healthpoint -= 12.5f;
  c.manapoint -= 8.0f;
  c.pos.x += 1.5f;
  c.pos.y -= 0.75f;
  set_rich_str(c.title, kHopTitles[step & 3u]);
  if (!c.quests.empty() && !c.quests.begin()->second.objectives.empty()) {
    auto& ob = c.quests.begin()->second.objectives[0];
    ob.current += 1u;
    set_rich_str(ob.desc, kHopDescs[step & 3u]);
  }
  if (!c.equips.empty()) {
    auto& eq = c.equips[0];
    eq.level += 1u;
    if (!eq.attributes.empty()) eq.attributes[0] += 5.0f;
    if (!eq.gems.empty()) eq.gems[0].tier += 1u;
  }
  c.path.emplace_back();
  c.path.back() = {c.pos.x, c.pos.y, c.pos.z};
  const uint64_t drop_key = 0x8000000000000000ull + (step & 1023u);
  rich_items_insert_drop(c, drop_key, step);
  rich_items_erase_key(c, erase_key);
}

static void mutate_rich_hop_pb(protobuf_rich::RichCharacter& m, uint64_t step, uint64_t erase_key, uint32_t first_quest_key) {
  m.set_level(m.level() + 1u);
  m.set_healthpoint(m.healthpoint() - 12.5f);
  m.set_manapoint(m.manapoint() - 8.0f);
  auto* p = m.mutable_pos();
  p->set_x(p->x() + 1.5f);
  p->set_y(p->y() - 0.75f);
  m.set_title(kHopTitles[step & 3u]);
  auto qit = m.mutable_quests()->find(first_quest_key);
  if (qit != m.mutable_quests()->end() && qit->second.objectives_size() > 0) {
    auto* ob = qit->second.mutable_objectives(0);
    ob->set_current(ob->current() + 1u);
    ob->set_desc(kHopDescs[step & 3u]);
  }
  if (m.equips_size() > 0) {
    auto* eq = m.mutable_equips(0);
    eq->set_level(eq->level() + 1u);
    if (eq->attributes_size() > 0) eq->set_attributes(0, eq->attributes(0) + 5.0f);
    if (eq->gems_size() > 0) eq->mutable_gems(0)->set_tier(eq->gems(0).tier() + 1u);
  }
  auto* np = m.add_path();
  np->set_x(p->x()); np->set_y(p->y()); np->set_z(p->z());
  const uint64_t drop_key = 0x8000000000000000ull + (step & 1023u);
  auto& d = (*m.mutable_items())[drop_key];
  d.set_id(9001u + uint32_t(step & 15u));
  d.set_number(1u);
  d.set_uuid(drop_key);
  d.set_timestamp(1795660000ull + step);
  d.set_name(kHopItemNames[step & 3u]);
  d.clear_affixes();
  { auto* a = d.add_affixes(); a->set_attr_id(101u); a->set_value(42.5f); }
  { auto* a = d.add_affixes(); a->set_attr_id(202u); a->set_value(18.25f); }
  d.clear_flags();
  d.add_flags(1u);
  d.add_flags(4u);
  m.mutable_items()->erase(erase_key);
}

// Release heap allocations made by mutating a deserialized cista buffer before reusing raw storage.
template <class Ch>
static void cleanup_cista_hop(Ch& c) {
  if (!c.quests.empty() && !c.quests.begin()->second.objectives.empty()) {
    c.quests.begin()->second.objectives[0].desc.reset();
  }
  if (!c.items.self_allocated_) {
    for (auto& kv : c.items) {
      kv.second.name.reset();
      kv.second.affixes.deallocate();
      kv.second.flags.deallocate();
    }
  }
  c.~Ch();
}

// Reversible in-place gameplay tick on RichCharacter (verifies net state unchanged after timing).
template <class Ch>
static uint64_t mutate_rich_rw_tick(Ch& c, uint64_t step, const char* orig_title, const char* orig_desc) {
  c.level += 1u;
  c.healthpoint += 1.0f;
  c.pos.x += 1.0f;
  set_rich_str(c.title, kHopTitles[step & 3u]);
  if (!c.quests.empty() && !c.quests.begin()->second.objectives.empty()) {
    auto& ob = c.quests.begin()->second.objectives[0];
    ob.current += 1u;
    set_rich_str(ob.desc, kHopDescs[step & 3u]);
  }
  if (!c.equips.empty()) {
    c.equips[0].level += 1u;
    if (!c.equips[0].attributes.empty()) c.equips[0].attributes[0] += 1.0f;
  }
  c.path.emplace_back();
  c.path.back() = {c.pos.x, c.pos.y, c.pos.z};
  const uint64_t drop_key = 0x8000000000000000ull + (step & 1023u);
  rich_items_insert_drop(c, drop_key, step);
  uint64_t h = uint64_t(c.items.size()) ^ uint64_t(c.level) ^ uint64_t(c.path.size());
  rich_items_erase_key(c, drop_key);
  c.path.pop_back();
  if (!c.equips.empty()) {
    c.equips[0].level -= 1u;
    if (!c.equips[0].attributes.empty()) c.equips[0].attributes[0] -= 1.0f;
  }
  if (!c.quests.empty() && !c.quests.begin()->second.objectives.empty()) {
    auto& ob = c.quests.begin()->second.objectives[0];
    ob.current -= 1u;
    set_rich_str(ob.desc, orig_desc);
  }
  set_rich_str(c.title, orig_title);
  c.pos.x -= 1.0f;
  c.healthpoint -= 1.0f;
  c.level -= 1u;
  return h;
}

static uint64_t mutate_rich_rw_tick_pb(protobuf_rich::RichCharacter& m, uint64_t step,
                                       uint32_t first_quest_key, const char* orig_title, const char* orig_desc) {
  m.set_level(m.level() + 1u);
  m.set_healthpoint(m.healthpoint() + 1.0f);
  auto* p = m.mutable_pos();
  p->set_x(p->x() + 1.0f);
  m.set_title(kHopTitles[step & 3u]);
  auto qit = m.mutable_quests()->find(first_quest_key);
  if (qit != m.mutable_quests()->end() && qit->second.objectives_size() > 0) {
    auto* ob = qit->second.mutable_objectives(0);
    ob->set_current(ob->current() + 1u);
    ob->set_desc(kHopDescs[step & 3u]);
  }
  if (m.equips_size() > 0) {
    auto* eq = m.mutable_equips(0);
    eq->set_level(eq->level() + 1u);
    if (eq->attributes_size() > 0) eq->set_attributes(0, eq->attributes(0) + 1.0f);
  }
  auto* np = m.add_path();
  np->set_x(p->x()); np->set_y(p->y()); np->set_z(p->z());
  const uint64_t drop_key = 0x8000000000000000ull + (step & 1023u);
  auto& d = (*m.mutable_items())[drop_key];
  d.set_id(9001u + uint32_t(step & 15u));
  d.set_number(1u);
  d.set_uuid(drop_key);
  d.set_timestamp(1795660000ull + step);
  d.set_name(kHopItemNames[step & 3u]);
  { auto* a = d.add_affixes(); a->set_attr_id(101u); a->set_value(42.5f); }
  { auto* a = d.add_affixes(); a->set_attr_id(202u); a->set_value(18.25f); }
  d.add_flags(1u);
  d.add_flags(4u);
  uint64_t h = uint64_t(m.items_size()) ^ uint64_t(m.level()) ^ uint64_t(m.path_size());
  m.mutable_items()->erase(drop_key);
  m.mutable_path()->DeleteSubrange(m.path_size() - 1, 1);
  if (m.equips_size() > 0) {
    auto* eq = m.mutable_equips(0);
    eq->set_level(eq->level() - 1u);
    if (eq->attributes_size() > 0) eq->set_attributes(0, eq->attributes(0) - 1.0f);
  }
  if (qit != m.mutable_quests()->end() && qit->second.objectives_size() > 0) {
    auto* ob = qit->second.mutable_objectives(0);
    ob->set_current(ob->current() - 1u);
    ob->set_desc(orig_desc);
  }
  m.set_title(orig_title);
  p->set_x(p->x() - 1.0f);
  m.set_healthpoint(m.healthpoint() - 1.0f);
  m.set_level(m.level() - 1u);
  return h;
}

// ======================== Flat Workload Runner ========================
static void run_flat_workload(const char* tag, size_t k, std::mt19937_64& rng) {
  const stdm::Character src_std = make_flat_scaled(k);
  const uint64_t want = checksum(src_std);
  const auto cap = XBuffer::maximum_capacity(1 << 24);
  std::vector<std::function<void()>> tasks;

  // XOffset
  tasks.push_back([&] {
    using C = xo::Character;
    auto src = TypedXBuffer<C>::create(64 * 1024, cap);
    src.edit([&](C& r) { xo::fill(r, src_std); });
    std::string w0 = src.save_verified();
    XReceiver<XLink::server_peer> rx(cap);
    auto held = TypedXBuffer<C>::load(w0, cap);
    expect(tag, checksum(held.plain_root()), want);
    std::string w = w0;
    put(tag, "wire_raw", "XOffset", double(w0.size()));
    put(tag, "wire_zstd3", "XOffset", double(zstd3(w0.data(), w0.size())));
    put(tag, "trusted_reuse", "XOffset", time_op([&](size_t) {
      src.save_verified_into(w); if (!rx.reload(held, w)) throw std::runtime_error("xo");
      return touch_flat(held.plain_root());
    }));
    put(tag, "checked_reuse", "XOffset", time_op([&](size_t) {
      src.save_verified_into(w); held.reload_verified(w); if (!held.ok()) throw std::runtime_error("xof");
      return touch_flat(held.plain_root());
    }));
    put(tag, "fresh_trusted", "XOffset", time_op([&](size_t) {
      std::string wf = src.save_verified(); auto r = rx.load<C>(wf); return touch_flat(r->root<C>());
    }));
    put(tag, "fresh_checked", "XOffset", time_op([&](size_t) {
      std::string wf = src.save_verified(); auto r = TypedXBuffer<C>::load(wf, cap); return touch_flat(r.plain_root());
    }));
  });

  // zpp::bits
  tasks.push_back([&] {
    std::vector<std::byte> w0, w; { zpp::bits::out out{w0}; (void)out(src_std); }
    stdm::Character held{}; { zpp::bits::in in{w0}; (void)in(held); expect(tag, checksum(held), want); }
    put(tag, "wire_raw", "zpp::bits", double(w0.size()));
    put(tag, "wire_zstd3", "zpp::bits", double(zstd3(w0.data(), w0.size())));
    double sr = time_op([&](size_t) {
      w.clear(); zpp::bits::out out{w}; (void)out(src_std);
      zpp::bits::in in{w}; (void)in(held); return touch_flat(held);
    });
    put(tag, "trusted_reuse", "zpp::bits", sr);
    put(tag, "checked_reuse", "zpp::bits", sr);
    double fr = time_op([&](size_t) {
      std::vector<std::byte> wf; zpp::bits::out out{wf}; (void)out(src_std);
      stdm::Character d{}; zpp::bits::in in{wf}; (void)in(d); return touch_flat(d);
    });
    put(tag, "fresh_trusted", "zpp::bits", fr);
    put(tag, "fresh_checked", "zpp::bits", fr);
  });

  // FlatBuffers
  tasks.push_back([&] {
    flatbuffers::FlatBufferBuilder b; std::vector<flatbuffers::Offset<fbt::Equip>> eq;
    fb_flat_encode(b, eq, src_std);
    stdm::Character held{}; fb_flat_decode(b.GetBufferPointer(), held); expect(tag, checksum(held), want);
    put(tag, "wire_raw", "FlatBuffers", double(b.GetSize()));
    put(tag, "wire_zstd3", "FlatBuffers", double(zstd3(b.GetBufferPointer(), b.GetSize())));
    put(tag, "trusted_reuse", "FlatBuffers", time_op([&](size_t) {
      fb_flat_encode(b, eq, src_std); fb_flat_decode(b.GetBufferPointer(), held); return touch_flat(held);
    }));
    put(tag, "checked_reuse", "FlatBuffers", time_op([&](size_t) {
      fb_flat_encode(b, eq, src_std);
      flatbuffers::Verifier vf(b.GetBufferPointer(), b.GetSize());
      if (!fbt::VerifyCharacterBuffer(vf)) throw std::runtime_error("fbv");
      fb_flat_decode(b.GetBufferPointer(), held); return touch_flat(held);
    }));
    put(tag, "fresh_trusted", "FlatBuffers", time_op([&](size_t) {
      flatbuffers::FlatBufferBuilder bf; std::vector<flatbuffers::Offset<fbt::Equip>> eqf;
      fb_flat_encode(bf, eqf, src_std); stdm::Character d{}; fb_flat_decode(bf.GetBufferPointer(), d); return touch_flat(d);
    }));
    put(tag, "fresh_checked", "FlatBuffers", time_op([&](size_t) {
      flatbuffers::FlatBufferBuilder bf; std::vector<flatbuffers::Offset<fbt::Equip>> eqf;
      fb_flat_encode(bf, eqf, src_std);
      flatbuffers::Verifier vf(bf.GetBufferPointer(), bf.GetSize());
      if (!fbt::VerifyCharacterBuffer(vf)) throw std::runtime_error("fbv");
      stdm::Character d{}; fb_flat_decode(bf.GetBufferPointer(), d); return touch_flat(d);
    }));
  });

  // cista raw & offset
  auto add_c = [&]<class Ch, cista::mode FM>(const char* nm) {
    tasks.push_back([=] {
      Ch src{}; fill_flat_cista(src, src_std);
      auto w0 = cista::serialize<FM>(src);
      { auto cp = w0; expect(nm, checksum(*cista::deserialize<Ch, FM>(cp)), want); }
      put(tag, "wire_raw", nm, double(w0.size()));
      put(tag, "wire_zstd3", nm, double(zstd3(w0.data(), w0.size())));
      cista::buf<std::vector<uint8_t>> out; std::vector<uint8_t> held;
      put(tag, "trusted_reuse", nm, time_op([&](size_t) {
        out.reset(); cista::serialize<FM>(out, src); held.assign(out.buf_.begin(), out.buf_.end());
        return touch_flat(*cista::deserialize<Ch, FM>(held));
      }));
      put(tag, "checked_reuse", nm, time_op([&](size_t) {
        out.reset(); cista::serialize<cista::mode::NONE>(out, src); held.assign(out.buf_.begin(), out.buf_.end());
        return touch_flat(*cista::deserialize<Ch, cista::mode::DEEP_CHECK>(held));
      }));
      put(tag, "fresh_trusted", nm, time_op([&](size_t) {
        auto wf = cista::serialize<FM>(src); std::vector<uint8_t> h(wf.begin(), wf.end());
        return touch_flat(*cista::deserialize<Ch, FM>(h));
      }));
      put(tag, "fresh_checked", nm, time_op([&](size_t) {
        auto wf = cista::serialize<cista::mode::NONE>(src); std::vector<uint8_t> h(wf.begin(), wf.end());
        return touch_flat(*cista::deserialize<Ch, cista::mode::DEEP_CHECK>(h));
      }));
    });
  };
  add_c.template operator()<cista_raw::Character, cista::mode::UNCHECKED>("cista raw");
  add_c.template operator()<cista_offset::Character, cista::mode::CAST>("cista offset");

  // Cap'n Proto (fair reuse)
  tasks.push_back([&] {
    std::vector<capnp::word> scratch(8192), flat;
    stdm::Character held{};
    { capnp::MallocMessageBuilder m(kj::ArrayPtr<capnp::word>(scratch.data(), scratch.size()));
      cp_flat_build(m, src_std); auto ptr = cp_write_flat_reused(m, flat);
      cp_flat_decode(ptr, held); expect(tag, checksum(held), want);
      put(tag, "wire_raw", "Cap'n Proto", double(ptr.size() * sizeof(capnp::word)));
      put(tag, "wire_zstd3", "Cap'n Proto", double(zstd3(ptr.begin(), ptr.size() * sizeof(capnp::word))));
    }
    double sr = time_op([&](size_t) {
      capnp::MallocMessageBuilder m(kj::ArrayPtr<capnp::word>(scratch.data(), scratch.size()));
      cp_flat_build(m, src_std); auto ptr = cp_write_flat_reused(m, flat);
      cp_flat_decode(ptr, held); return touch_flat(held);
    });
    put(tag, "trusted_reuse", "Cap'n Proto", sr);
    put(tag, "checked_reuse", "Cap'n Proto", sr);
    double fr = time_op([&](size_t) {
      capnp::MallocMessageBuilder m; cp_flat_build(m, src_std); auto wf = capnp::messageToFlatArray(m);
      stdm::Character d{}; cp_flat_decode(wf.asPtr(), d); return touch_flat(d);
    });
    put(tag, "fresh_trusted", "Cap'n Proto", fr);
    put(tag, "fresh_checked", "Cap'n Proto", fr);
  });

  // MessagePack
  tasks.push_back([&] {
    msgpack::sbuffer w0, w; msgpack::pack(w0, src_std);
    stdm::Character held{}; msgpack::unpack(w0.data(), w0.size()).get().convert(held); expect(tag, checksum(held), want);
    put(tag, "wire_raw", "MessagePack", double(w0.size()));
    put(tag, "wire_zstd3", "MessagePack", double(zstd3(w0.data(), w0.size())));
    double sr = time_op([&](size_t) {
      w.clear(); msgpack::pack(w, src_std);
      msgpack::unpack(w.data(), w.size()).get().convert(held); return touch_flat(held);
    });
    put(tag, "trusted_reuse", "MessagePack", sr);
    put(tag, "checked_reuse", "MessagePack", sr);
    double fr = time_op([&](size_t) {
      msgpack::sbuffer wf; msgpack::pack(wf, src_std);
      stdm::Character d{}; msgpack::unpack(wf.data(), wf.size()).get().convert(d); return touch_flat(d);
    });
    put(tag, "fresh_trusted", "MessagePack", fr);
    put(tag, "fresh_checked", "MessagePack", fr);
  });

  // Protocol Buffers
  tasks.push_back([&] {
    protobuf_test::Character src, held; fill_flat_pb(src, src_std);
    std::string w0, w; src.SerializeToString(&w0); held.ParseFromString(w0); expect(tag, checksum_flat_pb(held), want);
    put(tag, "wire_raw", "Protocol Buffers", double(w0.size()));
    put(tag, "wire_zstd3", "Protocol Buffers", double(zstd3(w0.data(), w0.size())));
    double sr = time_op([&](size_t) {
      src.SerializeToString(&w); held.ParseFromString(w);
      return uint64_t(held.id() ^ held.items_size() ^ held.equips_size());
    });
    put(tag, "trusted_reuse", "Protocol Buffers", sr);
    put(tag, "checked_reuse", "Protocol Buffers", sr);
    double fr = time_op([&](size_t) {
      std::string wf; protobuf_test::Character d; src.SerializeToString(&wf); d.ParseFromString(wf);
      return uint64_t(d.id() ^ d.items_size() ^ d.equips_size());
    });
    put(tag, "fresh_trusted", "Protocol Buffers", fr);
    put(tag, "fresh_checked", "Protocol Buffers", fr);
  });

  std::shuffle(tasks.begin(), tasks.end(), rng);
  for (auto& fn : tasks) fn();
}

// ======================== Rich Workload Runner ========================
static void run_rich_workload(const char* tag, const RichProfile& prof, std::mt19937_64& rng) {
  const rich_std::RichCharacter src_std = make_rich_character(2026, prof, false);
  const uint64_t want = checksum_rich(src_std);
  const std::string orig_title = src_std.title;
  const std::string orig_desc = src_std.quests[0].second.objectives[0].desc;
  const uint32_t src_first_quest = src_std.quests[0].first;
  // 16 distinct varied-shape RichCharacters (~60-200 KB total -> fits in 2 MiB L2, tests real reuse!)
  constexpr size_t kVaried = 16;
  std::vector<rich_std::RichCharacter> varied_std;
  varied_std.reserve(kVaried);
  std::array<uint64_t, kVaried> erase_keys{};
  std::array<uint32_t, kVaried> first_quest_keys{};
  for (size_t i = 0; i < kVaried; ++i) {
    varied_std.push_back(make_rich_character(9000 + i, prof, true));
    erase_keys[i] = varied_std.back().items.front().first;
    first_quest_keys[i] = varied_std.back().quests.front().first;
  }
  uint64_t want_hop0 = 0;
  {
    auto tmp = varied_std[0];
    mutate_rich_hop(tmp, 0, erase_keys[0]);
    want_hop0 = checksum_rich(tmp);
  }

  const auto cap = XBuffer::maximum_capacity(1 << 24);
  std::vector<std::function<void()>> tasks;

  // XOffset (reserved = best-practice fill; also reports compacted & unreserved)
  tasks.push_back([&] {
    using C = xo_rich::RichCharacter;
    auto mk = [&](const rich_std::RichCharacter& ch, bool do_reserve, bool do_compact) {
      auto b = TypedXBuffer<C>::create(128 * 1024, cap);
      b.edit([&](C& r) { fill_xo_rich(r, ch, do_reserve); });
      if (!b.ok()) throw std::runtime_error("xo rich fill");
      if (do_compact) {
        std::string cw = b.save_compact();
        return TypedXBuffer<C>::load(cw, cap);
      }
      return b;
    };
    auto run_xo_variant = [&](const char* label, bool do_reserve, bool do_compact, bool full_suite) {
      auto src = mk(src_std, do_reserve, do_compact);
      std::string w0 = src.save_verified();
      XReceiver<XLink::server_peer> rx(cap);
      XReceiver<XLink::from_client> rxc(cap);
      auto held = TypedXBuffer<C>::load(w0, cap);
      expect(label, checksum_rich(held.plain_root()), want);
      std::string w = w0;
      put(tag, "wire_raw", label, double(w0.size()));
      put(tag, "wire_zstd3", label, double(zstd3(w0.data(), w0.size())));

      put(tag, "send_only", label, time_op([&](size_t) {
        src.save_verified_into(w); clobber(w.data()); return uint64_t(uint8_t(w[40]));
      }));
      put(tag, "recv_trusted", label, time_op([&](size_t) {
        if (!rx.reload(held, w0)) throw std::runtime_error("xo r");
        return touch_rich(held.plain_root());
      }));
      put(tag, "recv_checked", label, time_op([&](size_t) {
        held.reload_verified(w0); if (!held.ok()) throw std::runtime_error("xo rf");
        return touch_rich(held.plain_root());
      }));
      put(tag, "trusted_reuse", label, time_op([&](size_t) {
        src.save_verified_into(w); if (!rx.reload(held, w)) throw std::runtime_error("xo sr");
        return touch_rich(held.plain_root());
      }));
      put(tag, "checked_reuse", label, time_op([&](size_t) {
        src.save_verified_into(w); held.reload_verified(w); if (!held.ok()) throw std::runtime_error("xo srf");
        return touch_rich(held.plain_root());
      }));
      put(tag, "fresh_trusted", label, time_op([&](size_t) {
        std::string wf = src.save_verified(); auto r = rx.load<C>(wf); return touch_rich(r->root<C>());
      }));
      put(tag, "fresh_checked", label, time_op([&](size_t) {
        std::string wf = src.save_verified(); auto r = TypedXBuffer<C>::load(wf, cap); return touch_rich(r.plain_root());
      }));
      if (full_suite) {
        put(tag, "view_admit", label, time_op([&](size_t) {
          src.save_verified_into(w); auto v = rx.view<C>(w); return touch_rich(v->root());
        }));
        put(tag, "view_bounds", label, time_op([&](size_t) {
          src.save_verified_into(w); auto v = rxc.view<C>(w); return touch_rich(v->root());
        }));
        auto rw_buf = mk(src_std, do_reserve, false);
        put(tag, "rw_rich", label, time_op([&](size_t i) {
          uint64_t h = 0;
          rw_buf.edit([&](C& r) { h = mutate_rich_rw_tick(r, i, orig_title.c_str(), orig_desc.c_str()); });
          if (!rw_buf.ok()) throw std::runtime_error("xo rw_rich");
          return h;
        }));
        expect("xo_rw_rich", checksum_rich(rw_buf.plain_root()), want);
      }
      std::vector<TypedXBuffer<C>> vxo; vxo.reserve(kVaried);
      std::vector<std::string> vwires; vwires.reserve(kVaried);
      for (const auto& ch : varied_std) {
        vxo.push_back(mk(ch, do_reserve, do_compact));
        vwires.push_back(vxo.back().save_verified());
      }
      put(tag, "varied_reuse", label, time_op([&](size_t i) {
        vxo[i & (kVaried - 1)].save_verified_into(w);
        if (!rx.reload(held, w)) throw std::runtime_error("xo v");
        return touch_rich(held.plain_root());
      }));
      put(tag, "varied_checked", label, time_op([&](size_t i) {
        vxo[i & (kVaried - 1)].save_verified_into(w);
        held.reload_verified(w); if (!held.ok()) throw std::runtime_error("xo vf");
        return touch_rich(held.plain_root());
      }));
      // Server-to-server hop: receive varied player -> gameplay mutate -> send on
      {
        auto held_hop = TypedXBuffer<C>(64 * 1024, cap.with_growth(XGrowPolicy::Automatic));
        std::string w_out;
        if (!rx.reload(held_hop, vwires[0])) throw std::runtime_error("xo hop check");
        held_hop.edit([&](C& r) { mutate_rich_hop(r, 0, erase_keys[0]); });
        if (!held_hop.ok()) throw std::runtime_error("xo hop edit");
        held_hop.save_verified_into(w_out);
        auto check_buf = TypedXBuffer<C>::load(w_out, cap);
        expect("xo_hop", checksum_rich(check_buf.plain_root()), want_hop0);

        put(tag, "hop_trusted", label, time_op([&](size_t i) {
          const size_t k = i & (kVaried - 1);
          if (!rx.reload(held_hop, vwires[k])) throw std::runtime_error("xo ht");
          held_hop.edit([&](C& r) { mutate_rich_hop(r, i, erase_keys[k]); });
          if (!held_hop.ok()) throw std::runtime_error("xo hte");
          held_hop.save_verified_into(w_out);
          clobber(w_out.data());
          return uint64_t(w_out.size()) ^ uint64_t(uint8_t(w_out[40]));
        }));
        put(tag, "hop_checked", label, time_op([&](size_t i) {
          const size_t k = i & (kVaried - 1);
          held_hop.reload_verified(vwires[k]);
          if (!held_hop.ok()) throw std::runtime_error("xo hc");
          held_hop.edit([&](C& r) { mutate_rich_hop(r, i, erase_keys[k]); });
          if (!held_hop.ok()) throw std::runtime_error("xo hce");
          held_hop.save_verified_into(w_out);
          clobber(w_out.data());
          return uint64_t(w_out.size()) ^ uint64_t(uint8_t(w_out[40]));
        }));
      }
    };
    run_xo_variant("XOffset", true, false, true);
    run_xo_variant("XOffset (compacted)", true, true, false);
    run_xo_variant("XOffset (unreserved)", false, false, false);
  });

  // zpp::bits (also measures shared std rw_rich)
  tasks.push_back([&] {
    std::vector<std::byte> w0, w; { zpp::bits::out out{w0}; (void)out(src_std); }
    rich_std::RichCharacter held{}; { zpp::bits::in in{w0}; (void)in(held); expect("zpp_rich", checksum_rich(held), want); }
    put(tag, "wire_raw", "zpp::bits", double(w0.size()));
    put(tag, "wire_zstd3", "zpp::bits", double(zstd3(w0.data(), w0.size())));
    put(tag, "send_only", "zpp::bits", time_op([&](size_t) {
      w.clear(); zpp::bits::out out{w}; (void)out(src_std); clobber(w.data()); return uint64_t(uint8_t(w[0]));
    }));
    put(tag, "recv_checked", "zpp::bits", time_op([&](size_t) {
      zpp::bits::in in{w0}; (void)in(held); return touch_rich(held);
    }));
    double sr = time_op([&](size_t) {
      w.clear(); zpp::bits::out out{w}; (void)out(src_std);
      zpp::bits::in in{w}; (void)in(held); return touch_rich(held);
    });
    put(tag, "trusted_reuse", "zpp::bits", sr);
    put(tag, "checked_reuse", "zpp::bits", sr);
    double fr = time_op([&](size_t) {
      std::vector<std::byte> wf; zpp::bits::out out{wf}; (void)out(src_std);
      rich_std::RichCharacter d{}; zpp::bits::in in{wf}; (void)in(d); return touch_rich(d);
    });
    put(tag, "fresh_trusted", "zpp::bits", fr);
    put(tag, "fresh_checked", "zpp::bits", fr);
    double vr = time_op([&](size_t i) {
      w.clear(); zpp::bits::out out{w}; (void)out(varied_std[i & (kVaried - 1)]);
      zpp::bits::in in{w}; (void)in(held); return touch_rich(held);
    });
    put(tag, "varied_reuse", "zpp::bits", vr);
    put(tag, "varied_checked", "zpp::bits", vr);
    // Hop
    std::vector<std::vector<std::byte>> vwires(kVaried);
    for (size_t i = 0; i < kVaried; ++i) { zpp::bits::out out{vwires[i]}; (void)out(varied_std[i]); }
    {
      zpp::bits::in in{vwires[0]}; (void)in(held);
      mutate_rich_hop(held, 0, erase_keys[0]);
      w.clear(); zpp::bits::out out{w}; (void)out(held);
      rich_std::RichCharacter chk{}; zpp::bits::in in2{w}; (void)in2(chk);
      expect("zpp_hop", checksum_rich(chk), want_hop0);
    }
    double hop = time_op([&](size_t i) {
      const size_t k = i & (kVaried - 1);
      zpp::bits::in in{vwires[k]}; (void)in(held);
      mutate_rich_hop(held, i, erase_keys[k]);
      w.clear(); zpp::bits::out out{w}; (void)out(held);
      clobber(w.data());
      return uint64_t(w.size()) ^ uint64_t(uint8_t(w[0]));
    });
    put(tag, "hop_trusted", "zpp::bits", hop);
    put(tag, "hop_checked", "zpp::bits", hop);
    // In-place rw_rich on std model
    auto rw_std = src_std;
    put(tag, "rw_rich", "zpp::bits", time_op([&](size_t i) {
      return mutate_rich_rw_tick(rw_std, i, orig_title.c_str(), orig_desc.c_str());
    }));
    expect("std_rw_rich", checksum_rich(rw_std), want);
  });

  // FlatBuffers
  tasks.push_back([&] {
    flatbuffers::FlatBufferBuilder b; FbRichScratch sc;
    fb_rich_encode(b, sc, src_std);
    std::vector<uint8_t> w0(b.GetBufferPointer(), b.GetBufferPointer() + b.GetSize());
    rich_std::RichCharacter held{}; fb_rich_decode(w0.data(), held); expect("fb_rich", checksum_rich(held), want);
    put(tag, "wire_raw", "FlatBuffers", double(w0.size()));
    put(tag, "wire_zstd3", "FlatBuffers", double(zstd3(w0.data(), w0.size())));
    put(tag, "send_only", "FlatBuffers", time_op([&](size_t) {
      fb_rich_encode(b, sc, src_std); clobber(b.GetBufferPointer()); return uint64_t(b.GetSize());
    }));
    put(tag, "recv_trusted", "FlatBuffers", time_op([&](size_t) {
      fb_rich_decode(w0.data(), held); return touch_rich(held);
    }));
    put(tag, "recv_checked", "FlatBuffers", time_op([&](size_t) {
      flatbuffers::Verifier vf(w0.data(), w0.size()); if (!fbr::VerifyRichCharacterBuffer(vf)) throw std::runtime_error("fbrv");
      fb_rich_decode(w0.data(), held); return touch_rich(held);
    }));
    put(tag, "trusted_reuse", "FlatBuffers", time_op([&](size_t) {
      fb_rich_encode(b, sc, src_std); fb_rich_decode(b.GetBufferPointer(), held); return touch_rich(held);
    }));
    put(tag, "checked_reuse", "FlatBuffers", time_op([&](size_t) {
      fb_rich_encode(b, sc, src_std);
      flatbuffers::Verifier vf(b.GetBufferPointer(), b.GetSize()); if (!fbr::VerifyRichCharacterBuffer(vf)) throw std::runtime_error("fbrv");
      fb_rich_decode(b.GetBufferPointer(), held); return touch_rich(held);
    }));
    put(tag, "view_admit", "FlatBuffers", time_op([&](size_t) {
      fb_rich_encode(b, sc, src_std); const auto* r = fbr::GetRichCharacter(b.GetBufferPointer());
      return uint64_t(r->id() ^ r->items()->size() ^ r->quests()->size());
    }));
    put(tag, "view_bounds", "FlatBuffers", time_op([&](size_t) {
      fb_rich_encode(b, sc, src_std);
      flatbuffers::Verifier vf(b.GetBufferPointer(), b.GetSize()); if (!fbr::VerifyRichCharacterBuffer(vf)) throw std::runtime_error("fbrv");
      const auto* r = fbr::GetRichCharacter(b.GetBufferPointer());
      return uint64_t(r->id() ^ r->items()->size() ^ r->quests()->size());
    }));
    put(tag, "fresh_trusted", "FlatBuffers", time_op([&](size_t) {
      flatbuffers::FlatBufferBuilder bf; FbRichScratch scf; fb_rich_encode(bf, scf, src_std);
      rich_std::RichCharacter d{}; fb_rich_decode(bf.GetBufferPointer(), d); return touch_rich(d);
    }));
    put(tag, "fresh_checked", "FlatBuffers", time_op([&](size_t) {
      flatbuffers::FlatBufferBuilder bf; FbRichScratch scf; fb_rich_encode(bf, scf, src_std);
      flatbuffers::Verifier vf(bf.GetBufferPointer(), bf.GetSize()); if (!fbr::VerifyRichCharacterBuffer(vf)) throw std::runtime_error("fbrvf");
      rich_std::RichCharacter d{}; fb_rich_decode(bf.GetBufferPointer(), d); return touch_rich(d);
    }));
    put(tag, "varied_reuse", "FlatBuffers", time_op([&](size_t i) {
      fb_rich_encode(b, sc, varied_std[i & (kVaried - 1)]); fb_rich_decode(b.GetBufferPointer(), held); return touch_rich(held);
    }));
    put(tag, "varied_checked", "FlatBuffers", time_op([&](size_t i) {
      fb_rich_encode(b, sc, varied_std[i & (kVaried - 1)]);
      flatbuffers::Verifier vf(b.GetBufferPointer(), b.GetSize()); if (!fbr::VerifyRichCharacterBuffer(vf)) throw std::runtime_error("fbrv");
      fb_rich_decode(b.GetBufferPointer(), held); return touch_rich(held);
    }));
    // Hop (unpack -> mutate -> rebuild)
    std::vector<std::vector<uint8_t>> vwires(kVaried);
    for (size_t i = 0; i < kVaried; ++i) {
      fb_rich_encode(b, sc, varied_std[i]);
      vwires[i].assign(b.GetBufferPointer(), b.GetBufferPointer() + b.GetSize());
    }
    {
      fb_rich_decode(vwires[0].data(), held);
      mutate_rich_hop(held, 0, erase_keys[0]);
      fb_rich_encode(b, sc, held);
      rich_std::RichCharacter chk{}; fb_rich_decode(b.GetBufferPointer(), chk);
      expect("fb_hop", checksum_rich(chk), want_hop0);
    }
    put(tag, "hop_trusted", "FlatBuffers", time_op([&](size_t i) {
      const size_t k = i & (kVaried - 1);
      fb_rich_decode(vwires[k].data(), held);
      mutate_rich_hop(held, i, erase_keys[k]);
      fb_rich_encode(b, sc, held);
      clobber(b.GetBufferPointer());
      return uint64_t(b.GetSize()) ^ uint64_t(b.GetBufferPointer()[0]);
    }));
    put(tag, "hop_checked", "FlatBuffers", time_op([&](size_t i) {
      const size_t k = i & (kVaried - 1);
      flatbuffers::Verifier vf(vwires[k].data(), vwires[k].size()); if (!fbr::VerifyRichCharacterBuffer(vf)) throw std::runtime_error("fbrvh");
      fb_rich_decode(vwires[k].data(), held);
      mutate_rich_hop(held, i, erase_keys[k]);
      fb_rich_encode(b, sc, held);
      clobber(b.GetBufferPointer());
      return uint64_t(b.GetSize()) ^ uint64_t(b.GetBufferPointer()[0]);
    }));
  });

  // cista raw & offset
  auto add_c = [&]<class Ch, cista::mode FM>(const char* nm) {
    tasks.push_back([=] {
      Ch src{}; fill_cista_rich(src, src_std);
      auto w0 = cista::serialize<FM>(src);
      { auto cp = w0; expect(nm, checksum_rich(*cista::deserialize<Ch, FM>(cp)), want); }
      auto w0c = cista::serialize<cista::mode::NONE>(src);
      put(tag, "wire_raw", nm, double(w0.size()));
      put(tag, "wire_zstd3", nm, double(zstd3(w0.data(), w0.size())));
      cista::buf<std::vector<uint8_t>> out; std::vector<uint8_t> held;
      put(tag, "send_only", nm, time_op([&](size_t) {
        out.reset(); cista::serialize<FM>(out, src); clobber(out.buf_.data()); return uint64_t(out.buf_[0]);
      }));
      put(tag, "recv_trusted", nm, time_op([&](size_t) {
        held.assign(w0.begin(), w0.end()); return touch_rich(*cista::deserialize<Ch, FM>(held));
      }));
      put(tag, "recv_checked", nm, time_op([&](size_t) {
        held.assign(w0c.begin(), w0c.end()); return touch_rich(*cista::deserialize<Ch, cista::mode::DEEP_CHECK>(held));
      }));
      double tr = time_op([&](size_t) {
        out.reset(); cista::serialize<FM>(out, src); held.assign(out.buf_.begin(), out.buf_.end());
        return touch_rich(*cista::deserialize<Ch, FM>(held));
      });
      put(tag, "trusted_reuse", nm, tr);
      double cr = time_op([&](size_t) {
        out.reset(); cista::serialize<cista::mode::NONE>(out, src); held.assign(out.buf_.begin(), out.buf_.end());
        return touch_rich(*cista::deserialize<Ch, cista::mode::DEEP_CHECK>(held));
      });
      put(tag, "checked_reuse", nm, cr);
      put(tag, "view_admit", nm, time_op([&](size_t) {
        out.reset(); cista::serialize<FM>(out, src);
        return touch_rich(*cista::deserialize<Ch, FM>(out.buf_));
      }));
      put(tag, "view_bounds", nm, time_op([&](size_t) {
        out.reset(); cista::serialize<cista::mode::NONE>(out, src);
        return touch_rich(*cista::deserialize<Ch, cista::mode::DEEP_CHECK>(out.buf_));
      }));
      put(tag, "fresh_trusted", nm, time_op([&](size_t) {
        auto wf = cista::serialize<FM>(src); std::vector<uint8_t> h(wf.begin(), wf.end());
        return touch_rich(*cista::deserialize<Ch, FM>(h));
      }));
      put(tag, "fresh_checked", nm, time_op([&](size_t) {
        auto wf = cista::serialize<cista::mode::NONE>(src); std::vector<uint8_t> h(wf.begin(), wf.end());
        return touch_rich(*cista::deserialize<Ch, cista::mode::DEEP_CHECK>(h));
      }));
      std::vector<Ch> vc(kVaried);
      std::vector<std::vector<uint8_t>> vwires_t(kVaried), vwires_c(kVaried);
      for (size_t i = 0; i < kVaried; ++i) {
        fill_cista_rich(vc[i], varied_std[i]);
        vwires_t[i] = cista::serialize<FM>(vc[i]);
        vwires_c[i] = cista::serialize<cista::mode::NONE>(vc[i]);
      }
      put(tag, "varied_reuse", nm, time_op([&](size_t i) {
        out.reset(); cista::serialize<FM>(out, vc[i & (kVaried - 1)]); held.assign(out.buf_.begin(), out.buf_.end());
        return touch_rich(*cista::deserialize<Ch, FM>(held));
      }));
      put(tag, "varied_checked", nm, time_op([&](size_t i) {
        out.reset(); cista::serialize<cista::mode::NONE>(out, vc[i & (kVaried - 1)]); held.assign(out.buf_.begin(), out.buf_.end());
        return touch_rich(*cista::deserialize<Ch, cista::mode::DEEP_CHECK>(held));
      }));
      // Hop
      {
        held.assign(vwires_t[0].begin(), vwires_t[0].end());
        auto* obj = cista::deserialize<Ch, FM>(held);
        mutate_rich_hop(*obj, 0, erase_keys[0]);
        out.reset(); cista::serialize<FM>(out, *obj);
        cleanup_cista_hop(*obj);
        auto chk = out.buf_;
        expect("cista_hop", checksum_rich(*cista::deserialize<Ch, FM>(chk)), want_hop0);
      }
      put(tag, "hop_trusted", nm, time_op([&](size_t i) {
        const size_t k = i & (kVaried - 1);
        held.assign(vwires_t[k].begin(), vwires_t[k].end());
        auto* obj = cista::deserialize<Ch, FM>(held);
        mutate_rich_hop(*obj, i, erase_keys[k]);
        out.reset(); cista::serialize<FM>(out, *obj);
        cleanup_cista_hop(*obj);
        clobber(out.buf_.data());
        return uint64_t(out.buf_.size()) ^ uint64_t(out.buf_[0]);
      }));
      put(tag, "hop_checked", nm, time_op([&](size_t i) {
        const size_t k = i & (kVaried - 1);
        held.assign(vwires_c[k].begin(), vwires_c[k].end());
        auto* obj = cista::deserialize<Ch, cista::mode::DEEP_CHECK>(held);
        mutate_rich_hop(*obj, i, erase_keys[k]);
        out.reset(); cista::serialize<cista::mode::NONE>(out, *obj);
        cleanup_cista_hop(*obj);
        clobber(out.buf_.data());
        return uint64_t(out.buf_.size()) ^ uint64_t(out.buf_[0]);
      }));
      // In-place rw_rich on cista model
      Ch rw_c{}; fill_cista_rich(rw_c, src_std);
      put(tag, "rw_rich", nm, time_op([&](size_t i) {
        return mutate_rich_rw_tick(rw_c, i, orig_title.c_str(), orig_desc.c_str());
      }));
      expect("cista_rw_rich", checksum_rich(rw_c), want);
    });
  };
  add_c.template operator()<cista_rich_raw::RichCharacter, cista::mode::UNCHECKED>("cista raw");
  add_c.template operator()<cista_rich_offset::RichCharacter, cista::mode::CAST>("cista offset");

  // Cap'n Proto (fair zero-alloc reuse)
  tasks.push_back([&] {
    std::vector<capnp::word> scratch(16384), flat, w0;
    rich_std::RichCharacter held{};
    { capnp::MallocMessageBuilder m(kj::ArrayPtr<capnp::word>(scratch.data(), scratch.size()));
      cp_rich_build(m, src_std); auto ptr = cp_write_flat_reused(m, flat);
      w0.assign(ptr.begin(), ptr.end());
      cp_rich_decode(ptr, held); expect("cp_rich", checksum_rich(held), want);
      put(tag, "wire_raw", "Cap'n Proto", double(ptr.size() * sizeof(capnp::word)));
      put(tag, "wire_zstd3", "Cap'n Proto", double(zstd3(ptr.begin(), ptr.size() * sizeof(capnp::word))));
    }
    put(tag, "send_only", "Cap'n Proto", time_op([&](size_t) {
      capnp::MallocMessageBuilder m(kj::ArrayPtr<capnp::word>(scratch.data(), scratch.size()));
      cp_rich_build(m, src_std); auto ptr = cp_write_flat_reused(m, flat); clobber(ptr.begin()); return uint64_t(ptr.size());
    }));
    put(tag, "recv_checked", "Cap'n Proto", time_op([&](size_t) {
      cp_rich_decode(kj::ArrayPtr<const capnp::word>(w0.data(), w0.size()), held); return touch_rich(held);
    }));
    double sr = time_op([&](size_t) {
      capnp::MallocMessageBuilder m(kj::ArrayPtr<capnp::word>(scratch.data(), scratch.size()));
      cp_rich_build(m, src_std); auto ptr = cp_write_flat_reused(m, flat);
      cp_rich_decode(ptr, held); return touch_rich(held);
    });
    put(tag, "trusted_reuse", "Cap'n Proto", sr);
    put(tag, "checked_reuse", "Cap'n Proto", sr);
    put(tag, "view_bounds", "Cap'n Proto", time_op([&](size_t) {
      capnp::MallocMessageBuilder m(kj::ArrayPtr<capnp::word>(scratch.data(), scratch.size()));
      cp_rich_build(m, src_std); auto ptr = cp_write_flat_reused(m, flat);
      capnp::FlatArrayMessageReader reader(ptr);
      auto r = reader.getRoot<cpr::RichCharacter>();
      return uint64_t(r.getId() ^ r.getItems().size() ^ r.getQuests().size());
    }));
    double fr = time_op([&](size_t) {
      capnp::MallocMessageBuilder m; cp_rich_build(m, src_std); auto wf = capnp::messageToFlatArray(m);
      rich_std::RichCharacter d{}; cp_rich_decode(wf.asPtr(), d); return touch_rich(d);
    });
    put(tag, "fresh_trusted", "Cap'n Proto", fr);
    put(tag, "fresh_checked", "Cap'n Proto", fr);
    double vr = time_op([&](size_t i) {
      capnp::MallocMessageBuilder m(kj::ArrayPtr<capnp::word>(scratch.data(), scratch.size()));
      cp_rich_build(m, varied_std[i & (kVaried - 1)]); auto ptr = cp_write_flat_reused(m, flat);
      cp_rich_decode(ptr, held); return touch_rich(held);
    });
    put(tag, "varied_reuse", "Cap'n Proto", vr);
    put(tag, "varied_checked", "Cap'n Proto", vr);
    // Hop
    std::vector<std::vector<capnp::word>> vwires(kVaried);
    for (size_t i = 0; i < kVaried; ++i) {
      capnp::MallocMessageBuilder m(kj::ArrayPtr<capnp::word>(scratch.data(), scratch.size()));
      cp_rich_build(m, varied_std[i]); auto ptr = cp_write_flat_reused(m, flat);
      vwires[i].assign(ptr.begin(), ptr.end());
    }
    {
      cp_rich_decode(kj::ArrayPtr<const capnp::word>(vwires[0].data(), vwires[0].size()), held);
      mutate_rich_hop(held, 0, erase_keys[0]);
      capnp::MallocMessageBuilder m(kj::ArrayPtr<capnp::word>(scratch.data(), scratch.size()));
      cp_rich_build(m, held); auto ptr = cp_write_flat_reused(m, flat);
      rich_std::RichCharacter chk{}; cp_rich_decode(ptr, chk);
      expect("cp_hop", checksum_rich(chk), want_hop0);
    }
    double hop = time_op([&](size_t i) {
      const size_t k = i & (kVaried - 1);
      cp_rich_decode(kj::ArrayPtr<const capnp::word>(vwires[k].data(), vwires[k].size()), held);
      mutate_rich_hop(held, i, erase_keys[k]);
      capnp::MallocMessageBuilder m(kj::ArrayPtr<capnp::word>(scratch.data(), scratch.size()));
      cp_rich_build(m, held); auto ptr = cp_write_flat_reused(m, flat);
      clobber(ptr.begin());
      return uint64_t(ptr.size());
    });
    put(tag, "hop_trusted", "Cap'n Proto", hop);
    put(tag, "hop_checked", "Cap'n Proto", hop);
  });

  // MessagePack
  tasks.push_back([&] {
    msgpack::sbuffer w0, w; msgpack::pack(w0, src_std);
    rich_std::RichCharacter held{}; msgpack::unpack(w0.data(), w0.size()).get().convert(held); expect("mp_rich", checksum_rich(held), want);
    put(tag, "wire_raw", "MessagePack", double(w0.size()));
    put(tag, "wire_zstd3", "MessagePack", double(zstd3(w0.data(), w0.size())));
    put(tag, "send_only", "MessagePack", time_op([&](size_t) {
      w.clear(); msgpack::pack(w, src_std); clobber(w.data()); return uint64_t(w.size());
    }));
    put(tag, "recv_checked", "MessagePack", time_op([&](size_t) {
      msgpack::unpack(w0.data(), w0.size()).get().convert(held); return touch_rich(held);
    }));
    double sr = time_op([&](size_t) {
      w.clear(); msgpack::pack(w, src_std);
      msgpack::unpack(w.data(), w.size()).get().convert(held); return touch_rich(held);
    });
    put(tag, "trusted_reuse", "MessagePack", sr);
    put(tag, "checked_reuse", "MessagePack", sr);
    double fr = time_op([&](size_t) {
      msgpack::sbuffer wf; msgpack::pack(wf, src_std);
      rich_std::RichCharacter d{}; msgpack::unpack(wf.data(), wf.size()).get().convert(d); return touch_rich(d);
    });
    put(tag, "fresh_trusted", "MessagePack", fr);
    put(tag, "fresh_checked", "MessagePack", fr);
    double vr = time_op([&](size_t i) {
      w.clear(); msgpack::pack(w, varied_std[i & (kVaried - 1)]);
      msgpack::unpack(w.data(), w.size()).get().convert(held); return touch_rich(held);
    });
    put(tag, "varied_reuse", "MessagePack", vr);
    put(tag, "varied_checked", "MessagePack", vr);
    // Hop
    std::vector<std::string> vwires(kVaried);
    for (size_t i = 0; i < kVaried; ++i) {
      w.clear(); msgpack::pack(w, varied_std[i]);
      vwires[i].assign(w.data(), w.size());
    }
    {
      msgpack::unpack(vwires[0].data(), vwires[0].size()).get().convert(held);
      mutate_rich_hop(held, 0, erase_keys[0]);
      w.clear(); msgpack::pack(w, held);
      rich_std::RichCharacter chk{}; msgpack::unpack(w.data(), w.size()).get().convert(chk);
      expect("mp_hop", checksum_rich(chk), want_hop0);
    }
    double hop = time_op([&](size_t i) {
      const size_t k = i & (kVaried - 1);
      msgpack::unpack(vwires[k].data(), vwires[k].size()).get().convert(held);
      mutate_rich_hop(held, i, erase_keys[k]);
      w.clear(); msgpack::pack(w, held);
      clobber(w.data());
      return uint64_t(w.size());
    });
    put(tag, "hop_trusted", "MessagePack", hop);
    put(tag, "hop_checked", "MessagePack", hop);
  });

  // Protocol Buffers
  tasks.push_back([&] {
    protobuf_rich::RichCharacter src, held; fill_rich_pb(src, src_std);
    std::string w0, w; src.SerializeToString(&w0); held.ParseFromString(w0); expect("pb_rich", checksum_rich_pb(held), want);
    put(tag, "wire_raw", "Protocol Buffers", double(w0.size()));
    put(tag, "wire_zstd3", "Protocol Buffers", double(zstd3(w0.data(), w0.size())));
    put(tag, "send_only", "Protocol Buffers", time_op([&](size_t) {
      src.SerializeToString(&w); clobber(w.data()); return uint64_t(w.size());
    }));
    put(tag, "recv_checked", "Protocol Buffers", time_op([&](size_t) {
      held.ParseFromString(w0); return touch_rich_pb(held);
    }));
    double sr = time_op([&](size_t) {
      src.SerializeToString(&w); held.ParseFromString(w); return touch_rich_pb(held);
    });
    put(tag, "trusted_reuse", "Protocol Buffers", sr);
    put(tag, "checked_reuse", "Protocol Buffers", sr);
    double fr = time_op([&](size_t) {
      std::string wf; protobuf_rich::RichCharacter d; src.SerializeToString(&wf); d.ParseFromString(wf); return touch_rich_pb(d);
    });
    put(tag, "fresh_trusted", "Protocol Buffers", fr);
    put(tag, "fresh_checked", "Protocol Buffers", fr);
    std::vector<protobuf_rich::RichCharacter> vpb(kVaried);
    std::vector<std::string> vwires(kVaried);
    for (size_t i = 0; i < kVaried; ++i) {
      fill_rich_pb(vpb[i], varied_std[i]);
      vpb[i].SerializeToString(&vwires[i]);
    }
    double vr = time_op([&](size_t i) {
      vpb[i & (kVaried - 1)].SerializeToString(&w); held.ParseFromString(w); return touch_rich_pb(held);
    });
    put(tag, "varied_reuse", "Protocol Buffers", vr);
    put(tag, "varied_checked", "Protocol Buffers", vr);
    // Hop
    {
      held.ParseFromString(vwires[0]);
      mutate_rich_hop_pb(held, 0, erase_keys[0], first_quest_keys[0]);
      held.SerializeToString(&w);
      protobuf_rich::RichCharacter chk; chk.ParseFromString(w);
      expect("pb_hop", checksum_rich_pb(chk), want_hop0);
    }
    double hop = time_op([&](size_t i) {
      const size_t k = i & (kVaried - 1);
      held.ParseFromString(vwires[k]);
      mutate_rich_hop_pb(held, i, erase_keys[k], first_quest_keys[k]);
      held.SerializeToString(&w);
      clobber(w.data());
      return uint64_t(w.size());
    });
    put(tag, "hop_trusted", "Protocol Buffers", hop);
    put(tag, "hop_checked", "Protocol Buffers", hop);
    // In-place rw_rich on Protobuf
    protobuf_rich::RichCharacter rw_pb; fill_rich_pb(rw_pb, src_std);
    put(tag, "rw_rich", "Protocol Buffers", time_op([&](size_t i) {
      return mutate_rich_rw_tick_pb(rw_pb, i, src_first_quest, orig_title.c_str(), orig_desc.c_str());
    }));
    expect("pb_rw_rich", checksum_rich_pb(rw_pb), want);
  });

  std::shuffle(tasks.begin(), tasks.end(), rng);
  for (auto& fn : tasks) fn();
}

int main(int argc, char** argv) {
  uint64_t seed = 1;
  for (int i = 1; i < argc; ++i) {
    if (std::string(argv[i]) == "--seed" && i + 1 < argc) seed = std::stoull(argv[++i]);
    else if (std::string(argv[i]) == "--quick") g_quick = true;
  }

  auto rc3 = make_rich_character(2026, kRich3KB, false);
  auto rc12 = make_rich_character(2026, kRich12KB, false);
  std::printf("# byte_serializable: flat_item=%d flat_item_pair=%d flat_pos=%d flat_skill=%d | rich_item=%d rich_equip=%d rich_quest=%d\n",
              int(zpp::bits::concepts::byte_serializable<stdm::Item>),
              int(zpp::bits::concepts::byte_serializable<std::pair<uint64_t, stdm::Item>>),
              int(zpp::bits::concepts::byte_serializable<stdm::Position>),
              int(zpp::bits::concepts::byte_serializable<stdm::Skill>),
              int(zpp::bits::concepts::byte_serializable<rich_std::RichItem>),
              int(zpp::bits::concepts::byte_serializable<rich_std::RichEquip>),
              int(zpp::bits::concepts::byte_serializable<rich_std::Quest>));
  std::printf("# containers/strings count: Flat-3KB=14, Rich-3KB=%zu, Flat-12KB=38, Rich-12KB=%zu\n",
              count_rich_containers(rc3), count_rich_containers(rc12));

  std::mt19937_64 rng(seed * 0x9e3779b97f4a7c15ull + 1337);
  run_flat_workload("Flat-3KB", 1, rng);
  run_rich_workload("Rich-3KB", kRich3KB, rng);
  run_flat_workload("Flat-12KB", 4, rng);
  run_rich_workload("Rich-12KB", kRich12KB, rng);
  return 0;
}
