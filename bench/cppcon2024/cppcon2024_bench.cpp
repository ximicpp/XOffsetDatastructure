// CppCon 2024 comparison, rerun on XOffset 4.0.
//
// The 2024 talk (tag v1.0.0, benchmark/xbenchmark3.cpp) compared eight libraries on one
// game Character: send+recv time, the read/write loop, and wire size. This program keeps
// the data shape, the loop and the rivals, with today's measurement rules:
//   - timed: only data read/write and send/receive (no RNG, no setup, no compression);
//   - send = state -> bytes another process can use; recv = bytes -> state the receiver can
//     read AND modify (the 2024 claim); every decoded state is checked once, untimed;
//   - XOffset receives on its trusted tier (XReceiver<server_peer>), rivals without verify
//     (FlatBuffers no Verifier, cista UNCHECKED/CAST); XOffset's full check is report-only;
//   - bytes: raw and zstd level 3 (zstd not timed); the wire format is not tuned for size.
// Two send+recv modes: "reuse" (sender and receiver keep their buffers and objects, the
// steady state of a server loop) and "fresh" (new buffers and objects per message, as 2024).
#include "xoffsetdatastructure.hpp"
#include "xoffset_receiver.hpp"

#define MSGPACK_NO_BOOST 1
#include <msgpack.hpp>
#include "zpp_bits.h"
#include <cista/containers/hash_map.h>
#include <cista/containers/string.h>
#include <cista/containers/vector.h>
#include <cista/mode.h>
#include <cista/serialization.h>
#include <cista/targets/buf.h>
#include <flatbuffers/flatbuffers.h>
#include <capnp/message.h>
#include <capnp/serialize.h>
#include <zstd.h>

#include "character_generated.h"   // flatc schema/character.fbs
#include "character.capnp.h"        // capnp compile schema/character.capnp
#include "character.pb.h"           // protoc schema/character.proto

#include <cstring>
#include <map>
#include <memory>
#include <optional>

#include "common.hpp"

using namespace cc24;

// ============================ XOffset 4.0 model ============================
#include "character.hpp"

// ============================ cista models (as 2024: raw and offset) ============================
#define CC24_CISTA_MODEL(NS)                                                                                   \
  namespace cista_##NS {                                                                                      \
  struct Skill { uint32_t id, level; };                                                                       \
  struct Item { uint32_t id; uint32_t number; uint64_t uuid; uint64_t timestamp; };                           \
  struct Position { float x, y, z; };                                                                         \
  struct Equip { uint32_t id; uint32_t number; uint64_t uuid; uint64_t timestamp; uint32_t level;             \
                 cista::NS::vector<float> attributes; };                                                      \
  struct Character {                                                                                          \
    uint64_t id; cista::NS::string name; uint32_t level; float healthpoint, manapoint, speed; Position pos;   \
    cista::NS::hash_map<uint64_t, Item> items; cista::NS::vector<Position> path;                             \
    cista::NS::vector<float> attributes; cista::NS::vector<Equip> equips; cista::NS::vector<Skill> skills;   \
  };                                                                                                          \
  /* tuned layout: equipment attributes inline up to 8 (the "xoffset inline" layout) */                       \
  struct InlineEquip { uint32_t id; uint32_t number; uint64_t uuid; uint64_t timestamp; uint32_t level;       \
                       cc24::InlineAttrs<cista::NS::vector<float>> attributes; };                             \
  struct CharacterInline {                                                                                    \
    uint64_t id; cista::NS::string name; uint32_t level; float healthpoint, manapoint, speed; Position pos;   \
    cista::NS::hash_map<uint64_t, Item> items; cista::NS::vector<Position> path;                             \
    cista::NS::vector<float> attributes; cista::NS::vector<InlineEquip> equips;                               \
    cista::NS::vector<Skill> skills;                                                                          \
  };                                                                                                          \
  }
CC24_CISTA_MODEL(raw)
CC24_CISTA_MODEL(offset)

// ============================ MessagePack bindings for the std model ============================
namespace cc24::stdm {
}  // the std model is an aggregate; MessagePack binds it non-intrusively below
namespace msgpack { MSGPACK_API_VERSION_NAMESPACE(MSGPACK_DEFAULT_API_NS) { namespace adaptor {
#define CC24_MSGPACK(T, ...)                                                                                    \
  template <> struct convert<T> { const msgpack::object& operator()(const msgpack::object& o, T& v) const {    \
      auto& [__VA_ARGS__] = v; msgpack::type::make_define_array(__VA_ARGS__).msgpack_unpack(o); return o; } };   \
  template <> struct pack<T> { template <class S> msgpack::packer<S>& operator()(msgpack::packer<S>& p, const T& v) const { \
      const auto& [__VA_ARGS__] = v; msgpack::type::make_define_array(__VA_ARGS__).msgpack_pack(p); return p; } };
CC24_MSGPACK(cc24::stdm::Skill, a, b)
CC24_MSGPACK(cc24::stdm::Item, a, b, c, d)
CC24_MSGPACK(cc24::stdm::Position, a, b, c)
CC24_MSGPACK(cc24::stdm::Equip, a, b, c, d, e, f)
CC24_MSGPACK(cc24::stdm::Character, a, b, c, d, e, f, g, h, i, j, k, l)
}}}

// ============================ output ============================
static void put(const char* lib, const char* metric, double v) { std::printf("%-22s %-26s %12.1f\n", lib, metric, v); std::fflush(stdout); }
static size_t zstd3(const void* p, size_t n) {
  std::vector<char> out(ZSTD_compressBound(n));
  size_t z = ZSTD_compress(out.data(), out.size(), p, n, 3);
  if (ZSTD_isError(z)) throw std::runtime_error("zstd");
  return z;
}
static void put_bytes(const char* lib, const void* p, size_t n) { put(lib, "bytes", double(n)); put(lib, "bytes_zstd3", double(zstd3(p, n))); }
static void expect(const char* lib, uint64_t got, uint64_t want) {
  if (got != want) { std::fprintf(stderr, "%s: decoded state differs from the sent state\n", lib); std::exit(2); }
}

static size_t kRwIters = 100000, kSrIters = 50000; static int kRounds = 5;
static stdm::Character g_char;
static uint64_t g_sum;      // checksum of the data every format sends
static uint64_t g_rw_sum;   // checksum after the timed read/write rounds (the std model's, computed first)
static std::vector<OpSet> g_ops;
static volatile uint64_t g_sink;

// send+recv timing; f(i) does one message and returns something read from the receiver's state.
template <class F> static double time_sr(F&& f) {
  uint64_t acc = 0; double ns = median_of(kRounds, [&] { return ns_per(kSrIters, [&](size_t i) { acc += f(i); }); });
  g_sink = g_sink + acc; return ns;
}
template <class F> static double time_rw(F&& f) {
  return median_of(kRounds, [&] { return ns_per(kRwIters, [&](size_t i) { f(g_ops[i & (kOpRing - 1)]); }); });
}

// ============================ std model (rw shared by zpp::bits, MessagePack, FlatBuffers, Cap'n Proto) ============================
static void bench_std_rw() {
  stdm::Character c = g_char;
  put("std (shared)", "rw", time_rw([&](const OpSet& o) {
    rw_once<stdm::Character, stdm::Item>(c, o, [](auto& m, uint64_t k, const stdm::Item& it) { stdm::items_set(m, k, it); },
                                           [](auto& m, uint64_t k) { stdm::items_erase(m, k); }); }));
  g_rw_sum = checksum(c);
  // tuned: inline equipment attributes and the bulk calls (see rw_tuned)
  stdm::CharacterInline t{}; fill_inline(t, g_char);
  expect("std inline", checksum(t), g_sum);
  put("std inline", "rw", time_rw([&](const OpSet& o) {
    rw_tuned<stdm::CharacterInline, stdm::Item, stdm::Position>(t, o,
      [](auto& m, uint64_t k, const stdm::Item& it) { return stdm::items_upsert(m, k, it); },
      [](auto& m, auto pos) { m.erase(pos); }); }));
  expect("std inline rw", checksum(t), g_rw_sum);
}

// ============================ XOffset 4.0 ============================
// Same Character and mutations as rw_once, using typed range assignment and
// reducing the number of independently checked path and attribute operations.
template<class C>
static void rw_xoffset_bulk(C& r, const OpSet& o) {
  r.level += 1; r.healthpoint += 1; r.manapoint += 1; r.speed += 1;
  r.pos = {o.pos[0], o.pos[1], o.pos[2]};
  if (kEquipPart) for (const auto& eo : o.equip) {
    auto& e = r.equips.emplace_back();
    e.id = eo.id; e.uuid = eo.uuid; e.number = eo.number; e.timestamp = eo.timestamp; e.level = eo.level;
    if constexpr (std::is_same_v<C, xo::Character>)
      e.attributes.assign_range(eo.attr);
    else
      e.attributes.assign(eo.attr);
    observe(e);
    r.equips.pop_back();
  }
  const auto old_path_size = r.path.size();
  if (kPathPart) {
    if constexpr (std::is_same_v<C, xo::Character>) {
      r.path.resize(old_path_size + std::size(o.path));
      auto* tail = r.path.data() + old_path_size;
      for (const auto& p : o.path) *tail++ = {p[0], p[1], p[2]};
    } else {
      std::array<xo::Position, std::size(OpSet{}.path)> tail;
      for (size_t i = 0; i < tail.size(); ++i)
        tail[i] = {o.path[i][0], o.path[i][1], o.path[i][2]};
      r.path.append_range(tail);
    }
    r.attributes.push_back(o.attribute);
  }
  if (kItemPart) {
    const xo::Item item{o.item_id, o.item_number, o.item_uuid, o.item_timestamp};
    auto [it, inserted] = r.items.try_emplace(o.key, item);
    if (!inserted) it->second = item;
    observe(r);
    r.items.erase_at(it);
  } else {
    observe(r);
  }
  if (kPathPart) {
    r.attributes.pop_back();
    r.path.resize(old_path_size);
  }
  r.speed -= 1; r.manapoint -= 1; r.healthpoint -= 1; r.level -= 1;
}

static void bench_xoffset() {
  using namespace XOffsetDatastructure;
  using C = xo::Character;
  const auto cap = XBuffer::maximum_capacity(1 << 24);
  auto make = [&] {
    auto b = TypedXBuffer<C>::create(16 * 1024, cap);
    b.edit([&](C& r) { xo::fill(r, g_char); });
    if (!b.ok()) throw std::runtime_error("xoffset fill failed");
    return b;
  };
  auto src = make();
  const std::string w0 = src.save_verified();
  put_bytes("xoffset 4.0", w0.data(), w0.size());
  XReceiver<XLink::server_peer> rx(cap);
  {  // decoded state check, both tiers
    auto t = rx.load<C>(w0); if (!t) throw std::runtime_error("trusted load rejected");
    expect("xoffset trusted", checksum(t->root<C>()), g_sum);
    auto v = TypedXBuffer<C>::load(w0, cap); expect("xoffset full", checksum(v.plain_root()), g_sum);
  }
  // reuse: save into the sender's string; trusted reload into the receiver's buffer
  std::string w; auto held = TypedXBuffer<C>::load(w0, cap);
  put("xoffset 4.0", "sr_reuse", time_sr([&](size_t) {
    src.save_verified_into(w);
    if (!rx.reload(held, w)) throw std::runtime_error("trusted reload rejected");
    return held.plain_root().id; }));
  // fresh: a new wire string and a new receiving buffer per message
  put("xoffset 4.0", "sr_fresh", time_sr([&](size_t) {
    std::string wf = src.save_verified();
    auto r = rx.load<C>(wf); if (!r) throw std::runtime_error("trusted load rejected");
    return r->root<C>().id; }));
  // report-only: the full check (what an untrusted link uses)
  put("xoffset 4.0 full", "sr_reuse", time_sr([&](size_t) {
    src.save_verified_into(w);
    held.reload_verified(w);
    if (!held.ok()) throw std::runtime_error("full reload rejected");
    return held.plain_root().id; }));
  put("xoffset 4.0 full", "sr_fresh", time_sr([&](size_t) {
    std::string wf = src.save_verified(); auto r = TypedXBuffer<C>::load(wf, cap); return r.plain_root().id; }));
  // read/write in place: one pin scope (edit) per iteration, the recommended hot-loop
  // style (plain references inside; blocks freed inside a scope are reused after it)
  auto rwb = make();
  auto body = [](C& r, const OpSet& o) {
    rw_once<C, xo::Item>(r, o, [](auto& m, uint64_t k, const xo::Item& it) { m[k] = it; }, [](auto& m, uint64_t k) { m.erase(k); }); };
  double rw = time_rw([&](const OpSet& o) { rwb.edit([&](C& r) { body(r, o); }); });
  if (!rwb.ok()) throw std::runtime_error("xoffset rw failed");
  // report-only: plain_root() reference, no pin scope (what the 2024 code did with a raw pointer)
  auto rwp = make(); C& pr = rwp.plain_root();
  put("xoffset 4.0 plain_root", "rw", time_rw([&](const OpSet& o) { body(pr, o); }));
  if (!rwp.ok()) throw std::runtime_error("xoffset rw (plain_root) failed");
  auto bulk = make();
  put("xoffset bulk", "rw", time_rw([&](const OpSet& o) {
    bulk.edit([&](C& r) { rw_xoffset_bulk(r, o); });
  }));
  if (!bulk.ok()) throw std::runtime_error("xoffset bulk rw failed");
  expect("xoffset bulk rw", checksum(bulk.plain_root()), g_rw_sum);
  put("xoffset 4.0", "rw", rw);
  expect("xoffset rw", checksum(rwb.plain_root()), g_rw_sum);
  expect("xoffset rw plain_root", checksum(rwp.plain_root()), g_rw_sum);
  // report-only: what a send after the loop carries (free blocks travel with the wire)
  auto after = rwb.save_verified();
  auto compact = rwb.save_compact();
  put("xoffset 4.0", "bytes_after_rw", double(after.size()));
  put("xoffset 4.0", "bytes_after_rw_zstd3", double(zstd3(after.data(), after.size())));
  put("xoffset 4.0", "bytes_after_rw_compact", double(compact.size()));
  put("xoffset 4.0", "bytes_after_rw_compact_zstd3", double(zstd3(compact.data(), compact.size())));
}

template<class C>
static void bench_xoffset_layout(const char* name) {
  using namespace XOffsetDatastructure;
  const auto cap = XBuffer::maximum_capacity(1 << 24);
  auto src = TypedXBuffer<C>::create(16 * 1024, cap);
  src.edit([&](C& r) { xo::fill(r, g_char); });
  if (!src.ok()) throw std::runtime_error("xoffset layout fill failed");
  const auto wire = src.save_verified();
  put_bytes(name, wire.data(), wire.size());
  XReceiver<XLink::server_peer> rx(cap);
  auto held = TypedXBuffer<C>::load(wire, cap);
  expect(name, checksum(held.plain_root()), g_sum);
  auto trusted = rx.load<C>(wire);
  if (!trusted) throw std::runtime_error("trusted layout load rejected");
  expect(name, checksum(trusted->template root<C>()), g_sum);

  std::string w;
  put(name, "sr_reuse", time_sr([&](size_t) {
    src.save_verified_into(w);
    if (!rx.reload(held, w)) throw std::runtime_error("trusted layout reload rejected");
    return held.plain_root().id;
  }));
  put(name, "sr_fresh", time_sr([&](size_t) {
    auto fresh = rx.load<C>(src.save_verified());
    if (!fresh) throw std::runtime_error("trusted layout load rejected");
    return fresh->template root<C>().id;
  }));
  // rw: one edit() per iteration, as the "xoffset 4.0" row (each iteration is one update).
  put(name, "rw", time_rw([&](const OpSet& o) {
    if (!src.edit([&](C& r) { rw_xoffset_bulk(r, o); })) throw std::runtime_error("xoffset layout edit failed");
  }));
  expect(name, checksum(src.plain_root()), g_rw_sum);
  // report-only: a round is one edit batch (one pin scope around all iterations). The
  // timer includes opening and closing the pin.
  put(name, "rw_one_scope", median_of(kRounds, [&] {
    const auto start = Clock::now();
    if (!src.edit([&](C& r) {
      for (size_t i = 0; i < kRwIters; ++i)
        rw_xoffset_bulk(r, g_ops[i & (kOpRing - 1)]);
    })) throw std::runtime_error("xoffset layout edit failed");
    const auto end = Clock::now();
    return std::chrono::duration<double, std::nano>(end - start).count() / double(kRwIters);
  }));
  expect(name, checksum(src.plain_root()), g_rw_sum);
  const auto after = src.save_verified();
  put(name, "bytes_after_rw", double(after.size()));
  put(name, "bytes_after_rw_zstd3", double(zstd3(after.data(), after.size())));
  const auto compact = src.save_compact();
  put(name, "bytes_after_rw_compact", double(compact.size()));
  put(name, "bytes_after_rw_compact_zstd3", double(zstd3(compact.data(), compact.size())));
  auto recovered = TypedXBuffer<C>::load(after, cap);
  expect(name, checksum(recovered.plain_root()), g_rw_sum);
  auto recovered_compact = TypedXBuffer<C>::load(compact, cap);
  expect(name, checksum(recovered_compact.plain_root()), g_rw_sum);
}

// ============================ zpp::bits ============================
// Ch = stdm::Character (the 2024 layout) or stdm::CharacterInline (the tuned layout).
template <class Ch> static void bench_zpp(const char* lib, const Ch& src) {
  std::vector<std::byte> w0; { zpp::bits::out out{w0}; if (zpp::bits::failure(out(src))) throw std::runtime_error("zpp out"); }
  put_bytes(lib, w0.data(), w0.size());
  { Ch d{}; zpp::bits::in in{w0}; (void)in(d); expect(lib, checksum(d), g_sum); }
  std::vector<std::byte> w; Ch held{};
  put(lib, "sr_reuse", time_sr([&](size_t) {
    w.clear(); zpp::bits::out out{w}; (void)out(src); zpp::bits::in in{w}; (void)in(held); return held.id; }));
  put(lib, "sr_fresh", time_sr([&](size_t) {
    std::vector<std::byte> wf; zpp::bits::out out{wf}; (void)out(src); Ch d{}; zpp::bits::in in{wf}; (void)in(d); return d.id; }));
}

// ============================ MessagePack ============================
static void bench_msgpack() {
  stdm::Character src = g_char;
  msgpack::sbuffer w0; msgpack::pack(w0, src);
  put_bytes("MessagePack", w0.data(), w0.size());
  { stdm::Character d{}; msgpack::unpack(w0.data(), w0.size()).get().convert(d); expect("msgpack", checksum(d), g_sum); }
  msgpack::sbuffer w; stdm::Character held{};
  put("MessagePack", "sr_reuse", time_sr([&](size_t) {
    w.clear(); msgpack::pack(w, src); msgpack::object_handle oh = msgpack::unpack(w.data(), w.size()); oh.get().convert(held); return held.id; }));
  put("MessagePack", "sr_fresh", time_sr([&](size_t) {
    msgpack::sbuffer wf; msgpack::pack(wf, src); stdm::Character d{}; msgpack::unpack(wf.data(), wf.size()).get().convert(d); return d.id; }));
}

// ============================ Protocol Buffers (its own mutable message as state, as 2024) ============================
static void fill_pb(protobuf_test::Character& m, const stdm::Character& c) {
  m.set_id(c.id); m.set_name(c.name); m.set_level(c.level); m.set_healthpoint(c.healthpoint); m.set_manapoint(c.manapoint); m.set_speed(c.speed);
  auto* p = m.mutable_pos(); p->set_x(c.pos.x); p->set_y(c.pos.y); p->set_z(c.pos.z);
  for (const auto& [k, it] : c.items) { auto& x = (*m.mutable_items())[k]; x.set_id(it.id); x.set_uuid(it.uuid); x.set_number(it.number); x.set_timestamp(it.timestamp); }
  for (const auto& q : c.path) { auto* x = m.add_path(); x->set_x(q.x); x->set_y(q.y); x->set_z(q.z); }
  for (float a : c.attributes) m.add_attributes(a);
  for (const auto& e : c.equips) { auto* x = m.add_equips(); x->set_id(e.id); x->set_uuid(e.uuid); x->set_number(e.number); x->set_timestamp(e.timestamp); x->set_level(e.level); for (float a : e.attributes) x->add_attributes(a); }
  for (const auto& s : c.skills) { auto* x = m.add_skills(); x->set_id(s.id); x->set_level(s.level); }
}
static uint64_t checksum_pb(const protobuf_test::Character& m) {
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
// Bulk: the 8 equipment attributes are added as one range, the one bulk call that helps
// here. A message has no inline scalar array, and RemoveLast already keeps the cleared
// objects for reuse (DeleteSubrange and erase-by-iterator would free them instead).
template <bool Bulk> static void pb_rw(protobuf_test::Character& r, const OpSet& o) {
  r.set_level(r.level() + 1); r.set_healthpoint(r.healthpoint() + 1); r.set_manapoint(r.manapoint() + 1); r.set_speed(r.speed() + 1);
  auto* p = r.mutable_pos(); p->set_x(o.pos[0]); p->set_y(o.pos[1]); p->set_z(o.pos[2]);
  if (kEquipPart) for (const auto& eo : o.equip) {
    auto* e = r.add_equips(); e->set_id(eo.id); e->set_uuid(eo.uuid); e->set_number(eo.number); e->set_timestamp(eo.timestamp); e->set_level(eo.level);
    if constexpr (Bulk) e->mutable_attributes()->Add(std::begin(eo.attr), std::end(eo.attr));
    else for (float a : eo.attr) e->add_attributes(a);
    observe(*e);
    r.mutable_equips()->RemoveLast();
  }
  if (kPathPart) {
    for (const auto& q : o.path) { auto* x = r.add_path(); x->set_x(q[0]); x->set_y(q[1]); x->set_z(q[2]); }
    r.add_attributes(o.attribute);
  }
  if (kItemPart) { auto& it = (*r.mutable_items())[o.key]; it.set_id(o.item_id); it.set_uuid(o.item_uuid); it.set_number(o.item_number); it.set_timestamp(o.item_timestamp); }
  observe(r);
  if (kItemPart) r.mutable_items()->erase(o.key);
  if (kPathPart) {
    r.mutable_attributes()->RemoveLast();
    for (int i = 0; i < 8; ++i) r.mutable_path()->RemoveLast();
  }
  r.set_speed(r.speed() - 1); r.set_manapoint(r.manapoint() - 1); r.set_healthpoint(r.healthpoint() - 1); r.set_level(r.level() - 1);
}
static void bench_protobuf() {
  protobuf_test::Character src; fill_pb(src, g_char);
  std::string w0;
  if (!src.SerializeToString(&w0)) throw std::runtime_error("pb serialize");
  put_bytes("Protocol Buffers", w0.data(), w0.size());
  { protobuf_test::Character d; if (!d.ParseFromString(w0)) throw std::runtime_error("pb parse"); expect("protobuf", checksum_pb(d), g_sum); }
  std::string w; protobuf_test::Character held;
  put("Protocol Buffers", "sr_reuse", time_sr([&](size_t) {
    if (!src.SerializeToString(&w) || !held.ParseFromString(w))
      throw std::runtime_error("pb reuse round trip");
    return held.id(); }));
  put("Protocol Buffers", "sr_fresh", time_sr([&](size_t) {
    std::string wf;
    protobuf_test::Character d;
    if (!src.SerializeToString(&wf) || !d.ParseFromString(wf))
      throw std::runtime_error("pb fresh round trip");
    return d.id(); }));
  protobuf_test::Character r; fill_pb(r, g_char);
  put("Protocol Buffers", "rw", time_rw([&](const OpSet& o) { pb_rw<false>(r, o); }));
  expect("protobuf rw", checksum_pb(r), g_rw_sum);
  protobuf_test::Character rb; fill_pb(rb, g_char);
  put("Protocol Buffers bulk", "rw", time_rw([&](const OpSet& o) { pb_rw<true>(rb, o); }));
  expect("protobuf bulk rw", checksum_pb(rb), g_rw_sum);
}

// ============================ FlatBuffers (state: std model; recv unpacks into it) ============================
namespace fbt = flatbuffers_test;
static void fb_encode(flatbuffers::FlatBufferBuilder& b, std::vector<flatbuffers::Offset<fbt::Equip>>& eq, const stdm::Character& c) {
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
static void fb_decode(const uint8_t* data, stdm::Character& c) {
  const auto* r = fbt::GetCharacter(data);   // no Verifier: trusted input
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
static void bench_flatbuffers() {
  stdm::Character src = g_char;
  flatbuffers::FlatBufferBuilder b0; std::vector<flatbuffers::Offset<fbt::Equip>> eq0; fb_encode(b0, eq0, src);
  put_bytes("FlatBuffers", b0.GetBufferPointer(), b0.GetSize());
  { stdm::Character d{}; fb_decode(b0.GetBufferPointer(), d); expect("flatbuffers", checksum(d), g_sum); }
  flatbuffers::FlatBufferBuilder b; std::vector<flatbuffers::Offset<fbt::Equip>> eq; stdm::Character held{};
  put("FlatBuffers", "sr_reuse", time_sr([&](size_t) { fb_encode(b, eq, src); fb_decode(b.GetBufferPointer(), held); return held.id; }));
  put("FlatBuffers", "sr_fresh", time_sr([&](size_t) {
    flatbuffers::FlatBufferBuilder bf; std::vector<flatbuffers::Offset<fbt::Equip>> eqf; fb_encode(bf, eqf, src);
    stdm::Character d{}; fb_decode(bf.GetBufferPointer(), d); return d.id; }));
  // report-only: read-only access in the wire (no unpack; the receiver cannot modify it)
  put("FlatBuffers view", "sr_reuse", time_sr([&](size_t) {
    fb_encode(b, eq, src); const auto* r = fbt::GetCharacter(b.GetBufferPointer()); return r->id() + r->items()->size(); }));
}

// ============================ Cap'n Proto (state: std model; recv copies into it) ============================
namespace cpt = capnp_character_test;
static void cp_build(capnp::MessageBuilder& msg, const stdm::Character& c) {
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
static void cp_decode(kj::ArrayPtr<const capnp::word> words, stdm::Character& c) {
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
static void bench_capnp() {
  stdm::Character src = g_char;
  kj::Array<capnp::word> w0; { capnp::MallocMessageBuilder m; cp_build(m, src); w0 = capnp::messageToFlatArray(m); }
  put_bytes("Cap'n Proto", w0.begin(), w0.size() * sizeof(capnp::word));
  { stdm::Character d{}; cp_decode(w0.asPtr(), d); expect("capnp", checksum(d), g_sum); }
  // reuse: Cap'n Proto builders are single-use, so a new MallocMessageBuilder per message
  // (its usual API, messageToFlatArray); only the receiver's object is reused.
  stdm::Character held{};
  put("Cap'n Proto", "sr_reuse", time_sr([&](size_t) {
    capnp::MallocMessageBuilder m; cp_build(m, src); auto wf = capnp::messageToFlatArray(m);
    cp_decode(wf.asPtr(), held); return held.id; }));
  put("Cap'n Proto", "sr_fresh", time_sr([&](size_t) {
    capnp::MallocMessageBuilder m; cp_build(m, src); auto wf = capnp::messageToFlatArray(m);
    stdm::Character d{}; cp_decode(wf.asPtr(), d); return d.id; }));
}

// ============================ cista (raw and offset, as 2024; containers are cista's own) ============================
template <class Ch> static void fill_cista(Ch& r, const stdm::Character& c) {
  r.id = c.id; r.name = c.name; r.level = c.level; r.healthpoint = c.healthpoint; r.manapoint = c.manapoint; r.speed = c.speed;
  r.pos = {c.pos.x, c.pos.y, c.pos.z};
  for (const auto& [k, it] : c.items) { auto& x = r.items[k]; x.id = it.id; x.number = it.number; x.uuid = it.uuid; x.timestamp = it.timestamp; }
  for (const auto& p : c.path) { r.path.emplace_back(); r.path.back() = {p.x, p.y, p.z}; }
  for (float a : c.attributes) r.attributes.push_back(a);
  for (const auto& e : c.equips) {
    auto& x = r.equips.emplace_back(); x.id = e.id; x.number = e.number; x.uuid = e.uuid; x.timestamp = e.timestamp; x.level = e.level;
    for (float a : e.attributes) x.attributes.push_back(a);
  }
  for (const auto& s : c.skills) { r.skills.emplace_back(); r.skills.back() = {s.id, s.level}; }
}
// Tuned = false: the 2024 layout and loop (rw_once). Tuned = true: Ch is the model's
// CharacterInline and the loop is rw_tuned, as for "xoffset inline" and "std inline".
template <class Ch, class ItemT, class PosT, cista::mode Mode, bool Tuned> static void bench_cista(const char* lib) {
  Ch src{};
  if constexpr (Tuned) fill_inline(src, g_char); else fill_cista(src, g_char);
  std::vector<uint8_t> w0 = cista::serialize<Mode>(src);
  put_bytes(lib, w0.data(), w0.size());
  { std::vector<uint8_t> copy = w0; expect(lib, checksum(*cista::deserialize<Ch, Mode>(copy)), g_sum); }
  // The receiver copies the wire into a buffer it owns and deserializes it in place (no
  // check: UNCHECKED for raw, CAST for offset); the object then lives in that buffer and can
  // be modified (containers that grow move to the heap). Same as the 4.0 scorecard's cista runner.
  cista::buf<std::vector<uint8_t>> out; std::vector<uint8_t> held;
  put(lib, "sr_reuse", time_sr([&](size_t) {
    out.reset(); cista::serialize<Mode>(out, src);
    held.assign(out.buf_.begin(), out.buf_.end());
    return cista::deserialize<Ch, Mode>(held)->id; }));
  put(lib, "sr_fresh", time_sr([&](size_t) {
    std::vector<uint8_t> wf = cista::serialize<Mode>(src); std::vector<uint8_t> h(wf.begin(), wf.end());
    return cista::deserialize<Ch, Mode>(h)->id; }));
  Ch r{};
  if constexpr (Tuned) {
    fill_inline(r, g_char);
    put(lib, "rw", time_rw([&](const OpSet& o) {
      rw_tuned<Ch, ItemT, PosT>(r, o,
        [](auto& m, uint64_t k, const ItemT& it) { auto pos = m.emplace(k, it).first; pos->second = it; return pos; },
        [](auto& m, auto pos) { m.erase(pos); }); }));
  } else {
    fill_cista(r, g_char);
    put(lib, "rw", time_rw([&](const OpSet& o) {
      rw_once<Ch, ItemT>(r, o, [](auto& m, uint64_t k, const ItemT& it) { m[k] = it; }, [](auto& m, uint64_t k) { m.erase(k); }); }));
  }
  expect(lib, checksum(r), g_rw_sum);
}

int main(int argc, char** argv) {
  if (argc > 1 && std::string_view(argv[1]) == "--quick") { kRwIters = 10000; kSrIters = 2000; kRounds = 3; }
  g_char = make_character(); g_sum = checksum(g_char); g_ops = make_ops();
  std::printf("# cppcon2024 rerun: rw %zu iters, send+recv %zu msgs, median of %d rounds, ns per iteration/message\n", kRwIters, kSrIters, kRounds);
  bench_std_rw();   // first: its end state is the reference for every other rw
  bench_xoffset();
  bench_xoffset_layout<xo::CharacterInline>("xoffset inline");
  bench_zpp<stdm::Character>("zpp::bits", g_char);
  { stdm::CharacterInline t{}; fill_inline(t, g_char); bench_zpp<stdm::CharacterInline>("zpp::bits inline", t); }
  bench_msgpack();
  bench_protobuf();
  bench_flatbuffers();
  bench_capnp();
  bench_cista<cista_raw::Character, cista_raw::Item, cista_raw::Position, cista::mode::UNCHECKED, false>("cista raw");
  bench_cista<cista_offset::Character, cista_offset::Item, cista_offset::Position, cista::mode::CAST, false>("cista offset");
  bench_cista<cista_raw::CharacterInline, cista_raw::Item, cista_raw::Position, cista::mode::UNCHECKED, true>("cista raw inline");
  bench_cista<cista_offset::CharacterInline, cista_offset::Item, cista_offset::Position, cista::mode::CAST, true>("cista offset inline");
  return 0;
}
