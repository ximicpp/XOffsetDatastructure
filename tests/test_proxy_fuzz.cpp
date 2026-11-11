// Growth policy part 4: no-crash fuzz of proxies (XRef) and pin scopes.
//
// Random interleavings of proxy operations, batch operations, edit / read /
// batch scopes (nested too), growth past the first reservation (buffers start
// at 4 KiB, so almost every few insertions move them, and some batches cross
// 1 MiB), XBuffer moves and swaps, reset and reload, destruction with proxies
// still held, and injected mapping / commit / registration failures. Every
// operation runs against a std model of the same data:
//   - nothing may crash (run it under ASan + UBSan, and in stress mode);
//   - an operation that reports failure has no effect, one that reports
//     success has its effect: the buffer always equals its model;
//   - proxies of a destroyed or replaced buffer read value-initialized
//     results, refuse writes and report `stale`;
//   - a buffer with a recorded failure refuses to emit, and works again
//     after clear_error().
//
//   test_proxy_fuzz [seeds] [steps] [first-seed]     (default 6 seeds x 2500 steps)
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <map>
#include <optional>
#include <random>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include "xoffsetdatastructure.hpp"
using namespace XOffsetDatastructure;

struct Player {
    std::int32_t id{0};
    std::int32_t level{0};
    XString name;
    XVector<std::int32_t> items;
};
XOFFSET_RECORD(Player, id, level, name, items)
struct Root {
    XVector<std::int32_t> ints;
    XVector<Player> players;
    XMap<std::int32_t, std::int32_t> map;
    XSet<std::int32_t> set;
    XString title;
    std::int64_t counter{0};
};
XOFFSET_RECORD(Root, ints, players, map, set, title, counter)
XOFFSET_REGISTER_SCHEMA_NAME(Root, "test.proxy_fuzz.Root")

namespace {

struct PlayerM {
    std::int32_t id = 0;
    std::int32_t level = 0;
    std::string name;
    std::vector<std::int32_t> items;
};
struct Model {
    std::vector<std::int32_t> ints;
    std::vector<PlayerM> players;
    std::map<std::int32_t, std::int32_t> map;
    std::set<std::int32_t> set;
    std::string title;
    std::int64_t counter = 0;
};

using RootRef = XRef<Root>;
using IntsRef = decltype(std::declval<RootRef&>().field<&Root::ints>());
using PlayersRef = decltype(std::declval<RootRef&>().field<&Root::players>());
using PlayerRef = typename PlayersRef::element_ref;
using MapRef = decltype(std::declval<RootRef&>().field<&Root::map>());
using MapValRef = typename MapRef::mapped_ref;
using SetRef = decltype(std::declval<RootRef&>().field<&Root::set>());
using TitleRef = decltype(std::declval<RootRef&>().field<&Root::title>());
using CounterRef = decltype(std::declval<RootRef&>().field<&Root::counter>());

int g_failures = 0;
std::uint64_t g_seed = 0;
int g_step = 0;
const char* g_op = "";
#define REQUIRE(c)                                                                                  \
    do {                                                                                            \
        if (!(c)) {                                                                                 \
            std::fprintf(stderr, "FAIL seed %llu step %d op %s: %s:%d: %s\n",                      \
                         (unsigned long long)g_seed, g_step, g_op, __FILE__, __LINE__, #c);         \
            ++g_failures;                                                                           \
            return;                                                                                 \
        }                                                                                           \
    } while (0)

// ---- comparisons -------------------------------------------------------------
bool same_player(const Player& p, const PlayerM& m) {
    if (p.id != m.id || p.level != m.level || p.name.view() != m.name || p.items.size() != m.items.size())
        return false;
    for (std::size_t i = 0; i < m.items.size(); ++i)
        if (p.items[std::uint32_t(i)] != m.items[i]) return false;
    return true;
}
// Full compare (`deep`) or sizes and a sample.
const char* differs(const Root& r, const Model& m, bool deep) {
    if (r.ints.size() != m.ints.size()) return "ints.size";
    if (r.players.size() != m.players.size()) return "players.size";
    if (r.map.size() != m.map.size()) return "map.size";
    if (r.set.size() != m.set.size()) return "set.size";
    if (r.title.view() != m.title) return "title";
    if (r.counter != m.counter) return "counter";
    const std::size_t n = m.ints.size();
    const std::size_t step = deep ? 1 : (n / 64 + 1);
    for (std::size_t i = 0; i < n; i += step)
        if (r.ints[std::uint32_t(i)] != m.ints[i]) return "ints";
    if (n && r.ints[std::uint32_t(n - 1)] != m.ints[n - 1]) return "ints.back";
    for (std::size_t i = 0; i < m.players.size(); ++i)
        if (!same_player(r.players[std::uint32_t(i)], m.players[i])) return "players";
    auto it = r.map.begin();
    for (const auto& [k, v] : m.map) {
        if (it->first != k || it->second != v) return "map";
        ++it;
    }
    auto st = r.set.begin();
    for (const auto k : m.set) {
        if (*st != k) return "set";
        ++st;
    }
    return nullptr;
}

std::string make_name(std::mt19937_64& rng) {
    // Short (inline) and long (heap block) names.
    const int len = (rng() % 4 == 0) ? int(rng() % 300) : int(rng() % 20);
    std::string s(std::size_t(len), 'a');
    for (auto& c : s) c = char('a' + rng() % 26);
    return s;
}

// ---- the harness --------------------------------------------------------------
struct Slot {
    std::optional<XBuffer> buf;
    std::uint64_t lin = 0;          // the model it holds (0: none)
};
struct HeldInts { std::uint64_t lin; IntsRef ref; };
struct HeldPlayer { std::uint64_t lin; std::uint32_t index; PlayerRef ref; };
struct HeldMapVal { std::uint64_t lin; std::int32_t key; MapValRef ref; };
struct HeldTitle { std::uint64_t lin; TitleRef ref; };

class Fuzz {
public:
    explicit Fuzz(std::uint64_t seed) : rng_(seed) {}

    void run(int steps) {
        for (int i = 0; i < 2; ++i) create(std::size_t(i));
        for (g_step = 0; g_step < steps && g_failures == 0; ++g_step) {
            step();
            detail::clear_vm_failures();
            detail::clear_registry_failures();
        }
        g_op = "final";
        for (std::size_t i = 0; i < slots_.size() && g_failures == 0; ++i) verify(i, true);
    }

private:
    std::mt19937_64 rng_;
    std::vector<Slot> slots_ = std::vector<Slot>(4);
    std::map<std::uint64_t, Model> models_;
    std::uint64_t next_lin_ = 1;
    std::vector<HeldInts> ints_;
    std::vector<HeldPlayer> players_;
    std::vector<HeldMapVal> mapvals_;
    std::vector<HeldTitle> titles_;
    std::vector<std::int32_t> scratch_;

    std::uint32_t pick(std::uint32_t n) { return n ? std::uint32_t(rng_() % n) : 0u; }
    bool chance(int percent) { return int(rng_() % 100) < percent; }
    std::int32_t value() { return std::int32_t(rng_() % 2000000) - 1000000; }

    Model* model_of(std::uint64_t lin) {
        auto it = models_.find(lin);
        return it == models_.end() ? nullptr : &it->second;
    }
    Slot* live_slot() {
        std::vector<std::size_t> live;
        for (std::size_t i = 0; i < slots_.size(); ++i)
            if (slots_[i].buf) live.push_back(i);
        if (live.empty()) return nullptr;
        return &slots_[live[pick(std::uint32_t(live.size()))]];
    }
    void create(std::size_t i) {
        auto& s = slots_[i];
        auto cap = XBuffer::relocating().with_reserve(chance(80) ? 4096 : 0);
        s.buf.emplace(XBuffer::create<Root>(64, cap));
        s.lin = next_lin_++;
        models_[s.lin] = Model{};
    }
    void retire_lineage(std::uint64_t lin) { models_.erase(lin); }

    // Arms one injected failure of the storage layer for the next operation.
    void maybe_inject() {
        if (!chance(6)) return;
        switch (pick(4)) {
            case 0: detail::fail_next_vm_operation(detail::VmOperation::Reserve); break;
            case 1: detail::fail_next_vm_operation(detail::VmOperation::Enable); break;
            case 2: detail::fail_next_vm_operation(detail::VmOperation::Prepare); break;
            default: detail::fail_next_registry_registration(); break;
        }
    }

    void verify(std::size_t i, bool deep) {
        auto& s = slots_[i];
        if (!s.buf) return;
        const Model* m = model_of(s.lin);
        REQUIRE(m != nullptr);
        // (A read scope may not return a pointer: the reason comes back as a string.)
        const std::string why = s.buf->read<Root>([&](const Root& r) {
            const char* d = differs(r, *m, deep);
            return std::string(d ? d : "");
        });
        if (!why.empty()) std::fprintf(stderr, "  mismatch in %s\n", why.c_str());
        REQUIRE(why.empty());
    }

    // A proxy of the root of a live slot (fresh), with the slot's model.
    std::pair<Slot*, Model*> target() {
        Slot* s = live_slot();
        if (!s) return {nullptr, nullptr};
        return {s, model_of(s->lin)};
    }

    void step() {
        const std::uint32_t op = pick(100);
        if (op < 10) return op_push_back();
        if (op < 16) return op_append();
        if (op < 19) return op_assign();
        if (op < 23) return op_erase_insert();
        if (op < 31) return op_players();
        if (op < 37) return op_held_player();
        if (op < 44) return op_map();
        if (op < 48) return op_set();
        if (op < 52) return op_title();
        if (op < 54) return op_counter();
        if (op < 63) return op_edit();
        if (op < 67) return op_batch();
        if (op < 70) return op_read();
        if (op < 73) return op_destroy_create();
        if (op < 77) return op_move_swap();
        if (op < 81) return op_reload_reset();
        if (op < 85) return op_stale();
        if (op < 89) return op_errors();
        if (op < 92) return op_grow_big();
        g_op = "verify";
        for (std::size_t i = 0; i < slots_.size(); ++i) verify(i, chance(10));
    }

    // Keeps a few proxies of every kind around (they may go stale later).
    void hold(Slot& s) {
        auto r = s.buf->ref<Root>();
        if (ints_.size() < 8) ints_.push_back({s.lin, r.field<&Root::ints>()});
        else ints_[pick(8)] = {s.lin, r.field<&Root::ints>()};
        if (titles_.size() < 4) titles_.push_back({s.lin, r.field<&Root::title>()});
    }

    void op_push_back() {
        g_op = "push_back";
        auto [s, m] = target();
        if (!s) return;
        if (chance(30)) hold(*s);
        IntsRef ints = s->buf->ref<Root>().field<&Root::ints>();
        std::uint64_t lin = s->lin;
        if (!ints_.empty() && chance(50)) {
            auto& h = ints_[pick(std::uint32_t(ints_.size()))];
            ints = h.ref;
            lin = h.lin;
            m = model_of(lin);
        }
        maybe_inject();
        const int n = 1 + int(pick(40));
        for (int i = 0; i < n; ++i) {
            const std::int32_t v = value();
            const bool ok = ints.push_back(v);
            if (!m) {
                REQUIRE(!ok && ints.error() == XErrc::stale && ints.size() == 0u);
                return;
            }
            if (ok) m->ints.push_back(v);
            REQUIRE(ints.size() == m->ints.size());
        }
    }

    void op_append() {
        g_op = "append";
        auto [s, m] = target();
        if (!s) return;
        auto ints = s->buf->ref<Root>().field<&Root::ints>();
        maybe_inject();
        if (chance(25) && !m->ints.empty()) {
            // From itself (a source inside the payload).
            const std::uint32_t from = pick(std::uint32_t(m->ints.size()));
            const std::uint32_t count = pick(std::uint32_t(m->ints.size()) - from + 1);
            const bool ok = s->buf->read<Root>([&](const Root& r) { return r.ints.size() >= from + count; }) &&
                            ints.append(std::span<const std::int32_t>(s->buf->root<Root>().ints.data() + from, count));
            if (ok) {
                std::vector<std::int32_t> copy(m->ints.begin() + from, m->ints.begin() + from + count);
                m->ints.insert(m->ints.end(), copy.begin(), copy.end());
            }
        } else if (chance(30)) {
            const std::uint32_t count = pick(500);
            const std::int32_t v = value();
            if (ints.append(count, v)) m->ints.insert(m->ints.end(), count, v);
        } else {
            scratch_.resize(pick(3000));
            for (auto& v : scratch_) v = value();
            if (ints.append(std::span<const std::int32_t>(scratch_)))
                m->ints.insert(m->ints.end(), scratch_.begin(), scratch_.end());
        }
        REQUIRE(ints.size() == m->ints.size());
    }

    void op_assign() {
        g_op = "assign";
        auto [s, m] = target();
        if (!s) return;
        auto ints = s->buf->ref<Root>().field<&Root::ints>();
        maybe_inject();
        scratch_.resize(pick(chance(20) ? 20000 : 400));
        for (auto& v : scratch_) v = value();
        if (ints.assign(std::span<const std::int32_t>(scratch_))) m->ints = scratch_;
        REQUIRE(ints.size() == m->ints.size());
    }

    void op_erase_insert() {
        g_op = "erase_insert";
        auto [s, m] = target();
        if (!s) return;
        auto ints = s->buf->ref<Root>().field<&Root::ints>();
        const std::uint32_t n = std::uint32_t(m->ints.size());
        const std::uint32_t at = pick(n + 3);
        if (chance(50)) {
            const bool ok = ints.erase(at);
            REQUIRE(ok == (at < n));
            if (ok) m->ints.erase(m->ints.begin() + at);
        } else {
            maybe_inject();
            const std::int32_t v = value();
            const bool ok = ints.insert(at, v);
            if (at > n) REQUIRE(!ok);
            if (ok) m->ints.insert(m->ints.begin() + at, v);
            if (at > n) s->buf->clear_error();
        }
        REQUIRE(ints.size() == m->ints.size());
    }

    void op_players() {
        g_op = "players";
        auto [s, m] = target();
        if (!s) return;
        auto players = s->buf->ref<Root>().field<&Root::players>();
        maybe_inject();
        if (chance(15) && !m->players.empty()) {
            const std::uint32_t at = pick(std::uint32_t(m->players.size()));
            REQUIRE(players.erase(at));
            m->players.erase(m->players.begin() + at);
            return;
        }
        if (chance(15)) {
            const std::uint32_t count = pick(40);
            const std::uint32_t first = players.emplace_n(count);
            if (first != PlayersRef::npos) {
                REQUIRE(first == m->players.size());
                m->players.resize(m->players.size() + count);
            }
            REQUIRE(players.size() == m->players.size());
            return;
        }
        auto p = players.emplace_back();
        if (!p.valid()) {
            REQUIRE(players.size() == m->players.size());
            return;
        }
        m->players.emplace_back();
        PlayerM& pm = m->players.back();
        pm.id = value();
        REQUIRE(p.field<&Player::id>().set(pm.id));
        p.field<&Player::level>() += 3;
        pm.level += 3;
        const std::string name = make_name(rng_);
        if (p.field<&Player::name>().assign(name)) pm.name = name;
        auto items = p.field<&Player::items>();
        for (std::uint32_t k = pick(20); k > 0; --k) {
            const std::int32_t v = value();
            if (items.push_back(v)) pm.items.push_back(v);
        }
        if (players_.size() < 12) players_.push_back({s->lin, players.size() - 1u, p});
        else players_[pick(12)] = {s->lin, players.size() - 1u, p};
    }

    // A held element proxy names container + index: it reads whatever is at
    // that index now (or nothing past the end).
    void op_held_player() {
        g_op = "held_player";
        if (players_.empty()) return;
        auto& h = players_[pick(std::uint32_t(players_.size()))];
        Model* m = model_of(h.lin);
        maybe_inject();
        if (!m) {
            REQUIRE(!h.ref.valid() && h.ref.field<&Player::id>().get() == 0);
            REQUIRE(!h.ref.field<&Player::level>().set(1));
            REQUIRE(!h.ref.field<&Player::name>().assign("x"));
            REQUIRE(h.ref.field<&Player::items>().size() == 0u && !h.ref.field<&Player::items>().push_back(1));
            REQUIRE(h.ref.error() == XErrc::stale);
            return;
        }
        if (h.index >= m->players.size()) {
            REQUIRE(!h.ref.valid() && h.ref.field<&Player::level>().get() == 0);
            const bool wrote = h.ref.field<&Player::level>().set(5);
            REQUIRE(!wrote);
            Slot* s = slot_of(h.lin);
            REQUIRE(s && s->buf->error() != XErrc::ok);
            s->buf->clear_error();
            return;
        }
        PlayerM& pm = m->players[h.index];
        REQUIRE(h.ref.field<&Player::id>().get() == pm.id);
        REQUIRE(h.ref.field<&Player::name>() == pm.name);
        switch (pick(3)) {
            case 0: {
                const std::string name = make_name(rng_);
                if (h.ref.field<&Player::name>().assign(name)) pm.name = name;
                break;
            }
            case 1: {
                auto items = h.ref.field<&Player::items>();
                scratch_.resize(pick(200));
                for (auto& v : scratch_) v = value();
                if (items.append(std::span<const std::int32_t>(scratch_)))
                    pm.items.insert(pm.items.end(), scratch_.begin(), scratch_.end());
                break;
            }
            default:
                h.ref.field<&Player::level>() = pm.level = value();
                break;
        }
        REQUIRE(h.ref.field<&Player::items>().size() == pm.items.size());
    }

    Slot* slot_of(std::uint64_t lin) {
        for (auto& s : slots_)
            if (s.buf && s.lin == lin) return &s;
        return nullptr;
    }

    void op_map() {
        g_op = "map";
        auto [s, m] = target();
        if (!s) return;
        auto map = s->buf->ref<Root>().field<&Root::map>();
        maybe_inject();
        switch (pick(5)) {
            case 0: {
                for (std::uint32_t k = 1 + pick(30); k > 0; --k) {
                    const std::int32_t key = std::int32_t(pick(5000));
                    const std::int32_t v = value();
                    const bool absent = !m->map.count(key);
                    const bool ok = map.try_emplace(key, v);
                    if (!absent) REQUIRE(!ok);
                    if (ok) m->map[key] = v;
                }
                break;
            }
            case 1: {
                std::vector<std::pair<std::int32_t, std::int32_t>> kv(pick(chance(10) ? 20000 : 300));
                for (auto& e : kv) e = {std::int32_t(pick(60000)), value()};
                const std::uint32_t inserted = map.insert(kv);
                if (inserted != 0 || kv.empty() || s->buf->ok()) {
                    std::uint32_t expect = 0;
                    for (const auto& e : kv)
                        if (m->map.emplace(e.first, e.second).second) ++expect;
                    REQUIRE(inserted == expect);
                }
                break;
            }
            case 2: {
                const std::int32_t key = std::int32_t(pick(5000));
                REQUIRE(map.erase(key) == (m->map.erase(key) != 0));
                break;
            }
            case 3: {
                const std::int32_t key = std::int32_t(pick(5000));
                auto v = map.find(key);
                REQUIRE(v.valid() == (m->map.count(key) != 0));
                if (v.valid() && mapvals_.size() < 8) mapvals_.push_back({s->lin, key, v});
                break;
            }
            default: {
                if (mapvals_.empty()) break;
                auto& h = mapvals_[pick(std::uint32_t(mapvals_.size()))];
                Model* hm = model_of(h.lin);
                const std::int32_t v = value();
                const bool ok = h.ref.set(v);
                const bool present = hm && hm->map.count(h.key);
                REQUIRE(ok == present);
                if (ok) hm->map[h.key] = v;
                else if (hm) {
                    if (Slot* hs = slot_of(h.lin)) hs->buf->clear_error();
                }
                break;
            }
        }
        REQUIRE(map.size() == m->map.size());
    }

    void op_set() {
        g_op = "set";
        auto [s, m] = target();
        if (!s) return;
        auto set = s->buf->ref<Root>().field<&Root::set>();
        maybe_inject();
        if (chance(30)) {
            scratch_.resize(pick(chance(10) ? 8000 : 100));
            for (auto& v : scratch_) v = std::int32_t(pick(30000));
            const std::uint32_t inserted = set.insert(std::span<const std::int32_t>(scratch_));
            if (inserted != 0 || scratch_.empty() || s->buf->ok()) {
                std::uint32_t expect = 0;
                for (const auto v : scratch_) expect += m->set.insert(v).second ? 1u : 0u;
                REQUIRE(inserted == expect);
            }
        } else if (chance(50)) {
            const std::int32_t v = std::int32_t(pick(30000));
            const bool absent = !m->set.count(v);
            const bool ok = set.insert(v);
            if (!absent) REQUIRE(!ok);
            if (ok) m->set.insert(v);
        } else {
            const std::int32_t v = std::int32_t(pick(30000));
            REQUIRE(set.erase(v) == (m->set.erase(v) != 0));
        }
        REQUIRE(set.size() == m->set.size());
    }

    void op_title() {
        g_op = "title";
        auto [s, m] = target();
        if (!s) return;
        if (chance(20)) hold(*s);
        // (Assigning one string proxy to another copies the characters, so
        // the proxy to use is chosen at construction.)
        const HeldTitle* held = !titles_.empty() && chance(40) ? &titles_[pick(std::uint32_t(titles_.size()))] : nullptr;
        TitleRef title = held ? held->ref : s->buf->ref<Root>().field<&Root::title>();
        Model* tm = held ? model_of(held->lin) : m;
        maybe_inject();
        const std::string text = make_name(rng_);
        if (chance(50)) {
            const bool ok = title.assign(text);
            if (!tm) {
                REQUIRE(!ok && title.copy().empty() && title.error() == XErrc::stale);
                return;
            }
            if (ok) tm->title = text;
        } else {
            const bool ok = title.append(text);
            if (!tm) {
                REQUIRE(!ok);
                return;
            }
            if (ok) tm->title += text;
        }
        REQUIRE(title.copy() == tm->title);
    }

    void op_counter() {
        g_op = "counter";
        auto [s, m] = target();
        if (!s) return;
        CounterRef c = s->buf->ref<Root>().field<&Root::counter>();
        const std::int64_t d = value();
        c += d;
        m->counter += d;
        REQUIRE(c.get() == m->counter);
    }

    // Plain references inside an edit scope; each operation's effect is
    // read back from the container (with exceptions the failing operation
    // ends the scope; without, it has no effect and the scope goes on).
    void raw_ops(Root& r, Model& m, XBuffer& b, int depth) {
        const int n = 1 + int(pick(30));
        for (int i = 0; i < n; ++i) {
            switch (pick(depth < 2 ? 9 : 7)) {
                case 0: {
                    const std::int32_t v = value();
                    const auto before = r.ints.size();
                    r.ints.push_back(v);
                    if (r.ints.size() != before) m.ints.push_back(v);
                    break;
                }
                case 1: {
                    const auto before = r.players.size();
                    Player& p = r.players.emplace_back();
                    if (r.players.size() == before) break;
                    m.players.emplace_back();
                    p.id = m.players.back().id = value();
                    const std::string name = make_name(rng_);
                    p.name = name;
                    if (p.name.view() == name) m.players.back().name = name;
                    break;
                }
                case 2: {
                    if (m.players.empty()) break;
                    const std::uint32_t at = pick(std::uint32_t(m.players.size()));
                    Player& p = r.players[at];
                    const std::int32_t v = value();
                    const auto before = p.items.size();
                    p.items.push_back(v);
                    if (p.items.size() != before) m.players[at].items.push_back(v);
                    p.level = m.players[at].level = value();
                    break;
                }
                case 3: {
                    const std::int32_t key = std::int32_t(pick(5000)), v = value();
                    const auto before = r.map.size();
                    const bool absent = !m.map.count(key);
                    r.map.try_emplace(key, v);
                    if (absent && r.map.size() != before) m.map[key] = v;
                    break;
                }
                case 4: {
                    const std::int32_t v = std::int32_t(pick(30000));
                    const auto before = r.set.size();
                    r.set.insert(v);
                    if (r.set.size() != before) m.set.insert(v);
                    break;
                }
                case 5: {
                    const std::string text = make_name(rng_);
                    r.title = text;
                    if (r.title.view() == text) m.title = text;
                    break;
                }
                case 6: {
                    if (m.ints.empty()) break;
                    const std::uint32_t at = pick(std::uint32_t(m.ints.size()));
                    r.ints.erase(r.ints.begin() + at);
                    m.ints.erase(m.ints.begin() + at);
                    break;
                }
                case 7: {
                    // A proxy operation inside the scope: it may not move the
                    // payload (borrowed), else it works as outside.
                    auto ints = b.ref<Root>().field<&Root::ints>();
                    scratch_.resize(pick(chance(20) ? 4000 : 50));
                    for (auto& v : scratch_) v = value();
                    if (ints.append(std::span<const std::int32_t>(scratch_)))
                        m.ints.insert(m.ints.end(), scratch_.begin(), scratch_.end());
                    break;
                }
                default: {
                    // A nested scope (read or batch).
                    if (chance(50)) {
                        const auto n2 = b.read<Root>([](const Root& rr) { return rr.ints.size(); });
                        if (n2 != m.ints.size()) {
                            std::fprintf(stderr, "FAIL seed %llu step %d: nested read size\n",
                                         (unsigned long long)g_seed, g_step);
                            ++g_failures;
                        }
                    } else {
                        (void)b.ref<Root>().batch([&](Root& rr) { raw_ops(rr, m, b, depth + 1); });
                    }
                    break;
                }
            }
        }
    }

    void op_edit() {
        g_op = "edit";
        auto [s, m] = target();
        if (!s) return;
        maybe_inject();
        const std::size_t growth = chance(30) ? pick(200000) : 0;
        const bool ok = s->buf->edit<Root>([&](Root& r) { raw_ops(r, *m, *s->buf, 0); }, growth);
        REQUIRE(ok == s->buf->ok());
        REQUIRE(!s->buf->pinned());
        if (chance(70)) s->buf->clear_error();
    }

    void op_batch() {
        g_op = "batch";
        auto [s, m] = target();
        if (!s) return;
        maybe_inject();
        auto players = s->buf->ref<Root>().field<&Root::players>();
        const bool ok = players.batch([&](XVector<Player>& v) {
            for (std::uint32_t k = 1 + pick(20); k > 0; --k) {
                const auto before = v.size();
                Player& p = v.emplace_back();
                if (v.size() == before) continue;
                m->players.emplace_back();
                p.level = m->players.back().level = value();
                const std::int32_t item = value();
                p.items.push_back(item);
                if (p.items.size() == 1u) m->players.back().items.push_back(item);
            }
        }, chance(50) ? pick(100000) : 0);
        if (ok) REQUIRE(s->buf->ok());
        REQUIRE(!s->buf->pinned());
        if (chance(70)) s->buf->clear_error();
    }

    void op_read() {
        g_op = "read";
        auto [s, m] = target();
        if (!s) return;
        verify(std::size_t(s - slots_.data()), chance(20));
        // Reads through proxies agree too.
        auto r = s->buf->ref<Root>();
        REQUIRE(r.field<&Root::ints>().size() == m->ints.size());
        if (!m->ints.empty()) {
            const std::uint32_t at = pick(std::uint32_t(m->ints.size()));
            REQUIRE(r.field<&Root::ints>().get(at) == m->ints[at]);
        }
        REQUIRE(r.field<&Root::ints>().get(std::uint32_t(m->ints.size()) + pick(5)) == 0);
        std::size_t seen = 0;
        for (auto p : r.field<&Root::players>()) {
            if (seen >= m->players.size()) break;
            REQUIRE(p.field<&Player::id>().get() == m->players[seen].id);
            ++seen;
        }
        REQUIRE(seen == m->players.size());
    }

    void op_destroy_create() {
        g_op = "destroy_create";
        const std::size_t i = pick(std::uint32_t(slots_.size()));
        auto& s = slots_[i];
        if (s.buf && chance(60)) {
            retire_lineage(s.lin);
            s.buf.reset();
            s.lin = 0;
        } else if (!s.buf) {
            create(i);
        }
    }

    void op_move_swap() {
        g_op = "move_swap";
        const std::size_t a = pick(std::uint32_t(slots_.size())), b = pick(std::uint32_t(slots_.size()));
        if (a == b || !slots_[a].buf) return;
        if (!slots_[b].buf || chance(30)) {
            // Move (into an empty slot, or over a live one, which ends it).
            if (slots_[b].buf) retire_lineage(slots_[b].lin);
            slots_[b].buf = std::move(slots_[a].buf);
            slots_[a].buf.reset();
            slots_[b].lin = slots_[a].lin;
            slots_[a].lin = 0;
        } else {
            slots_[a].buf->swap(*slots_[b].buf);
            std::swap(slots_[a].lin, slots_[b].lin);
        }
    }

    void op_reload_reset() {
        g_op = "reload_reset";
        auto [s, m] = target();
        if (!s) return;
        if (chance(30)) {
            s->buf->reset<Root>();
            retire_lineage(s->lin);
            s->lin = next_lin_++;
            models_[s->lin] = Model{};
            REQUIRE(s->buf->ok());
            return;
        }
        if (chance(30)) {
            // build<T>(f): a new message in one step; it may outgrow the
            // reservation unless the growth is asked for up front.
            const std::uint32_t n = std::uint32_t(rng_() % 3000u);
            const std::size_t growth = chance(50) ? std::size_t(n) * 8u + 64u : 0u;
            const std::int64_t counter = std::int64_t(rng_() % 1000u);
            const bool ok = s->buf->build<Root>([&](Root& r) {
                r.counter = counter;
                for (std::uint32_t i = 0; i < n; ++i) r.ints.push_back(std::int32_t(i));
                r.title = "built";
            }, growth);
            retire_lineage(s->lin);
            s->lin = next_lin_++;
            Model fresh;
            fresh.counter = counter;
            if (ok) {
                fresh.ints.resize(n);
                for (std::uint32_t i = 0; i < n; ++i) fresh.ints[i] = std::int32_t(i);
                fresh.title = "built";
                REQUIRE(s->buf->ok() && !s->buf->pinned());
                models_[s->lin] = std::move(fresh);
            } else {
                // A failed message: whatever landed is not modelled; start
                // the lineage over from an empty root.
                REQUIRE(!s->buf->ok() && !s->buf->pinned());
                s->buf->clear_error();
                s->buf->reset<Root>();
                models_[s->lin] = Model{};
            }
            return;
        }
        if (!s->buf->ok()) s->buf->clear_error();
        const Model saved = *m;
        const std::string wire = s->buf->save_verified<Root>();
        REQUIRE(!wire.empty());
        Slot* d = live_slot();
        d->buf->reload_verified<Root>(wire);
        retire_lineage(d->lin);
        d->lin = next_lin_++;
        models_[d->lin] = saved;
        REQUIRE(d->buf->ok());
    }

    // Every proxy of a lineage that ended is stale.
    void op_stale() {
        g_op = "stale";
        for (auto& h : ints_) {
            if (model_of(h.lin)) continue;
            REQUIRE(!h.ref.valid() && h.ref.size() == 0u && h.ref.get(0) == 0);
            REQUIRE(!h.ref.push_back(1) && !h.ref.append({1, 2}) && !h.ref.assign({3}));
            REQUIRE(h.ref.emplace_n(2) == IntsRef::npos && !h.ref.erase(0) && !h.ref.insert(0, 1));
            REQUIRE(!h.ref.batch([](XVector<std::int32_t>& v) { v.push_back(1); }));
            REQUIRE(h.ref.error() == XErrc::stale);
        }
        for (auto& h : mapvals_) {
            if (model_of(h.lin)) continue;
            REQUIRE(!h.ref.valid() && h.ref.get() == 0 && !h.ref.set(1));
        }
    }

    void op_errors() {
        g_op = "errors";
        auto [s, m] = target();
        if (!s) return;
        XBuffer& b = *s->buf;
        if (b.ok()) {
            // Record a failure: a write past the end.
            auto players = b.ref<Root>().field<&Root::players>();
            REQUIRE(!players[std::uint32_t(m->players.size()) + 3u].field<&Player::id>().set(1));
            REQUIRE(b.error() == XErrc::out_of_range);
        }
        // A failed buffer emits nothing, and allocating operations fail.
        auto ints = b.ref<Root>().field<&Root::ints>();
        std::string wire;
#if XOFFSET_NO_EXCEPTIONS
        b.save_verified_into<Root>(wire);
        REQUIRE(wire.empty());
#else
        bool refused = false;
        try {
            b.save_verified_into<Root>(wire);
        } catch (const XException& e) {
            refused = e.code() == b.error();
        }
        REQUIRE(refused && wire.empty());
#endif
        // (A write that needs no allocation still lands: only allocating
        // operations are refused.)
        scratch_.assign(3, 1);
        if (ints.append(std::span<const std::int32_t>(scratch_))) m->ints.insert(m->ints.end(), 3, 1);
        REQUIRE(ints.size() == m->ints.size());
        REQUIRE(!b.edit<Root>([](Root& r) { r.counter = 1; }));
        b.clear_error();
        // After clear_error a small write works again.
        const std::int32_t v = value();
        REQUIRE(ints.push_back(v));
        m->ints.push_back(v);
    }

    // Growth past the first 1 MiB (a batch of 300k values).
    void op_grow_big() {
        g_op = "grow_big";
        if (!chance(15)) return;
        auto [s, m] = target();
        if (!s || m->ints.size() > 1200000) return;
        auto ints = s->buf->ref<Root>().field<&Root::ints>();
        maybe_inject();
        scratch_.resize(300000);
        for (auto& v : scratch_) v = value();
        if (ints.append(std::span<const std::int32_t>(scratch_)))
            m->ints.insert(m->ints.end(), scratch_.begin(), scratch_.end());
        REQUIRE(ints.size() == m->ints.size());
    }
};

} // namespace

int main(int argc, char** argv) {
    const int seeds = argc > 1 ? std::atoi(argv[1]) : 6;
    const int steps = argc > 2 ? std::atoi(argv[2]) : 2500;
    const std::uint64_t first = argc > 3 ? std::strtoull(argv[3], nullptr, 10) : 1;
    for (std::uint64_t seed = first; seed < first + std::uint64_t(seeds) && g_failures == 0; ++seed) {
        g_seed = seed;
        Fuzz fuzz(seed);
        fuzz.run(steps);
    }
    if (g_failures) {
        std::cout << "test_proxy_fuzz: FAILED" << std::endl;
        return 1;
    }
    std::cout << "test_proxy_fuzz: " << seeds << " seeds x " << steps << " steps, no crash, buffers equal their models"
              << std::endl;
    return 0;
}
