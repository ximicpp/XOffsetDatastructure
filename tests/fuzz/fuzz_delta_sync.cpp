// libFuzzer target for the delta sync receiver (xoffset_delta.hpp,
// docs/DELTA_SYNC.md): XBuffer::try_apply_delta_verified / apply_delta_verified
// on a replica that holds a verified state, the entry a client or a storage
// server feeds with bytes from the network.
//
// Every apply goes through apply_checked(), which requires:
//   - try_ and throwing entries agree (outcome, code, message, payload). The
//     throwing entry runs on a clone of the replica, which carries no map of
//     checked bytes, so it is verified in full: this is the differential of
//     the receiver's none / in_place fast paths against whole verification.
//   - the receiver's map of checked bytes equals one built afresh;
//   - a rejected delta leaves the replica (payload, holes, contents) and the
//     cursor exactly as they were (a key delta rejected after its header
//     checks may instead leave the replica empty and the cursor cleared);
//   - an accepted delta leaves a payload whose save_verified() wire loads
//     with load_verified and re-saves byte-identically;
//   - XReceiver<from_server>::apply_delta with XCheck::admit (trust tier P2),
//     on another clone with a copy of the cursor: it accepts every delta the
//     full check accepts, with the same payload, holes and cursor; it rejects
//     with the same error every delta the parse rejects (any code but the
//     ones the checks of section 3 give); a rejected admit leaves its clone
//     and cursor as they were, or (key delta) empty; a delta it accepts
//     beyond the full check is not read. Its own parse and patch must stay
//     in bounds on any input (ASan).
//
// The first input byte picks the schema (bit 0: the fuzz model fz::Root of
// v2_fuzz_core.hpp, or a viewport-like vector of unit records) and the mode
// (bits 1-2):
//   0 raw: the rest of the input is the block / hole section of a delta to
//     the replica's next version, behind a header the harness writes (so the
//     input passes the stream and version checks and reaches the block and
//     hole parsers and the checks of section 3). Flags come from the input.
//   1 stream: the input drives a sequence of sender steps (mutations, sender
//     options, baseline resets); each real delta is applied, after up to a
//     few input-chosen corruptions of it that are applied first. The real
//     delta must apply and make the replica the sender's byte for byte.
//   2 bare: the input is a whole delta (header included), applied to the
//     replica, and to an empty buffer as a key delta.
//   3 key: the input is the body of a key delta (header and fingerprint
//     written by the harness) applied to the replica and to an empty buffer.
// The state the replica holds is built from the second input byte (a seed)
// before the input applies, so a saved crash replays alone.
//
// Build: -DXOFFSET_BUILD_FUZZERS=ON (libFuzzer); fuzz_delta_sync_replay is the
// same file linked with replay_main.cpp (any compiler).
#include "v2_fuzz_core.hpp"

#include <span>

namespace dfz {
struct Unit {
    std::uint32_t id{};
    std::uint32_t owner{};
    std::int32_t x{};
    std::int32_t y{};
    std::int32_t hp{};
    std::uint32_t state{};
    bool alive{};
    xo::XString name;
    xo::XVector<std::uint16_t> buffs;
};
struct View {
    std::uint32_t tick{};
    std::uint32_t live{};
    xo::XVector<Unit> slots;
    xo::XSet<std::uint32_t> watched;
    xo::XMap<std::uint32_t, std::int64_t> score;
};
} // namespace dfz
XOFFSET_RECORD(dfz::Unit, id, owner, x, y, hp, state, alive, name, buffs)
XOFFSET_RECORD(dfz::View, tick, live, slots, watched, score)
XOFFSET_REGISTER_SCHEMA_NAME(dfz::View, "fuzz.delta.view")

namespace {
// Input reader: bytes past the end read as 0.
struct In {
    const std::uint8_t* p;
    std::size_t n;
    std::size_t at = 0;
    std::uint8_t u8() { return at < n ? p[at++] : 0; }
    std::uint32_t u16() { return u8() | std::uint32_t(u8()) << 8; }
    std::uint64_t u64() { std::uint64_t v = 0; for (int i = 0; i < 8; ++i) v |= std::uint64_t(u8()) << (8 * i); return v; }
    std::size_t left() const { return at < n ? n - at : 0; }
    std::string rest() { std::string s(reinterpret_cast<const char*>(p + at), left()); at = n; return s; }
    std::string take(std::size_t k) { k = std::min(k, left()); std::string s(reinterpret_cast<const char*>(p + at), k); at += k; return s; }
};

void need(bool ok, const std::string& what) { if (!ok) fail("delta: " + what); }

std::string payload_of(const xo::XBuffer& b) {
    const auto* sm = b.arena();
    if (!sm) return {};
    return std::string(static_cast<const char*>(b.get_address()), static_cast<std::size_t>(sm->used_end));
}

std::vector<std::pair<std::uint32_t, std::uint32_t>> holes_of(const xo::XBuffer& b) {
    std::vector<std::pair<std::uint32_t, std::uint32_t>> out;
    if (!b.arena()) return out;
    b.for_each_free_block([&](xo::XBufferCore::WireHole h) { out.emplace_back(h.offset, h.bytes); });
    std::sort(out.begin(), out.end());
    return out;
}

std::string dump(const dfz::View& v) {
    std::string s = std::to_string(v.tick) + "|" + std::to_string(v.live) + "|";
    for (const auto& u : v.slots) {
        s += std::to_string(u.id) + "," + std::to_string(u.owner) + "," + std::to_string(u.x) + "," +
             std::to_string(u.y) + "," + std::to_string(u.hp) + "," + std::to_string(u.state) + (u.alive ? "A" : "D") +
             std::string(u.name.view()) + "[";
        for (auto b : u.buffs) s += std::to_string(b) + ",";
        s += "]";
    }
    s += "|";
    for (auto w : v.watched) s += std::to_string(w) + ",";
    s += "|";
    for (const auto& kv : v.score) s += std::to_string(kv.first) + "=" + std::to_string(kv.second) + ";";
    return s;
}

template <class Root>
bool has_state(xo::TypedXBuffer<Root>& b) { return b.arena() && b.has_root(); }

template <class Root>
std::string dump_of(xo::TypedXBuffer<Root>& b) { return has_state(b) ? dump(b.plain_root()) : std::string("<none>"); }

// XOFFSET_FUZZ_STATS=1: at exit, applies by outcome and receiver check
// (key, none, in_place, full), to see that a corpus reaches the fast paths.
struct Stats {
    std::size_t rejected = 0, accepted[4] = {}, admitted = 0, admit_rejected = 0;
    ~Stats() {
        const char* e = std::getenv("XOFFSET_FUZZ_STATS");
        if (!e || !*e) return;
        std::fprintf(stderr, "fuzz_delta_sync: rejected %zu, accepted key %zu none %zu in_place %zu full %zu; "
                     "admit accepted %zu rejected %zu\n",
                     rejected, accepted[0], accepted[1], accepted[2], accepted[3], admitted, admit_rejected);
    }
} g_stats;

// One apply through both entries, with the checks of the header comment.
// `key` says whether `d` is (claims to be) a key delta (flag bit 0).
template <class Root>
xo::XStatus apply_checked(xo::TypedXBuffer<Root>& rep, const std::string& d, xo::XDeltaCursor& cur,
                          const std::string& where) {
    const bool had = has_state(rep);
    const auto before = payload_of(rep);
    const auto before_holes = holes_of(rep);
    const auto before_dump = dump_of(rep);
    const auto before_version = cur.version();
    const auto before_stream = cur.stream();
    const bool key = d.size() > 3 && (static_cast<unsigned char>(d[3]) & 1u);

    std::optional<xo::TypedXBuffer<Root>> twin;
    if (had) twin.emplace(rep.clone());
    else twin.emplace(xo::TypedXBuffer<Root>(4096, xo::XBuffer::maximum_capacity(kLimit)));
    auto twin_cursor = cur;
    std::optional<xo::XError> thrown;
    try {
        twin->apply_delta_verified(d, twin_cursor);
    } catch (const xo::XException& e) {
        thrown = xo::XError{e.code(), e.what()};
    }

    // Trust tier admit on another clone, before the replica changes.
    std::optional<xo::TypedXBuffer<Root>> admitted;
    if (had) admitted.emplace(rep.clone());
    else admitted.emplace(xo::TypedXBuffer<Root>(4096, xo::XBuffer::maximum_capacity(kLimit)));
    auto admit_cursor = cur;
    static xo::XReceiver<xo::XLink::from_server> rx(xo::XBuffer::maximum_capacity(kLimit));
    static_assert(!xo::detail::force_full_check, "the harness needs the real admit");
    const xo::XStatus a = rx.apply_delta(*admitted, d, admit_cursor);

    auto r = rep.try_apply_delta_verified(d, cur);
    if (r) ++g_stats.accepted[std::size_t(cur.last_check()) & 3];
    else ++g_stats.rejected;
    if (!r) need(r.code() != xo::XErrc::ok && !r.error().message.empty(), where + ": rejected without a code and message");
    {
        const char* what = "";
        need(xo::detail::DeltaAccess::checks_agree<Root>(cur, rep, what),
             where + ": receiver map differs from a fresh one: " + what);
    }
    need(bool(thrown) != bool(r), where + ": try_apply_delta_verified " +
         (r ? std::string("accepted") : "rejected (" + r.error().message + ")") +
         " where apply_delta_verified (full verification) " +
         (thrown ? "threw (" + thrown->message + ")" : std::string("accepted")));
    if (thrown) {
        need(thrown->code == r.code() && thrown->message == r.error().message,
             where + ": fast path and full verification report different errors: " + r.error().message + " / " +
             thrown->message);
    } else {
        need(payload_of(*twin) == payload_of(rep), where + ": full verification produced a different payload");
        need(holes_of(*twin) == holes_of(rep), where + ": full verification produced a different hole set");
        need(twin_cursor.version() == cur.version() && twin_cursor.stream() == cur.stream(),
             where + ": full verification moved the cursor differently");
    }

    if (r) {
        need(bool(a), where + ": admit rejected a delta the full check accepts (" +
             (a ? std::string() : a.error().message) + ")");
        if (a) {
            need(admit_cursor.last_check() == xo::XDeltaCheck::admit, where + ": admit not reported as admit");
            need(payload_of(*admitted) == payload_of(rep), where + ": admit produced a different payload");
            need(holes_of(*admitted) == holes_of(rep), where + ": admit produced a different hole set");
            need(admit_cursor.version() == cur.version() && admit_cursor.stream() == cur.stream(),
                 where + ": admit moved the cursor differently");
        }
    } else {
        // Codes only the checks after the parse give (section 3: the
        // verification, the in-place checks, the root binding, the digest
        // of the result). Admit checks the root before it patches and the
        // digest after, the full check the digest first: a delta wrong in
        // both is refused by each with its own first finding.
        // The whole verification also needs the replica's free table.
        const auto c = r.code();
        const bool checks_only = c == xo::XErrc::invalid_payload || c == xo::XErrc::root_mismatch ||
                                 c == xo::XErrc::digest_mismatch ||
                                 c == xo::XErrc::invalid_payload_header || c == xo::XErrc::capacity_exceeded ||
                                 c == xo::XErrc::out_of_storage || c == xo::XErrc::host_allocation_failed ||
                                 r.error().message.find("free table incomplete") != std::string::npos;
        if (!checks_only)
            need(!a && a.code() == r.code(), where + ": admit and the full check disagree on a parse error: " +
                 r.error().message + " / " + (a ? std::string("accepted") : a.error().message));
    }
    if (!a) {
        const bool unchanged = payload_of(*admitted) == before && holes_of(*admitted) == before_holes &&
                               admit_cursor.version() == before_version && admit_cursor.stream() == before_stream;
        const bool cleared = key && !has_state(*admitted) && admit_cursor.version() == 0;
        need(unchanged || cleared, where + ": a rejected admit changed the replica or the cursor (" +
             a.error().message + ")");
    }
    if (a) ++g_stats.admitted; else ++g_stats.admit_rejected;

    if (!r) {
        const bool unchanged = payload_of(rep) == before && holes_of(rep) == before_holes &&
                               dump_of(rep) == before_dump && cur.version() == before_version &&
                               cur.stream() == before_stream;
        // A key delta rejected after its header checks leaves the buffer
        // empty and the cursor cleared (as a failed reload_verified).
        const bool cleared = key && !has_state(rep) && cur.version() == 0;
        need(unchanged || cleared, where + ": a rejected delta changed the replica or the cursor (" +
             r.error().message + ")");
    } else {
        need(has_state(rep), where + ": an accepted delta left no root");
        const auto wire = rep.save_verified();
        auto again = typed_load<Root>(wire);
        need(bool(again), where + ": an accepted delta left a payload that does not verify");
        need(again->save_verified() == wire, where + ": accepted payload does not re-save byte-identically");
        need(dump(again->plain_root()) == dump(rep.plain_root()), where + ": accepted payload reads differently after a load");
    }
    return r;
}

// ---------------------------------------------------------------------------
// Senders: fill and mutate, per schema.
// ---------------------------------------------------------------------------
void fill(fz::Root& r, std::mt19937_64& rng) {
    r.epoch = rng();
    r.title = random_text(rng, 30);
    for (int i = int(rng() % 9); i > 0; --i) {
        auto& it = r.items.emplace_back();
        it.id = std::uint32_t(rng());
        it.flag = rng() & 1;
        it.name = random_text(rng, 30);
        for (int j = int(rng() % 7); j > 0; --j) it.vals.push_back(std::uint16_t(rng()));
    }
    for (int i = int(rng() % 5); i > 0; --i) r.tags.emplace_back(random_text(rng, 20), r.tags.get_stored_allocator());
    for (int i = int(rng() % 6); i > 0; --i) r.keys.insert(std::uint32_t(rng() % 1000));
    for (int i = int(rng() % 4); i > 0; --i)
        r.attrs.insert_or_assign(std::string_view(random_text(rng, 14)), std::uint64_t(rng()));
}

// Structure-keeping changes (deltas the receiver applies unchecked or with
// in-place checks), then sometimes structural ones.
void mutate(fz::Root& r, std::mt19937_64& rng, unsigned kind) {
    if (kind % 4 == 3) { mutate_valid(r, rng); return; }
    for (int n = 1 + int(rng() % 4); n > 0; --n) {
        switch (rng() % 8) {
        case 0: r.epoch = rng(); break;
        case 1: if (!r.items.empty()) r.items[rng() % r.items.size()].id = std::uint32_t(rng()); break;
        case 2: if (!r.items.empty()) { auto& it = r.items[rng() % r.items.size()]; it.flag = !it.flag; } break;
        case 3:
            if (!r.items.empty()) {
                auto& it = r.items[rng() % r.items.size()];
                if (!it.vals.empty()) it.vals[rng() % it.vals.size()] = std::uint16_t(rng());
            }
            break;
        case 4:
            if (!r.items.empty()) r.items[rng() % r.items.size()].name = random_text(rng, rng() % 2 ? 10 : 40);
            break;
        case 5: if (!r.attrs.empty()) r.attrs.begin()->second = rng(); break;
        case 6:
            if (rng() % 2) r.keys.insert(std::uint32_t(rng() % 1000));
            else if (!r.keys.empty()) r.keys.erase(*(r.keys.begin() + std::ptrdiff_t(rng() % r.keys.size())));
            break;
        case 7:
            if (!r.items.empty()) {
                auto& it = r.items[rng() % r.items.size()];
                if (rng() % 2 || it.vals.empty()) it.vals.push_back(std::uint16_t(rng()));
                else it.vals.pop_back();
            }
            break;
        }
    }
}

void fill_unit(dfz::Unit& u, std::mt19937_64& rng) {
    u.id = std::uint32_t(rng());
    u.owner = std::uint32_t(rng() % 8);
    u.x = std::int32_t(rng() % 2000);
    u.y = std::int32_t(rng() % 2000);
    u.hp = std::int32_t(rng() % 1000);
    u.state = 1 + std::uint32_t(rng() % 4);
    u.alive = rng() & 1;
    u.name = random_text(rng, rng() % 2 ? 10 : 30);
    for (int j = int(rng() % 4); j > 0; --j) u.buffs.push_back(std::uint16_t(rng()));
}

void fill(dfz::View& v, std::mt19937_64& rng) {
    v.tick = std::uint32_t(rng());
    if (rng() % 2) v.slots.reserve(4 + rng() % 12);
    for (int i = int(rng() % 10); i > 0; --i) fill_unit(v.slots.emplace_back(), rng);
    v.live = std::uint32_t(v.slots.size());
    for (int i = int(rng() % 5); i > 0; --i) v.watched.insert(std::uint32_t(rng() % 500));
    for (int i = int(rng() % 4); i > 0; --i) v.score.insert_or_assign(std::uint32_t(rng() % 50), std::int64_t(rng()));
}

void mutate(dfz::View& v, std::mt19937_64& rng, unsigned kind) {
    const bool structural = kind % 4 == 3;
    for (int n = 1 + int(rng() % 5); n > 0; --n) {
        switch (rng() % (structural ? 13 : 8)) {
        case 0: ++v.tick; break;
        case 1: if (!v.slots.empty()) { auto& u = v.slots[rng() % v.slots.size()]; u.x += 1 + int(rng() % 5); u.y -= 2; } break;
        case 2: if (!v.slots.empty()) v.slots[rng() % v.slots.size()].hp -= int(rng() % 40); break;
        case 3: if (!v.slots.empty()) { auto& u = v.slots[rng() % v.slots.size()]; u.alive = !u.alive; } break;
        case 4: if (!v.slots.empty()) v.slots[rng() % v.slots.size()].state = std::uint32_t(rng() % 5); break;
        case 5:
            if (!v.slots.empty()) {
                auto& b = v.slots[rng() % v.slots.size()].buffs;
                if (!b.empty()) b[rng() % b.size()] = std::uint16_t(rng());
            }
            break;
        case 6: if (!v.slots.empty()) v.slots[rng() % v.slots.size()].name = random_text(rng, 10); break;
        case 7: if (!v.score.empty()) v.score.begin()->second += 1; break;
        // Structural: records appended (within capacity or past it), erased
        // from the middle, popped; heap strings; set and map inserts.
        case 8: fill_unit(v.slots.emplace_back(), rng); ++v.live; break;
        case 9: if (!v.slots.empty()) { v.slots.erase(v.slots.begin() + std::ptrdiff_t(rng() % v.slots.size())); --v.live; } break;
        case 10: if (!v.slots.empty()) { v.slots.pop_back(); --v.live; } break;
        case 11: if (!v.slots.empty()) v.slots[rng() % v.slots.size()].name = random_text(rng, 40); break;
        case 12:
            if (rng() % 2) v.watched.insert(std::uint32_t(rng() % 500));
            else v.score.insert_or_assign(std::uint32_t(rng() % 50), std::int64_t(rng()));
            if (!v.slots.empty()) v.slots[rng() % v.slots.size()].buffs.push_back(std::uint16_t(rng()));
            break;
        }
    }
}

// ---------------------------------------------------------------------------
// Fixture: a sender, its baseline and a replica that holds the sender's
// state after a key delta and `warm` further deltas (so its map of checked
// bytes is built and has followed changes).
// ---------------------------------------------------------------------------
template <class Root>
struct Fixture {
    xo::TypedXBuffer<Root> src;
    xo::XDeltaBaseline base;
    xo::TypedXBuffer<Root> rep;
    xo::XDeltaCursor cur;
    std::mt19937_64 rng;

    Fixture(std::uint8_t seed, unsigned warm)
        : src(xo::TypedXBuffer<Root>::create(4096, xo::XBuffer::maximum_capacity(kLimit).with_growth(xo::XGrowPolicy::Automatic))),
          base(0x5eedull * 2 + 1 + seed * 2ull),
          rep(4096, xo::XBuffer::maximum_capacity(kLimit)),
          rng(0xde17a000ull + seed) {
        fill(src.plain_root(), rng);
        for (unsigned step = 0; step <= warm; ++step) {
            if (step) mutate(src.plain_root(), rng, unsigned(rng()));
            std::string d;
            const auto stats = src.save_delta_into(d, base);
            if (stats.empty) continue;
            auto r = rep.try_apply_delta_verified(d, cur);
            if (!r) fail("delta: fixture delta rejected: " + r.error().message);
        }
        if (payload_of(src) != payload_of(rep)) fail("delta: fixture replica differs from the sender");
    }
};

std::string varint(std::uint64_t v) {
    std::string out;
    for (; v >= 0x80; v >>= 7) out += char(0x80 | (v & 0x7F));
    return out + char(v);
}

// Header of a delta to the cursor's next version, flags from the input. With
// EXTENT, used and root come from the input (often the replica's own).
template <class Root>
std::string header_for(Fixture<Root>& f, In& in, std::uint8_t flags, bool key) {
    std::string h = {'X', 'D', char(0x10), char(flags)};
    std::uint64_t stream = f.cur.stream();
    std::uint32_t version = f.cur.version() + 1;
    const std::uint8_t tweak = in.u8();
    if (tweak == 0xff) stream = in.u64();          // another stream
    if (tweak == 0xfe) version = in.u16();         // another version
    h.append(reinterpret_cast<const char*>(&stream), 8);
    h += varint(version);
    if (flags & 8u) {
        const auto* sm = f.rep.arena();
        std::uint64_t used = sm ? (sm->used_end - 48) / 8 : 0;
        std::uint64_t root = sm ? sm->root_offset - 48 : 0;
        const std::uint8_t e = in.u8();
        if (e & 1) used = in.u16();
        else if (e & 2) used += in.u8();
        if (e & 4) root = std::uint64_t(in.u16()) * 8;
        h += varint(used);
        h += varint(root);
    }
    if (key) {
        std::uint64_t hash = xo::detail::wire_schema_hash<Root>(), type = xo::detail::wire_root_type_id<Root>();
        if (in.u8() == 0xff) hash ^= 1;
        h.append(reinterpret_cast<const char*>(&hash), 8);
        h.append(reinterpret_cast<const char*>(&type), 8);
    }
    return h;
}

template <class Root>
void run_raw(std::uint8_t seed, In& in) {
    Fixture<Root> f(seed, in.u8() % 4);
    const std::uint8_t flags = in.u8() & 0x0e; // HOLES, DIGEST, EXTENT; not KEY
    std::string d = header_for(f, in, flags, false);
    if (flags & 4u) { // DIGEST: the input's, or the right one for an unchanged payload
        std::uint64_t dg = in.u8() == 0 ? in.u64() : 0;
        d.append(reinterpret_cast<const char*>(&dg), 8);
    }
    d += varint(in.u8() % 8);                      // block count
    d += in.rest();                                // block and hole sections
    apply_checked(f.rep, d, f.cur, "raw");
}

template <class Root>
void run_key(std::uint8_t seed, In& in) {
    Fixture<Root> f(seed, in.u8() % 2);
    const std::uint8_t flags = std::uint8_t(1u | 2u | 8u | (in.u8() & 4u));
    std::string d = header_for(f, in, flags, true);
    if (flags & 4u) { std::uint64_t dg = in.u64(); d.append(reinterpret_cast<const char*>(&dg), 8); }
    d += varint(in.u8() % 16);
    d += in.rest();
    apply_checked(f.rep, d, f.cur, "key onto replica");
    xo::TypedXBuffer<Root> empty(4096, xo::XBuffer::maximum_capacity(kLimit));
    xo::XDeltaCursor ec;
    apply_checked(empty, d, ec, "key onto empty");
}

template <class Root>
void run_bare(std::uint8_t seed, In& in) {
    Fixture<Root> f(seed, in.u8() % 3);
    const std::string d = in.rest();
    apply_checked(f.rep, d, f.cur, "bare onto replica");
    xo::TypedXBuffer<Root> empty(4096, xo::XBuffer::maximum_capacity(kLimit));
    xo::XDeltaCursor ec;
    apply_checked(empty, d, ec, "bare onto empty");
}

template <class Root>
void run_stream(std::uint8_t seed, In& in) {
    Fixture<Root> f(seed, in.u8() % 3);
    for (int step = 0; step < 24 && in.left(); ++step) {
        const std::string where = "stream step " + std::to_string(step);
        const std::uint8_t op = in.u8();
        f.rng.seed(0x51e9ull * (step + 1) + in.u16());
        if (op % 16 == 0) f.base.reset();
        else if (op % 16 == 1) { f.src.reset(); fill(f.src.plain_root(), f.rng); }
        else if (op % 16 != 2) mutate(f.src.plain_root(), f.rng, op >> 4);
        xo::XDeltaOptions o;
        o.digest = op & 0x20;
        o.whole_blocks = (op & 0xc0) == 0xc0;
        std::string d;
        const auto stats = f.src.save_delta_into(d, f.base, o);
        if (stats.empty) {
            need(payload_of(f.src) == payload_of(f.rep), where + ": empty delta although the payload changed");
            continue;
        }
        // Corruptions chosen by the input, each applied to the replica first
        // (with a copy of its cursor, so the same check path is taken).
        for (int k = in.u8() % 4; k > 0 && !d.empty(); --k) {
            std::string bad = d;
            const std::uint8_t how = in.u8();
            const std::size_t pos = in.u16() % bad.size();
            switch (how % 5) {
            case 0: bad[pos] ^= char(1u << (how >> 5)); break;
            case 1: bad[pos] = char(in.u8()); break;
            case 2: bad.resize(pos); break;
            case 3: bad.insert(pos, in.take(1 + how % 8)); break;
            case 4: bad.erase(pos, 1 + how % 8); break;
            }
            if (bad == d) continue;
            auto saved = f.rep.clone();
            auto tc = f.cur;
            auto r = apply_checked(f.rep, bad, tc, where + " corrupted");
            if (r) {
                if (o.digest && !stats.key) need(payload_of(f.rep) == payload_of(f.src), where + ": a digest delta accepted another result");
                f.rep = std::move(saved); // its next delta is verified in full
            } else if (!has_state(f.rep)) {
                f.rep = std::move(saved); // rejected key delta: replica emptied
            }
        }
        auto r = apply_checked(f.rep, d, f.cur, where);
        need(bool(r), where + ": a real delta was rejected: " + (r ? std::string() : r.error().message));
        need(payload_of(f.src) == payload_of(f.rep), where + ": replica payload differs from the sender's");
        need(holes_of(f.src) == holes_of(f.rep), where + ": replica hole set differs from the sender's");
        need(dump(f.src.plain_root()) == dump(f.rep.plain_root()), where + ": replica contents differ");
        need(f.cur.version() == f.base.version() && f.cur.stream() == f.base.stream(), where + ": cursor does not follow");
    }
}

template <class Root>
void run(std::uint8_t mode, std::uint8_t seed, In& in) {
    switch (mode) {
    case 0: run_raw<Root>(seed, in); break;
    case 1: run_stream<Root>(seed, in); break;
    case 2: run_bare<Root>(seed, in); break;
    default: run_key<Root>(seed, in); break;
    }
}
} // namespace

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    inject_crash_drill(data, size);
    if (size < 2) return 0;
    In in{data, size};
    const std::uint8_t sel = in.u8();
    const std::uint8_t seed = in.u8() % 32;
    if (sel & 1) run<dfz::View>((sel >> 1) & 3, seed, in);
    else run<fz::Root>((sel >> 1) & 3, seed, in);
    return 0;
}

// Seeds: for each schema and mode a few inputs, plus every mode's real
// deltas written as bare inputs (mode 2), so the fuzzer starts from valid
// block and hole sections.
extern "C" int XOffsetFuzzWriteSeeds(const char* dir) {
    std::filesystem::create_directories(dir);
    std::size_t count = 0;
    auto put = [&](const std::string& bytes) {
        const std::string path = std::string(dir) + "/seed-" + std::to_string(count++);
        std::FILE* f = std::fopen(path.c_str(), "wb");
        if (!f || std::fwrite(bytes.data(), 1, bytes.size(), f) != bytes.size() || std::fclose(f) != 0) {
            std::cerr << "fuzz_delta_sync: cannot write " << path << "\n";
            std::exit(2);
        }
    };
    std::mt19937_64 rng(20261104);
    for (int schema = 0; schema < 2; ++schema) {
        for (int mode = 0; mode < 4; ++mode) {
            for (int k = 0; k < 6; ++k) {
                std::string s = {char(schema | mode << 1), char(k * 5)};
                const int len = 8 + int(rng() % 120);
                for (int i = 0; i < len; ++i) s += char(rng());
                put(s);
            }
        }
        // Bare real deltas: the fixture's next delta, written whole.
        for (std::uint8_t seed = 0; seed < 8; ++seed) {
            auto bare = [&](auto tag) {
                using Root = typename decltype(tag)::type;
                Fixture<Root> f(seed, 1);
                mutate(f.src.plain_root(), f.rng, seed);
                xo::XDeltaOptions o;
                o.digest = seed & 1;
                o.whole_blocks = seed & 2;
                std::string d;
                (void)f.src.save_delta_into(d, f.base, o);
                if (d.empty()) return;
                // Mode 2 rebuilds the fixture with warm = in.u8() % 3: the
                // byte 1 (warm 1) matches the fixture above.
                put(std::string{char(schema | 2 << 1), char(seed), char(1)} + d);
            };
            if (schema) bare(std::type_identity<dfz::View>{}); else bare(std::type_identity<fz::Root>{});
        }
    }
    std::cout << "fuzz_delta_sync: wrote " << count << " seeds to " << dir << "\n";
    return 0;
}
