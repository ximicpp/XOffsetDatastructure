// Shared deltas through the receiver (XReceiver::apply_sync, XS 1.0,
// docs/SHARED_DELTA.md): a server writes a stream of messages (units enter,
// change and leave; the head and fixed cells change) and two clients apply
// each one, with XCheck::full and with XCheck::admit. Both hold exactly the
// server's records after every message and their buffers pass load_verified.
// Corrupted copies (bits flipped, cut, extended, two messages spliced) go
// to the full client first: rejected, they change nothing (records and
// cursor); accepted, admit on a copy gives the same result. Then the rules:
// no key yet or a version gap (delta_base_mismatch), header errors, unknown
// tables, kinds a table does not take, ids not held or already held, an
// entry twice in a message, bytes after the last op, and the cursor after a
// partly applied admit.
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

// A message with the header the stream's next delta has and ops from text.
std::string next_delta(const Replica& r, std::uint32_t count, std::string_view ops) {
    xs::XSyncOut out;
    xs::put_header(out, xs::XSyncHeader{0, 0, r.cur.version() + 1, count});
    std::string m(out.view());
    m.append(ops);
    return m;
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
    // Header errors.
    expect(apply_full(r, std::string_view()), XErrc::truncated, "empty message", -1);
    expect(apply_full(r, "XS\x01"), XErrc::truncated, "short header", -1);
    expect(apply_full(r, std::string_view("XT\x01\x00\x01\x00\x00\x00", 8)), XErrc::bad_magic, "magic", -1);
    expect(apply_full(r, std::string_view("XS\x02\x00\x01\x00\x00\x00", 8)), XErrc::unsupported_version, "format byte", -1);
    expect(apply_admit(r, std::string_view("XS\x01\x02\x01\x00\x00\x00", 8)), XErrc::unsupported_flags, "flags", -1);
    expect(apply_full(r, std::string_view("XS\x01\x01\x00\x00", 6)), XErrc::truncated, "key without its stream", -1);
    const std::uint32_t v = r.cur.version();
    // An empty delta applies and advances the version.
    expect(apply_full(r, next_delta(r, 0, "")), XErrc::ok, "empty delta", -1);
    if (r.cur.version() != v + 1) fail("empty delta version", -1);
    s.version = r.cur.version();
    // Op errors, for both tiers; none of them changes anything.
    const std::uint32_t held = s.ids.empty() ? 0 : s.ids[0];
    if (s.ids.empty()) fail("model has no units", -1);
    std::string held_id;
    {
        char b[5];
        held_id.assign(b, xs::encode_varint(b, held));
    }
    struct Bad { std::uint32_t count; std::string ops; XErrc code; const char* what; };
    const std::string leave_held = std::string(1, char(kUnit << 2 | xs::leave)) + held_id;
    const std::string empty_body = std::string(1, '\0') + std::string(1, '\0');   // no blocks, no containers
    const Bad bad[] = {
        {1, std::string("\x0c\x00", 2), XErrc::invalid_payload, "unknown table"},
        {1, std::string(1, char(kCell << 2 | xs::leave)) + std::string(1, '\0'), XErrc::invalid_payload, "leave on a fixed table"},
        {1, std::string(1, char(kCell << 2 | 3)) + std::string(1, '\0'), XErrc::invalid_payload, "kind 3"},
        {1, std::string(1, char(kCell << 2 | xs::change)) + "\x09" + std::string(1, '\0'), XErrc::invalid_payload, "cell not held"},
        {1, std::string(1, char(kHead << 2 | xs::change)) + "\x01" + std::string(1, '\0'), XErrc::invalid_payload, "head id 1"},
        {1, std::string(1, char(kUnit << 2 | xs::change)) + "\x7f" + empty_body, XErrc::invalid_payload, "change of a unit not held"},
        {1, std::string(1, char(kUnit << 2 | xs::leave)) + "\x7f", XErrc::invalid_payload, "leave of a unit not held"},
        {1, std::string(1, char(kUnit << 2 | xs::enter)) + held_id + empty_body, XErrc::invalid_payload, "enter of a unit held"},
        {2, leave_held + leave_held, XErrc::invalid_payload, "a unit twice"},
        {1, leave_held + std::string(1, '\0'), XErrc::invalid_payload, "bytes after the last op"},
        {2, leave_held, XErrc::truncated, "fewer ops than counted"},
    };
    for (const Bad& b : bad) {
        const std::string m = next_delta(r, b.count, b.ops);
        Replica before(r);
        expect(apply_full(r, m), b.code, b.what, -1);
        if (!same_state(r, before)) fail("a rejected message changed the full client", -1);
        Replica twin(r);
        const xo::XStatus a = apply_admit(twin, m);
        if (a) fail("admit accepted a bad message", -1);
    }
    // admit applies op by op: an op it rejects after others applied leaves
    // the cursor empty (only a key applies next).
    {
        const std::string m = next_delta(r, 2, leave_held + std::string(1, char(kUnit << 2 | xs::leave)) + "\x7f");
        Replica twin(r);
        expect(apply_admit(twin, m), XErrc::invalid_payload, "partly applied admit", -1);
        if (!twin.cur.empty()) fail("cursor after a partly applied admit", -1);
        expect(apply_admit(twin, next_delta(r, 0, "")), XErrc::delta_base_mismatch, "delta after a partly applied admit", -1);
        // full on the same message changes nothing.
        Replica before(r);
        expect(apply_full(r, m), XErrc::invalid_payload, "full on the same message", -1);
        if (!same_state(r, before)) fail("full changed the client", -1);
    }
    // Entries twice: messages of up to 64 ops look them up in a filter
    // (an op whose bit is set is compared with the ops before it), longer
    // ones in the cursor's table.
    {
        auto key_msg = [&](const std::vector<std::uint32_t>& ids) {
            xs::XSyncOut out;
            xs::put_header(out, xs::XSyncHeader{xs::kFlagKey, 0x77, 1, std::uint32_t(ids.size())});
            std::string m(out.view());
            for (const std::uint32_t id : ids) {
                char b[5];
                m += char(kUnit << 2 | xs::enter);
                m.append(b, xs::encode_varint(b, id));
                m += empty_body;
            }
            return m;
        };
        std::vector<std::uint32_t> many;
        for (std::uint32_t i = 0; i < 70; ++i) many.push_back(1000 + i);
        Replica k;
        expect(apply_full(k, key_msg(many)), XErrc::ok, "70 entries", -1);
        if (k.t.idx.size() != 70) fail("70 entries held", -1);
        many.push_back(1035);
        Replica k2, fresh;
        expect(apply_full(k2, key_msg(many)), XErrc::invalid_payload, "an entry twice in 71 ops", -1);
        if (!same_state(k2, fresh)) fail("71 ops with an entry twice changed the client", -1);
        // Two entries on the same filter bit are two entries.
        auto bit = [](std::uint32_t id) { return unsigned((((std::uint64_t(kUnit) << 32) | id) * 0x9E3779B97F4A7C15ull) >> 56); };
        std::uint32_t a = 1, b = 2;
        while (bit(b) != bit(a)) ++b;
        Replica k3;
        expect(apply_full(k3, key_msg({a, b})), XErrc::ok, "two entries on one filter bit", -1);
        if (k3.t.idx.size() != 2) fail("two entries on one filter bit held", -1);
        Replica k4;
        expect(apply_full(k4, key_msg({a, b, a})), XErrc::invalid_payload, "an entry twice behind a filter hit", -1);
        if (!same_state(k4, fresh)) fail("an entry twice behind a filter hit changed the client", -1);
    }
    // A record body that does not check: a VSET of more elements than bytes.
    {
        std::string ops(1, char(kUnit << 2 | xs::change));
        ops += held_id;
        ops += std::string(1, '\0');               // no blocks
        ops += std::string(1, '\x01');             // one container op
        ops += std::string(1, char(1 << 2 | 1));   // container 1 (buffs), VSET
        ops += std::string(1, '\x7f');             // 127 elements, no bytes
        Replica before(r);
        expect(apply_full(r, next_delta(r, 1, ops)), XErrc::invalid_payload, "body that does not check", -1);
        if (!same_state(r, before)) fail("a rejected body changed the client", -1);
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
