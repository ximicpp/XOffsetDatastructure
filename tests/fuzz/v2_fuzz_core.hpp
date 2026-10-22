// Shared core of the V2 validator fuzzers: test_v2_fuzz (seeded mutation,
// runs in CTest) and fuzz_v2_wire (libFuzzer, coverage guided).
//
// Builds random valid V2 wires (inline/heap strings, nested vectors, set, map,
// holes left by mutation), then feeds mutated wires to five acceptors:
//   1. an independent spec oracle written here from the V2 format rules,
//   2. the typed validator (XBuffer::load_verified<T>),
//   3. the raw validator (receive_raw::validate<T>, flat),
//   4. the verified receive path (begin_verified_receive/finish_verified),
//   5. the in-place reload (XBuffer::reload_verified<T>) into ONE long-lived
//      buffer: it must accept exactly what load_verified accepts, and an
//      accepted reload must hold byte-identical payload (in particular gap and
//      tail bytes left by every earlier reload read as zero) and re-save
//      byte-identically; after a reject typed root access must throw.
// Every acceptor must agree on accept/reject (raw/receive only see inputs whose
// wire header is well formed). Accepted inputs are then read, mutated and
// re-saved; the re-saved wire must load again and round-trip byte-identically.
#pragma once
#include "xoffsetdatastructure.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <memory_resource>
#include <optional>
#include <random>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace xo = XOffsetDatastructure;
namespace rr = xo::detail::receive_raw;

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

XOFFSET_REGISTER_SCHEMA_NAME(fz::Root, "fuzz.v2.root")

namespace XOffsetDatastructure {
template<> struct wire_record_layout<fz::Item> { static constexpr bool ordinary_members = true; };
template<> struct wire_record_layout<fz::Root> { static constexpr bool ordinary_members = true; };
}

namespace {
using Buffer = xo::TypedXBuffer<fz::Root>;
constexpr std::size_t kHeader = sizeof(xo::XWireHeaderV2);
constexpr std::uint32_t kMaxPayload = 0x7ffffff8u;

// Every acceptor and the oracle use this payload limit. An input that declares
// more is a header-stage rejection (threat model R1: a short sparse wire may
// declare a huge payload).
constexpr std::size_t kLimit = std::size_t(1) << 22;

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
    bool binding(std::size_t head) {
        if (rd<std::int32_t>(head) == 0) return bad("unbound head");
        auto t = rel(head);
        if (!t || *t != 0) return bad("binding not payload base");
        return true;
    }
    bool string(std::size_t h, std::string* out = nullptr) {
        if (heads) heads->push_back(h);
        if (!binding(h)) return false;
        const auto cap = rd<std::uint32_t>(h + 12);
        if (cap & 0x80000000u) {
            const unsigned len = (cap >> 24) & 0x7fu;
            if (len > 10) return bad("inline length");
            for (unsigned i = len; i < 11; ++i)
                if (p[h + 4 + i] != 0) return bad("inline padding");
            if (out) out->assign(reinterpret_cast<const char*>(p + h + 4), len);
            return true;
        }
        const auto size = rd<std::uint32_t>(h + 8);
        if (size > cap || cap >= kMaxPayload) return bad("string size/cap");
        if (cap == 0) {
            if (size != 0 || rd<std::int32_t>(h + 4) != 0) return bad("empty string payload");
            if (out) out->clear();
            return true;
        }
        auto d = rel(h + 4);
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
        if (!binding(h)) return false;
        const auto size = rd<std::uint32_t>(h + 8), cap = rd<std::uint32_t>(h + 12);
        if (size > cap || cap > (kMaxPayload - 8u) / sizeof(Elem)) return bad("vector size/cap");
        if (cap == 0) {
            if (size != 0 || rd<std::int32_t>(h + 4) != 0) return bad("empty vector payload");
            return true;
        }
        auto d = rel(h + 4);
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

Oracle oracle(const std::string& wire, bool collect_heads = false) {
    Oracle result;
    auto reject = [&](Oracle::Stage stage, std::string why) {
        result.ok = false; result.stage = stage; result.why = std::move(why); return result;
    };
    if (wire.size() < kHeader) return reject(Oracle::Stage::Header, "short");
    xo::XWireHeaderV2 h;
    std::memcpy(&h, wire.data(), kHeader);
    if (std::memcmp(h.magic, xo::XWIRE_MAGIC_V2, 8) != 0) return reject(Oracle::Stage::Header, "magic");
    if (h.format_major != 2 || h.format_minor != 0 || h.header_size != kHeader ||
        h.endian_tag != 0x01020304u || h.flags != 0 || h.crc32c != 0 || h.reserved0 != 0)
        return reject(Oracle::Stage::Header, "header fields");
    if (h.root_type_id != xo::detail::wire_root_type_id<fz::Root>() ||
        h.schema_hash_lo != xo::detail::wire_schema_hash<fz::Root>() || h.schema_hash_hi != 0)
        return reject(Oracle::Stage::Header, "schema");
    const std::uint64_t body = wire.size() - kHeader;
    if (h.used_bytes > kMaxPayload || h.extent_count > body / 8) return reject(Oracle::Stage::Header, "sizes");
    if (h.used_bytes > kLimit) return reject(Oracle::Stage::Header, "declared size above limit");
    // Extent table (sparse) -> gaps; stored bytes.
    const auto* table = reinterpret_cast<const unsigned char*>(wire.data()) + kHeader;
    std::uint64_t stored = h.used_bytes, end = 0;
    std::vector<std::pair<std::uint64_t, std::uint64_t>> extents, gaps;
    if (h.extent_count) {
        stored = 0;
        for (std::uint64_t i = 0; i < h.extent_count; ++i) {
            std::uint32_t off, b; std::memcpy(&off, table + i * 8, 4); std::memcpy(&b, table + i * 8 + 4, 4);
            if (b == 0 || off % 8 || b % 8 || (i == 0 ? off != 0 : off <= end) || std::uint64_t(off) + b > h.used_bytes)
                return reject(Oracle::Stage::Header, "extent table");
            if (i) gaps.push_back({end, off});
            extents.push_back({off, b});
            end = std::uint64_t(off) + b; stored += b;
        }
        if (end < h.used_bytes) gaps.push_back({end, h.used_bytes});
        if (gaps.empty()) return reject(Oracle::Stage::Header, "extent table");
    }
    const auto tbytes = h.extent_count * 8;
    if (stored > body - tbytes) return reject(Oracle::Stage::Header, "sizes");
    const auto rest = body - tbytes - stored;
    if (h.hole_count > rest / 8 || h.hole_count * 8 != rest) return reject(Oracle::Stage::Header, "sizes");
    if (h.root_offset >= h.used_bytes) return reject(Oracle::Stage::Header, "root offset");
    std::uint64_t hole_sum = 0;
    const auto* holes = table + tbytes + stored;
    for (std::uint64_t i = 0; i < h.hole_count; ++i) {
        std::uint32_t b; std::memcpy(&b, holes + i * 8 + 4, 4); hole_sum += b;
    }
    if (hole_sum != h.hole_bytes) return reject(Oracle::Stage::Header, "hole bytes");
    // Payload stage (also what the raw/receive validators see). Gaps read as 0.
    std::vector<unsigned char> payload(std::size_t(h.used_bytes), 0);
    if (h.extent_count == 0 && !payload.empty()) std::memcpy(payload.data(), table, payload.size());
    else {
        const auto* src = table + tbytes;
        for (auto [off, b] : extents) { std::memcpy(payload.data() + off, src, b); src += b; }
    }
    OracleRun run;
    run.p = payload.data();
    run.used = std::size_t(h.used_bytes);
    if (collect_heads) run.heads = &result.heads;
    auto preject = [&](std::string why) { return reject(Oracle::Stage::Payload, std::move(why)); };
    if (run.used < 48) return preject("payload too small");
    if (std::memcmp(run.p, "XBUFV2\0\0", 8) != 0 || run.rd<std::uint32_t>(8) != 48)
        return preject("payload header");
    if (run.rd<std::uint64_t>(24) != run.used) return preject("used_end");
    const auto aroot = run.rd<std::uint64_t>(16);
    if (aroot == 0 || aroot > run.used || sizeof(fz::Root) > run.used - aroot || aroot % 8)
        return preject("root not found");
    if (aroot != h.root_offset) return preject("root offset mismatch");
    if (run.rd<std::uint32_t>(12) != 0 || run.rd<std::uint64_t>(32) != 0 || run.rd<std::uint64_t>(40) != 0)
        return preject("payload flags");
    run.ranges.push_back({0, 48});
    for (std::uint64_t i = 0; i < h.hole_count; ++i) {
        std::uint32_t off, b;
        std::memcpy(&off, holes + i * 8, 4); std::memcpy(&b, holes + i * 8 + 4, 4);
        if (b == 0 || b % 8 || off % 8 || off < 48 || off > run.used || b > run.used - off)
            return preject("hole entry");
        run.ranges.push_back({off, std::uint64_t(off) + b});
    }
    if (!run.add(aroot, (sizeof(fz::Root) + 7) & ~std::size_t(7))) return preject(run.why);
    if (!run.root(std::size_t(aroot))) return preject(run.why);
    std::sort(run.ranges.begin(), run.ranges.end(),
              [](auto a, auto b) { return a.begin < b.begin; });
    std::uint64_t covered = 0;
    for (std::size_t i = 0; i < run.ranges.size(); ++i) {
        if (i && run.ranges[i].begin < run.ranges[i - 1].end) return preject("overlap");
        covered += run.ranges[i].end - run.ranges[i].begin;
    }
    if (covered != run.used) return preject("coverage");
    // Every gap byte inside a hole.
    std::vector<std::pair<std::uint64_t, std::uint64_t>> free;
    for (std::uint64_t i = 0; i < h.hole_count; ++i) {
        std::uint32_t off, b; std::memcpy(&off, holes + i * 8, 4); std::memcpy(&b, holes + i * 8 + 4, 4);
        free.push_back({off, std::uint64_t(off) + b});
    }
    std::sort(free.begin(), free.end());
    for (auto [gb, ge] : gaps) {
        std::uint64_t pos = gb;
        for (auto [fb, fe] : free) { if (fb <= pos && fe > pos) pos = fe; if (pos >= ge) break; }
        if (pos < ge) return preject("gap not free");
    }
    return result;
}

// ---------------------------------------------------------------------------
// Library acceptors.
// ---------------------------------------------------------------------------
template <class Root = fz::Root>
std::optional<xo::TypedXBuffer<Root>> typed_load(const std::string& wire, std::string* error = nullptr) {
    try {
        // Growable so accepted inputs can be mutated afterwards.
        return xo::TypedXBuffer<Root>(xo::XBuffer::load_verified<Root>(
            wire, xo::XBuffer::maximum_capacity(kLimit).with_growth(xo::XGrowPolicy::Automatic)));
    } catch (const xo::XException& e) {
        if (error) *error = e.what();
        return std::nullopt;
    }
}

// Acceptor 5: one buffer reused for every case (same maximum as typed_load).
Buffer& reloader() {
    static Buffer buffer = Buffer::create(
        4096, xo::XBuffer::maximum_capacity(kLimit).with_growth(xo::XGrowPolicy::Automatic));
    return buffer;
}

bool reload_accepts(const std::string& wire, std::string* error) {
    try {
        reloader().reload_verified(wire);
        return true;
    } catch (const xo::XException& e) {
        if (error) *error = e.what();
        bool refused = false;
        try { (void)reloader().root(); } catch (const xo::XException&) { refused = true; }
        if (!refused) fail("root() succeeded after a rejected reload_verified");
        if (reloader().has_root()) fail("has_root() after a rejected reload_verified");
        return false;
    }
}

// Accepted reload vs fresh load of the same wire.
void compare_reload(Buffer& fresh) {
    auto& again = reloader();
    const auto a = fresh.get_buffer(), b = again.get_buffer();
    if (a.size() != b.size() || std::memcmp(a.data(), b.data(), a.size()) != 0)
        fail("reload_verified payload differs from load_verified (stale bytes?)");
    if (fresh.free_block_count() != again.free_block_count() ||
        fresh.free_block_bytes() != again.free_block_bytes())
        fail("reload_verified free table differs from load_verified");
    // Newly exposed tail bytes must read as zero after any earlier content.
    const auto size = again.get_size();
    if (!again.grow(4096)) fail("reloaded buffer cannot grow");
    for (std::size_t i = size; i < again.get_size(); ++i)
        if (again.data()[i] != 0) fail("stale byte exposed after reload_verified growth");
    if (again.save_verified() != fresh.save_verified())
        fail("reload_verified re-save differs from load_verified re-save");
}

// Header-valid wire -> {aligned payload with gaps zeroed, gaps, hole pointer}.
struct Unpacked {
    std::vector<std::uint64_t> storage;
    std::vector<xo::XBufferCore::WireHole> gaps;
    const char* holes{};
    const char* table{};
    xo::XWireHeaderV2 h{};
    char* base() { return reinterpret_cast<char*>(storage.data()); }
};
Unpacked unpack(const std::string& wire) {
    Unpacked u;
    std::memcpy(&u.h, wire.data(), kHeader);
    const auto used = std::size_t(u.h.used_bytes);
    u.storage.assign((used + 7) / 8 + 1, 0);
    const char* p = wire.data() + kHeader;
    u.table = p;
    if (u.h.extent_count == 0) { std::memcpy(u.base(), p, used); u.holes = p + used; return u; }
    const char* src = p + u.h.extent_count * 8;
    std::uint64_t end = 0;
    for (std::uint64_t i = 0; i < u.h.extent_count; ++i) {
        std::uint32_t off, b; std::memcpy(&off, p + i * 8, 4); std::memcpy(&b, p + i * 8 + 4, 4);
        if (i) u.gaps.push_back({std::uint32_t(end), std::uint32_t(off - end)});
        std::memcpy(u.base() + off, src, b); src += b; end = std::uint64_t(off) + b;
    }
    if (end < used) u.gaps.push_back({std::uint32_t(end), std::uint32_t(used - end)});
    u.holes = src;
    return u;
}

template <class Root = fz::Root>
bool raw_accepts(const std::string& wire, std::string* error) {
    auto u = unpack(wire);
    const auto used = std::size_t(u.h.used_bytes);
    auto* base = u.base();
    try {
        auto proof = rr::validate<Root>(base, used, u.h.root_offset, u.holes,
                                            std::size_t(u.h.hole_count), std::pmr::new_delete_resource(),
                                            u.gaps.data(), u.gaps.size());
        (void)proof;
        return true;
    } catch (const xo::XException& e) {
        if (error) *error = e.what();
        return false;
    }
}

template <class Root = fz::Root>
bool receive_accepts(const std::string& wire, std::string* error) {
    auto u = unpack(wire);
    const auto used = std::size_t(u.h.used_bytes);
    try {
        auto token = xo::begin_verified_receive<Root>(used, xo::XBuffer::maximum_capacity(kLimit), std::size_t(u.h.hole_count),
                                                          std::size_t(u.h.extent_count));
        {
            auto op = token.acquire_operation();
            auto hb = op.header_bytes(); std::memcpy(hb.data(), wire.data(), kHeader);
            // Scatter: write only extents (gap bytes stay as received storage).
            auto pb = op.payload_bytes();
            if (u.h.extent_count == 0) { if (used) std::memcpy(pb.data(), u.base(), used); }
            else {
                std::uint64_t pos = 0;
                for (const auto& g : u.gaps) {
                    std::memcpy(pb.data() + pos, u.base() + pos, g.offset - pos);
                    pos = std::uint64_t(g.offset) + g.bytes;
                }
                if (pos < used) std::memcpy(pb.data() + pos, u.base() + pos, used - pos);
            }
            // Empty spans have null data(); memcpy requires non-null pointers.
            auto eb = op.extent_bytes(); if (!eb.empty()) std::memcpy(eb.data(), u.table, eb.size());
            auto lb = op.hole_bytes(); if (!lb.empty()) std::memcpy(lb.data(), u.holes, lb.size());
            op.complete(kHeader, used, lb.size(), eb.size());
        }
        auto received = xo::finish_verified<Root>(token);
        (void)received;
        return true;
    } catch (const xo::XException& e) {
        if (error) *error = e.what();
        return false;
    }
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

std::string build_wire(std::mt19937_64& rng) {
    auto buf = Buffer::create(4096, xo::XBuffer::maximum_capacity(kLimit).with_growth(xo::XGrowPolicy::Automatic));
    auto& r = buf.root();
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
    return buf.save_verified();
}

// ---------------------------------------------------------------------------
// Mutations of a valid wire.
// ---------------------------------------------------------------------------
// Bytes before the hole table (extent table + stored payload), or nullopt.
std::optional<std::uint64_t> data_bytes(const std::string& w, const xo::XWireHeaderV2& h) {
    const auto body = w.size() - kHeader;
    if (h.extent_count == 0) return h.used_bytes <= body ? std::optional<std::uint64_t>(h.used_bytes) : std::nullopt;
    if (h.extent_count > body / 8) return std::nullopt;
    std::uint64_t stored = 0;
    for (std::uint64_t i = 0; i < h.extent_count; ++i) {
        std::uint32_t b; std::memcpy(&b, w.data() + kHeader + i * 8 + 4, 4); stored += b;
    }
    const auto total = h.extent_count * 8 + stored;
    return total <= body ? std::optional<std::uint64_t>(total) : std::nullopt;
}

void fix_header_sizes(std::string& w) {
    xo::XWireHeaderV2 h; std::memcpy(&h, w.data(), kHeader);
    const auto body = w.size() - kHeader;
    const auto data = data_bytes(w, h);
    if (!data) return; // leave the inconsistent header as is
    h.hole_count = (body - *data) / 8;
    std::uint64_t sum = 0;
    for (std::uint64_t i = 0; i < h.hole_count; ++i) {
        std::uint32_t b; std::memcpy(&b, w.data() + kHeader + *data + i * 8 + 4, 4); sum += b;
    }
    h.hole_bytes = sum;
    std::memcpy(w.data(), &h, kHeader);
}

// File position of a payload offset (nullopt inside a sparse gap).
std::optional<std::size_t> file_pos(const std::string& w, std::uint64_t off) {
    xo::XWireHeaderV2 h; std::memcpy(&h, w.data(), kHeader);
    if (h.extent_count == 0) return kHeader + off;
    // Division form: extent_count * 8 can wrap after a header mutation.
    if (h.extent_count > (w.size() - kHeader) / 8) return std::nullopt;
    std::uint64_t cum = 0;
    for (std::uint64_t i = 0; i < h.extent_count; ++i) {
        std::uint32_t eo, eb; std::memcpy(&eo, w.data() + kHeader + i * 8, 4); std::memcpy(&eb, w.data() + kHeader + i * 8 + 4, 4);
        if (off >= eo && off < std::uint64_t(eo) + eb) return kHeader + h.extent_count * 8 + cum + (off - eo);
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

void mutate_wire(std::string& w, std::mt19937_64& rng, const std::vector<std::size_t>& heads) {
    xo::XWireHeaderV2 h; std::memcpy(&h, w.data(), kHeader);
    const auto used = std::size_t(std::min<std::uint64_t>(h.used_bytes, w.size() - kHeader));
    // Earlier mutations may have desynchronized the header: clamp.
    h.hole_count = std::min<std::uint64_t>(h.hole_count, (w.size() - kHeader) / 8);
    const auto hole_at = w.size() - std::size_t(h.hole_count) * 8;
    static const std::uint32_t interesting[] = {
        0u, 1u, 4u, 7u, 8u, 9u, 10u, 11u, 16u, 24u, 0x7ffffff8u, 0x7fffffffu, 0x80000000u,
        0x85000000u, 0x8a000000u, 0x8b000000u, 0xff000000u, 0xfffffff8u, 0xfffffff0u};
    switch (rng() % 11) {
    case 0: { // random bit flip anywhere
        const auto at = std::size_t(rng() % w.size());
        w[at] = char(w[at] ^ (1u << (rng() % 8)));
        break; }
    case 1: case 2: case 3: { // targeted head field
        if (heads.empty()) break;
        const auto mapped = file_pos(w, heads[rng() % heads.size()]);
        if (!mapped) break;
        const auto head = *mapped;
        const auto field = std::size_t(rng() % 4) * 4;
        auto v = get32(w, head + field);
        switch (rng() % 4) {
        case 0: v = interesting[rng() % std::size(interesting)]; break;
        case 1: v += std::uint32_t(8 * (1 + rng() % 4)); break;
        case 2: v -= std::uint32_t(8 * (1 + rng() % 4)); break;
        case 3: v ^= 1u << (rng() % 32); break;
        }
        put32(w, head + field, v);
        break; }
    case 4: { // aligned payload word
        if (used < 8) break;
        const auto at = kHeader + (std::size_t(rng() % (used / 4)) * 4);
        put32(w, at, rng() & 1 ? interesting[rng() % std::size(interesting)] : std::uint32_t(rng()));
        break; }
    case 5: { // hole entry field
        if (h.hole_count == 0) break;
        const auto e = hole_at + std::size_t(rng() % h.hole_count) * 8 + (rng() & 1) * 4;
        auto v = get32(w, e);
        v = rng() % 3 == 0 ? v + 8 : rng() % 2 ? v - 8 : interesting[rng() % std::size(interesting)];
        put32(w, e, v);
        if (rng() & 1) fix_header_sizes(w);
        break; }
    case 6: { // delete / duplicate / add a hole entry, with header fix-up
        const auto choice = rng() % 3;
        if (choice == 0 && h.hole_count) {
            const auto e = hole_at + std::size_t(rng() % h.hole_count) * 8;
            w.erase(e, 8);
        } else if (choice == 1 && h.hole_count) {
            const auto e = hole_at + std::size_t(rng() % h.hole_count) * 8;
            w.insert(e, w.substr(e, 8));
        } else {
            std::uint32_t entry[2] = {std::uint32_t((48 + rng() % (used + 8)) & ~7u), std::uint32_t(8 * (1 + rng() % 8))};
            w.append(reinterpret_cast<const char*>(entry), 8);
        }
        fix_header_sizes(w);
        break; }
    case 7: { // header field
        static const std::size_t fields[] = {16, 24, 32, 40};
        const auto at = fields[rng() % 4];
        std::uint64_t v; std::memcpy(&v, w.data() + at, 8);
        v += (rng() & 1) ? 8 : std::uint64_t(-8);
        std::memcpy(w.data() + at, &v, 8);
        break; }
    case 8: { // truncate / extend
        if (rng() & 1) { if (w.size() > kHeader) w.resize(w.size() - 1 - rng() % std::min<std::size_t>(16, w.size() - kHeader)); }
        else w.append(std::size_t(1 + rng() % 16), char(rng()));
        if (rng() & 1) fix_header_sizes(w);
        break; }
    case 10: { // extent table entry
        if (h.extent_count == 0 || h.extent_count > (w.size() - kHeader) / 8) break;
        const auto e = kHeader + std::size_t(rng() % h.extent_count) * 8 + (rng() & 1) * 4;
        put32(w, e, rng() % 2 ? get32(w, e) + 8 : get32(w, e) - 8);
        break; }
    case 9: { // payload header field
        static const std::size_t fields[] = {8, 12, 16, 24, 32, 40};
        const auto mapped = file_pos(w, fields[rng() % 6]);
        if (!mapped) break;
        put32(w, *mapped, get32(w, *mapped) + std::uint32_t(8 * (1 + rng() % 2)));
        break; }
    }
}

void exercise(Buffer& buf, std::mt19937_64& rng) {
    auto& r = buf.root();
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
    if (dump(again->root()) != before) fail("re-saved wire changed semantics");
    if (again->save_verified() != saved) fail("write-read-write not byte stable after mutation");
}

enum class Outcome { Accepted, HeaderReject, PayloadReject };

// One input through the oracle and all library acceptors. They must agree;
// an accepted input is then read, mutated and re-saved (see exercise()).
Outcome check_case(const std::string& w, std::mt19937_64& rng, const std::string& id) {
    const auto o = oracle(w);
    std::string terr, rerr, verr;
    auto typed = typed_load(w, &terr);
    if (bool(typed) != o.ok)
        fail("case " + id + ": typed=" + (typed ? "accept" : "reject(" + terr + ")") +
             " oracle=" + (o.ok ? "accept" : "reject(" + o.why + ")"));
    std::string lerr;
    if (reload_accepts(w, &lerr) != bool(typed))
        fail("case " + id + ": reload=" + (typed ? "reject(" + lerr + ")" : "accept") +
             " typed=" + (typed ? "accept" : "reject(" + terr + ")"));
    if (typed) compare_reload(*typed);
    if (o.ok || o.stage == Oracle::Stage::Payload) {
        const bool raw = raw_accepts(w, &rerr);
        const bool recv = receive_accepts(w, &verr);
        if (raw != o.ok)
            fail("case " + id + ": raw=" + (raw ? "accept" : "reject(" + rerr + ")") +
                 " oracle=" + (o.ok ? "accept" : "reject(" + o.why + ")"));
        if (recv != o.ok)
            fail("case " + id + ": receive=" + (recv ? "accept" : "reject(" + verr + ")") +
                 " oracle=" + (o.ok ? "accept" : "reject(" + o.why + ")"));
    }
    if (o.ok) { exercise(*typed, rng); return Outcome::Accepted; }
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
    if (loaded->save_verified() != w) fail(where + ": golden write-read-write not byte stable");
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
    auto& r = buf.root();
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
    constexpr std::uint64_t page = xo::detail::WireGranuleIndex::inline_extent;
    const auto content = rng;
    rng.discard(1);
    std::vector<std::string> out;
    // Two bytes per value; the rest of the root is well under 1200 bytes.
    for (std::size_t vals = page / 2 - 600; out.size() < 3; ++vals) {
        auto w = sized_wire(content, vals);
        xo::XWireHeaderV2 h; std::memcpy(&h, w.data(), kHeader);
        const auto target = page - 8 + 8 * out.size();
        if (h.used_bytes > page + 8) fail("no library wire with used = " + std::to_string(target));
        if (h.used_bytes != target) continue;
        if (h.extent_count != 0) fail("page boundary wire is sparse");
        check_golden(w, "page boundary used=" + std::to_string(target));
        out.push_back(std::move(w));
    }
    const auto& at_page = out[1];
    xo::XWireHeaderV2 h; std::memcpy(&h, at_page.data(), kHeader);
    auto shorter = at_page;
    shorter.erase(kHeader + page - 1, 1);
    h.used_bytes = page - 1;
    std::memcpy(shorter.data(), &h, kHeader);
    fix_header_sizes(shorter);
    auto longer = at_page;
    longer.insert(kHeader + page, 1, '\0');
    h.used_bytes = page + 1;
    std::memcpy(longer.data(), &h, kHeader);
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
