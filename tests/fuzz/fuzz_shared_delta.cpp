// libFuzzer target for the shared delta receiver (XS 1.0,
// xoffset_shared_delta.hpp, docs/SHARED_DELTA.md):
// XReceiver<from_server>::apply_sync on a client view that holds a server's
// records (tests/fuzz/shared_delta_model.hpp), the entry a client feeds with
// bytes from the network.
//
// Every message goes through apply_checked(), the differential of the two
// tiers:
//   - XCheck::full on the replica. A message it rejects leaves the records
//     and the cursor exactly as they were; one it accepts leaves a buffer
//     that load_verified accepts.
//   - XCheck::admit on a copy of the replica as it was, for every message
//     full accepts: it accepts too and leaves the same records and cursor.
//     admit trusts bodies, so it never sees a message full rejects.
//
// The first input byte picks the mode (bits 0-1), the second the replica's
// state (a server built from it, its key message applied):
//   0 raw: the rest is the op section of the stream's next delta, behind a
//     header the harness writes (count: the third byte), so the input
//     passes the header, stream and version checks.
//   1 stream: the input drives the server (units enter, change and leave;
//     cells and the head change); each real delta is applied after up to a
//     few input-chosen corruptions of it are applied first. Real deltas
//     must apply with both tiers and leave the server's records.
//   2 bare: the rest is a whole message (header included).
//   3 key: the rest is the op section of a key message, applied to the
//     replica after it empties its units.
//
// Build: -DXOFFSET_BUILD_FUZZERS=ON (libFuzzer); fuzz_shared_delta_replay is
// the same file linked with replay_main.cpp (any compiler).
#define XOFFSET_FORCE_FULL_CHECK 0
#include "shared_delta_model.hpp"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <random>
#include <string>

namespace {
using namespace sdm;

[[noreturn]] void fail(const std::string& what) {
    std::cerr << "fuzz_shared_delta: " << what << "\n";
    std::abort();
}
void need(bool ok, const char* what) { if (!ok) fail(what); }

// Drill builds only (CMake -DXOFFSET_FUZZ_INJECT_CRASH=ON): an input starting
// with XOFFSET-INJECT crashes the harness.
void inject_crash_drill([[maybe_unused]] const std::uint8_t* data, [[maybe_unused]] std::size_t size) {
#ifdef XOFFSET_FUZZ_INJECT_CRASH
    constexpr char marker[] = "XOFFSET-INJECT";
    if (size >= sizeof(marker) - 1 && std::memcmp(data, marker, sizeof(marker) - 1) == 0) {
        std::cerr << "XOFFSET_FUZZ_INJECT_CRASH: injected crash (drill input)\n";
        std::abort();
    }
#endif
}

// Input reader: bytes past the end read as 0.
struct In {
    const std::uint8_t* p;
    std::size_t n;
    std::size_t at = 0;
    std::uint8_t u8() { return at < n ? p[at++] : 0; }
    std::size_t left() const { return at < n ? n - at : 0; }
    std::string rest() { std::string s(reinterpret_cast<const char*>(p + at), left()); at = n; return s; }
};

// XOFFSET_FUZZ_STATS=1: at exit, messages by outcome (accepted, rejected
// by code), to see that a corpus gets past the checks.
struct Stats {
    std::size_t accepted = 0, rejected[256] = {};
    ~Stats() {
        const char* v = std::getenv("XOFFSET_FUZZ_STATS");
        if (!v || !*v) return;
        std::cerr << "fuzz_shared_delta: accepted " << accepted;
        for (unsigned c = 0; c < 256; ++c)
            if (rejected[c]) std::cerr << ", " << xo::to_string(xo::XErrc(c)) << " " << rejected[c];
        std::cerr << "\n";
    }
} g_stats;

// Applies m to r with the full check, and with admit to a copy of r as it
// was when full accepts; returns whether full accepted.
bool apply_checked(Replica& r, std::string_view m) {
    Replica before(r);
    const xo::XStatus full = apply_full(r, m);
    if (!full) {
        ++g_stats.rejected[unsigned(full.code()) & 255];
        need(same_state(r, before), "a rejected message changed the replica");
        return false;
    }
    ++g_stats.accepted;
    need(r.verifies(), "an accepted message left a buffer that does not verify");
    Replica twin(before);
    const xo::XStatus admit = apply_admit(twin, m);
    if (!admit) fail("admit rejected a message full accepts: " + admit.error().message);
    need(same_state(twin, r), "admit and full differ");
    return true;
}

// The replica's state: a server stepped from the seed and its key applied.
struct World {
    Server s;
    Replica r{24};
    std::mt19937 rng;
    explicit World(std::uint8_t seed) : rng(seed) {
        auto pick = [&](unsigned n) { return unsigned(rng() % n); };
        for (int i = 0; i < 8 + seed % 24; ++i) s.step(pick);
        need(bool(apply_full(r, s.key())), "fixture key rejected");
        for (int i = 0; i < seed % 4; ++i) {
            s.step(pick);
            need(bool(apply_full(r, s.delta())), "fixture delta rejected");
        }
        need(s.matches(r), "fixture differs from the server");
    }
};

std::string header(bool key, std::uint64_t stream, std::uint32_t version, std::uint32_t count) {
    xs::XSyncOut out;
    xs::put_header(out, xs::XSyncHeader{std::uint8_t(key ? xs::kFlagKey : 0), stream, version, count});
    return std::string(out.view());
}

void run_raw(World& w, In& in) {
    const std::uint32_t count = in.u8();
    apply_checked(w.r, header(false, 0, w.r.cur.version() + 1, count) + in.rest());
}

void run_bare(World& w, In& in) {
    const std::string m = in.rest();
    apply_checked(w.r, m);
    // The same bytes as the first message of an empty client.
    Replica fresh(8);
    apply_checked(fresh, m);
}

void run_key(World& w, In& in) {
    const std::uint32_t count = in.u8();
    w.r.reset();
    apply_checked(w.r, header(true, 0x5d3 + in.u8(), 1 + in.u8(), count) + in.rest());
}

std::string corrupt(std::string m, In& in) {
    switch (in.u8() % 5) {
    case 0: m[4 + in.u8() % (m.size() - 4)] ^= char(1u << (in.u8() % 8)); break;
    case 1: {
        const std::size_t lo = in.u8(), hi = in.u8();
        m[4 + (lo | hi << 8) % (m.size() - 4)] = char(in.u8());
        break;
    }
    case 2: m.resize(4 + in.u8() % (m.size() - 4)); break;
    case 3: m.push_back(char(in.u8())); break;
    default: {
        const std::size_t at = 4 + in.u8() % (m.size() - 4);
        m.insert(at, 1, char(in.u8()));
        break;
    }
    }
    return m;
}

void run_stream(World& w, In& in) {
    auto pick = [&](unsigned n) { return unsigned(in.u8() % n); };
    while (in.left()) {
        for (unsigned i = 0, n = pick(4); i < n; ++i) w.s.step(pick);
        const std::string m(w.s.delta());
        for (unsigned i = 0, n = pick(4); i < n && in.left(); ++i) {
            if (apply_checked(w.r, corrupt(m, in))) {
                // Accepted: bytes that still check. Back to the server's records.
                w.r.reset();
                need(apply_checked(w.r, w.s.key()), "resync key rejected");
                break;
            }
        }
        if (w.r.cur.version() + 1 == w.s.version) need(apply_checked(w.r, m), "a real delta was rejected");
        need(w.s.matches(w.r), "the replica differs from the server");
    }
}
} // namespace

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    inject_crash_drill(data, size);
    if (size < 2) return 0;
    In in{data, size};
    const std::uint8_t mode = in.u8() & 3;
    World w(in.u8() % 32);
    switch (mode) {
    case 0: run_raw(w, in); break;
    case 1: run_stream(w, in); break;
    case 2: run_bare(w, in); break;
    default: run_key(w, in); break;
    }
    return 0;
}

// Seeds: random inputs for every mode, and every fixture's real next delta
// as a raw op section (mode 0) and a bare message (mode 2), its key message
// as a key op section (mode 3).
extern "C" int XOffsetFuzzWriteSeeds(const char* dir) {
    std::filesystem::create_directories(dir);
    std::size_t count = 0;
    auto put = [&](const std::string& bytes) {
        const std::string path = std::string(dir) + "/seed-" + std::to_string(count++);
        std::FILE* f = std::fopen(path.c_str(), "wb");
        if (!f || std::fwrite(bytes.data(), 1, bytes.size(), f) != bytes.size() || std::fclose(f) != 0) {
            std::cerr << "fuzz_shared_delta: cannot write " << path << "\n";
            std::exit(2);
        }
    };
    std::mt19937_64 rng(20261110);
    for (int mode = 0; mode < 4; ++mode)
        for (int k = 0; k < 6; ++k) {
            std::string s = {char(mode), char(k * 5)};
            const int len = 8 + int(rng() % 120);
            for (int i = 0; i < len; ++i) s += char(rng());
            put(s);
        }
    for (std::uint8_t seed = 0; seed < 12; ++seed) {
        World w(seed);
        std::mt19937 step_rng(seed + 100u);
        auto pick = [&](unsigned n) { return unsigned(step_rng() % n); };
        for (int i = 0; i < 3; ++i) w.s.step(pick);
        const std::string m(w.s.delta());
        xs::XSyncHeader h;
        const char* ops = xs::get_header(m.data(), m.data() + m.size(), h);
        if (!ops || h.count > 255) continue;
        put(std::string{char(0), char(seed), char(h.count)} + std::string(ops, m.data() + m.size()));
        put(std::string{char(2), char(seed)} + m);
        const std::string k(w.s.key());
        const char* kops = xs::get_header(k.data(), k.data() + k.size(), h);
        if (kops && h.count <= 255)
            put(std::string{char(3), char(seed), char(h.count), char(0), char(0)} + std::string(kops, k.data() + k.size()));
    }
    std::cout << "fuzz_shared_delta: wrote " << count << " seeds to " << dir << "\n";
    return 0;
}
