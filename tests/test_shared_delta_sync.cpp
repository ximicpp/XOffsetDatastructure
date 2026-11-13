// Shared deltas through the receiver (XReceiver::apply_sync, XS 2.0,
// docs/SHARED_DELTA.md): a server writes a stream of messages (units enter,
// change and leave; the head and fixed cells change) and two clients apply
// each one, with XCheck::full and with XCheck::admit. Both hold exactly the
// server's records after every message and their buffers pass load_verified.
// Corrupted copies (bits flipped, cut, extended, two messages spliced) go
// to the full client first: rejected, they change nothing (records and
// cursor); accepted, admit on a copy gives the same result. Then the rules:
// no key yet or a version gap (delta_base_mismatch), head errors, sections
// on the root or on unknown tables, kinds a table does not take, ids not held
// or already held, an entry twice in a message, sections cut short, bodies
// that do not check, and the cursor after a partly applied admit.
//
// Built with XOFFSET_FORCE_FULL_CHECK=0 so admit is the real admit path. No
// exception of its own: also built as test_shared_delta_sync_noexc.
#define XOFFSET_FORCE_FULL_CHECK 0
#include "fuzz/shared_delta_model.hpp"

#include <cstdio>
#include <cstdlib>
#include <random>
#include <string>
#include <vector>

namespace {
using namespace sdm;
using xo::XErrc;

[[noreturn]] void fail(const char* what, int step) {
    std::fprintf(stderr, "test_shared_delta_sync: %s (step %d)\n", what, step);
    std::exit(1);
}

void expect(const xo::XStatus& r, XErrc code, const char* what, int step) {
    if (r.code() != code) {
        std::fprintf(stderr, "test_shared_delta_sync: %s: got %s (%s), want %s\n", what, xo::to_string(r.code()),
                     r ? "" : r.error().message.c_str(), xo::to_string(code));
        fail(what, step);
    }
}

std::string u32(std::uint32_t v) { return std::string(reinterpret_cast<const char*>(&v), 4); }
std::string section(unsigned table, unsigned kind, std::uint32_t count) { return u32(count << 8 | table << 2 | kind); }
// The head record's whole body.
std::string root(const Head& h) {
    xs::XSyncOut o;
    xs::put_whole_op(o, 0, h);
    return std::string(o.view().substr(4));
}
// A message with the head and root the stream's next delta has, then sections from text.
std::string next_delta(const Replica& r, std::string_view sections, const Head& h = Head{}) {
    return std::string("XS\x02\x00", 4) + u32(r.cur.version() + 1) + root(h) + std::string(sections);
}
std::string key_head(std::uint64_t stream, std::uint32_t version) {
    return std::string("XS\x02\x01", 4) + u32(version) + std::string(reinterpret_cast<const char*>(&stream), 8) + root(Head{});
}
// Unit u's whole body and the change body of the members in mask.
std::string whole(const Unit& u) {
    xs::XSyncOut o;
    xs::put_whole_op(o, 0, u);
    return std::string(o.view().substr(4));
}
std::string change(const Unit& u, std::uint64_t mask) {
    xs::XSyncOut o;
    xs::put_change_op(o, 0, u, mask);
    return std::string(o.view().substr(4));
}

void rules() {
    Server s;
    std::mt19937 rng(7);
    auto pick = [&](unsigned n) { return unsigned(rng() % n); };
    for (int i = 0; i < 20; ++i) s.step(pick);
    Replica r;
    // No key applied yet.
    s.delta();
    expect(apply_full(r, s.msg.view()), XErrc::delta_base_mismatch, "delta before a key (full)", -1);
    expect(apply_admit(r, s.msg.view()), XErrc::delta_base_mismatch, "delta before a key (admit)", -1);
    if (xs::is_key(s.msg.view())) fail("is_key on a delta", -1);
    std::string key(s.key());
    if (!xs::is_key(key)) fail("is_key on a key", -1);
    expect(apply_full(r, key), XErrc::ok, "key", -1);
    if (r.cur.empty() || r.cur.version() != s.version || r.cur.stream() != s.stream) fail("cursor after a key", -1);
    if (!s.matches(r)) fail("key result", -1);
    // A gap: skip one message.
    s.step(pick);
    s.delta();
    s.step(pick);
    std::string next(s.delta());
    expect(apply_full(r, next), XErrc::delta_base_mismatch, "version gap (full)", -1);
    expect(apply_admit(r, next), XErrc::delta_base_mismatch, "version gap (admit)", -1);
    if (r.cur.empty()) fail("a gap emptied the cursor", -1);
    // Recover with a key on a reset view.
    r.reset();
    expect(apply_admit(r, s.key()), XErrc::ok, "recovery key", -1);
    if (!s.matches(r)) fail("recovery result", -1);
    // Head errors.
    expect(apply_full(r, std::string_view()), XErrc::truncated, "empty message", -1);
    expect(apply_full(r, "XS\x02"), XErrc::truncated, "short head", -1);
    expect(apply_full(r, std::string_view("XT\x02\x00\x01\x00\x00\x00", 8)), XErrc::bad_magic, "magic", -1);
    expect(apply_full(r, std::string_view("XS\x01\x00\x01\x00\x00\x00", 8)), XErrc::unsupported_version, "format byte (XS 1.0)", -1);
    expect(apply_admit(r, std::string_view("XS\x02\x02\x01\x00\x00\x00", 8)), XErrc::unsupported_flags, "flags", -1);
    expect(apply_full(r, std::string_view("XS\x02\x01\x01\x00\x00\x00\x00\x00", 10)), XErrc::truncated, "key without its stream", -1);
    expect(apply_full(r, next_delta(r, "").substr(0, 12)), XErrc::truncated, "delta cut inside its root", -1);
    expect(apply_admit(r, key_head(9, 1).substr(0, 20)), XErrc::truncated, "key cut inside its root", -1);
    const std::uint32_t v = r.cur.version();
    // A delta of the root only applies and advances the version.
    expect(apply_full(r, next_delta(r, "", Head{77, 5, true})), XErrc::ok, "root-only delta", -1);
    if (r.cur.version() != v + 1 || r.t.view().head.tick != 77 || !r.t.view().head.paused) fail("root-only delta result", -1);
    // An empty section applies too.
    expect(apply_admit(r, next_delta(r, section(kUnit, xs::change, 0))), XErrc::ok, "empty section", -1);
    s.version = r.cur.version();
    s.v().head = r.t.view().head;
    // Op errors, for both tiers; none of them changes anything under full.
    if (s.ids.empty()) fail("model has no units", -1);
    const std::uint32_t held = s.ids[0];
    const Unit& hu = s.v().units[0];
    const std::string leave_held = section(kUnit, xs::leave, 1) + u32(held);
    struct Bad { std::string sections; XErrc code; const char* what; bool admit_rejects; };
    const Bad bad[] = {
        {section(5, xs::change, 1) + u32(0) + std::string(1, '\0'), XErrc::invalid_payload, "unknown table", true},
        {section(kHead, xs::change, 1) + u32(0) + std::string(1, '\0'), XErrc::invalid_payload, "an op on the root", true},
        {section(kCell, xs::leave, 1) + u32(0), XErrc::invalid_payload, "leave on a fixed table", true},
        {section(kCell, 3, 1) + u32(0), XErrc::invalid_payload, "kind 3", true},
        {section(kUnit, 3, 1) + u32(held), XErrc::invalid_payload, "kind 3 on a keyed table", true},
        {section(kCell, xs::change, 1) + u32(9) + std::string(1, '\0'), XErrc::invalid_payload, "cell not held", true},
        {section(kUnit, xs::change, 1) + u32(0x7f) + change(hu, 1), XErrc::invalid_payload, "change of a unit not held", true},
        {section(kUnit, xs::leave, 1) + u32(0x7f), XErrc::invalid_payload, "leave of a unit not held", true},
        {section(kUnit, xs::enter, 1) + u32(held) + whole(hu), XErrc::invalid_payload, "enter of a unit held", true},
        {section(kUnit, xs::leave, 2) + u32(held) + u32(held), XErrc::invalid_payload, "a unit twice in a section", true},
        {leave_held + section(kUnit, xs::change, 1) + u32(held) + change(hu, 1), XErrc::invalid_payload, "a unit in two sections", true},
        {section(kUnit, xs::leave, 2) + u32(held), XErrc::truncated, "fewer ops than counted", true},
        {leave_held + std::string(3, '\0'), XErrc::truncated, "bytes after the last section", true},
        {section(kUnit, xs::change, 1) + u32(held) + std::string("\x00\x02", 2), XErrc::invalid_payload, "member mask past the members", false},
        {section(kCell, xs::change, 1) + u32(0) + std::string("\x04\x02", 2), XErrc::invalid_payload, "a bool of 2 (plain member)", false},
        {section(kCell, xs::enter, 1) + u32(0) + std::string(12, '\0') + std::string(1, '\x03'), XErrc::invalid_payload, "a bool of 3 (whole body)", false},
        {section(kUnit, xs::change, 1) + u32(held) + std::string("\x10\x00", 2) + u32(1000) + "abc", XErrc::invalid_payload, "string past the end", false},
        {section(kUnit, xs::change, 1) + u32(held) + std::string("\x20\x00", 2) + u32(3) + "abcd", XErrc::invalid_payload, "vector past the end", false},
        {section(kCell, xs::change, 1) + u32(0) + std::string(1, '\x01'), XErrc::invalid_payload, "member cut short", false},
    };
    for (const Bad& b : bad) {
        const std::string m = next_delta(r, b.sections);
        Replica before(r);
        expect(apply_full(r, m), b.code, b.what, -1);
        if (!same_state(r, before)) fail("a rejected message changed the full client", -1);
        if (b.admit_rejects) {
            Replica twin(r);
            if (apply_admit(twin, m)) fail("admit accepted a bad message", -1);
        }
    }
    // A root bool that is not 0 or 1: full rejects it.
    {
        std::string m = next_delta(r, "");
        m[8 + 12] = 2;   // Head::paused, after tick and seq
        Replica before(r);
        expect(apply_full(r, m), XErrc::invalid_payload, "root bool of 2", -1);
        if (!same_state(r, before)) fail("a rejected root changed the full client", -1);
    }
    // admit applies op by op: an op it rejects after others applied leaves
    // the cursor empty (only a key applies next).
    {
        const std::string m = next_delta(r, section(kUnit, xs::leave, 2) + u32(held) + u32(0x7f));
        Replica twin(r);
        expect(apply_admit(twin, m), XErrc::invalid_payload, "partly applied admit", -1);
        if (!twin.cur.empty()) fail("cursor after a partly applied admit", -1);
        expect(apply_admit(twin, next_delta(r, "")), XErrc::delta_base_mismatch, "delta after a partly applied admit", -1);
        // full on the same message changes nothing.
        Replica before(r);
        expect(apply_full(r, m), XErrc::invalid_payload, "full on the same message", -1);
        if (!same_state(r, before)) fail("full changed the client", -1);
    }
    // Entries twice: messages of up to 64 ops look them up in a filter
    // (an op whose bit is set is compared with the ops before it), longer
    // ones in the cursor's table.
    {
        const std::string empty_unit(xs::XRecordBody<Unit>::whole_body::fixed, '\0');   // plain bytes and container mask zero
        auto key_msg = [&](const std::vector<std::uint32_t>& ids, std::size_t split) {
            std::string m = key_head(0x77, 1);
            // Two sections (the second from split on): the check spans them.
            for (std::size_t a = 0; a < ids.size();) {
                const std::size_t b = a < split ? std::min(split, ids.size()) : ids.size();
                m += section(kUnit, xs::enter, std::uint32_t(b - a));
                for (std::size_t i = a; i < b; ++i) m += u32(ids[i]) + empty_unit;
                a = b;
            }
            return m;
        };
        std::vector<std::uint32_t> many;
        for (std::uint32_t i = 0; i < 300; ++i) many.push_back(1000 + i * 7);
        Replica k;
        expect(apply_full(k, key_msg(many, 40)), XErrc::ok, "300 entries", -1);
        if (k.t.idx.size() != 300) fail("300 entries held", -1);
        Replica fresh;
        for (const std::size_t at : {std::size_t(10), std::size_t(64), std::size_t(65), std::size_t(299)}) {
            std::vector<std::uint32_t> twice = many;
            twice.insert(twice.begin() + std::ptrdiff_t(at), many[at / 2]);
            Replica k2;
            expect(apply_full(k2, key_msg(twice, 40)), XErrc::invalid_payload, "an entry twice", -1);
            if (!same_state(k2, fresh)) fail("a message with an entry twice changed the client", -1);
        }
        // Two entries on the same filter bit are two entries.
        auto bit = [](std::uint32_t id) { return unsigned((((std::uint64_t(kUnit) << 32) | id) * 0x9E3779B97F4A7C15ull) >> 56); };
        std::uint32_t a = 1, b = 2;
        while (bit(b) != bit(a)) ++b;
        Replica k3;
        expect(apply_full(k3, key_msg({a, b}, 1)), XErrc::ok, "two entries on one filter bit", -1);
        if (k3.t.idx.size() != 2) fail("two entries on one filter bit held", -1);
        Replica k4;
        expect(apply_full(k4, key_msg({a, b, a}, 2)), XErrc::invalid_payload, "an entry twice behind a filter hit", -1);
        if (!same_state(k4, fresh)) fail("an entry twice behind a filter hit changed the client", -1);
        // The same id in two tables is two entries.
        Replica k5;
        expect(apply_full(k5, key_head(0x77, 1) + section(kUnit, xs::enter, 1) + u32(1) + empty_unit +
                                  section(kCell, xs::change, 1) + u32(1) + std::string(1, '\0')),
               XErrc::ok, "id 1 in two tables", -1);
    }
}

std::string corrupt(std::string m, std::mt19937& rng, const std::string& other) {
    switch (rng() % 4) {
    case 0: for (unsigned i = 0, n = 1 + unsigned(rng() % 3); i < n; ++i) m[4 + rng() % (m.size() - 4)] ^= char(1u << (rng() % 8)); break;
    case 1: m.resize(4 + rng() % (m.size() - 4)); break;
    case 2: m.push_back(char(rng())); break;
    default: if (other.size() > 8 && m.size() > 8) m = m.substr(0, 8 + rng() % (m.size() - 8)) + other.substr(8 + rng() % (other.size() - 8)); break;
    }
    return m;
}
} // namespace

int main() {
    rules();
    std::mt19937 rng(20261110);
    auto pick = [&](unsigned n) { return unsigned(rng() % n); };
    Server s;
    for (int i = 0; i < 30; ++i) s.step(pick);
    Replica full(16), admit(200);
    const std::string key(s.key());
    expect(apply_full(full, key), XErrc::ok, "key (full)", 0);
    expect(apply_admit(admit, key), XErrc::ok, "key (admit)", 0);
    if (!s.matches(full) || !s.matches(admit)) fail("key result", 0);
    std::size_t messages = 0, bytes = 0, rejected = 0, accepted_corrupt = 0, keys = 1;
    std::string prev;
    for (int step = 1; step <= 1500; ++step) {
        for (unsigned i = 0, n = pick(4); i < n; ++i) s.step(pick);
        if (pick(97) == 0) {
            // A key on reset views.
            const std::string k(s.key());
            full.reset();
            admit.reset();
            expect(apply_full(full, k), XErrc::ok, "key (full)", step);
            expect(apply_admit(admit, k), XErrc::ok, "key (admit)", step);
            ++keys;
        } else {
            const std::string m(s.delta());
            ++messages;
            bytes += m.size();
            if (pick(3) == 0) {
                const std::string bad = corrupt(m, rng, prev);
                Replica before(full);
                const xo::XStatus r = apply_full(full, bad);
                if (!r) {
                    ++rejected;
                    if (!same_state(full, before)) fail("a rejected message changed the full client", step);
                } else {
                    // Accepted (bytes that still check): admit gives the same, and the buffer verifies.
                    ++accepted_corrupt;
                    Replica twin(before);
                    expect(apply_admit(twin, bad), XErrc::ok, "admit of a message full accepts", step);
                    if (!same_state(full, twin)) fail("admit and full differ", step);
                    if (!full.verifies()) fail("an accepted corrupt message left a buffer that does not verify", step);
                    // Back to the server's state.
                    full.reset();
                    expect(apply_full(full, s.key()), XErrc::ok, "key after an accepted corrupt message", step);
                    admit.reset();
                    expect(apply_admit(admit, s.msg.view()), XErrc::ok, "key (admit)", step);
                    ++keys;
                    prev = m;
                    continue;
                }
            }
            expect(apply_full(full, m), XErrc::ok, "delta (full)", step);
            expect(apply_admit(admit, m), XErrc::ok, "delta (admit)", step);
            prev = m;
        }
        if (!s.matches(full)) fail("full client differs from the server", step);
        if (!s.matches(admit)) fail("admit client differs from the server", step);
        if (step % 100 == 0 && (!full.verifies() || !admit.verifies())) fail("a client buffer does not verify", step);
    }
    if (!full.verifies() || !admit.verifies()) fail("a client buffer does not verify at the end", -1);
    std::printf("test_shared_delta_sync: %zu deltas (%zu bytes, %.1f per message), %zu keys; corrupt: %zu rejected, %zu accepted\n",
                messages, bytes, messages ? double(bytes) / double(messages) : 0.0, keys, rejected, accepted_corrupt);
    return 0;
}
