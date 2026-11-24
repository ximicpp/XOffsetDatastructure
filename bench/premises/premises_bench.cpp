// Premises sweep: where XOffset wins, ties or loses send+receive, as a function of
// message size, buffer dirtiness and validation tier.
//
// Reuses the CppCon 2024 bench adapters unchanged (bench/cppcon2024/cppcon2024_bench.cpp is
// included with its main renamed): the same Character schema for XOffset, zpp::bits, cista,
// Protocol Buffers and FlatBuffers; cereal is added here over the same std model.
//
// Axes
//   size  k:    the 2024 Character with every container count scaled by k (k = 1 is the
//               2024 shape: 64 items, 16 path points, 16 attributes, 8 equips x 8, 4 skills).
//   dirt:       the sender's XOffset buffer after (a) grow-shrink churn of amplitude g (each
//               container grows by g x its size and shrinks back: same logical state, spare
//               capacity and freed blocks left behind), (b) n iterations of the 2024
//               read/write loop (logically neutral), (c) 100,000 random edits (the logical
//               state changes; every rival is then measured on that same final state).
//               "+relayout" (XOffset only): the same dirty sender after one relayout()
//               (docs/USER_GUIDE.md 4.7), which lays its content out as save_compact does.
//   tier:       trusted (no verification: XOffset admit, cista UNCHECKED, FlatBuffers without
//               Verifier) and checked (safe on hostile input with a mutable result: XOffset
//               full, cista NONE / DEEP_CHECK, FlatBuffers Verifier + unpack). zpp::bits,
//               protobuf and cereal always check bounds while decoding: the same cell is
//               their trusted and checked one.
// Timed: one message = sender state -> bytes -> receiver state (reuse mode: both ends keep
// their buffers / objects). No compression in the timed loop; bytes are raw and zstd-3.
// Each cell: 3 warm-up messages, calibration to ~10 ms per round, 7 rounds, median.
// Cells run in a different random order in every process (seed = process index).
#define main cc24_bench_main
#include "cppcon2024_bench.cpp"
#undef main

#include <cereal/archives/binary.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/utility.hpp>
#include <cereal/types/vector.hpp>

#include <concepts>
#include <functional>
#include <istream>
#include <map>
#include <regex>
#include <ostream>
#include <streambuf>

// ---- cereal bindings for the std model (constrained so zpp::bits' ADL lookup ignores them) ----
namespace cc24::stdm {
template <class A>
concept CerealArchive = std::derived_from<A, cereal::detail::OutputArchiveBase> || std::derived_from<A, cereal::detail::InputArchiveBase>;
template <CerealArchive A> void serialize(A& ar, Skill& v) { ar(v.id, v.level); }
template <CerealArchive A> void serialize(A& ar, Item& v) { ar(v.id, v.number, v.uuid, v.timestamp); }
template <CerealArchive A> void serialize(A& ar, Position& v) { ar(v.x, v.y, v.z); }
template <CerealArchive A> void serialize(A& ar, Equip& v) { ar(v.id, v.number, v.uuid, v.timestamp, v.level, v.attributes); }
template <CerealArchive A> void serialize(A& ar, Character& v) {
  ar(v.id, v.name, v.level, v.healthpoint, v.manapoint, v.speed, v.pos, v.items, v.path, v.attributes, v.equips, v.skills);
}
}  // namespace cc24::stdm

namespace sweep {
using namespace XOffsetDatastructure;
using C = xo::Character;
using cc24::stdm::Character;

// ---------------------------------------------------------------- data
struct Scale { const char* label; double k; };
// k = 5 and 17 leave every container half empty after its last doubling
// (push_back builds); the powers of two fill them to capacity.
static const Scale kSizes[] = {{"0", 0}, {"1/64", 1.0 / 64}, {"1/16", 1.0 / 16}, {"1/8", 0.125}, {"1/4", 0.25},
                               {"1/2", 0.5}, {"1", 1}, {"2", 2}, {"4", 4}, {"5", 5}, {"16", 16}, {"17", 17},
                               {"64", 64}, {"256", 256}};
static size_t cnt(double base, double k) { return size_t(std::floor(base * k + 0.5)); }

// The 2024 make_character with every count scaled by k (k = 1 gives the same Character).
static Character make_scaled(double k) {
  cc24::Gen r;
  const std::string cs = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
  std::uniform_int_distribution<int> d(0, int(cs.size()) - 1);
  std::string name; for (int i = 0; i < 8; ++i) name += cs[size_t(d(r.g))];
  r.g.seed(0);
  Character c{};
  c.id = r.u64(); c.name = name; c.level = r.u32();
  c.healthpoint = r.fl(); c.manapoint = r.fl(); c.speed = r.fl();
  c.pos.x = r.fl(); c.pos.y = r.fl(); c.pos.z = r.fl();
  for (size_t i = 0; i < cnt(64, k); ++i) {
    uint64_t key = r.u64(); cc24::stdm::Item it{}; it.id = r.u32(); it.uuid = r.u64(); it.number = r.u32(); it.timestamp = r.u64();
    cc24::items_set(c.items, key, it);
  }
  for (size_t i = 0; i < cnt(16, k); ++i) { cc24::stdm::Position p{}; p.x = r.fl(); p.y = r.fl(); p.z = r.fl(); c.path.push_back(p); }
  for (size_t i = 0; i < cnt(16, k); ++i) c.attributes.push_back(r.fl());
  for (size_t i = 0; i < cnt(8, k); ++i) {
    cc24::stdm::Equip e{}; e.id = r.u32(); e.uuid = r.u64(); e.number = r.u32(); e.timestamp = r.u64(); e.level = r.u32();
    for (int a = 0; a < 8; ++a) e.attributes.push_back(r.fl());
    c.equips.push_back(std::move(e));
  }
  for (size_t i = 0; i < cnt(4, k); ++i) { cc24::stdm::Skill s{}; s.id = r.u32(); s.level = r.u32(); c.skills.push_back(s); }
  return c;
}

// XOffset state -> std model (for measuring the rivals on a state XOffset churned).
static Character to_std(const C& x) {
  Character c{};
  c.id = x.id; c.name = std::string(cc24::sv(x.name)); c.level = x.level;
  c.healthpoint = x.healthpoint; c.manapoint = x.manapoint; c.speed = x.speed;
  c.pos = {x.pos.x, x.pos.y, x.pos.z};
  for (const auto& kv : x.items) c.items.push_back({kv.first, cc24::stdm::Item{kv.second.id, kv.second.number, kv.second.uuid, kv.second.timestamp}});
  std::sort(c.items.begin(), c.items.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
  for (const auto& p : x.path) c.path.push_back({p.x, p.y, p.z});
  for (float a : x.attributes) c.attributes.push_back(a);
  for (const auto& e : x.equips) {
    cc24::stdm::Equip d{e.id, e.number, e.uuid, e.timestamp, e.level, {}};
    for (float a : e.attributes) d.attributes.push_back(a);
    c.equips.push_back(std::move(d));
  }
  for (const auto& s : x.skills) c.skills.push_back({s.id, s.level});
  return c;
}

static const auto kCap = XBuffer::maximum_capacity(size_t(1) << 27);
// Senders grow automatically (states up to ~1 MB and churned); the 2024 bench used a
// fixed 16 KiB buffer, enough for k = 1 only.
static const auto kCapAuto = kCap.with_growth(XGrowPolicy::Automatic);
static TypedXBuffer<C> make_xo(const Character& c) {
  auto b = TypedXBuffer<C>::create(16 * 1024, kCapAuto);
  b.edit([&](C& r) { xo::fill(r, c); });
  if (!b.ok()) throw std::runtime_error("xoffset fill failed: " + std::to_string(int(b.error())));
  return b;
}

// ---------------------------------------------------------------- dirt
struct Dirt { std::string label; char kind; double g; size_t n; };  // kind: 'c' clean, 'g' grow-shrink, 'r' rw loop, 'x' random churn

// Grow-shrink: every container grows by g x its size (at least 1 element when g > 0) and
// shrinks back, one edit() scope per container; the name goes to 64 characters and back.
static void dirty_grow(TypedXBuffer<C>& b, double g) {
  if (g <= 0) return;
  auto extra = [&](size_t n) { return std::max<size_t>(1, size_t(std::ceil(double(n) * g))); };
  std::mt19937_64 rng(777);
  b.edit([&](C& r) {
    const auto n0 = r.path.size(); const auto add = extra(n0);
    for (size_t i = 0; i < add; ++i) r.path.push_back(xo::Position{1.f, 2.f, 3.f});
    r.path.resize(n0);
  });
  b.edit([&](C& r) {
    const auto n0 = r.attributes.size(); const auto add = extra(n0);
    for (size_t i = 0; i < add; ++i) r.attributes.push_back(4.f);
    r.attributes.resize(n0);
  });
  b.edit([&](C& r) {
    const auto n0 = r.equips.size(); const auto add = extra(n0);
    for (size_t i = 0; i < add; ++i) {
      auto& e = r.equips.emplace_back(); e.id = 1; e.level = 2;
      for (int a = 0; a < 8; ++a) e.attributes.push_back(float(a));
    }
    while (r.equips.size() > n0) r.equips.pop_back();
  });
  b.edit([&](C& r) {
    const auto n0 = r.skills.size(); const auto add = extra(n0);
    for (size_t i = 0; i < add; ++i) r.skills.push_back(xo::Skill{1, 2});
    r.skills.resize(n0);
  });
  b.edit([&](C& r) {
    const auto add = extra(r.items.size());
    std::vector<uint64_t> keys;
    while (keys.size() < add) { uint64_t key = rng(); if (r.items.find(key) == r.items.end()) { keys.push_back(key); r.items[key] = xo::Item{1, 2, 3, 4}; } }
    for (uint64_t key : keys) r.items.erase(key);
  });
  b.edit([&](C& r) {
    const std::string old(cc24::sv(r.name));
    r.name = std::string(64, 'n').c_str();
    r.name = old.c_str();
  });
  if (!b.ok()) throw std::runtime_error("grow dirt failed");
}

static void dirty_rw(TypedXBuffer<C>& b, size_t n) {
  for (size_t i = 0; i < n; ++i)
    b.edit([&](C& r) {
      cc24::rw_once<C, xo::Item>(r, op_at(i % g_keys.size()), [](auto& m, uint64_t key, const xo::Item& it) { m[key] = it; },
                                 [](auto& m, uint64_t key) { m.erase(key); });
    });
  if (!b.ok()) throw std::runtime_error("rw dirt failed");
}

// Random churn: 100,000 edits, one edit() each; container sizes stay between half and
// one and a half times their scaled size (at least 2), equipment gets 4..12 attributes,
// the name 4..20 characters. Deterministic (seed 11).
static void dirty_churn(TypedXBuffer<C>& b, double k, size_t steps) {
  std::mt19937_64 rng(11);
  auto lo = [](size_t base) { return base / 2; };
  auto hi = [](size_t base) { return std::max<size_t>(2, base + base / 2); };
  const size_t ni = cnt(64, k), np = cnt(16, k), na = cnt(16, k), ne = cnt(8, k);
  auto rnd = [&](size_t n) { return size_t(rng() % std::max<size_t>(1, n)); };
  for (size_t s = 0; s < steps; ++s) {
    const unsigned op = unsigned(rng() % 8);
    b.edit([&](C& r) {
      r.level += 1;
      switch (op) {
        case 0: case 1: {  // insert or erase an item
          const bool insert = (op == 0 && r.items.size() < hi(ni)) || r.items.size() <= lo(ni);
          if (insert) { uint64_t key = rng(); r.items[key] = xo::Item{uint32_t(key), 7, key ^ 1, key ^ 2}; }
          else if (r.items.size()) r.items.erase_at(r.items.begin() + std::ptrdiff_t(rnd(r.items.size())));
          break;
        }
        case 2: case 3: {  // path grows by 1..4 or shrinks by 1..4
          const size_t d = 1 + rnd(4);
          const bool grow = (op == 2 && r.path.size() + d <= hi(np)) || r.path.size() < lo(np) + d;
          if (grow) for (size_t i = 0; i < d; ++i) r.path.push_back(xo::Position{float(i), 1.f, 2.f});
          else r.path.resize(r.path.size() - d);
          break;
        }
        case 4: {  // one equip's attributes replaced by 4..12 new ones
          if (!r.equips.size()) break;
          auto& e = r.equips[rnd(r.equips.size())];
          float tmp[12]; const size_t n = 4 + rnd(9);
          for (size_t i = 0; i < n; ++i) tmp[i] = float(rng() % 1000);
          e.attributes.assign_range(std::span<const float>(tmp, n));
          break;
        }
        case 5: {  // add or remove an equip
          if ((r.equips.size() < hi(ne) && (rng() & 1)) || r.equips.size() <= lo(ne)) {
            auto& e = r.equips.emplace_back(); e.id = uint32_t(rng()); e.level = 3;
            const size_t n = 4 + rnd(9);
            for (size_t i = 0; i < n; ++i) e.attributes.push_back(float(i));
          } else if (r.equips.size()) {
            r.equips.erase(r.equips.begin() + std::ptrdiff_t(rnd(r.equips.size())));
          }
          break;
        }
        case 6: {  // attributes push or pop
          if ((r.attributes.size() < hi(na) && (rng() & 1)) || r.attributes.size() <= lo(na)) r.attributes.push_back(float(rng() % 100));
          else r.attributes.pop_back();
          break;
        }
        case 7: {  // rename
          const size_t n = 4 + rnd(17);
          std::string nm(n, 'a'); for (auto& ch : nm) ch = char('a' + rnd(26));
          r.name = nm.c_str();
          break;
        }
      }
    });
  }
  if (!b.ok()) throw std::runtime_error("churn dirt failed");
}

// The sender sheds what the dirt left behind: one relayout() (same logical state).
static void relayout_once(TypedXBuffer<C>& b) {
  const XStatus r = b.relayout();
  if (!r) throw std::runtime_error("relayout failed: " + std::string(r.error().message));
}

// ---------------------------------------------------------------- cells
struct Prepared {
  std::function<uint64_t()> op;
  std::function<uint64_t()> check;  // checksum of the receiver's state after one op (untimed)
  uint64_t want = 0;
  std::map<std::string, double> info;  // bytes etc., printed once per cell
  std::shared_ptr<void> keep;
};
struct Cell { std::string id, group, fmt, size, dirt; std::function<Prepared()> prep; };
static std::vector<Cell> g_cells;

static double zbytes(const void* p, size_t n) { return double(zstd3(p, n)); }

// XOffset variants on a given sender buffer.
enum class XoV { admit, full, view_admit, view_bounds, admit_zero_runs, compact_admit, compact0_admit };
static const char* xo_name(XoV v) {
  switch (v) {
    case XoV::admit: return "xo_admit"; case XoV::full: return "xo_full";
    case XoV::view_admit: return "xo_view_admit"; case XoV::view_bounds: return "xo_view_bounds";
    case XoV::admit_zero_runs: return "xo_admit_zero_runs"; case XoV::compact_admit: return "xo_compact_admit";
    case XoV::compact0_admit: return "xo_compact0_admit";
  }
  return "?";
}
struct XoState {
  TypedXBuffer<C> src, held; std::string w;
  XReceiver<XLink::server_peer> rx{kCap}; XReceiver<XLink::from_client> rxc{kCap};
  XoState(TypedXBuffer<C>&& s, TypedXBuffer<C>&& h) : src(std::move(s)), held(std::move(h)) {}
};
static Prepared prep_xo(TypedXBuffer<C> src, XoV v) {
  const std::string w0 = src.save_verified();
  auto st = std::make_shared<XoState>(std::move(src), TypedXBuffer<C>::load(w0, kCap));
  XoState& s = *st;
  Prepared p; p.keep = st;
  p.want = cc24::checksum(s.src.plain_root());
  {
    const auto zr = s.src.save_verified(XSaveOptions{XWireEmission::zero_runs});
    const auto c8 = s.src.save_compact(), c0 = s.src.save_compact(XCompactOptions{0});
    p.info["bytes"] = double(w0.size()); p.info["bytes_z"] = zbytes(w0.data(), w0.size());
    p.info["bytes_zero_runs"] = double(zr.size());
    p.info["bytes_compact"] = double(c8.size()); p.info["bytes_compact_z"] = zbytes(c8.data(), c8.size());
    p.info["bytes_compact0"] = double(c0.size());
  }
  auto id_of_held = [&s] { return uint64_t(s.held.plain_root().id); };
  switch (v) {
    case XoV::admit:
      p.op = [&s, id_of_held] { s.src.save_verified_into(s.w); if (!s.rx.reload(s.held, s.w)) throw std::runtime_error("admit"); return id_of_held(); };
      break;
    case XoV::full:
      p.op = [&s, id_of_held] { s.src.save_verified_into(s.w); s.held.reload_verified(s.w); if (!s.held.ok()) throw std::runtime_error("full"); return id_of_held(); };
      break;
    case XoV::admit_zero_runs:
      p.op = [&s, id_of_held] { s.src.save_verified_into(s.w, XSaveOptions{XWireEmission::zero_runs}); if (!s.rx.reload(s.held, s.w)) throw std::runtime_error("zr"); return id_of_held(); };
      break;
    case XoV::compact_admit:
      p.op = [&s, id_of_held] { s.src.save_compact_into(s.w); if (!s.rx.reload(s.held, s.w)) throw std::runtime_error("compact"); return id_of_held(); };
      break;
    case XoV::compact0_admit:
      p.op = [&s, id_of_held] { s.src.save_compact_into(s.w, XCompactOptions{0}); if (!s.rx.reload(s.held, s.w)) throw std::runtime_error("compact0"); return id_of_held(); };
      break;
    case XoV::view_admit:
      // a view needs a dense wire (no extent table): XWireEmission::dense, as documented
      p.op = [&s] { s.src.save_verified_into(s.w, XSaveOptions{XWireEmission::dense}); auto r = s.rx.view<C>(s.w); if (!r) throw std::runtime_error("view admit"); return uint64_t(r->root().id + r->root().items.size()); };
      p.check = [&s] { auto r = s.rx.view<C>(s.w); return cc24::checksum(r->root()); };
      return p;
    case XoV::view_bounds:
      p.op = [&s] { s.src.save_verified_into(s.w, XSaveOptions{XWireEmission::dense}); auto r = s.rxc.view<C>(s.w); if (!r) throw std::runtime_error("view bounds"); return uint64_t(r->root().id + r->root().items.size()); };
      p.check = [&s] { auto r = s.rxc.view<C>(s.w); return cc24::checksum(r->root()); };
      return p;
  }
  p.check = [&s] { return cc24::checksum(s.held.plain_root()); };
  return p;
}

struct ZppState { Character src, held; std::vector<std::byte> w; };
static Prepared prep_zpp(const Character& c) {
  auto st = std::make_shared<ZppState>(); st->src = c; auto& s = *st;
  Prepared p; p.keep = st; p.want = cc24::checksum(c);
  { std::vector<std::byte> w; zpp::bits::out out{w}; if (zpp::bits::failure(out(c))) throw std::runtime_error("zpp out");
    p.info["bytes"] = double(w.size()); p.info["bytes_z"] = zbytes(w.data(), w.size()); }
  p.op = [&s] { s.w.clear(); zpp::bits::out out{s.w}; (void)out(s.src); zpp::bits::in in{s.w}; (void)in(s.held); return uint64_t(s.held.id); };
  p.check = [&s] { return cc24::checksum(s.held); };
  return p;
}

template <class Ch, cista::mode Ser, cista::mode De>
struct CistaState { Ch src{}; cista::buf<std::vector<uint8_t>> out; std::vector<uint8_t> held; Ch* root = nullptr; };
template <class Ch, cista::mode Ser, cista::mode De>
static Prepared prep_cista(const Character& c) {
  auto st = std::make_shared<CistaState<Ch, Ser, De>>(); auto& s = *st;
  fill_cista(s.src, c);
  Prepared p; p.keep = st; p.want = cc24::checksum(c);
  { auto w = cista::serialize<Ser>(s.src); p.info["bytes"] = double(w.size()); p.info["bytes_z"] = zbytes(w.data(), w.size()); }
  p.op = [&s] { s.out.reset(); cista::serialize<Ser>(s.out, s.src); s.held.assign(s.out.buf_.begin(), s.out.buf_.end());
                s.root = cista::deserialize<Ch, De>(s.held); return uint64_t(s.root->id); };
  // raw mode turns offsets into pointers in place: never deserialize the same bytes twice
  p.check = [&s] { return cc24::checksum(*s.root); };
  return p;
}

struct PbState { protobuf_test::Character src, held; std::string w; };
static Prepared prep_pb(const Character& c) {
  auto st = std::make_shared<PbState>(); auto& s = *st; fill_pb(s.src, c);
  Prepared p; p.keep = st; p.want = cc24::checksum(c);
  { std::string w; s.src.SerializeToString(&w); p.info["bytes"] = double(w.size()); p.info["bytes_z"] = zbytes(w.data(), w.size()); }
  p.op = [&s] { if (!s.src.SerializeToString(&s.w) || !s.held.ParseFromString(s.w)) throw std::runtime_error("pb"); return uint64_t(s.held.id()); };
  p.check = [&s] { return checksum_pb(s.held); };
  return p;
}

enum class FbV { noverify, verify, view_noverify, view_verify };
struct FbState { Character src, held; flatbuffers::FlatBufferBuilder b; std::vector<flatbuffers::Offset<fbt::Equip>> eq; };
static Prepared prep_fb(const Character& c, FbV v) {
  auto st = std::make_shared<FbState>(); auto& s = *st; s.src = c;
  Prepared p; p.keep = st; p.want = cc24::checksum(c);
  fb_encode(s.b, s.eq, s.src);
  p.info["bytes"] = double(s.b.GetSize()); p.info["bytes_z"] = zbytes(s.b.GetBufferPointer(), s.b.GetSize());
  auto verify = [&s] { flatbuffers::Verifier vf(s.b.GetBufferPointer(), s.b.GetSize()); if (!fbt::VerifyCharacterBuffer(vf)) throw std::runtime_error("fb verify"); };
  switch (v) {
    case FbV::noverify: p.op = [&s] { fb_encode(s.b, s.eq, s.src); fb_decode(s.b.GetBufferPointer(), s.held); return uint64_t(s.held.id); }; break;
    case FbV::verify: p.op = [&s, verify] { fb_encode(s.b, s.eq, s.src); verify(); fb_decode(s.b.GetBufferPointer(), s.held); return uint64_t(s.held.id); }; break;
    case FbV::view_noverify: p.op = [&s] { fb_encode(s.b, s.eq, s.src); const auto* r = fbt::GetCharacter(s.b.GetBufferPointer()); return uint64_t(r->id() + r->items()->size()); }; break;
    case FbV::view_verify: p.op = [&s, verify] { fb_encode(s.b, s.eq, s.src); verify(); const auto* r = fbt::GetCharacter(s.b.GetBufferPointer()); return uint64_t(r->id() + r->items()->size()); }; break;
  }
  if (v == FbV::noverify || v == FbV::verify) p.check = [&s] { return cc24::checksum(s.held); };
  else p.check = [&s] { Character d{}; fb_decode(s.b.GetBufferPointer(), d); return cc24::checksum(d); };
  return p;
}

struct StringSink : std::streambuf {
  explicit StringSink(std::string& s) : out(s) {}
  int_type overflow(int_type ch) override { if (ch != traits_type::eof()) out.push_back(char(ch)); return ch; }
  std::streamsize xsputn(const char* p, std::streamsize n) override { out.append(p, size_t(n)); return n; }
  std::string& out;
};
struct MemSource : std::streambuf {
  explicit MemSource(const std::string& s) { char* b = const_cast<char*>(s.data()); setg(b, b, b + s.size()); }
};
struct CerealState { Character src, held; std::string w; };
static Prepared prep_cereal(const Character& c) {
  auto st = std::make_shared<CerealState>(); auto& s = *st; s.src = c;
  Prepared p; p.keep = st; p.want = cc24::checksum(c);
  p.op = [&s] {
    s.w.clear();
    { StringSink sink(s.w); std::ostream os(&sink); cereal::BinaryOutputArchive ar(os); ar(s.src); }
    { MemSource src(s.w); std::istream is(&src); cereal::BinaryInputArchive ar(is); ar(s.held); }
    return uint64_t(s.held.id);
  };
  (void)p.op();
  p.info["bytes"] = double(s.w.size()); p.info["bytes_z"] = zbytes(s.w.data(), s.w.size());
  p.check = [&s] { return cc24::checksum(s.held); };
  return p;
}

// The rival cells for one std state; `sz`, `dirt` label the cell.
static void add_rivals(const std::string& group, const std::string& sz, const std::string& dirt, std::function<Character()> state) {
  auto add = [&](const char* fmt, std::function<Prepared(const Character&)> f) {
    g_cells.push_back({group + "/" + fmt + "/" + sz + "/" + dirt, group, fmt, sz, dirt, [state, f] { return f(state()); }});
  };
  add("zpp_bits", prep_zpp);
  add("cista_raw_unchecked", prep_cista<cista_raw::Character, cista::mode::UNCHECKED, cista::mode::UNCHECKED>);
  add("cista_raw_checked", prep_cista<cista_raw::Character, cista::mode::NONE, cista::mode::NONE>);
  add("cista_raw_deep", prep_cista<cista_raw::Character, cista::mode::NONE, cista::mode::DEEP_CHECK>);
  add("cista_offset_cast", prep_cista<cista_offset::Character, cista::mode::CAST, cista::mode::CAST>);
  add("protobuf", prep_pb);
  add("flatbuffers_noverify", [](const Character& c) { return prep_fb(c, FbV::noverify); });
  add("flatbuffers_verify", [](const Character& c) { return prep_fb(c, FbV::verify); });
  add("flatbuffers_view_noverify", [](const Character& c) { return prep_fb(c, FbV::view_noverify); });
  add("flatbuffers_view_verify", [](const Character& c) { return prep_fb(c, FbV::view_verify); });
  add("cereal", prep_cereal);
}
static void add_xo(const std::string& group, const std::string& sz, const std::string& dirt,
                   std::function<TypedXBuffer<C>()> make, std::initializer_list<XoV> vs) {
  for (XoV v : vs)
    g_cells.push_back({group + "/" + xo_name(v) + "/" + sz + "/" + dirt, group, xo_name(v), sz, dirt, [make, v] { return prep_xo(make(), v); }});
}

static void build_cells() {
  // 1. size sweep, clean sender (as filled): every format, both tiers.
  for (const auto& s : kSizes) {
    const double k = s.k;
    add_xo("size", s.label, "clean", [k] { return make_xo(make_scaled(k)); },
           {XoV::admit, XoV::full, XoV::view_admit, XoV::view_bounds, XoV::compact_admit});
    add_rivals("size", s.label, "clean", [k] { return make_scaled(k); });
  }
  // 2. dirt sweep: XOffset only (the rivals' wire depends on the logical state, which is
  // unchanged); compared with the rivals of the size sweep at the same k.
  const std::vector<Dirt> dirts = {
      {"g0.25", 'g', 0.25, 0}, {"g0.5", 'g', 0.5, 0}, {"g1", 'g', 1, 0}, {"g2", 'g', 2, 0}, {"g4", 'g', 4, 0}, {"g8", 'g', 8, 0},
      {"rw1", 'r', 0, 1}, {"rw100000", 'r', 0, 100000}};
  for (const char* sz : {"1/4", "1", "2", "4", "16"}) {
    double k = 0; for (const auto& s : kSizes) if (std::string(s.label) == sz) k = s.k;
    for (const auto& d : dirts) {
      add_xo("dirt", sz, d.label, [k, d] {
        auto b = make_xo(make_scaled(k));
        if (d.kind == 'g') dirty_grow(b, d.g); else dirty_rw(b, d.n);
        return b;
      }, {XoV::admit, XoV::full, XoV::admit_zero_runs, XoV::compact_admit, XoV::compact0_admit});
    }
    add_xo("dirt", sz, "g8+relayout", [k] { auto b = make_xo(make_scaled(k)); dirty_grow(b, 8); relayout_once(b); return b; },
           {XoV::admit, XoV::full, XoV::admit_zero_runs, XoV::compact_admit, XoV::compact0_admit});
    // 3. random churn: the logical state changes, so every rival runs on the same final state.
    add_xo("churn", sz, "churn100000", [k] { auto b = make_xo(make_scaled(k)); dirty_churn(b, k, 100000); return b; },
           {XoV::admit, XoV::full, XoV::admit_zero_runs, XoV::compact_admit, XoV::compact0_admit});
    add_rivals("churn", sz, "churn100000", [k] { auto b = make_xo(make_scaled(k)); dirty_churn(b, k, 100000); return to_std(b.plain_root()); });
    add_xo("churn", sz, "churn100000+relayout", [k] { auto b = make_xo(make_scaled(k)); dirty_churn(b, k, 100000); relayout_once(b); return b; },
           {XoV::admit, XoV::full, XoV::admit_zero_runs, XoV::compact_admit, XoV::compact0_admit});
  }
}

// ---------------------------------------------------------------- timing
static volatile uint64_t g_out;
struct Timing { double median; std::vector<double> rounds; size_t iters; };
static Timing time_op(const std::function<uint64_t()>& op, int rounds, double round_ms) {
  uint64_t acc = 0;
  for (int i = 0; i < 3; ++i) acc += op();
  size_t n = 1; double per = 0;
  for (;;) {
    auto t0 = cc24::Clock::now();
    for (size_t i = 0; i < n; ++i) acc += op();
    const double ms = std::chrono::duration<double, std::milli>(cc24::Clock::now() - t0).count();
    if (ms >= 2.0 || n >= (size_t(1) << 26)) { per = ms / double(n); break; }
    n *= 2;
  }
  const size_t iters = std::max<size_t>(5, size_t(round_ms / per));
  Timing t; t.iters = iters;
  for (int r = 0; r < rounds; ++r) {
    auto t0 = cc24::Clock::now();
    for (size_t i = 0; i < iters; ++i) acc += op();
    t.rounds.push_back(std::chrono::duration<double, std::nano>(cc24::Clock::now() - t0).count() / double(iters));
  }
  g_out = g_out + acc;
  auto v = t.rounds; std::sort(v.begin(), v.end()); t.median = v[v.size() / 2];
  return t;
}

static std::string jnum(double v) { char b[64]; std::snprintf(b, sizeof b, "%.6g", v); return b; }
}  // namespace sweep

int main(int argc, char** argv) {
  using namespace sweep;
  int proc = 0; int rounds = 7; double round_ms = 10; std::string only, filter; size_t iters = 0; bool list = false;
  for (int i = 1; i < argc; ++i) {
    std::string a = argv[i];
    if (a == "--proc") proc = std::atoi(argv[++i]);
    else if (a == "--rounds") rounds = std::atoi(argv[++i]);
    else if (a == "--round-ms") round_ms = std::atof(argv[++i]);
    else if (a == "--only") only = argv[++i];            // exact cell id (Ir mode with --iters)
    else if (a == "--filter") filter = argv[++i];        // regex searched in the cell id
    else if (a == "--iters") iters = size_t(std::atoll(argv[++i]));
    else if (a == "--list") list = true;
  }
  g_ops = make_ops(); g_keys = make_keys(100000);
  build_cells();
  if (list) { for (auto& c : g_cells) std::printf("%s\n", c.id.c_str()); return 0; }
  if (!only.empty()) {  // Ir mode: setup, then exactly `iters` messages, no timing
    for (auto& c : g_cells) if (c.id == only) {
      auto p = c.prep(); uint64_t acc = 0;
      for (size_t i = 0; i < iters; ++i) acc += p.op();
      std::printf("%s %zu %llu\n", c.id.c_str(), iters, (unsigned long long)acc);
      return 0;
    }
    std::fprintf(stderr, "no cell %s\n", only.c_str()); return 2;
  }
  const std::regex filter_re(filter.empty() ? std::string(".") : filter);
  std::vector<size_t> order(g_cells.size());
  for (size_t i = 0; i < order.size(); ++i) order[i] = i;
  std::mt19937_64 rng(uint64_t(proc) * 7919 + 1);
  std::shuffle(order.begin(), order.end(), rng);
  for (size_t idx : order) {
    auto& c = g_cells[idx];
    if (!std::regex_search(c.id, filter_re)) continue;
    std::fprintf(stderr, "run %s\n", c.id.c_str());
    Prepared p = c.prep();
    (void)p.op();
    const uint64_t got = p.check();
    if (got != p.want) { std::fprintf(stderr, "%s: received state differs from the sent state\n", c.id.c_str()); return 3; }
    const Timing t = time_op(p.op, rounds, round_ms);
    std::string js = "{\"proc\":" + std::to_string(proc) + ",\"cell\":\"" + c.id + "\",\"group\":\"" + c.group + "\",\"fmt\":\"" + c.fmt +
                     "\",\"size\":\"" + c.size + "\",\"dirt\":\"" + c.dirt + "\",\"ns\":" + jnum(t.median) + ",\"iters\":" + std::to_string(t.iters) + ",\"rounds\":[";
    for (size_t i = 0; i < t.rounds.size(); ++i) js += (i ? "," : "") + jnum(t.rounds[i]);
    js += "]";
    for (auto& [key, v] : p.info) js += ",\"" + key + "\":" + jnum(v);
    js += "}";
    std::printf("%s\n", js.c_str()); std::fflush(stdout);
  }
  return 0;
}
