// SLG prototype, demo case set, keys D* for zpp_bits (4.0 W1, 2026-11-12): the
// rival's shared encoding, the same server work split as XOffset's D3
// (xoffset_runner.hpp, "D3 prototype") with whole-record zpp_bits bodies.
//
// Server, once per tick (D*.pub): every unit the world changed that some view
// holds is brought up to date in the server's image of it (the members its
// dirty flags name, copied from the world) and encoded whole, once, into this
// tick's body arena; the entry keeps where. A unit no one holds only collects
// its dirty flags (no copy, no encoding) until its first use. Marches, tiles
// and units that enter a view without a change are encoded on their first use
// in the tick (out of line), once. Which view holds what is the tracker every
// format shares (ViewTracker, DeltaPlan); the holder count follows the same
// rule as XOffset's D3 (+1 on an enter sent, -1 on a leave).
//
// Server, per delivery (D*.send): the message is
//     int64 request_id, int64 tick,
//     uint32 n + int32 ids of the units that left, the same for marches,
//     uint32 n + n unit bodies, uint32 n + n march bodies, uint32 n + n tile bodies,
// every body a copy of the entity's bytes of this tick. An entering unit uses
// the same body as a changed one (a whole record either way). Every field is
// what zpp_bits writes for that type (default options: little endian, uint32
// size prefixes), so the message is plain zpp_bits; nothing per connection
// beyond the tracker.
//
// Client (D*.recv, D*T.recv; D*W / D*TW warm devices): zpp_bits' checked
// decoding (every read bounds-checked) straight into the view it holds: a
// leave swaps the last record into the slot; a body's leading id (each
// record's first member) finds the held record through the client's id
// index, the same index type as U*.apply, and the body is decoded over it
// (strings and lists reuse their storage), or onto a new record at the end.
// zpp_bits has one check level, so the trust tier (D*T, the scoring column)
// and the full-check row run the same code, each into its own copy of the view.
//
// ZppSharedViewT<true> (format zpp_bits_dirty, diagnostic): the second
// variant, the dirty-flag encoding a zpp_bits user writes by hand for each
// field. A unit that changed while some view holds it gets a change body
// once per tick: its id, the uint64 dirty mask, then only the members the
// mask names, each as zpp_bits writes it; an entering unit still gets the
// whole record. The message carries the units in two lists, entering
// (whole bodies) then changed (change bodies); the client decodes the masked
// members over the record it holds.
#pragma once
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>
#include <zpp_bits.h>
#include "slg/model_std.hpp"
#include "slg/view.hpp"

namespace slg {

#if defined(__GNUC__) || defined(__clang__)
#define SLG_ZS_PREFETCH(p) __builtin_prefetch(p)
#else
#define SLG_ZS_PREFETCH(p) ((void)(p))
#endif

template <bool kDirty> class ZppSharedViewT {
public:
    // Clients 0..clients-1, then the warm devices' views (clients + d).
    void d_init(const MapWorld& w) {
        const ViewScale& vs = w.scale();
        img_.assign(std::size_t(vs.units), {});
        for (int32_t i = 0; i < vs.units; ++i) copy_unit(img_[std::size_t(i)], w.unit(i));
        ue_.assign(std::size_t(vs.units), {});
        te_.assign(std::size_t(kTiles) * kTiles, {});
        me_.clear();
        const std::size_t views = std::size_t(vs.clients + warm_devices(vs.clients));
        cli_.assign(views, {});
        clt_.assign(views, {});
        tick_ = w.tick_no();
        arena_.reset();
        ucap_ = int32_t(int64_t(3) * vs.units * kViewW * kViewH / (int64_t(2) * kMapSize * kMapSize)) + 32;
        mcap_ = vs.marches / 4 + 32;
    }
    // The client's view: its rect and the tiles under it (zero until its key
    // message fills them); room for the records that come and go, as held().
    void d_view(int c, const ViewRect& r) {
        for (Client* L : {&cli_[std::size_t(c)], &clt_[std::size_t(c)]}) {
            *L = Client{};
            L->v.x0 = r.x0; L->v.y0 = r.y0; L->v.x1 = r.x1; L->v.y1 = r.y1;
            L->span = tile_span(r.x0, r.y0, r.x1, r.y1);
            L->v.tiles.assign(std::size_t(L->span.cols * L->span.rows), {});
            L->v.units.reserve(std::size_t(ucap_));
            L->v.marches.reserve(std::size_t(mcap_));
        }
    }
    stdm::ViewSnapshot& d_held(int c) { return cli_[std::size_t(c)].v; }
    stdm::ViewSnapshot& d_held_trusted(int c) { return clt_[std::size_t(c)].v; }
    std::string_view d_wire() const { return {reinterpret_cast<const char*>(msg_.buf.data()), msg_.n}; }

    // Once per tick: dirty flags noted; every changed unit some view holds is
    // updated in the image and encoded whole, once for all its holders.
    void d_publish(const MapWorld& w) {
        tick_ = w.tick_no();
        arena_.reset();
        const auto& cu = w.changed_units();
        for (int32_t i : cu) SLG_ZS_PREFETCH(&ue_[std::size_t(i)]);
        for (int32_t i : cu) {
            UEnt& e = ue_[std::size_t(i)];
            e.pend |= w.dirty(i);
            if (e.hold > 0) SLG_ZS_PREFETCH(&img_[std::size_t(i)]);
        }
        for (int32_t i : cu) if (ue_[std::size_t(i)].hold > 0) {
            if constexpr (kDirty) change_body(w, i);
            else unit_body(w, i);
        }
    }

    // The key message of client c's first pull (untimed): every unit, march
    // and tile in its view.
    void d_key(int c, const MapWorld& w, const DeltaPlan& p) {
        (void)c;
        msg_.n = 0;
        head(0, w.tick_no());
        msg_.u32(0); msg_.u32(0);
        msg_.u32(uint32_t(p.units.size()));
        for (int32_t i : p.units) { const Ent& e = unit_body(w, i); ++ue_[std::size_t(i)].hold; msg_.put(arena_.data() + e.off, e.len); }
        if constexpr (kDirty) msg_.u32(0);
        msg_.u32(uint32_t(p.marches.size()));
        for (int32_t i : p.marches) { const Ent& e = march_body(w, i); msg_.put(arena_.data() + e.off, e.len); }
        const TileSpan sp = cli_[std::size_t(c)].span;
        msg_.u32(uint32_t(sp.cols * sp.rows));
        for (int32_t ty = sp.ty0; ty < sp.ty0 + sp.rows; ++ty)
            for (int32_t tx = sp.tx0; tx < sp.tx0 + sp.cols; ++tx) {
                const Ent& e = tile_body(w, ty * kTiles + tx);
                msg_.put(arena_.data() + e.off, e.len);
            }
    }
    // One delivery: header, leaves, then a copy of each entity's body of this tick.
    std::size_t d_send(int c, const MapWorld& w, const DeltaPlan& p, int64_t id) {
        (void)c;
        msg_.n = 0;
        head(id, w.tick_no());
        msg_.u32(uint32_t(p.removed.size()));
        for (int32_t uid : p.removed) { msg_.i32(uid); --ue_[std::size_t(uid - 1)].hold; }   // unit id = index + 1
        msg_.u32(uint32_t(p.removed_marches.size()));
        for (int32_t mid : p.removed_marches) msg_.i32(mid);
        if constexpr (kDirty) {
            // Entering units whole, then the changed ones' change bodies.
            const std::size_t at = msg_.n;
            msg_.u32(0);
            uint32_t ne = 0;
            for (std::size_t q = 0; q < p.units.size(); ++q) {
                if (!p.unew[q]) continue;
                const int32_t i = p.units[q];
                const Ent& e = unit_body(w, i);
                ++ue_[std::size_t(i)].hold;
                msg_.put(arena_.data() + e.off, e.len);
                ++ne;
            }
            std::memcpy(msg_.buf.data() + at, &ne, sizeof ne);
            msg_.u32(uint32_t(p.units.size()) - ne);
            for (std::size_t q = 0; q < p.units.size(); ++q) {
                if (p.unew[q]) continue;
                const UEnt& e = change_body(w, p.units[q]);
                msg_.put(arena_.data() + e.coff, e.clen);
            }
        } else {
            // Every unit in the plan has a body: a changed one changed this tick,
            // an entering one is sent whole anyway.
            msg_.u32(uint32_t(p.units.size()));
            for (std::size_t q = 0; q < p.units.size(); ++q) {
                const int32_t i = p.units[q];
                const Ent& e = unit_body(w, i);
                if (p.unew[q]) ++ue_[std::size_t(i)].hold;
                msg_.put(arena_.data() + e.off, e.len);
            }
        }
        msg_.u32(uint32_t(p.marches.size()));
        for (int32_t i : p.marches) { const Ent& e = march_body(w, i); msg_.put(arena_.data() + e.off, e.len); }
        msg_.u32(uint32_t(p.tiles.size()));
        for (int32_t t : p.tiles) { const Ent& e = tile_body(w, t); msg_.put(arena_.data() + e.off, e.len); }
        return msg_.n;
    }

#ifdef SLG_XS2_DIAG
    // Diagnostic build only (not in scoring builds): XOffset's d_diag_touch, same bits.
    uint64_t d_diag_touch(int c, std::string_view msg) {
        static const int bits = [] { const char* e = std::getenv("SLG_XS2_DIAG"); return e ? std::atoi(e) : 0; }();
        Client& L = clt_[std::size_t(c)];
        uint64_t h = 0;
        if (bits & 1) h += L.at.units.diag_touch(64) + L.at.marches.diag_touch(64);
        if (bits & 8) h += L.at.units.diag_touch(4096) + L.at.marches.diag_touch(4096);
        if (bits & 4) for (std::size_t i = 0; i < msg.size(); i += 64) h += uint64_t(uint8_t(msg[i]));
        return h;
    }
#endif
    void d_recv(int c) { apply(cli_[std::size_t(c)], d_wire()); }
    void d_recv_trusted(int c) { apply(clt_[std::size_t(c)], d_wire()); }
    void d_recv_from(int c, std::string_view m) { apply(cli_[std::size_t(c)], m); }
    void d_recv_trusted_from(int c, std::string_view m) { apply(clt_[std::size_t(c)], m); }

private:
    // A body of this tick: arena offset and length (tick == the world's tick).
    struct Ent { int64_t tick = -1; uint32_t off = 0, len = 0; };
    // pend: dirty flags of changes the image has not taken yet.
    // Dirty variant: the change body of tick ctick at coff, clen.
    struct UEnt : Ent { uint64_t pend = 0; int64_t ctick = -1; uint32_t coff = 0, clen = 0; int32_t hold = 0; };
    struct Client {
        stdm::ViewSnapshot v;
        HeldIndex at;
        TileSpan span{};
    };
    // A grow-only byte buffer (the message; zpp_bits' own out archive grows
    // its vector the same way).
    struct Out {
        std::vector<std::byte> buf;
        std::size_t n = 0;
        void room(std::size_t k) { if (n + k > buf.size()) buf.resize(std::max(buf.size() * 2, n + k + 4096)); }
        void put(const std::byte* p, std::size_t k) { room(k); std::memcpy(buf.data() + n, p, k); n += k; }
        template <class T> void pod(T v) { room(sizeof v); std::memcpy(buf.data() + n, &v, sizeof v); n += sizeof v; }
        void u32(uint32_t v) { pod(v); }
        void i32(int32_t v) { pod(v); }
    };
    // This tick's bodies: one zpp_bits out archive over a grow-only vector,
    // rewound every tick.
    struct Arena {
        std::vector<std::byte> buf;
        zpp::bits::out<std::vector<std::byte>> out{buf};
        Arena() = default;
        Arena(const Arena&) = delete;
        Arena& operator=(const Arena&) = delete;
        void reset() { out.reset(); }
        const std::byte* data() const { return buf.data(); }
        template <class T> Ent add(const T& v, int64_t tick) {
            Ent e;
            e.tick = tick;
            e.off = uint32_t(out.position());
            if (const auto r = out(v); r != std::errc{}) throw std::runtime_error("zpp_bits shared encode failed");
            e.len = uint32_t(out.position() - e.off);
            return e;
        }
    };

    void head(int64_t id, int64_t tick) { msg_.pod(id); msg_.pod(tick); }

    // Unit i's body of this tick, encoded on first use.
    const Ent& unit_body(const MapWorld& w, int32_t i) {
        UEnt& e = ue_[std::size_t(i)];
        if (e.tick == tick_) return e;
        unit_fill(w, i);
        return e;
    }
    [[gnu::noinline]] void unit_fill(const MapWorld& w, int32_t i) {
        UEnt& e = ue_[std::size_t(i)];
        stdm::ViewUnit& r = img_[std::size_t(i)];
        catch_up(w, i, e, r);
        static_cast<Ent&>(e) = arena_.add(r, tick_);
    }
    // The image takes the members changed since it last saw the unit.
    static void catch_up(const MapWorld& w, int32_t i, UEnt& e, stdm::ViewUnit& r) {
        if (const uint64_t m = e.pend) {
            e.pend = 0;
            const WorldUnit& u = w.unit(i);
#define SLG_X(f) if (m & SLG_D(f)) r.f = u.f;
            SLG_VUNIT_SCALARS(SLG_X)
#undef SLG_X
            if (m & SLG_D(name)) set_str(r.name, u.name);
            if (m & SLG_D(alliance_tag)) set_str(r.alliance_tag, u.alliance_tag);
            if (m & SLG_D(owner_name)) set_str(r.owner_name, u.owner_name);
            if (m & SLG_D(commander)) copy_uhero(r.commander, u.commander);
            if (m & SLG_D(heroes)) copy_list(r.heroes, u.heroes, [](auto& a, const auto& x) { copy_uhero(a, x); });
            if (m & SLG_D(troops)) copy_list(r.troops, u.troops, [](auto& a, const auto& x) { a.type = x.type; a.count = x.count; a.wounded = x.wounded; });
            if (m & SLG_D(buffs)) copy_list(r.buffs, u.buffs, [](auto& a, const auto& x) { a.id = x.id; a.stacks = x.stacks; a.expire_ms = x.expire_ms; });
        }
    }
    // Dirty variant: unit i's change body of this tick (a held unit that
    // changed): id, mask, the masked members. The mask is the change of this
    // tick: a held unit's image takes every change in the tick it happens.
    const UEnt& change_body(const MapWorld& w, int32_t i) {
        UEnt& e = ue_[std::size_t(i)];
        if (e.ctick != tick_) change_fill(w, i);
        return e;
    }
    [[gnu::noinline]] void change_fill(const MapWorld& w, int32_t i) {
        UEnt& e = ue_[std::size_t(i)];
        stdm::ViewUnit& r = img_[std::size_t(i)];
        const uint64_t m = e.pend;
        catch_up(w, i, e, r);
        e.ctick = tick_;
        e.coff = uint32_t(arena_.out.position());
        auto& out = arena_.out;
        bool ok = out(r.id, m) == std::errc{};
#define SLG_X(f) if (m & SLG_D(f)) ok = ok && out(r.f) == std::errc{};
        SLG_VUNIT_SCALARS(SLG_X)
        SLG_X(name) SLG_X(alliance_tag) SLG_X(owner_name) SLG_X(commander) SLG_X(heroes) SLG_X(troops) SLG_X(buffs)
#undef SLG_X
        if (!ok) throw std::runtime_error("zpp_bits shared encode failed");
        e.clen = uint32_t(out.position() - e.coff);
    }
    // Marches and tiles by world index (stable within a tick).
    const Ent& march_body(const MapWorld& w, int32_t i) {
        if (std::size_t(i) >= me_.size()) me_.resize(std::size_t(i) + 64);
        Ent& e = me_[std::size_t(i)];
        if (e.tick != tick_) march_fill(w, i);
        return e;
    }
    [[gnu::noinline]] void march_fill(const MapWorld& w, int32_t i) {
        stdm::ViewMarch m;
        copy_vmarch(m, w.march(i));
        me_[std::size_t(i)] = arena_.add(m, tick_);
    }
    const Ent& tile_body(const MapWorld& w, int32_t t) {
        Ent& e = te_[std::size_t(t)];
        if (e.tick != tick_) tile_fill(w, t);
        return e;
    }
    [[gnu::noinline]] void tile_fill(const MapWorld& w, int32_t t) {
        stdm::Tile s;
        copy_vtile(s, w, t % kTiles, t / kTiles);
        te_[std::size_t(t)] = arena_.add(s, tick_);
    }

    // The client: zpp_bits' checked decoding straight into its held view.
    static int32_t peek_id(std::span<const std::byte> m, std::size_t at) {
        if (at + sizeof(int32_t) > m.size()) throw std::runtime_error("zpp_bits shared: truncated message");
        int32_t id;
        std::memcpy(&id, m.data() + at, sizeof id);
        return id;
    }
    static void check(std::errc e) {
        if (e != std::errc{}) throw std::runtime_error("zpp_bits shared decode: " + std::make_error_code(e).message());
    }
    static void apply(Client& L, std::string_view wire) {
        const std::span<const std::byte> m(reinterpret_cast<const std::byte*>(wire.data()), wire.size());
        zpp::bits::in in(m);
        stdm::ViewSnapshot& v = L.v;
        check(in(v.request_id, v.tick));
        uint32_t n = 0;
        check(in(n));
        for (uint32_t k = 0; k < n; ++k) {
            int32_t id;
            check(in(id));
            const auto it = L.at.units.find(id);
            if (it == L.at.units.end()) continue;
            const int32_t pos = it->second, last = int32_t(v.units.size()) - 1;
            L.at.units.erase(it);
            if (pos != last) {
                v.units[std::size_t(pos)] = std::move(v.units[std::size_t(last)]);
                L.at.units[v.units[std::size_t(pos)].id] = pos;
            }
            v.units.pop_back();
        }
        check(in(n));
        for (uint32_t k = 0; k < n; ++k) {
            int32_t id;
            check(in(id));
            const auto it = L.at.marches.find(id);
            if (it == L.at.marches.end()) continue;
            const int32_t pos = it->second, last = int32_t(v.marches.size()) - 1;
            L.at.marches.erase(it);
            if (pos != last) {
                v.marches[std::size_t(pos)] = v.marches[std::size_t(last)];
                L.at.marches[v.marches[std::size_t(pos)].id] = pos;
            }
            v.marches.pop_back();
        }
        check(in(n));
        for (uint32_t k = 0; k < n; ++k) {
            const int32_t id = peek_id(m, in.position());
            const auto it = L.at.units.find(id);
            if (it != L.at.units.end()) check(in(v.units[std::size_t(it->second)]));
            else {
                L.at.units.emplace(id, int32_t(v.units.size()));
                check(in(v.units.emplace_back()));
            }
        }
        if constexpr (kDirty) {
            check(in(n));
            for (uint32_t k = 0; k < n; ++k) {
                int32_t id;
                uint64_t mask;
                check(in(id, mask));
                const auto it = L.at.units.find(id);
                if (it == L.at.units.end()) throw std::runtime_error("zpp_bits shared: change of a unit the view does not hold");
                stdm::ViewUnit& u = v.units[std::size_t(it->second)];
#define SLG_X(f) if (mask & SLG_D(f)) check(in(u.f));
                SLG_VUNIT_SCALARS(SLG_X)
                SLG_X(name) SLG_X(alliance_tag) SLG_X(owner_name) SLG_X(commander) SLG_X(heroes) SLG_X(troops) SLG_X(buffs)
#undef SLG_X
            }
        }
        check(in(n));
        for (uint32_t k = 0; k < n; ++k) {
            const int32_t id = peek_id(m, in.position());
            const auto it = L.at.marches.find(id);
            if (it != L.at.marches.end()) check(in(v.marches[std::size_t(it->second)]));
            else {
                L.at.marches.emplace(id, int32_t(v.marches.size()));
                check(in(v.marches.emplace_back()));
            }
        }
        check(in(n));
        for (uint32_t k = 0; k < n; ++k) {
            stdm::Tile t;
            check(in(t));
            const uint32_t tx = uint32_t(t.x - L.span.tx0), ty = uint32_t(t.y - L.span.ty0);
            if (tx >= uint32_t(L.span.cols) || ty >= uint32_t(L.span.rows)) throw std::runtime_error("zpp_bits shared: tile outside the view");
            v.tiles[std::size_t(ty) * std::size_t(L.span.cols) + tx] = t;
        }
        if (in.position() != m.size()) throw std::runtime_error("zpp_bits shared decode: trailing bytes");
    }

    std::vector<stdm::ViewUnit> img_;   // every unit as its holders have it (by world index)
    std::vector<UEnt> ue_;
    std::vector<Ent> te_, me_;
    std::vector<Client> cli_, clt_;     // clt_: the same clients, the trust-tier row
    Arena arena_;
    Out msg_;
    int64_t tick_ = -1;
    int32_t ucap_ = 0, mcap_ = 0;   // view capacity, as XOffset's D3 views
};
#undef SLG_ZS_PREFETCH
using ZppSharedView = ZppSharedViewT<false>;
using ZppSharedDirtyView = ZppSharedViewT<true>;

} // namespace slg
