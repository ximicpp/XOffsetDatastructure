// SLG benchmark, demo case set: a player's own data on its client, and dirty-module
// saves to the storage process.
//
// L (login): a client logs in and gets its whole player (armies, heroes, city,
// research, items) once; it keeps it for the session. LD (self delta): after
// that, the server changes the player (training troops, items, buildings,
// resources) and sends only the changed parts as a PlayerPatch (changed armies
// whole by index, changed item counts, the city when it changed); the client
// applies it to the player it holds. The server knows what changed from its own
// dirty marks (recorded by the edit, not timed).
//
// SD (dirty-module save): the storage process gets only the modules that change
// often (all armies and items) as a PlayerPatch instead of the whole player.
//
// XOffset native (xoffset_delta.hpp): LK.* login as a key delta, LN.* self
// deltas with save_delta_into / try_apply_delta_verified (one baseline per
// online client); SN.* saves to storage as a delta against what storage holds.
// Every native receiver's state is checked equal to the application-level one.
#pragma once
#include <algorithm>
#include <cstdint>
#include <stdexcept>
#include <type_traits>
#include <vector>
#include "generic.hpp"

namespace slg {

struct SelfDirty {
    std::vector<int32_t> armies, items;
    bool city = false;
    void clear() { armies.clear(); items.clear(); city = false; }
};

// One player's own actions in one tick; records what it touched.
// touch(object) records a changed object or container for native XOffset
// deltas (a no-op for the others).
struct NoTouch { template <class T> void operator()(const T&) const {} };
template <class M, class K, class V, class Touch> void bump(M& m, K k, V by, Touch& touch) {
    if constexpr (std::is_same_v<Touch, NoTouch>) m[k] += by;   // the same edit, no lookup first
    else if (m.find(k) != m.end()) { auto& v = m[k]; v += by; touch(v); }
    else { m[k] += by; touch(m); }   // an insert moves entries
}
template <class P, class Touch = NoTouch> void self_edits(P& p, Rng& rng, int ops, SelfDirty& d, Touch touch = {}) {
    for (int i = 0; i < ops; ++i) {
        switch (rng.below(4)) {
        case 0: { const int32_t k = rng.below(200); bump(p.items, k, rng.range(1, 100), touch); d.items.push_back(k); break; }
        case 1:
            if (p.armies.size() > 0) {
                const int32_t a = rng.below(int32_t(p.armies.size()));
                auto& ar = p.armies[std::size_t(a)];
                if (ar.troops.size() > 0) {
                    auto& t = ar.troops[std::size_t(rng.below(int32_t(ar.troops.size())))];
                    t.count += rng.range(1, 50);
                    touch(t);
                }
                d.armies.push_back(a);
            }
            break;
        case 2: bump(p.city.buildings, rng.below(30), 1, touch); d.city = true; break;
        default: bump(p.city.resources, rng.below(8), rng.range(10, 1000), touch); d.city = true; break;
        }
    }
    std::sort(d.armies.begin(), d.armies.end()); d.armies.erase(std::unique(d.armies.begin(), d.armies.end()), d.armies.end());
    std::sort(d.items.begin(), d.items.end()); d.items.erase(std::unique(d.items.begin(), d.items.end()), d.items.end());
}

template <class M, class P> void fill_patch(M& m, P& p, const SelfDirty& d, int64_t seq) {
    m.player_id = p.player_id; m.seq = seq;
    m.army_index.clear(); m.armies.clear();
    m.army_index.reserve(d.armies.size()); m.armies.reserve(d.armies.size());
    for (int32_t a : d.armies) { m.army_index.emplace_back(a); copy_army(m.armies.emplace_back(), p.armies[std::size_t(a)]); }
    m.items.clear();
    for (int32_t k : d.items) m.items[k] = p.items[k];
    m.city_changed = d.city ? 1 : 0;
    if (d.city) copy_city(m.city, p.city);
    else { m.city.city_id = 0; m.city.level = 0; m.city.buildings.clear(); m.city.resources.clear(); }
}

// Dirty-module save: all armies and all items.
template <class M, class P> void fill_modules(M& m, const P& p, int64_t seq) {
    m.player_id = p.player_id; m.seq = seq;
    m.army_index.clear(); m.armies.clear();
    m.army_index.reserve(p.armies.size()); m.armies.reserve(p.armies.size());
    for (std::size_t a = 0; a < p.armies.size(); ++a) { m.army_index.emplace_back(int32_t(a)); copy_army(m.armies.emplace_back(), p.armies[a]); }
    copy_map(m.items, p.items);
    m.city_changed = 0;
    m.city.city_id = 0; m.city.level = 0; m.city.buildings.clear(); m.city.resources.clear();
}

template <class P, class M> void apply_patch(P& p, const M& m) {
    for (std::size_t i = 0; i < m.army_index.size(); ++i) copy_army(p.armies[std::size_t(m.army_index[i])], m.armies[i]);
    for (const auto& kv : m.items) p.items[kv.first] = kv.second;
    if (m.city_changed) copy_city(p.city, m.city);
}

// What the client reads after a change (its armies, items and city).
template <class P> uint64_t query_self(const P& p) {
    uint64_t h = uint64_t(p.player_id) * 31u;
    for (const auto& a : p.armies) {
        h += uint64_t(a.army_id) * 7u;
        for (const auto& t : a.troops) h += uint64_t(t.count) * 3u + uint64_t(t.type);
    }
    for (const auto& kv : p.items) h += uint64_t(kv.first) * 1315423911u + uint64_t(kv.second);
    for (const auto& kv : p.city.buildings) h += uint64_t(kv.first) * 2654435761u + uint64_t(kv.second);
    for (const auto& kv : p.city.resources) h += uint64_t(kv.first) * 40503u + uint64_t(kv.second);
    return h;
}

// Ops (provided by each runner):
//   int online()                           players with a client
//   auto& server(int p)                    the server's player (mutable)
//   std::size_t login_encode(int p)        encode the whole player into the login wire
//   void login_recv(int p)                 the client receives it into its held player
//   auto& held(int p)
//   std::size_t patch_encode(auto fill)    fill(PlayerPatch&) then encode
//   void patch_recv()                      the client receives it into the reused receive object
//   const auto& patch()
// Optional native: n_login_key(p) -> size, n_login_recv(p), n_diff(p) -> size, n_recv(p), n_held(p).
template <class Ops> void run_login(Ops& ops, uint64_t seed, int cycle, int rounds, CycleStats& st, std::vector<int64_t>& seq) {
    constexpr bool native = requires { ops.n_diff(0); };
    const int online = ops.online();
    if (seq.size() != std::size_t(online)) seq.assign(std::size_t(online), 0);
    std::vector<int> logins;
    if (cycle == 0) for (int p = 0; p < online; ++p) logins.push_back(p);
    else {
        Rng rng(mix_seed(seed, 0x1061 + uint64_t(cycle)));
        for (int i = 0; i < online / 4; ++i) logins.push_back(rng.below(online));
    }
    for (int p : logins) {
        std::size_t n = 0;
        timed(st, "L.send", [&] { n = ops.login_encode(p); });
        uint64_t q = 0;
        timed(st, "L.recv", [&] { ops.login_recv(p); q = query_self(ops.held(p)); });
        st.report_sum += q;
        st.bytes["L.player"] += n;
        st.bytes["L.logins"] += 1;
        if constexpr (native) {
            std::size_t k = 0;
            timed(st, "LK.send", [&] { k = ops.n_login_key(p); });
            timed(st, "LK.recv", [&] { ops.n_login_recv(p); });
            if (query_self(ops.n_held(p)) != q) throw std::runtime_error("native login: player differs");
            st.bytes["LK.player"] += k;
        }
    }
    SelfDirty dirty;
    for (int r = 0; r < rounds; ++r)
        for (int p = 0; p < online; ++p) {
            Rng rng(mix_seed(seed, (uint64_t(cycle) * 64 + uint64_t(r)) * 100003 + uint64_t(p)));
            dirty.clear();
            if constexpr (requires { ops.n_touch(0, 0); })
                self_edits(ops.server(p), rng, 2, dirty, [&](const auto& o) { ops.n_touch(p, o); });
            else
                self_edits(ops.server(p), rng, 2, dirty);
            const int64_t sq = ++seq[std::size_t(p)];
            std::size_t n = 0;
            timed(st, "LD.send", [&] { n = ops.patch_encode([&](auto& m) { fill_patch(m, ops.server(p), dirty, sq); }); });
            timed(st, "LD.recv", [&] { ops.patch_recv(); });
            timed(st, "LD.apply", [&] { apply_patch(ops.held(p), ops.patch()); });
            const uint64_t q = query_self(ops.held(p));
            st.report_sum += q;
            st.bytes["LD.patch"] += n;
            st.bytes["LD.patches"] += 1;
            if constexpr (native) {
                std::size_t k = 0;
                if constexpr (requires { ops.n_diff_dirty(0, dirty); })
                    timed(st, "LN.diff", [&] { k = ops.n_diff_dirty(p, dirty); });
                else
                    timed(st, "LN.diff", [&] { k = ops.n_diff(p); });
                timed(st, "LN.recv", [&] { ops.n_recv(p); });
                if (query_self(ops.n_held(p)) != q) throw std::runtime_error("native self delta: player differs");
                st.bytes["LN.delta"] += k;
            }
        }
}

// Ops for the dirty-module save:
//   auto& player(int p)                    the scene's resident player
//   std::size_t modules_encode(auto fill)  fill(PlayerPatch&) then encode
//   void modules_recv()                    storage receives it (checked) and keeps the bytes
// Optional native: n_store_diff(p) -> size, n_store_recv(p), n_store_held(p),
// n_store_prime_all(): storage already holds every player (from earlier saves), so in
// cycle 0 (not pooled) each player's baseline is primed once, untimed; the timed
// deltas are then the steady state, not first saves.
template <class Ops> void run_dirty_save(Ops& ops, const std::vector<int>& who, int cycle, CycleStats& st) {
    constexpr bool native = requires { ops.n_store_diff(0); };
    if constexpr (requires { ops.n_store_prime_all(); }) if (cycle == 0) ops.n_store_prime_all();
    for (int p : who) {
        std::size_t n = 0;
        timed(st, "SD.save_encode", [&] { n = ops.modules_encode([&](auto& m) { fill_modules(m, ops.player(p), cycle); }); });
        timed(st, "SD.store_recv", [&] { ops.modules_recv(); });
        st.bytes["SD.modules"] += n;
        if constexpr (native) {
            std::size_t k = 0;
            timed(st, "SN.diff", [&] { k = ops.n_store_diff(p); });
            timed(st, "SN.store_recv", [&] { ops.n_store_recv(p); });
            if constexpr (requires { ops.n_store_check(p); }) {
                static const char* const names[5] = {"SN.check_key", "SN.check_none", "SN.check_in_place", "SN.check_full", "SN.check_empty"};
                st.bytes[names[ops.n_store_check(p)]] += 1;
            }
            Digest a, b;
            dig_player(a, ops.player(p)); dig_player(b, ops.n_store_held(p));
            if (a.h != b.h) throw std::runtime_error("native storage delta: player differs");
            st.bytes["SN.delta"] += k;
        }
    }
}

} // namespace slg
