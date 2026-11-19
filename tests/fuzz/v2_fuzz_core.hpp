// Shared core of the V2 validator fuzzers: test_v2_fuzz (seeded mutation,
// runs in CTest) and fuzz_v2_wire (libFuzzer, coverage guided).
//
// Builds random valid wires (inline/heap strings, nested vectors, set, map,
// free blocks left by mutation), then feeds mutated wires to these acceptors:
//   1. an independent spec oracle written here from the format rules
//      (docs/WIRE_FORMAT.md, format 3.0),
//   2. the typed validator (XBuffer::load_verified<T>),
//   3. the in-place reload (XBuffer::reload_verified<T>) into ONE long-lived
//      buffer: it must accept exactly what load_verified accepts, and an
//      accepted reload must hold byte-identical payload (in particular gap and
//      tail bytes left by every earlier reload read as zero) and re-save
//      byte-identically; after a reject typed root access must throw.
//   4. the direct reload (begin_verified_reload) into one long-lived buffer,
//      plus interrupted deliveries that must all reject;
//   5. (R1) the same on slot storage: a fresh load, a reload and a direct
//      reload on movable slots must equal the VM load byte for byte, and the
//      same mutations on a pinned slot must re-save the same wire.
//   6. (trust tier P1) XReceiver<server_peer>::reload with XCheck::admit into
//      ONE long-lived buffer. It skips the graph walk, so it is not one more
//      vote: it must accept every wire load_verified accepts, with the same
//      payload, free table and re-save (as acceptor 3), and reject every wire
//      the oracle rejects at the header stage; a wire it accepts beyond those
//      is not read (the payload is the sender's on a trusted link). Its own
//      parse must stay in bounds on any input (ASan).
//   7. (client uplink, 3.5) XReceiver<from_client>::view with XCheck::bounds,
//      over a copy of the wire in a heap block of exactly its size. Not one
//      more vote either: it must accept every dense wire load_verified accepts
//      and read the same fields, refuse every sparse wire (a view needs the
//      payload in one piece) and every wire the oracle rejects at the header
//      stage; a wire it accepts beyond those is read whole through the view,
//      in bounds (ASan): the walk does not look for overlap or key order.
// Every acceptor must agree on accept/reject. Acceptors 2, 3, 4 and 5 go
// through the non-throwing entries (try_load_verified, try_reload_verified,
// try_begin_verified_reload / try_finish); with exceptions on, typed_load
// also runs the throwing load_verified and requires the same outcome, error
// code and message. Without exceptions (XOFFSET_NO_EXCEPTIONS) the "root()
// throws after a reject" check is skipped. Accepted inputs are then read, mutated and
// re-saved; the re-saved wire must load again and round-trip byte-identically.
// The mutated buffer is then copied with clone() and with clone_into() into
// ONE long-lived buffer: both copies must hold its bytes and free table,
// re-save identically and stay identical to it under the same next mutation.
#pragma once
// Acceptor 6 runs the real admit, also under ASan and without NDEBUG, where
// XOFFSET_FORCE_FULL_CHECK would otherwise make it the full check.
#ifndef XOFFSET_FORCE_FULL_CHECK
#define XOFFSET_FORCE_FULL_CHECK 0
#endif
#include "xoffsetdatastructure.hpp"
#include "xoffset_receiver.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <memory>
#include <memory_resource>
#include <optional>
#include <random>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace xo = XOffsetDatastructure;

// Older libraries (the differential's base) predate the switch.
#if !defined(XOFFSET_NO_EXCEPTIONS)
#define XOFFSET_NO_EXCEPTIONS 0
#endif

// The root as a plain reference: TypedXBuffer::plain_root() (root() returns a
// proxy), or root() in older libraries (the differential's base).
template <class B> decltype(auto) plain_root_of(B&& b) {
    if constexpr (requires { b.plain_root(); }) return b.plain_root();
    else return b.root();
}

namespace fz {
struct Item {
    std::uint32_t id{};
    bool flag{};
    xo::XString name;
    xo::XVector<std::uint16_t> vals;
};
struct Root {
    std::uint64_t epoch{};
    xo::XString title;
    xo::XVector<Item> items;
    xo::XVector<xo::XString> tags;
    xo::XSet<std::uint32_t> keys;
    xo::XMap<xo::XString, std::uint64_t> attrs;
};
} // namespace fz
// tools/validation_differential.sh also builds this harness against releases
// that predate field lists (reflection only).
#ifdef XOFFSET_RECORD_DERIVED
XOFFSET_RECORD_DERIVED(fz::Item, (), id, flag, name, vals)
XOFFSET_RECORD_DERIVED(fz::Root, (), epoch, title, items, tags, keys, attrs)
#endif

XOFFSET_REGISTER_SCHEMA_NAME(fz::Root, "fuzz.v2.root")

namespace XOffsetDatastructure {
template<> struct wire_record_layout<fz::Item> { static constexpr bool ordinary_members = true; };
template<> struct wire_record_layout<fz::Root> { static constexpr bool ordinary_members = true; };
}

namespace {
using Buffer = xo::TypedXBuffer<fz::Root>;
constexpr std::size_t kHeader = sizeof(xo::XWireHeader);  // the image header, 40 bytes
constexpr std::size_t kBegin = xo::XWIRE_PAYLOAD_BEGIN;   // the root's offset, after the header
constexpr std::uint32_t kMaxPayload = 0x7ffffff8u;

// Every acceptor and the oracle use this payload limit. An input that declares
// more is a header-stage rejection (threat model R1: a short sparse wire may
// declare a huge payload).
constexpr std::size_t kLimit = std::size_t(1) << 22;
// Payloads up to this size keep the validator's granule bitmaps inline.
constexpr std::uint64_t kInlineBitmapExtent = 32 * 1024;

inline std::size_t inflated_comparisons = 0; // sparse vs dense-payload checks run
inline std::size_t composed_comparisons = 0; // save_compact_into(xassign...) vs assign + save_compact
// Set by validation_outcomes: every built wire is dense, so a library that
// still requires gap bytes to be holes accepts the pool.
inline bool g_dense_pool = false;

// What fail() reports besides its message. test_v2_fuzz sets these per case:
// the input wire under test, a file-name tag and the command that replays the
// case. The libFuzzer targets leave them empty (libFuzzer saves the input).
struct FailContext {
    const std::string* wire = nullptr;
    std::string tag;
    std::string replay;
};
FailContext g_fail;

[[noreturn]] void fail(const std::string& message) {
    std::cerr << "v2 fuzz FAILED: " << message << "\n";
    if (!g_fail.replay.empty()) std::cerr << "v2 fuzz replay: " << g_fail.replay << "\n";
    // XOFFSET_FUZZ_DUMP_DIR: keep the failing input (<tag>.wire) and the report
    // (<tag>.txt) for the nightly report job, which has no other copy of it.
    const char* dir = std::getenv("XOFFSET_FUZZ_DUMP_DIR");
    if (dir && *dir && g_fail.wire) {
        std::error_code ec;
        std::filesystem::create_directories(dir, ec);
        const std::string base = std::string(dir) + "/" + (g_fail.tag.empty() ? "v2fuzz" : g_fail.tag);
        const std::string report = "v2 fuzz FAILED: " + message + "\nreplay: " + g_fail.replay + "\n";
        bool ok = true;
        for (const auto& [path, bytes] : {std::pair{base + ".wire", g_fail.wire}, std::pair{base + ".txt", &report}}) {
            std::FILE* f = std::fopen(path.c_str(), "wb");
            ok = f && std::fwrite(bytes->data(), 1, bytes->size(), f) == bytes->size() && std::fclose(f) == 0 && ok;
        }
        std::cerr << "v2 fuzz dump: " << base << ".wire" << (ok ? "" : " (write FAILED)") << "\n";
    }
    std::abort(); // libFuzzer records abort as a crash and saves the input
}

// Drill builds only (CMake -DXOFFSET_FUZZ_INJECT_CRASH=ON): an input starting
// with XOFFSET-INJECT crashes the harness, so the nightly crash pipeline can be
// exercised end to end without a real bug.
inline void inject_crash_drill([[maybe_unused]] const std::uint8_t* data, [[maybe_unused]] std::size_t size) {
#ifdef XOFFSET_FUZZ_INJECT_CRASH
    constexpr char marker[] = "XOFFSET-INJECT";
    if (size >= sizeof(marker) - 1 && std::memcmp(data, marker, sizeof(marker) - 1) == 0) {
        std::cerr << "XOFFSET_FUZZ_INJECT_CRASH: injected crash (drill input)\n";
        std::abort();
    }
#endif
}

// With XOFFSET_FUZZ_OUTCOMES set, the harnesses print one line per input to
// stdout: the FNV-1a hash of the input, then each acceptor's outcome ("ok" or
// the exact error message), tab separated:
//   outcome <hash> T:<typed load> L:<reload> R:<raw> V:<verified receive>
// (R and V only where the harness runs them; builds without exceptions have
// neither). tools/fuzz_outcome_diff.sh replays one corpus through the
// reflection, field-list and no-exceptions builds and compares the lines.
inline const bool g_outcomes = [] {
    const char* v = std::getenv("XOFFSET_FUZZ_OUTCOMES");
    return v && *v;
}();
inline std::string g_outcome;
inline void outcome_begin(const std::uint8_t* data, std::size_t size) {
    if (!g_outcomes) return;
    std::uint64_t h = 0xcbf29ce484222325ull;
    for (std::size_t i = 0; i < size; ++i) h = (h ^ data[i]) * 0x100000001b3ull;
    char hex[17];
    std::snprintf(hex, sizeof hex, "%016llx", static_cast<unsigned long long>(h));
    g_outcome = std::string("outcome\t") + hex;
}
inline void outcome_add(const char* tag, bool ok, const std::string& error) {
    if (!g_outcomes) return;
    g_outcome += std::string("\t") + tag + ":";
    if (ok) { g_outcome += "ok"; return; }
    for (char c : error) g_outcome += c == '\n' ? std::string("\\n") : c == '\t' ? std::string("\\t") : std::string(1, c);
}
inline void outcome_end() {
    if (g_outcomes) std::cout << g_outcome << "\n";
}

// Per-case random stream of test_v2_fuzz: case `id` of a run with `seed`
// depends on nothing else, so `test_v2_fuzz --replay <seed> <id>` reruns it
// alone (splitmix64 finalizer over both values).
std::mt19937_64 case_rng(std::uint64_t seed, std::uint64_t id) {
    auto mix = [](std::uint64_t z) {
        z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ull;
        z = (z ^ (z >> 27)) * 0x94d049bb133111ebull;
        return z ^ (z >> 31);
    };
    return std::mt19937_64(mix(seed ^ mix(id + 0x9e3779b97f4a7c15ull)));
}

// ---------------------------------------------------------------------------
// Independent oracle. Reads bytes only; layouts come from offsetof/sizeof.
// ---------------------------------------------------------------------------
struct Oracle {
    enum class Stage { Header, Payload };
    bool ok = true;
    Stage stage = Stage::Payload;
    std::string why;
    // Valid-wire bookkeeping for targeted mutation.
    std::vector<std::size_t> heads; // payload offsets of container heads
};

struct OracleRun {
    const unsigned char* p{};
    std::size_t used{};
    struct Range { std::uint64_t begin, end; };
    std::vector<Range> ranges;
    std::vector<std::size_t>* heads{};
    std::string why;

    template <class T> T rd(std::size_t off) const {
        T v; std::memcpy(&v, p + off, sizeof(T)); return v;
    }
    bool fits(std::uint64_t off, std::uint64_t n) const { return off <= used && n <= used - off; }
    bool bad(const char* w) { why = w; return false; }
    bool add(std::uint64_t off, std::uint64_t n) {
        if (n == 0) return true;
        if (!fits(off, n)) return bad("range out of bounds");
        ranges.push_back({off, off + n});
        return true;
    }
    // rel32 field at `field`; returns target or nullopt when out of [0, used).
    std::optional<std::uint64_t> rel(std::size_t field) const {
        const auto d = rd<std::int32_t>(field);
        const auto t = std::int64_t(field) + d;
        if (t < 0 || std::uint64_t(t) >= used) return std::nullopt;
        return std::uint64_t(t);
    }
    bool string(std::size_t h, std::string* out = nullptr) {
        if (heads) heads->push_back(h);
        // V2.1 head {data rel32, size u32, capacity u32}; inline: bytes 0..10,
        // byte 11 = 0x80 | length.
        const auto cap = rd<std::uint32_t>(h + 8);
        if (cap & 0x80000000u) {
            const unsigned len = (cap >> 24) & 0x7fu;
            if (len > 10) return bad("inline length");
            for (unsigned i = len; i < 11; ++i)
                if (p[h + i] != 0) return bad("inline padding");
            if (out) out->assign(reinterpret_cast<const char*>(p + h), len);
            return true;
        }
        const auto size = rd<std::uint32_t>(h + 4);
        if (size > cap || cap >= kMaxPayload) return bad("string size/cap");
        if (cap == 0) {
            if (size != 0 || rd<std::int32_t>(h) != 0) return bad("empty string payload");
            if (out) out->clear();
            return true;
        }
        auto d = rel(h);
        if (!d) return bad("string data out of bounds");
        if (*d % 8) return bad("string data misaligned");
        const std::uint64_t ext = (std::uint64_t(cap) + 1 + 7) & ~std::uint64_t(7);
        if (!add(*d, ext)) return false;
        if (p[*d + size] != 0) return bad("string terminator");
        if (out) out->assign(reinterpret_cast<const char*>(p + *d), size);
        return true;
    }
    template <class Elem, class Fn>
    bool vector(std::size_t h, Fn&& each) {
        if (heads) heads->push_back(h);
        const auto size = rd<std::uint32_t>(h + 4), cap = rd<std::uint32_t>(h + 8);
        if (size > cap || cap > (kMaxPayload - 8u) / sizeof(Elem)) return bad("vector size/cap");
        if (cap == 0) {
            if (size != 0 || rd<std::int32_t>(h) != 0) return bad("empty vector payload");
            return true;
        }
        auto d = rel(h);
        if (!d) return bad("vector data out of bounds");
        if (*d % 8) return bad("vector data misaligned");
        const std::uint64_t bytes = std::uint64_t(cap) * sizeof(Elem);
        if (!add(*d, (bytes + 7) & ~std::uint64_t(7))) return false;
        for (std::uint32_t i = 0; i < size; ++i)
            if (!each(std::size_t(*d + std::uint64_t(i) * sizeof(Elem)))) return false;
        return true;
    }
    bool item(std::size_t o) {
        const auto flag = p[o + offsetof(fz::Item, flag)];
        if (flag > 1) return bad("bool value");
        if (!string(o + offsetof(fz::Item, name))) return false;
        return vector<std::uint16_t>(o + offsetof(fz::Item, vals), [](std::size_t) { return true; });
    }
    bool root(std::size_t o) {
        if (!string(o + offsetof(fz::Root, title))) return false;
        if (!vector<fz::Item>(o + offsetof(fz::Root, items), [&](std::size_t e) { return item(e); })) return false;
        if (!vector<xo::XString>(o + offsetof(fz::Root, tags), [&](std::size_t e) { return string(e); })) return false;
        bool have = false; std::uint32_t last = 0;
        if (!vector<std::uint32_t>(o + offsetof(fz::Root, keys), [&](std::size_t e) {
                const auto v = rd<std::uint32_t>(e);
                if (have && !(last < v)) return bad("set order");
                have = true; last = v; return true; })) return false;
        bool have_key = false; std::string last_key;
        using KV = xo::XKeyValue<xo::XString, std::uint64_t>;
        return vector<KV>(o + offsetof(fz::Root, attrs), [&](std::size_t e) {
            std::string key;
            if (!string(e + offsetof(KV, first), &key)) return false;
            if (have_key && !(std::string_view(last_key) < std::string_view(key)))
                return bad("map order");
            have_key = true; last_key = key; return true;
        });
    }
};

// The free regions of an image (docs/WIRE_FORMAT.md sections 4 and 5): the
// directory block, then every block of every size's stack, appended to
// run.ranges; false (run.why) when the directory or a stack is malformed.
bool free_regions(OracleRun& run, std::uint32_t dir, std::uint32_t free_bytes) {
    if (dir == 0) return free_bytes == 0 || run.bad("free bytes without a directory");
    const std::uint64_t used = run.used;
    if (dir % 8 || dir < kBegin || std::uint64_t(dir) + 8 > used) return run.bad("free directory");
    const auto n = run.rd<std::uint32_t>(dir), cap = run.rd<std::uint32_t>(dir + 4);
    if (n >= cap || std::uint64_t(dir) + 8 + 8 * std::uint64_t(cap) > used) return run.bad("free directory");
    run.ranges.push_back({dir, std::uint64_t(dir) + 8 + 8 * std::uint64_t(cap)});
    std::uint64_t total = 0, blocks = 0;
    std::uint32_t previous = 0;
    for (std::uint32_t i = 0; i < cap; ++i) {
        const auto size = run.rd<std::uint32_t>(dir + 8 + 8 * std::size_t(i));
        const auto head = run.rd<std::uint32_t>(dir + 12 + 8 * std::size_t(i));
        if (i >= n) {
            if (size != 0xFFFFFFFFu || head != 0) return run.bad("unused directory entry");
            continue;
        }
        if (size <= previous || size % 8) return run.bad("directory sizes");
        previous = size;
        for (std::uint32_t at = head; at != 0; at = run.rd<std::uint32_t>(at)) {
            if (at % 8 || at < kBegin || std::uint64_t(at) + size > used || run.rd<std::uint32_t>(at + 4) != 0 ||
                ++blocks > used / 8)
                return run.bad("free block");
            run.ranges.push_back({at, std::uint64_t(at) + size});
            total += size;
        }
    }
    return total == free_bytes || run.bad("free bytes");
}

Oracle oracle(const std::string& wire, bool collect_heads = false) {
    Oracle result;
    auto reject = [&](Oracle::Stage stage, std::string why) {
        result.ok = false; result.stage = stage; result.why = std::move(why); return result;
    };
    // 3.0 header: 'X' 'O' 3 0, header size 40, schema hash, root type id,
    // used_bytes, extent_count, free directory, free bytes.
    if (wire.size() < 4 || wire[0] != 'X' || wire[1] != 'O') return reject(Oracle::Stage::Header, "magic");
    if (wire[2] != 3 || wire[3] != 0) return reject(Oracle::Stage::Header, "version");
    if (wire.size() < kHeader) return reject(Oracle::Stage::Header, "short");
    xo::XWireHeader h;
    std::memcpy(&h, wire.data(), kHeader);
    if (h.header_bytes != kHeader) return reject(Oracle::Stage::Header, "header size");
    if (h.root_type_id != xo::detail::wire_root_type_id<fz::Root>() ||
        h.schema_hash != xo::detail::wire_schema_hash<fz::Root>())
        return reject(Oracle::Stage::Header, "schema");
    if (h.used_bytes < kBegin || h.used_bytes > kMaxPayload || h.used_bytes % 8)
        return reject(Oracle::Stage::Header, "sizes");
    if (h.used_bytes == kBegin) return reject(Oracle::Stage::Header, "root offset");
    const std::uint64_t body = wire.size() - kHeader;
    if (h.extent_count > body / 8) return reject(Oracle::Stage::Header, "sizes");
    if (h.used_bytes > kLimit) return reject(Oracle::Stage::Header, "declared size above limit");
    // A dense wire is the image. A sparse one: the extent table, then the
    // extents' bytes; the first extent starts at 40, after the header.
    const auto* table = reinterpret_cast<const unsigned char*>(wire.data()) + kHeader;
    std::vector<std::pair<std::uint64_t, std::uint64_t>> extents;
    if (h.extent_count == 0) {
        if (wire.size() != h.used_bytes) return reject(Oracle::Stage::Header, "sizes");
    } else {
        std::uint64_t stored = 0, end = 0;
        bool gap = false;
        for (std::uint64_t i = 0; i < h.extent_count; ++i) {
            std::uint32_t off, b; std::memcpy(&off, table + i * 8, 4); std::memcpy(&b, table + i * 8 + 4, 4);
            if (b == 0 || off % 8 || b % 8 || (i == 0 ? off != kBegin : off <= end) || std::uint64_t(off) + b > h.used_bytes)
                return reject(Oracle::Stage::Header, "extent table");
            gap |= i != 0;
            extents.push_back({off, b});
            end = std::uint64_t(off) + b; stored += b;
        }
        gap |= end < h.used_bytes;
        if (!gap) return reject(Oracle::Stage::Header, "extent table");
        if (stored != body - h.extent_count * 8) return reject(Oracle::Stage::Header, "sizes");
    }
    // Image stage (also what the receive validators see). Gaps read as 0;
    // the loaded header's extent count is 0.
    std::vector<unsigned char> image(std::size_t(h.used_bytes), 0);
    if (h.extent_count == 0) {
        std::memcpy(image.data(), wire.data(), image.size());
    } else {
        std::memcpy(image.data(), wire.data(), kHeader);
        std::memset(image.data() + offsetof(xo::XWireHeader, extent_count), 0, 4);
        const auto* src = table + h.extent_count * 8;
        for (auto [off, b] : extents) { std::memcpy(image.data() + off, src, b); src += b; }
    }
    OracleRun run;
    run.p = image.data();
    run.used = std::size_t(h.used_bytes);
    if (collect_heads) run.heads = &result.heads;
    auto preject = [&](std::string why) { return reject(Oracle::Stage::Payload, std::move(why)); };
    if (sizeof(fz::Root) > run.used - kBegin) return preject("root not found");
    run.ranges.push_back({0, kBegin});
    if (!free_regions(run, h.free_dir, h.free_bytes)) return preject(run.why);
    if (!run.add(kBegin, (sizeof(fz::Root) + 7) & ~std::size_t(7))) return preject(run.why);
    if (!run.root(kBegin)) return preject(run.why);
    std::sort(run.ranges.begin(), run.ranges.end(),
              [](auto a, auto b) { return a.begin < b.begin; });
    std::uint64_t covered = 0;
    for (std::size_t i = 0; i < run.ranges.size(); ++i) {
        if (i && run.ranges[i].begin < run.ranges[i - 1].end) return preject("overlap");
        covered += run.ranges[i].end - run.ranges[i].begin;
    }
    if (covered != run.used) return preject("coverage");
    // Gap bytes load as zero wherever they lie: a gap is not itself a reason
    // to reject (zero-run elision), the image above decides.
    return result;
}

// ---------------------------------------------------------------------------
// Library acceptors.
// ---------------------------------------------------------------------------
// The verified load of every acceptor. Through try_load_verified where the
// library has it (older libraries, as built by the differential, throw).
template <class Root = fz::Root>
std::optional<xo::TypedXBuffer<Root>> load_entry(const std::string& wire, xo::XBuffer::MaximumCapacity cap,
                                                 std::string* error) {
    auto loaded = xo::TypedXBuffer<Root>::try_load_verified(wire, cap);
#if !XOFFSET_NO_EXCEPTIONS
    // The throwing entry must agree exactly: outcome, code and message.
    std::optional<xo::XError> thrown;
    try {
        (void)xo::XBuffer::load_verified<Root>(wire, cap);
    } catch (const xo::XException& e) {
        thrown = xo::XError{e.code(), e.what()};
    }
    if (bool(thrown) == bool(loaded))
        fail(std::string("try_load_verified ") + (loaded ? "accepted" : "rejected (" + loaded.error().message + ")") +
             " where load_verified " + (thrown ? "threw (" + thrown->message + ")" : "accepted"));
    if (thrown && (thrown->code != loaded.code() || thrown->message != loaded.error().message))
        fail("try_load_verified reported " + std::string(xo::to_string(loaded.code())) + " (" +
             loaded.error().message + "), load_verified threw " + xo::to_string(thrown->code) + " (" +
             thrown->message + ")");
#endif
    if (!loaded) {
        if (loaded.code() == xo::XErrc::ok || loaded.error().message.empty())
            fail("try_load_verified rejected without a code and message");
        if (error) *error = loaded.error().message;
        return std::nullopt;
    }
    return std::move(*loaded);
}

template <class Root = fz::Root>
std::optional<xo::TypedXBuffer<Root>> typed_load(const std::string& wire, std::string* error = nullptr) {
    // Growable so accepted inputs can be mutated afterwards.
    return load_entry<Root>(wire, xo::XBuffer::maximum_capacity(kLimit).with_growth(xo::XGrowPolicy::Automatic),
                            error);
}

// After a rejected reload the target has no root; with exceptions on, root()
// must also throw.
template <class B>
void require_no_root(B& target, const char* who) {
#if !XOFFSET_NO_EXCEPTIONS
    bool refused = false;
    try { (void)plain_root_of(target); } catch (const xo::XException&) { refused = true; }
    if (!refused) fail(std::string("root() succeeded after a rejected ") + who);
#endif
    if (target.has_root()) fail(std::string("has_root() after a rejected ") + who);
}

// Acceptor 5: one buffer reused for every case (same maximum as typed_load).
Buffer& reloader() {
    static Buffer buffer = Buffer::create(
        4096, xo::XBuffer::maximum_capacity(kLimit).with_growth(xo::XGrowPolicy::Automatic));
    return buffer;
}

bool reload_accepts(Buffer& target, const std::string& wire, std::string* error) {
    const auto status = target.try_reload_verified(wire);
    if (status) return true;
    if (error) *error = status.error().message;
    require_no_root(target, "reload_verified");
    return false;
}
bool reload_accepts(const std::string& wire, std::string* error) { return reload_accepts(reloader(), wire, error); }

// Accepted reload (or direct reload) vs fresh load of the same wire.
void compare_reload(Buffer& fresh, Buffer& again, const char* who) {
    const auto a = fresh.get_buffer(), b = again.get_buffer();
    if (a.size() != b.size() || std::memcmp(a.data(), b.data(), a.size()) != 0)
        fail(std::string(who) + " payload differs from load_verified (stale bytes?)");
    if (fresh.free_block_count() != again.free_block_count() ||
        fresh.free_block_bytes() != again.free_block_bytes())
        fail(std::string(who) + " free table differs from load_verified");
    // Newly exposed tail bytes must read as zero after any earlier content.
    const auto size = again.get_size();
    if (!again.grow(4096)) fail(std::string(who) + ": buffer cannot grow");
    for (std::size_t i = size; i < again.get_size(); ++i)
        if (again.data()[i] != 0) fail(std::string("stale byte exposed after ") + who + " growth");
    if (again.save_verified() != fresh.save_verified())
        fail(std::string(who) + " re-save differs from load_verified re-save");
}
void compare_reload(Buffer& fresh) { compare_reload(fresh, reloader(), "reload_verified"); }

// Acceptor 8: admit into one buffer reused for every case (same maximum as typed_load).
void check_admit(const std::string& w, const Oracle& o, Buffer* typed, const std::string& id) {
    static Buffer target = Buffer::create(
        4096, xo::XBuffer::maximum_capacity(kLimit).with_growth(xo::XGrowPolicy::Automatic));
    static xo::XReceiver<xo::XLink::server_peer> rx(xo::XBuffer::maximum_capacity(kLimit));
    static_assert(!xo::detail::force_full_check, "acceptor 6 needs the real admit");
    const xo::XStatus st = rx.reload(target, w);
    if (typed) {
        if (!st) fail(id + ": admit rejected a wire load_verified accepts (" + st.error().message + ")");
        compare_reload(*typed, target, "XReceiver admit");
        return;
    }
    if (st && !o.ok && o.stage == Oracle::Stage::Header)
        fail(id + ": admit accepted a wire the oracle rejects at the header (" + o.why + ")");
    if (!st) require_no_root(target, "XReceiver admit");
}

std::string dump(const fz::Root& r);
// Acceptor 9: the bounds-checked view of a client uplink.
void check_view(const std::string& w, const Oracle& o, Buffer* typed, const std::string& id) {
    static const xo::XReceiver<xo::XLink::from_client> rx(xo::XBuffer::maximum_capacity(kLimit));
    static_assert(!xo::detail::force_full_check, "acceptor 7 needs the real bounds walk");
    // Exactly the wire's bytes on the heap (8-aligned): a read past them is an ASan error.
    const std::unique_ptr<std::byte[]> exact(new std::byte[w.size() ? w.size() : 1]);
    if (!w.empty()) std::memcpy(exact.get(), w.data(), w.size());
    const auto v = rx.view<fz::Root, xo::XCheck::bounds>(std::span<const std::byte>(exact.get(), w.size()));
    xo::XWireHeader h{};
    if (w.size() >= kHeader) std::memcpy(&h, w.data(), kHeader);
    const bool sparse = w.size() >= kHeader && h.extent_count != 0;
    if (typed && !sparse) {
        if (!v) fail(id + ": bounds view rejected a dense wire load_verified accepts (" + v.error().message + ")");
        if (dump(v->root()) != dump(plain_root_of(*typed))) fail(id + ": bounds view reads differ from load_verified");
        return;
    }
    if (v && sparse) fail(id + ": bounds view accepted a sparse wire");
    if (v && !o.ok && o.stage == Oracle::Stage::Header)
        fail(id + ": bounds view accepted a wire the oracle rejects at the header (" + o.why + ")");
    if (v) (void)dump(v->root());  // every const access stays in the wire
}

// Acceptor 6: direct reload (XBuffer::begin_verified_reload) into ONE
// long-lived buffer, delivered the way a transport would: the section lengths
// come from the wire header, the header goes to the image's first 40 bytes,
// each extent lands at the offset its (not yet verified) table entry names,
// and a message the header does not frame exactly is dropped or completed
// with the counts actually written. It must accept exactly what load_verified
// accepts. The other modes must reject:
//   Abort:    the token is dropped after half the image;
//   Short:    complete() reports one image granule less than written;
//   Mismatch: begin declares a used or extent count other than the header's.
// After every reject the buffer has no root.
enum class DirectMode { Whole, Abort, Short, Mismatch };

Buffer& direct_slot() {
    static Buffer buffer = Buffer::create(
        4096, xo::XBuffer::maximum_capacity(kLimit).with_growth(xo::XGrowPolicy::Automatic));
    return buffer;
}

using DirectToken = xo::VerifiedReloadToken<fz::Root>;
std::optional<DirectToken> direct_begin(Buffer& target, std::size_t used, std::size_t extents, std::string* error) {
    auto begun = target.try_begin_verified_reload<fz::Root>(used, extents);
    if (begun) return std::optional<DirectToken>(std::move(*begun));
    if (error) *error = begun.error().message;
    return std::nullopt;
}
bool direct_finish(DirectToken& token, std::string* error) {
    const auto finished = token.try_finish();
    if (finished) return true;
    if (error) *error = finished.error().message;
    return false;
}

bool direct_deliver(Buffer& target, const std::string& wire, DirectMode mode, std::string* error) {
    if (wire.size() < kHeader) { if (error) *error = "transport: short header"; return false; }
    xo::XWireHeader h{};
    std::memcpy(&h, wire.data(), kHeader);
    std::size_t used = h.used_bytes, extents = h.extent_count;
    if (mode == DirectMode::Mismatch) {
        switch (wire.size() % 3) {
        case 0: used = used ? used - 8 : 8; break;
        case 1: used += 8; break;
        default: extents = extents ? extents - 1 : 1; break;
        }
    }
    {
        auto begun = direct_begin(target, used, extents, error);
        if (!begun) return false;
        auto& token = *begun;
        std::size_t at = 0;
        // Copies up to n bytes of the message into to[off, off + n) as far
        // as both reach; returns the bytes copied.
        auto take = [&](std::span<std::byte> to, std::size_t off, std::size_t n) {
            n = std::min(n, wire.size() - at);
            n = off <= to.size() ? std::min(n, to.size() - off) : 0;
            if (n) std::memcpy(to.data() + off, wire.data() + at, n);
            at += n;
            return n;
        };
        const auto image = token.image_bytes();
        std::size_t ib = take(image, 0, kHeader);
        const auto table = token.extent_bytes();
        const auto eb = extents == 0 ? 0 : take(table, 0, table.size());
        if (extents == 0) {
            const auto rest = image.size() > kHeader ? image.size() - kHeader : 0;
            const auto n = mode == DirectMode::Abort ? rest / 2 : rest;
            ib += take(image, kHeader, n);
            if (mode == DirectMode::Abort) {
                if (error) *error = "transport: aborted";
                return false; // token dropped mid-image
            }
        } else {
            bool landed = ib == kHeader && eb == table.size();
            for (std::size_t i = 0; landed && i < eb / 8u; ++i) {
                std::uint32_t e[2];
                std::memcpy(e, table.data() + i * 8u, sizeof(e));
                landed = e[0] >= kBegin && e[0] <= image.size() && e[1] <= image.size() - e[0] &&
                         take(image, e[0], e[1]) == e[1];
                if (mode == DirectMode::Abort && i * 2 + 1 >= eb / 8u) {
                    if (error) *error = "transport: aborted";
                    return false;
                }
            }
            ib = landed ? image.size() : 0;
        }
        if (at != wire.size()) {
            if (error) *error = "transport: message length differs from its header";
            return false; // token dropped
        }
        if (mode == DirectMode::Short) ib = ib >= 8 ? ib - 8 : ib + 8;
        token.complete(ib, eb);
        return direct_finish(token, error);
    }
}

bool direct_accepts(Buffer& target, const std::string& wire, std::string* error,
                    DirectMode mode = DirectMode::Whole) {
    const bool ok = direct_deliver(target, wire, mode, error);
    // A wire shorter than the header is refused by the transport before a
    // reload begins, so the target keeps whatever it held (#130).
    if (!ok && wire.size() >= kHeader) require_no_root(target, "direct reload");
    return ok;
}
bool direct_accepts(const std::string& wire, std::string* error, DirectMode mode = DirectMode::Whole) {
    return direct_accepts(direct_slot(), wire, error, mode);
}

// Every interrupted form rejects; then the whole wire must match `accepted`
// (load_verified's outcome), and an accepted payload must equal `fresh`.
void check_direct(const std::string& w, bool accepted, Buffer* fresh, const std::string& where) {
    for (auto mode : {DirectMode::Abort, DirectMode::Mismatch, DirectMode::Short})
        if (direct_accepts(w, nullptr, mode))
            fail(where + ": interrupted direct reload (mode " +
                 std::to_string(int(mode)) + ") accepted");
    std::string derr;
    if (direct_accepts(w, &derr) != accepted)
        fail(where + ": direct=" + (accepted ? "reject(" + derr + ")" : "accept") +
             " typed=" + (accepted ? "accept" : "reject"));
    if (fresh) compare_reload(*fresh, direct_slot(), "direct reload");
}

// Acceptor 7 (R1). Movable slots: Explicit growth, maximum kLimit, starting
// in the smallest class, so reloads keep moving the owner to larger slots
// (and to VM above SLOT_MAX). Each accepted payload must equal `fresh` (a VM
// load) byte for byte, including the free table, the zero tail exposed by
// growth (which may move it again) and the re-save.
Buffer& slot_reloader() {
    static Buffer buffer = Buffer::create(512, xo::XBuffer::maximum_capacity(kLimit));
    return buffer;
}
Buffer& slot_direct() {
    static Buffer buffer = Buffer::create(512, xo::XBuffer::maximum_capacity(kLimit));
    return buffer;
}

void check_slots(const std::string& w, bool accepted, Buffer* fresh, const std::string& where) {
    std::optional<Buffer> loaded;
    std::string error;
    if (auto r = load_entry(w, xo::XBuffer::maximum_capacity(kLimit), &error)) loaded.emplace(std::move(*r));
    if (bool(loaded) != accepted)
        fail(where + ": slot load=" + (loaded ? "accept" : "reject(" + error + ")") +
             " typed=" + (accepted ? "accept" : "reject"));
    if (loaded) {
        const bool small = loaded->get_size() <= xo::XBufferCore::SLOT_MAX;
        if (small != (loaded->storage_kind() == xo::XStorageKind::MovableSlot))
            fail(where + ": slot load picked the wrong storage");
        compare_reload(*fresh, *loaded, "slot load");
    }
    if (reload_accepts(slot_reloader(), w, &error) != accepted)
        fail(where + ": slot reload=" + (accepted ? "reject(" + error + ")" : "accept"));
    if (accepted) compare_reload(*fresh, slot_reloader(), "slot reload");
    if (direct_accepts(slot_direct(), w, nullptr, DirectMode::Mismatch))
        fail(where + ": interrupted direct reload on a slot accepted");
    if (direct_accepts(slot_direct(), w, &error) != accepted)
        fail(where + ": slot direct=" + (accepted ? "reject(" + error + ")" : "accept"));
    if (accepted) compare_reload(*fresh, slot_direct(), "slot direct reload");
}

// Semantic dump of a root (for round-trip comparison).
std::string dump(const fz::Root& r) {
    std::string s = std::to_string(r.epoch) + "|" + std::string(r.title.view()) + "|";
    for (const auto& it : r.items) {
        s += std::to_string(it.id) + (it.flag ? "T" : "F") + std::string(it.name.view()) + "[";
        for (auto v : it.vals) s += std::to_string(v) + ",";
        s += "]";
    }
    s += "|";
    for (const auto& t : r.tags) s += std::string(t.view()) + ";";
    s += "|";
    for (auto k : r.keys) s += std::to_string(k) + ",";
    s += "|";
    for (const auto& kv : r.attrs) s += std::string(kv.first.view()) + "=" + std::to_string(kv.second) + ";";
    return s;
}

std::string random_text(std::mt19937_64& rng, std::size_t max_len) {
    const auto n = std::size_t(rng() % (max_len + 1));
    std::string s(n, 'a');
    for (auto& c : s) c = char('a' + rng() % 26);
    return s;
}

void mutate_valid(fz::Root& r, std::mt19937_64& rng) {
    const int ops = int(rng() % 12);
    for (int k = 0; k < ops; ++k) {
        switch (rng() % 8) {
        case 0: r.title = random_text(rng, 40); break;
        case 1: if (!r.items.empty()) r.items[rng() % r.items.size()].name = random_text(rng, 40); break;
        case 2: if (!r.items.empty()) { auto& v = r.items[rng() % r.items.size()].vals;
                    for (int i = int(rng() % 9); i > 0; --i) v.push_back(std::uint16_t(rng())); } break;
        case 3: if (!r.items.empty()) r.items.erase(r.items.begin() + std::ptrdiff_t(rng() % r.items.size())); break;
        case 4: { auto& it = r.items.emplace_back(); it.id = std::uint32_t(rng()); it.flag = rng() & 1;
                  it.name = random_text(rng, 30); } break;
        case 5: r.tags.emplace_back(random_text(rng, 24), r.tags.get_stored_allocator()); break;
        case 6: r.keys.insert(std::uint32_t(rng() % 1000)); break;
        case 7: r.attrs.insert_or_assign(std::string_view(random_text(rng, 16)), std::uint64_t(rng())); break;
        }
    }
}

std::string dense_of(const std::string& w);
std::string elide_zero_runs(const std::string& dense);

std::string build_wire(std::mt19937_64& rng) {
    auto buf = Buffer::create(4096, xo::XBuffer::maximum_capacity(kLimit).with_growth(xo::XGrowPolicy::Automatic));
    auto& r = plain_root_of(buf);
    r.epoch = rng();
    r.title = random_text(rng, 30);
    const int items = int(rng() % 9);
    for (int i = 0; i < items; ++i) {
        auto& it = r.items.emplace_back();
        it.id = std::uint32_t(rng());
        it.flag = rng() & 1;
        it.name = random_text(rng, 30);
        for (int j = int(rng() % 7); j > 0; --j) it.vals.push_back(std::uint16_t(rng()));
    }
    for (int i = int(rng() % 5); i > 0; --i) r.tags.emplace_back(random_text(rng, 20), r.tags.get_stored_allocator());
    for (int i = int(rng() % 6); i > 0; --i) r.keys.insert(std::uint32_t(rng() % 1000));
    for (int i = int(rng() % 5); i > 0; --i)
        r.attrs.insert_or_assign(std::string_view(random_text(rng, 14)), std::uint64_t(rng()));
    if (rng() % 3) mutate_valid(r, rng); // leaves holes
    if (rng() % 3 == 0) {
        // A large freed block (>= 4 KiB) makes the emission sparse.
        auto& big = r.items.emplace_back();
        big.name = random_text(rng, 40);
        for (int i = 2048 + int(rng() % 4096); i > 0; --i) big.vals.push_back(std::uint16_t(rng()));
        auto& after = r.items.emplace_back();
        after.name = random_text(rng, 40);
        r.items.erase(r.items.begin() + std::ptrdiff_t(r.items.size() - 2));
    }
    // Three wire forms of the same payload: the library's default emission,
    // dense, and zero-run elision. The last two are byte operations, so the
    // core also builds against libraries without XSaveOptions
    // (validation_outcomes); test_v2_fuzz checks them against the library's
    // own dense and zero_runs emissions.
    const auto w = buf.save_verified();
    const auto form = rng() % 3;
    if (g_dense_pool || form == 1) return dense_of(w);
    return form == 2 ? elide_zero_runs(dense_of(w)) : w;
}

// ---------------------------------------------------------------------------
// Mutations of a valid wire.
// ---------------------------------------------------------------------------
// Bytes after the header (extent table + stored image bytes) that the
// header frames, or nullopt when its extent table does not fit.
std::optional<std::uint64_t> data_bytes(const std::string& w, const xo::XWireHeader& h) {
    const auto body = w.size() - kHeader;
    if (h.extent_count == 0)
        return h.used_bytes >= kBegin ? std::optional<std::uint64_t>(h.used_bytes - kBegin) : std::nullopt;
    if (h.extent_count > body / 8) return std::nullopt;
    std::uint64_t stored = 0;
    for (std::uint64_t i = 0; i < h.extent_count; ++i) {
        std::uint32_t b; std::memcpy(&b, w.data() + kHeader + i * 8 + 4, 4); stored += b;
    }
    return h.extent_count * 8 + stored;
}

// Makes a dense wire's used_bytes its length again after a resize (a sparse
// wire is left as is).
void fix_header_sizes(std::string& w) {
    xo::XWireHeader h; std::memcpy(&h, w.data(), kHeader);
    if (h.extent_count != 0) return;
    h.used_bytes = std::uint32_t(w.size());
    std::memcpy(w.data(), &h, kHeader);
}

// Wire position of an image offset (nullopt inside a sparse gap). The header
// is on the wire in both forms; a dense wire is the image itself.
std::optional<std::size_t> file_pos(const std::string& w, std::uint64_t off) {
    xo::XWireHeader h; std::memcpy(&h, w.data(), kHeader);
    if (off < kHeader) return std::size_t(off);
    if (h.extent_count == 0) return off < w.size() ? std::optional<std::size_t>(std::size_t(off)) : std::nullopt;
    // Division form: extent_count * 8 can wrap after a header mutation.
    if (h.extent_count > (w.size() - kHeader) / 8) return std::nullopt;
    std::uint64_t cum = 0;
    for (std::uint64_t i = 0; i < h.extent_count; ++i) {
        std::uint32_t eo, eb; std::memcpy(&eo, w.data() + kHeader + i * 8, 4); std::memcpy(&eb, w.data() + kHeader + i * 8 + 4, 4);
        if (off >= eo && off < std::uint64_t(eo) + eb) {
            const auto at = kHeader + h.extent_count * 8 + cum + (off - eo);
            return at < w.size() ? std::optional<std::size_t>(std::size_t(at)) : std::nullopt;
        }
        cum += eb;
    }
    return std::nullopt;
}

void put32(std::string& w, std::size_t at, std::uint32_t v) {
    if (at + 4 <= w.size()) std::memcpy(w.data() + at, &v, 4);
}
std::uint32_t get32(const std::string& w, std::size_t at) {
    std::uint32_t v = 0; if (at + 4 <= w.size()) std::memcpy(&v, w.data() + at, 4); return v;
}

using Range = std::pair<std::uint64_t, std::uint64_t>; // image [begin, end)

// Extents of a wire whose extent table is well formed (the oracle's header
// rules) and whose framing adds up; {[40, used)} for a dense wire.
std::optional<std::vector<Range>> extents_of(const std::string& w) {
    if (w.size() < kHeader) return std::nullopt;
    xo::XWireHeader h; std::memcpy(&h, w.data(), kHeader);
    if (h.used_bytes > kLimit || h.used_bytes < kBegin) return std::nullopt;
    const auto data = data_bytes(w, h);
    if (!data || *data != w.size() - kHeader) return std::nullopt;
    if (h.extent_count == 0) return std::vector<Range>{{kBegin, h.used_bytes}};
    std::vector<Range> out;
    std::uint64_t end = kBegin;
    for (std::uint64_t i = 0; i < h.extent_count; ++i) {
        const auto off = get32(w, kHeader + i * 8), b = get32(w, kHeader + i * 8 + 4);
        if (b == 0 || off % 8 || b % 8 || (i == 0 ? off != kBegin : off <= end) || std::uint64_t(off) + b > h.used_bytes)
            return std::nullopt;
        out.push_back({off, std::uint64_t(off) + b});
        end = std::uint64_t(off) + b;
    }
    if (out.size() == 1 && end == h.used_bytes) return std::nullopt; // no gap: invalid table
    return out;
}

// The dense wire of the image a sparse wire loads (gap bytes zero).
std::optional<std::string> inflate(const std::string& w) {
    const auto ext = extents_of(w);
    if (!ext) return std::nullopt;
    xo::XWireHeader h; std::memcpy(&h, w.data(), kHeader);
    std::string out(std::size_t(h.used_bytes), '\0');
    std::size_t at = kHeader + std::size_t(h.extent_count) * 8;
    for (auto [b, e] : *ext) { std::memcpy(out.data() + b, w.data() + at, e - b); at += e - b; }
    h.extent_count = 0;
    std::memcpy(out.data(), &h, kHeader);
    return out;
}

std::string dense_of(const std::string& w) { return inflate(w).value_or(w); }

// Re-emits a dense wire with `gaps` (sorted, disjoint, nonadjacent, inside
// [48, used)) skipped through an extent table.
std::string sparsify(const std::string& dense, const std::vector<Range>& gaps) {
    xo::XWireHeader h; std::memcpy(&h, dense.data(), kHeader);
    const auto used = h.used_bytes;
    std::string table, data;
    std::uint64_t begin = kBegin;
    auto extent = [&](std::uint64_t from, std::uint64_t to) {
        const std::uint32_t e[2] = {std::uint32_t(from), std::uint32_t(to - from)};
        table.append(reinterpret_cast<const char*>(e), 8);
        data.append(dense, std::size_t(from), std::size_t(to - from));
    };
    for (auto [b, e] : gaps) { extent(begin, b); begin = e; }
    if (begin < used) extent(begin, used);
    h.extent_count = std::uint32_t(table.size() / 8);
    std::string out(reinterpret_cast<const char*>(&h), kHeader);
    return out + table + data;
}

// The library's zero_runs rule on a dense wire (the image): images of at
// least 4 KiB; a run grows from a 64-byte aligned zero block (aligned in
// image offsets) word by word on both sides, never starts before offset 48
// (the header and the first word after it stay), and is skipped when at
// least 64 bytes; the sparse form is kept only if it saves at least
// max(256, used / 16) bytes.
std::string elide_zero_runs(const std::string& dense) {
    xo::XWireHeader h; std::memcpy(&h, dense.data(), kHeader);
    const std::size_t used = h.used_bytes;
    if (h.extent_count != 0 || used < 4096 || used % 8 || dense.size() != used) return dense;
    const auto* p = reinterpret_cast<const unsigned char*>(dense.data());
    auto word = [&](std::size_t at) { std::uint64_t v; std::memcpy(&v, p + at, 8); return v; };
    auto block_zero = [&](std::size_t b) {
        for (std::size_t i = 0; i < 8; ++i) if (word(b * 64 + i * 8)) return false;
        return true;
    };
    std::vector<Range> gaps;
    std::size_t cursor = kBegin, skipped = 0;
    for (std::size_t b = 0, blocks = used / 64; b < blocks;) {
        if (!block_zero(b)) { ++b; continue; }
        std::size_t start = b * 64, e = b + 1;
        while (start > cursor && word(start - 8) == 0) start -= 8;
        while (e < blocks && block_zero(e)) ++e;
        std::size_t stop = e * 64;
        while (stop < used && word(stop) == 0) stop += 8;
        start = std::max<std::size_t>(start, kBegin + 8);
        if (stop - start >= 64) { gaps.push_back({start, stop}); skipped += stop - start; cursor = stop; }
        b = stop / 64 + 1;
    }
    if (gaps.empty()) return dense;
    const std::size_t extents = gaps.size() + (cursor < used ? 1 : 0);
    if (skipped - extents * 8 < std::max<std::size_t>(256, used / 16)) return dense;
    return sparsify(dense, gaps);
}

// Adds one gap of 8..512 bytes at a random aligned image offset >= 48 to a
// wire with a well-formed extent table: those bytes now load as zero.
void cut_gap(std::string& w, std::mt19937_64& rng) {
    const auto ext = extents_of(w);
    const auto dense = inflate(w);
    if (!ext || !dense) return;
    xo::XWireHeader h; std::memcpy(&h, w.data(), kHeader);
    const auto used = h.used_bytes;
    if (used < kBegin + 24 || used % 8) return;
    std::vector<Range> gaps;
    std::uint64_t begin = kBegin;
    for (auto [b, e] : *ext) { if (b > begin) gaps.push_back({begin, b}); begin = e; }
    if (begin < used) gaps.push_back({begin, used});
    const std::uint64_t a = kBegin + 8 * (1 + rng() % ((used - kBegin) / 8 - 1));
    gaps.push_back({a, std::min<std::uint64_t>(used, a + 8 * (1 + rng() % 64))});
    std::sort(gaps.begin(), gaps.end());
    std::vector<Range> merged;
    for (auto g : gaps) {
        if (!merged.empty() && g.first <= merged.back().second) merged.back().second = std::max(merged.back().second, g.second);
        else merged.push_back(g);
    }
    w = sparsify(*dense, merged);
}

void mutate_wire(std::string& w, std::mt19937_64& rng, const std::vector<std::size_t>& heads) {
    xo::XWireHeader h; std::memcpy(&h, w.data(), kHeader);
    // Bytes after the header (a dense wire's image, a sparse wire's table
    // and extents).
    const auto body = w.size() - kHeader;
    static const std::uint32_t interesting[] = {
        0u, 1u, 4u, 7u, 8u, 9u, 10u, 11u, 16u, 24u, 40u, 48u, 0x7ffffff8u, 0x7fffffffu, 0x80000000u,
        0x85000000u, 0x8a000000u, 0x8b000000u, 0xff000000u, 0xfffffff8u, 0xfffffff0u, 0xffffffffu};
    auto tweak = [&](std::uint32_t v) {
        switch (rng() % 4) {
        case 0: return interesting[rng() % std::size(interesting)];
        case 1: return v + std::uint32_t(8 * (1 + rng() % 4));
        case 2: return v - std::uint32_t(8 * (1 + rng() % 4));
        default: return v ^ (1u << (rng() % 32));
        }
    };
    // The free directory's words on the wire: {n, cap} then cap entries,
    // capped at 64 entries (an earlier mutation may have changed cap).
    auto dir_word = [&]() -> std::optional<std::size_t> {
        if (h.free_dir == 0) return std::nullopt;
        const auto cap = file_pos(w, std::uint64_t(h.free_dir) + 4);
        const std::uint64_t words = 2 + 2 * std::min<std::uint64_t>(cap ? get32(w, *cap) : 0, 64);
        return file_pos(w, std::uint64_t(h.free_dir) + 4 * (rng() % words));
    };
    // A free block on a stack: a used entry's head (or one link further).
    auto free_block = [&]() -> std::optional<std::uint64_t> {
        if (h.free_dir == 0) return std::nullopt;
        const auto n_at = file_pos(w, h.free_dir);
        const auto n = n_at ? std::min<std::uint32_t>(get32(w, *n_at), 64) : 0;
        if (n == 0) return std::nullopt;
        const auto head_at = file_pos(w, std::uint64_t(h.free_dir) + 12 + 8 * (rng() % n));
        std::uint64_t at = head_at ? get32(w, *head_at) : 0;
        if (at != 0 && rng() % 2) {
            const auto link = file_pos(w, at);
            if (link && get32(w, *link) != 0) at = get32(w, *link);
        }
        return at ? std::optional<std::uint64_t>(at) : std::nullopt;
    };
    switch (rng() % 13) {
    case 0: { // random bit flip anywhere
        const auto at = std::size_t(rng() % w.size());
        w[at] = char(w[at] ^ (1u << (rng() % 8)));
        break; }
    case 1: case 2: case 3: { // targeted head field
        if (heads.empty()) break;
        const auto mapped = file_pos(w, heads[rng() % heads.size()]);
        if (!mapped) break;
        const auto head = *mapped;
        const auto field = std::size_t(rng() % 3) * 4; // data, size, capacity
        put32(w, head + field, tweak(get32(w, head + field)));
        break; }
    case 4: { // aligned word after the header
        if (body < 8) break;
        const auto at = kHeader + (std::size_t(rng() % (body / 4)) * 4);
        put32(w, at, rng() & 1 ? interesting[rng() % std::size(interesting)] : std::uint32_t(rng()));
        break; }
    case 5: { // free directory word: n, cap, an entry's size or head
        const auto at = dir_word();
        if (at) put32(w, *at, tweak(get32(w, *at)));
        break; }
    case 6: { // a free block: its link, its second word or a body word
        const auto block = free_block();
        if (!block) break;
        const auto choice = rng() % 4;
        const auto at = file_pos(w, *block + (choice == 2 ? 4 : choice == 3 ? 8 * (1 + rng() % 4) : 0));
        if (!at) break;
        if (choice == 0) {
            // Link to a live head, to itself, to another free block or past used.
            const std::uint64_t targets[] = {
                heads.empty() ? *block : heads[rng() % heads.size()], *block,
                free_block().value_or(0), std::uint64_t(h.used_bytes)};
            put32(w, *at, std::uint32_t(targets[rng() % std::size(targets)]));
        } else {
            put32(w, *at, tweak(get32(w, *at)));
        }
        break; }
    case 7: { // header field: used_bytes, free_dir, free_bytes (+-8), extent_count (+-1)
        static const std::size_t fields[] = {24, 28, 32, 36};
        const auto at = fields[rng() % 4];
        const std::uint32_t d = at == 28 ? 1u : 8u;
        put32(w, at, (rng() & 1) ? get32(w, at) + d : get32(w, at) - d);
        break; }
    case 8: { // truncate / extend
        if (rng() & 1) { if (w.size() > kHeader) w.resize(w.size() - 1 - rng() % std::min<std::size_t>(16, w.size() - kHeader)); }
        else w.append(std::size_t(1 + rng() % 16), char(rng()));
        if (rng() & 1) fix_header_sizes(w);
        break; }
    case 10: { // extent table entry
        if (h.extent_count == 0 || h.extent_count > body / 8) break;
        const auto e = kHeader + std::size_t(rng() % h.extent_count) * 8 + (rng() & 1) * 4;
        put32(w, e, rng() % 2 ? get32(w, e) + 8 : get32(w, e) - 8);
        break; }
    case 11: case 12: // a gap over arbitrary image bytes
        cut_gap(w, rng);
        break;
    case 9: { // header_bytes, the version or the root (a dense wire cut to its header)
        switch (rng() % 3) {
        case 0: put32(w, 4, (rng() & 1) ? kHeader + 8 : kHeader - 8); break;
        case 1: w[2 + rng() % 2] = char(w[2 + (rng() % 2)] ^ (1 + rng() % 3)); break;
        default:
            if (h.extent_count == 0) { w.resize(kHeader); fix_header_sizes(w); }
            break;
        }
        break; }
    }
}

// clone_into() target reused for every case (same maximum as typed_load).
Buffer& clone_target() {
    static Buffer buffer = Buffer::create(
        4096, xo::XBuffer::maximum_capacity(kLimit).with_growth(xo::XGrowPolicy::Automatic));
    return buffer;
}

template <class B>
void same_as_source(const B& src, const B& copy, const char* what) {
    const auto used = std::size_t(src.arena()->used_end);
    if (copy.arena()->used_end != src.arena()->used_end || std::memcmp(copy.data(), src.data(), used) != 0)
        fail(std::string(what) + " payload differs from its source");
    if (copy.free_table_complete() != src.free_table_complete() ||
        copy.free_block_count() != src.free_block_count() || copy.free_block_bytes() != src.free_block_bytes())
        fail(std::string(what) + " free table differs from its source");
}

// The mutation streams are copies, so the case's own stream is not advanced.
// A template so the core still builds against libraries without clone()
// (validation_outcomes); test_v2_fuzz asserts that the check is active.
template <class B>
void check_clone(B& buf, const std::string& saved, const std::mt19937_64& rng) {
    if constexpr (requires { buf.clone(); buf.clone_into(buf); }) {
        auto copy = buf.clone();
        buf.clone_into(clone_target());
        same_as_source(buf, copy, "clone()");
        same_as_source(buf, clone_target(), "clone_into()");
        if (copy.save_verified() != saved) fail("clone() re-save differs");
        if (clone_target().save_verified() != saved) fail("clone_into() re-save differs");
        auto r1 = rng, r2 = rng, r3 = rng;
        mutate_valid(plain_root_of(buf), r1);
        mutate_valid(plain_root_of(copy), r2);
        mutate_valid(plain_root_of(clone_target()), r3);
        const auto next = buf.save_verified();
        if (copy.save_verified() != next || clone_target().save_verified() != next)
            fail("clone diverged from its source under the same mutation");
    }
}

// save_compact_into with assignments (members of `buf`'s root taken from
// another valid buffer, or from `buf` itself) must write exactly the
// save_compact wire of a clone with those members assigned, leave `buf`
// unchanged, and load. Works on a copy of the stream, so the case's own
// stream is not advanced; compiled only when the library has it (the core
// also builds against older libraries for validation_outcomes).
void check_composed([[maybe_unused]] Buffer& buf, [[maybe_unused]] const std::string& saved,
                    [[maybe_unused]] const std::mt19937_64& rng) {
    auto r = rng;
    auto src = typed_load(build_wire(r));
    if (!src) fail("composed: generated source wire rejected");
    if (r() % 2) mutate_valid(plain_root_of(*src), r);
    auto& d = plain_root_of(buf);
    const auto& s = plain_root_of(*src);
    const auto options = r() % 2 ? xo::XCompactOptions{.slack_div = 0} : xo::XCompactOptions{};
    const auto form = r() % 6;
    auto clone = typed_load(saved);
    if (!clone) fail("composed: clone of the case rejected");
    auto& c = plain_root_of(*clone);
    std::string out(std::size_t(r() % 2 ? 0 : r() % 4096), char(0xA5)); // stale bytes must not survive
    switch (form) {
    case 0: c.items = s.items;
            buf.save_compact_into(out, options, xo::xassign(d.items, s.items)); break;
    case 1: c.title = s.title; c.tags = s.tags;
            buf.save_compact_into(out, options, xo::xassign(d.title, s.title), xo::xassign(d.tags, s.tags)); break;
    case 2: c.epoch = s.epoch; c.items = s.items; c.attrs = s.attrs;
            buf.save_compact_into(out, options, xo::xassign(d.epoch, s.epoch), xo::xassign(d.items, s.items),
                                  xo::xassign(d.attrs, s.attrs)); break;
    case 3: c.keys = s.keys; c.attrs = s.attrs; c.title = s.title;
            buf.save_compact_into(out, options, xo::xassign(d.keys, s.keys), xo::xassign(d.attrs, s.attrs),
                                  xo::xassign(d.title, s.title)); break;
    case 4: clone.emplace(*typed_load(src->save_verified()));
            buf.save_compact_into(out, options, xo::xassign(d, s)); break;
    default: c.items = s.items; // tags from the draft itself
            buf.save_compact_into(out, options, xo::xassign(d.tags, d.tags), xo::xassign(d.items, s.items)); break;
    }
    if (out != clone->save_compact(options))
        fail("composed wire (form " + std::to_string(form) + ") differs from assign + save_compact");
    if (buf.save_verified() != saved) fail("save_compact_into with assignments changed the draft");
    auto loaded = typed_load(out);
    if (!loaded) fail("composed wire rejected by load_verified");
    if (dump(plain_root_of(*loaded)) != dump(plain_root_of(*clone))) fail("composed wire loads different values");
    ++composed_comparisons;
}

std::string exercise(Buffer& buf, std::mt19937_64& rng) {
    auto& r = plain_root_of(buf);
    std::uint64_t sink = r.epoch + r.title.size();
    for (const auto& it : r.items) { sink += it.id + it.name.size(); for (auto v : it.vals) sink += v; }
    for (const auto& t : r.tags) sink += t.size();
    for (auto k : r.keys) sink += k;
    for (const auto& kv : r.attrs) sink += kv.first.size() + kv.second;
    if (sink == 42) std::cerr << "";
    mutate_valid(r, rng);
    const auto before = dump(r);
    const auto saved = buf.save_verified();
    auto again = typed_load(saved);
    if (!again) fail("re-saved wire after mutating an accepted input was rejected");
    if (dump(plain_root_of(*again)) != before) fail("re-saved wire changed semantics");
    if (again->save_verified() != saved) fail("write-read-write not byte stable after mutation");
    check_composed(buf, saved, rng);
    check_clone(buf, saved, rng);
    return saved;
}

// The mutations `exercise` applies to a VM buffer, applied with the same rng
// to a pinned slot: the re-saved wire must be the same bytes.
void check_slot_mutation(const std::string& w, std::mt19937_64 rng, const std::string& saved_vm,
                         const std::string& where) {
    const auto cap = xo::XBuffer::maximum_capacity(xo::XBufferCore::SLOT_MAX).with_growth(xo::XGrowPolicy::Automatic);
    auto slot = Buffer(xo::XBuffer::load_verified<fz::Root>(w, cap));
    if (slot.storage_kind() != xo::XStorageKind::PinnedSlot) fail(where + ": expected a pinned slot");
    mutate_valid(plain_root_of(slot), rng);
    if (slot.save_verified() != saved_vm) fail(where + ": mutation on a pinned slot re-saves a different wire");
}

// A sparse wire must have exactly the outcome (and error) of the dense wire
// of the payload it loads, and an accepted one must load that payload.
template <class O>
void compare_inflated(const std::string& w, std::optional<Buffer>& typed, const std::string& terr,
                      const O& o, const std::string& where) {
    xo::XWireHeader h{}; std::memcpy(&h, w.data(), std::min(w.size(), kHeader));
    if (w.size() < kHeader || h.extent_count == 0 || o.stage == Oracle::Stage::Header) return;
    const auto dense = inflate(w);
    if (!dense) fail(where + ": header-valid sparse wire does not inflate");
    std::string derr;
    auto d = typed_load(*dense, &derr);
    if (bool(d) != bool(typed) || derr != terr)
        fail(where + ": sparse " + (typed ? "accept" : "reject(" + terr + ")") + " vs dense " +
             (d ? "accept" : "reject(" + derr + ")"));
    if (typed && typed->save_verified() != d->save_verified())
        fail(where + ": sparse and dense wires loaded different payloads");
    ++inflated_comparisons;
}

enum class Outcome { Accepted, HeaderReject, PayloadReject };

// One input through the oracle and all library acceptors. They must agree;
// an accepted input is then read, mutated and re-saved (see exercise()).
Outcome check_case(const std::string& w, std::mt19937_64& rng, const std::string& id) {
    const auto o = oracle(w);
    std::string terr;
    auto typed = typed_load(w, &terr);
    outcome_add("T", bool(typed), terr);
    if (bool(typed) != o.ok)
        fail("case " + id + ": typed=" + (typed ? "accept" : "reject(" + terr + ")") +
             " oracle=" + (o.ok ? "accept" : "reject(" + o.why + ")"));
    std::string lerr;
    const bool reload = reload_accepts(w, &lerr);
    outcome_add("L", reload, lerr);
    if (reload != bool(typed))
        fail("case " + id + ": reload=" + (typed ? "reject(" + lerr + ")" : "accept") +
             " typed=" + (typed ? "accept" : "reject(" + terr + ")"));
    if (typed) compare_reload(*typed);
    check_direct(w, bool(typed), typed ? &*typed : nullptr, "case " + id);
    check_admit(w, o, typed ? &*typed : nullptr, "case " + id);
    check_view(w, o, typed ? &*typed : nullptr, "case " + id);
    compare_inflated(w, typed, terr, o, "case " + id);
    {
        // load_verified with an upstream: the validation bitmaps come from it (a monotonic
        // resource over a small stack buffer, so large payloads spill), with
        // the same verdict, message and payload.
        alignas(std::max_align_t) unsigned char arena[2048];
        std::pmr::monotonic_buffer_resource upstream(arena, sizeof arena);
        auto viar = xo::XBuffer::try_load_verified<fz::Root>(
            std::span<const std::byte>(reinterpret_cast<const std::byte*>(w.data()), w.size()),
            xo::XBuffer::maximum_capacity(kLimit), &upstream);
        const std::string rmsg = viar ? std::string() : viar.error().message;
        if (bool(viar) != bool(typed) || (!typed && rmsg != terr))
            fail("case " + id + ": load_verified with upstream=" + (viar ? "accept" : "reject(" + rmsg + ")") +
                 " typed=" + (typed ? "accept" : "reject(" + terr + ")"));
        if (typed && xo::TypedXBuffer<fz::Root>(std::move(*viar)).save_verified() != typed->save_verified())
            fail("case " + id + ": load_verified with upstream loaded a different payload");
    }
    check_slots(w, bool(typed), typed ? &*typed : nullptr, "case " + id);
    if (o.ok) {
        const auto before = rng;
        const bool fits = typed->get_size() <= xo::XBufferCore::SLOT_MAX / 4;
        const auto saved = exercise(*typed, rng);
        if (fits) check_slot_mutation(w, before, saved, "case " + id);
        return Outcome::Accepted;
    }
    return o.stage == Oracle::Stage::Header ? Outcome::HeaderReject : Outcome::PayloadReject;
}

// A library-generated valid wire must pass the oracle and be golden
// (load -> save byte-identical).
void check_golden(const std::string& w, const std::string& where) {
    const auto o = oracle(w);
    if (!o.ok) fail(where + ": oracle rejects a library-generated wire: " + o.why);
    auto loaded = typed_load(w);
    if (!loaded) fail(where + ": typed load rejects a library-generated wire");
    if (!reload_accepts(w, nullptr)) fail(where + ": reload_verified rejects a library-generated wire");
    compare_reload(*loaded);
    check_direct(w, true, &*loaded, where);
    check_slots(w, true, &*loaded, where);
    // w may be any of the three forms; the default re-save carries the same
    // image and is itself byte stable.
    const auto again = loaded->save_verified();
    if (dense_of(again) != dense_of(w)) fail(where + ": golden write-read-write changed the payload");
    auto reloaded = typed_load(again);
    if (!reloaded || reloaded->save_verified() != again) fail(where + ": golden write-read-write not byte stable");
    // The default emission (docs/WIRE_FORMAT.md section 3, free_runs): dense
    // below 4 KiB of free blocks; else the free blocks of at least 256 bytes,
    // sorted, merge where adjacent, and in every merged run of at least
    // 4 KiB each block's body (all but its link word) is skipped. Nothing
    // else is.
    const auto ext = extents_of(again);
    if (!ext) fail(where + ": default emission has a malformed extent table");
    const auto image = dense_of(again);
    xo::XWireHeader h; std::memcpy(&h, image.data(), kHeader);
    OracleRun run;
    run.p = reinterpret_cast<const unsigned char*>(image.data());
    run.used = std::size_t(h.used_bytes);
    if (!free_regions(run, h.free_dir, h.free_bytes)) fail(where + ": re-saved free directory: " + run.why);
    std::vector<Range> blocks;
    std::uint64_t candidates = 0;
    for (std::size_t i = h.free_dir ? 1 : 0; i < run.ranges.size(); ++i)
        if (run.ranges[i].end - run.ranges[i].begin >= 256) {
            blocks.push_back({run.ranges[i].begin, run.ranges[i].end});
            candidates += run.ranges[i].end - run.ranges[i].begin;
        }
    std::sort(blocks.begin(), blocks.end());
    std::vector<Range> expected;
    if (h.free_bytes >= 4096 && candidates >= 4096) {
        for (std::size_t first = 0; first < blocks.size();) {
            std::size_t last = first + 1;
            while (last < blocks.size() && blocks[last].first == blocks[last - 1].second) ++last;
            if (blocks[last - 1].second - blocks[first].first >= 4096)
                for (std::size_t k = first; k < last; ++k) expected.push_back({blocks[k].first + 8, blocks[k].second});
            first = last;
        }
    }
    std::vector<Range> skipped;
    std::uint64_t prev = kBegin;
    for (auto [b, e] : *ext) {
        if (b > prev) skipped.push_back({prev, b});
        prev = e;
    }
    if (prev < h.used_bytes) skipped.push_back({prev, h.used_bytes});
    if (skipped != expected)
        fail(where + ": default emission skipped " + std::to_string(skipped.size()) + " ranges, expected " +
             std::to_string(expected.size()) + " free block bodies");
}

// The same content compacted: blocks in visiting preorder, no holes.
std::string canonical_wire(const std::string& w) {
    auto loaded = xo::XBuffer::load_verified<fz::Root>(w);
    auto compact = xo::XCompactor::compact<fz::Root>(loaded);
    return compact.save_verified<fz::Root>();
}

// count built wires, and the compacted form of every second one.
std::vector<std::string> seed_pool(std::mt19937_64& rng, std::size_t count) {
    std::vector<std::string> pool;
    for (std::size_t i = 0; i < count; ++i) {
        auto w = build_wire(rng);
        check_golden(w, "golden " + std::to_string(i));
        if (i % 2 == 0) {
            auto c = canonical_wire(w);
            check_golden(c, "canonical " + std::to_string(i));
            pool.push_back(std::move(c));
        }
        pool.push_back(std::move(w));
    }
    return pool;
}

// Same content for every call with the same rng; only the vector length varies.
std::string sized_wire(std::mt19937_64 rng, std::size_t vals) {
    auto buf = Buffer::create(4096, xo::XBuffer::maximum_capacity(kLimit).with_growth(xo::XGrowPolicy::Automatic));
    auto& r = plain_root_of(buf);
    r.epoch = rng();
    r.title = random_text(rng, 30);
    for (int i = int(rng() % 4); i > 0; --i) r.tags.emplace_back(random_text(rng, 20), r.tags.get_stored_allocator());
    for (int i = int(rng() % 6); i > 0; --i) r.keys.insert(std::uint32_t(rng() % 1000));
    auto& it = r.items.emplace_back();
    it.id = std::uint32_t(rng());
    it.name = random_text(rng, 30);
    it.vals.reserve(vals);
    for (std::size_t i = 0; i < vals; ++i) it.vals.push_back(std::uint16_t(rng()));
    return buf.save_verified();
}

// Wires around the validator's inline bitmap limit (payloads up to
// inline_extent keep both granule bitmaps inside the context, larger ones take
// host words). [0, 3): valid wires with used = page - 8, page, page + 8.
// Library wires keep used a multiple of 8, so [3, 5) patch the page-sized wire
// to used = page - 1 (last payload byte dropped) and page + 1 (one zero byte
// appended); they are mutated inputs.
std::vector<std::string> page_boundary_wires(std::mt19937_64& rng) {
    // WireGranuleIndex::inline_extent (test_v2_fuzz checks it); spelled out so
    // the core also builds against libraries without it (validation_outcomes).
    constexpr std::uint64_t page = kInlineBitmapExtent;
    const auto content = rng;
    rng.discard(1);
    std::vector<std::string> out;
    // Two bytes per value; the rest of the root is well under 1200 bytes.
    for (std::size_t vals = page / 2 - 600; out.size() < 3; ++vals) {
        auto w = sized_wire(content, vals);
        xo::XWireHeader h; std::memcpy(&h, w.data(), kHeader);
        const auto target = page - 8 + 8 * out.size();
        if (h.used_bytes > page + 8) fail("no library wire with used = " + std::to_string(target));
        if (h.used_bytes != target) continue;
        if (h.extent_count != 0) fail("page boundary wire is sparse");
        check_golden(w, "page boundary used=" + std::to_string(target));
        out.push_back(std::move(w));
    }
    // A dense wire is the image, so the patch is at image offset page - 1
    // (dropped) or page (one zero byte added); used_bytes follows the length.
    const auto& at_page = out[1];
    auto shorter = at_page;
    shorter.erase(page - 1, 1);
    fix_header_sizes(shorter);
    auto longer = at_page;
    longer.insert(page, 1, '\0');
    fix_header_sizes(longer);
    out.push_back(std::move(shorter));
    out.push_back(std::move(longer));
    return out;
}

// Seed corpus for fuzz_v2_wire: the golden pool plus the page boundary wires,
// one file per wire. Returns the number written.
std::size_t write_v2_seed_corpus(const std::string& dir) {
    std::mt19937_64 rng(20261021);
    std::filesystem::create_directories(dir);
    auto pool = seed_pool(rng, 64);
    for (auto& w : page_boundary_wires(rng)) pool.push_back(std::move(w));
    for (std::size_t i = 0; i < pool.size(); ++i) {
        const auto path = dir + "/seed-" + std::to_string(i) + ".wire";
        std::FILE* f = std::fopen(path.c_str(), "wb");
        if (!f || std::fwrite(pool[i].data(), 1, pool[i].size(), f) != pool[i].size() || std::fclose(f) != 0)
            fail("cannot write " + path);
    }
    return pool.size();
}
} // namespace
