// CppCon 2024 Character case, shared parts: the data, the read/write operations,
// the plain std model the std-based rivals use, timing and a checksum.
#pragma once
#include <algorithm>
#include <bit>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <random>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace cc24 {

// ---- plain std model (zpp::bits, MessagePack, FlatBuffers and Cap'n Proto hold this) ----
namespace stdm {
struct Skill { uint32_t id, level; };
struct Item { uint32_t id; uint32_t number; uint64_t uuid; uint64_t timestamp; };
struct Position { float x, y, z; };
struct Equip { uint32_t id; uint32_t number; uint64_t uuid; uint64_t timestamp; uint32_t level; std::vector<float> attributes; };
// items: a sorted vector, like XOffset's XMap (flat). The 2024 rivals used std::map,
// which made them slower; the 2026-11-03 fairness audit moved all std models to flat.
struct Character {
  uint64_t id; std::string name; uint32_t level; float healthpoint, manapoint, speed; Position pos;
  std::vector<std::pair<uint64_t, Item>> items; std::vector<Position> path; std::vector<float> attributes;
  std::vector<Equip> equips; std::vector<Skill> skills;
};
inline void items_set(std::vector<std::pair<uint64_t, Item>>& v, uint64_t k, const Item& it) {
  auto p = std::lower_bound(v.begin(), v.end(), k, [](const auto& e, uint64_t key) { return e.first < key; });
  if (p != v.end() && p->first == k) p->second = it; else v.insert(p, {k, it});
}
inline void items_erase(std::vector<std::pair<uint64_t, Item>>& v, uint64_t k) {
  auto p = std::lower_bound(v.begin(), v.end(), k, [](const auto& e, uint64_t key) { return e.first < key; });
  if (p != v.end() && p->first == k) v.erase(p);
}
}  // namespace stdm

// ---- the data: same shape as 2024 (64 items, 16 path points, 16 attributes, 8 equips x 8, 4 skills) ----
struct Gen {
  std::mt19937_64 g{0};
  std::uniform_int_distribution<uint64_t> i64;
  std::uniform_int_distribution<uint32_t> i32;
  std::uniform_real_distribution<float> f;
  uint64_t u64() { return i64(g); }
  uint32_t u32() { return i32(g); }
  float fl() { return f(g); }
};

inline stdm::Character make_character() {
  Gen r;
  const std::string cs = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
  std::uniform_int_distribution<int> d(0, int(cs.size()) - 1);
  std::string name; for (int i = 0; i < 8; ++i) name += cs[size_t(d(r.g))];
  r.g.seed(0);
  stdm::Character c{};
  c.id = r.u64(); c.name = name; c.level = r.u32();
  c.healthpoint = r.fl(); c.manapoint = r.fl(); c.speed = r.fl();
  c.pos.x = r.fl(); c.pos.y = r.fl(); c.pos.z = r.fl();
  for (int i = 0; i < 64; ++i) {
    uint64_t k = r.u64(); stdm::Item it{}; it.id = r.u32(); it.uuid = r.u64(); it.number = r.u32(); it.timestamp = r.u64();
    stdm::items_set(c.items, k, it);
  }
  for (int i = 0; i < 16; ++i) { stdm::Position p{}; p.x = r.fl(); p.y = r.fl(); p.z = r.fl(); c.path.push_back(p); }
  for (int i = 0; i < 16; ++i) c.attributes.push_back(r.fl());
  for (int i = 0; i < 8; ++i) {
    stdm::Equip e{}; e.id = r.u32(); e.uuid = r.u64(); e.number = r.u32(); e.timestamp = r.u64(); e.level = r.u32();
    for (int k = 0; k < 8; ++k) e.attributes.push_back(r.fl());
    c.equips.push_back(std::move(e));
  }
  for (int i = 0; i < 4; ++i) { stdm::Skill s{}; s.id = r.u32(); s.level = r.u32(); c.skills.push_back(s); }
  return c;
}

// ---- the 2024 read/write loop, with its random values drawn before timing ----
// One iteration: +1 on 4 scalars, set pos, 4x (append an equip with 8 attributes, drop it),
// append 8 path points and 1 attribute, insert and erase one item, drop the 8 points and
// the attribute, -1 on the 4 scalars. The 2024 loop drew 85 mt19937_64 numbers inside the
// timed loop, which cost about as much as the data work (2026-11-04 rerun: 871 of 890 ns).
struct EquipOp { uint32_t id; uint64_t uuid; uint32_t number; uint64_t timestamp; uint32_t level; float attr[8]; };
struct OpSet {
  float pos[3]; EquipOp equip[4]; float path[8][3]; float attribute; uint64_t key;
  uint32_t item_id; uint64_t item_uuid; uint32_t item_number; uint64_t item_timestamp;
};
constexpr size_t kOpRing = 1024;
inline std::vector<OpSet> make_ops() {
  Gen r; r.g.seed(2024);
  std::vector<OpSet> v(kOpRing);
  for (auto& o : v) {
    for (auto& x : o.pos) x = r.fl();
    for (auto& e : o.equip) { e.id = r.u32(); e.uuid = r.u64(); e.number = r.u32(); e.timestamp = r.u64(); e.level = r.u32(); for (auto& a : e.attr) a = r.fl(); }
    for (auto& p : o.path) for (auto& x : p) x = r.fl();
    o.attribute = r.fl(); o.key = r.u64();
    o.item_id = r.u32(); o.item_uuid = r.u64(); o.item_number = r.u32(); o.item_timestamp = r.u64();
  }
  return v;
}

// Make the temporary objects visible to the optimizer before removal.
// The GCC/Clang benchmark uses a compiler barrier with no runtime call.
template <class T>
inline void observe(const T& value) {
  asm volatile("" : : "g"(&value) : "memory");
}

// The loop body for every model with the 2024 member names (std, cista, XOffset).
// items_set / items_erase are found by argument-dependent lookup on the model's map type.
template <class Ch, class ItemT, class SetFn, class EraseFn>
inline void rw_once(Ch& r, const OpSet& o, SetFn&& set, EraseFn&& erase) {
  r.level += 1; r.healthpoint += 1; r.manapoint += 1; r.speed += 1;
  r.pos.x = o.pos[0]; r.pos.y = o.pos[1]; r.pos.z = o.pos[2];
  for (const auto& eo : o.equip) {
    auto& e = r.equips.emplace_back();
    e.id = eo.id; e.uuid = eo.uuid; e.number = eo.number; e.timestamp = eo.timestamp; e.level = eo.level;
    for (float a : eo.attr) e.attributes.push_back(a);
    observe(e);
    r.equips.pop_back();
  }
  for (const auto& p : o.path) { r.path.emplace_back(); auto& b = r.path.back(); b.x = p[0]; b.y = p[1]; b.z = p[2]; }
  r.attributes.push_back(o.attribute);
  ItemT it{}; it.id = o.item_id; it.uuid = o.item_uuid; it.number = o.item_number; it.timestamp = o.item_timestamp;
  set(r.items, o.key, it);
  observe(r);
  erase(r.items, o.key);
  r.attributes.pop_back();
  for (int i = 0; i < 8; ++i) r.path.pop_back();
  r.speed -= 1; r.manapoint -= 1; r.healthpoint -= 1; r.level -= 1;
}

// ---- checksum over every field (items order-independent: cista's map is a hash map) ----
inline uint64_t mix(uint64_t h, uint64_t v) { return (h ^ (v + 0x9e3779b97f4a7c15ull + (h << 6) + (h >> 2))) * 0xff51afd7ed558ccdull; }
inline uint64_t fbits(float f) { return std::bit_cast<uint32_t>(f); }
template <class S> inline std::string_view sv(const S& s) {
  if constexpr (requires { s.view(); }) return std::string_view(s.view().data(), s.view().size());
  else return std::string_view(s.data(), s.size());
}
template <class Ch> inline uint64_t checksum(const Ch& c) {
  uint64_t h = mix(0, c.id);
  h = mix(h, sv(c.name).size());
  for (char ch : sv(c.name)) h = mix(h, uint8_t(ch));
  h = mix(h, c.level); h = mix(h, fbits(c.healthpoint)); h = mix(h, fbits(c.manapoint)); h = mix(h, fbits(c.speed));
  h = mix(h, fbits(c.pos.x)); h = mix(h, fbits(c.pos.y)); h = mix(h, fbits(c.pos.z));
  uint64_t items = 0;
  for (const auto& kv : c.items) {
    const auto& it = kv.second;
    items += mix(mix(mix(mix(mix(1, kv.first), it.id), it.uuid), it.number), it.timestamp);
  }
  h = mix(h, items); h = mix(h, c.items.size());
  h = mix(h, c.path.size());
  for (const auto& p : c.path) { h = mix(h, fbits(p.x)); h = mix(h, fbits(p.y)); h = mix(h, fbits(p.z)); }
  h = mix(h, c.attributes.size());
  for (float a : c.attributes) h = mix(h, fbits(a));
  h = mix(h, c.equips.size());
  for (const auto& e : c.equips) {
    h = mix(mix(mix(mix(mix(h, e.id), e.uuid), e.number), e.timestamp), e.level);
    h = mix(h, e.attributes.size());
    for (float a : e.attributes) h = mix(h, fbits(a));
  }
  h = mix(h, c.skills.size());
  for (const auto& s : c.skills) h = mix(mix(h, s.id), s.level);
  return h;
}

// ---- timing ----
using Clock = std::chrono::steady_clock;
template <class F> double ns_per(size_t iters, F&& f) {
  auto t0 = Clock::now();
  for (size_t i = 0; i < iters; ++i) f(i);
  auto t1 = Clock::now();
  return std::chrono::duration<double, std::nano>(t1 - t0).count() / double(iters);
}
template <class F> double median_of(int rounds, F&& f) {
  std::vector<double> v; for (int r = 0; r < rounds; ++r) v.push_back(f());
  std::sort(v.begin(), v.end()); return v[v.size() / 2];
}

}  // namespace cc24
