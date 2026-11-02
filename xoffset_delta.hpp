#pragma once
// Delta sync: send only what changed to a receiver that already holds a copy.
//
// A sender keeps one XDeltaBaseline per receiver (or per group of receivers
// that always get the same deltas): a copy of the payload as that receiver
// holds it. XBuffer::save_delta_into<T> compares the buffer with it and
// writes the payload byte runs that differ (and the hole table when it
// changed), then advances the baseline. The receiver keeps an XDeltaCursor
// next to its buffer; XBuffer::apply_delta_verified<T> copies the runs into
// the buffer, verifies the patched payload exactly as load_verified<T>
// verifies a wire, and rolls the bytes back if it is rejected. Nothing is
// encoded or decoded per field: a run is raw payload bytes at a payload
// offset, which means the same thing on every 64-bit little-endian target.
//
// The first delta of a baseline (and the first after reset()) is a key
// delta: the difference from an all-zero payload, which any buffer accepts.
// Format and rules: docs/DELTA_SYNC.md.
//
// Included at the end of xoffsetdatastructure.hpp.

#include <algorithm>
#include <atomic>
#include <bit>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <span>
#include <string>
#include <vector>

#define XOFFSET_DELTA_SYNC_API 1

namespace XOffsetDatastructure {

    // Delta format 1.0 (docs/DELTA_SYNC.md). Little-endian.
    inline constexpr std::uint32_t XDELTA_MAGIC = 0x00014458u; // bytes 'X' 'D' 1 0
    inline constexpr std::uint32_t XDELTA_FORMAT_MAJOR = 1;
    inline constexpr std::uint32_t XDELTA_FORMAT_MINOR = 0;
    inline constexpr std::uint32_t XDELTA_FLAG_KEY = 1u;    // apply to an all-zero payload; carries the fingerprint
    inline constexpr std::uint32_t XDELTA_FLAG_HOLES = 2u;  // carries the whole new hole table
    inline constexpr std::uint32_t XDELTA_FLAG_DIGEST = 4u; // carries the digest of the resulting payload
    inline constexpr std::uint32_t XDELTA_FLAGS_KNOWN = 7u;

    struct XDeltaHeader {
        std::uint32_t magic;        // XDELTA_MAGIC
        std::uint32_t flags;        // XDELTA_FLAG_*
        std::uint64_t stream;       // the sender's baseline stream (nonzero)
        std::uint32_t version;      // version after this delta (>= 1); a non-key delta applies to version - 1
        std::uint32_t used_bytes;   // payload used_end after this delta
        std::uint32_t root_offset;  // root offset after this delta
        std::uint32_t run_count;    // entries of the run section
    };
    static_assert(sizeof(XDeltaHeader) == 32 && alignof(XDeltaHeader) == 8);

    namespace detail {
        struct DeltaRun {
            std::uint32_t offset;
            std::uint32_t bytes;
        };

        // ---- xxHash64 (the digest of XDELTA_FLAG_DIGEST, seed 0) -------------
        inline constexpr std::uint64_t xxh_p1 = 0x9E3779B185EBCA87ull;
        inline constexpr std::uint64_t xxh_p2 = 0xC2B2AE3D27D4EB4Full;
        inline constexpr std::uint64_t xxh_p3 = 0x165667B19E3779F9ull;
        inline constexpr std::uint64_t xxh_p4 = 0x85EBCA77C2B2AE63ull;
        inline constexpr std::uint64_t xxh_p5 = 0x27D4EB2F165667C5ull;

        inline std::uint64_t xxh_read64(const unsigned char* p) noexcept {
            std::uint64_t v;
            std::memcpy(&v, p, 8);
            return v;
        }
        inline std::uint32_t xxh_read32(const unsigned char* p) noexcept {
            std::uint32_t v;
            std::memcpy(&v, p, 4);
            return v;
        }
        inline std::uint64_t xxh_round(std::uint64_t acc, std::uint64_t input) noexcept {
            acc += input * xxh_p2;
            acc = std::rotl(acc, 31);
            return acc * xxh_p1;
        }
        inline std::uint64_t xxh_merge(std::uint64_t acc, std::uint64_t value) noexcept {
            acc ^= xxh_round(0, value);
            return acc * xxh_p1 + xxh_p4;
        }

        inline std::uint64_t xxh64(const void* data, std::size_t length, std::uint64_t seed = 0) noexcept {
            const auto* p = static_cast<const unsigned char*>(data);
            const auto* const end = p + length;
            std::uint64_t h;
            if (length >= 32) {
                std::uint64_t v1 = seed + xxh_p1 + xxh_p2;
                std::uint64_t v2 = seed + xxh_p2;
                std::uint64_t v3 = seed;
                std::uint64_t v4 = seed - xxh_p1;
                const auto* const limit = end - 32;
                do {
                    v1 = xxh_round(v1, xxh_read64(p));
                    v2 = xxh_round(v2, xxh_read64(p + 8));
                    v3 = xxh_round(v3, xxh_read64(p + 16));
                    v4 = xxh_round(v4, xxh_read64(p + 24));
                    p += 32;
                } while (p <= limit);
                h = std::rotl(v1, 1) + std::rotl(v2, 7) + std::rotl(v3, 12) + std::rotl(v4, 18);
                h = xxh_merge(h, v1);
                h = xxh_merge(h, v2);
                h = xxh_merge(h, v3);
                h = xxh_merge(h, v4);
            } else {
                h = seed + xxh_p5;
            }
            h += static_cast<std::uint64_t>(length);
            while (end - p >= 8) {
                h ^= xxh_round(0, xxh_read64(p));
                h = std::rotl(h, 27) * xxh_p1 + xxh_p4;
                p += 8;
            }
            if (end - p >= 4) {
                h ^= static_cast<std::uint64_t>(xxh_read32(p)) * xxh_p1;
                h = std::rotl(h, 23) * xxh_p2 + xxh_p3;
                p += 4;
            }
            while (p < end) {
                h ^= static_cast<std::uint64_t>(*p) * xxh_p5;
                h = std::rotl(h, 11) * xxh_p1;
                ++p;
            }
            h ^= h >> 33;
            h *= xxh_p2;
            h ^= h >> 29;
            h *= xxh_p3;
            h ^= h >> 32;
            return h;
        }

        // ---- LEB128 (unsigned, at most 5 bytes, values below 2^32) -------
        inline std::size_t delta_varint_size(std::uint32_t v) noexcept {
            return v < (1u << 7) ? 1u : v < (1u << 14) ? 2u : v < (1u << 21) ? 3u : v < (1u << 28) ? 4u : 5u;
        }
        inline char* delta_put_varint(char* p, std::uint32_t v) noexcept {
            while (v >= 0x80u) {
                *p++ = static_cast<char>((v & 0x7Fu) | 0x80u);
                v >>= 7;
            }
            *p++ = static_cast<char>(v);
            return p;
        }
        // False on truncation, a value of 2^32 or more, or more than 5 bytes.
        inline bool delta_get_varint(const unsigned char*& p, const unsigned char* end, std::uint32_t& out) noexcept {
            std::uint32_t v = 0;
            for (unsigned shift = 0; shift < 35; shift += 7) {
                if (p == end) return false;
                const unsigned b = *p++;
                if (shift == 28 && b > 0x0Fu) return false;
                v |= static_cast<std::uint32_t>(b & 0x7Fu) << shift;
                if ((b & 0x80u) == 0) {
                    out = v;
                    return true;
                }
            }
            return false;
        }

        // ---- byte comparison ------------------------------------------------
        inline std::uint64_t delta_load64(const char* p) noexcept {
            std::uint64_t v;
            std::memcpy(&v, p, 8);
            return v;
        }

        // First i in [from, n) with a[i] != b[i], or n.
        inline std::size_t delta_first_diff(const char* a, const char* b, std::size_t from, std::size_t n) noexcept {
            std::size_t i = from;
            // 64-byte blocks without an early exit inside (vectorizes on every
            // target), then words, then bytes.
            while (n - i >= 64) {
                std::uint64_t acc = 0;
                for (std::size_t k = 0; k < 64; k += 8) acc |= delta_load64(a + i + k) ^ delta_load64(b + i + k);
                if (acc != 0) break;
                i += 64;
            }
            while (n - i >= 8) {
                const std::uint64_t x = delta_load64(a + i) ^ delta_load64(b + i);
                if (x != 0) return i + (static_cast<std::size_t>(std::countr_zero(x)) >> 3);
                i += 8;
            }
            while (i < n && a[i] == b[i]) ++i;
            return i;
        }

        // First i in [from, n) with a[i] == b[i], or n.
        inline std::size_t delta_first_equal(const char* a, const char* b, std::size_t from, std::size_t n) noexcept {
            constexpr std::uint64_t ones = 0x0101010101010101ull;
            constexpr std::uint64_t highs = 0x8080808080808080ull;
            std::size_t i = from;
            while (n - i >= 8) {
                const std::uint64_t x = delta_load64(a + i) ^ delta_load64(b + i);
                // The lowest flagged byte is the first zero byte of x (higher
                // flags may be false positives; only the lowest is used).
                const std::uint64_t zero = (x - ones) & ~x & highs;
                if (zero != 0) return i + (static_cast<std::size_t>(std::countr_zero(zero)) >> 3);
                i += 8;
            }
            while (i < n && a[i] != b[i]) ++i;
            return i;
        }

        // Runs of [from, n) where cur and base differ; runs separated by at
        // most merge_gap equal bytes are joined.
        inline void delta_diff(const char* cur, const char* base, std::size_t from, std::size_t n,
                               std::uint32_t merge_gap, std::vector<DeltaRun>& runs) {
            std::size_t i = delta_first_diff(cur, base, from, n);
            while (i < n) {
                const std::size_t start = i;
                std::size_t end;
                for (;;) {
                    end = delta_first_equal(cur, base, i, n);
                    if (end == n) break;
                    const std::size_t limit = std::min<std::size_t>(n, end + std::size_t(merge_gap) + 1u);
                    const std::size_t next = delta_first_diff(cur, base, end, limit);
                    if (next >= limit) break;
                    i = next;
                }
                runs.push_back({static_cast<std::uint32_t>(start), static_cast<std::uint32_t>(end - start)});
                i = delta_first_diff(cur, base, end, n);
            }
        }

        inline std::uint64_t new_delta_stream() noexcept {
            static std::atomic<std::uint64_t> counter{0};
            std::uint64_t x = counter.fetch_add(1, std::memory_order_relaxed) * 0x9E3779B97F4A7C15ull;
            x ^= static_cast<std::uint64_t>(std::chrono::steady_clock::now().time_since_epoch().count());
            x ^= static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(&counter)) << 16;
            // splitmix64 finalizer
            x += 0x9E3779B97F4A7C15ull;
            x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ull;
            x = (x ^ (x >> 27)) * 0x94D049BB133111EBull;
            x ^= x >> 31;
            return x != 0 ? x : 1;
        }
    } // namespace detail

    /// Sender side of one delta stream: the state one receiver (or a group of
    /// receivers that always get the same deltas) holds, as a copy of its
    /// payload. Memory: one copy of the payload's used bytes.
    class XDeltaBaseline {
    public:
        XDeltaBaseline() = default;
        /// A stream id chosen by the caller (nonzero); by default one is drawn
        /// on the first delta.
        explicit XDeltaBaseline(std::uint64_t stream) noexcept : stream_(stream) {}

        /// No state recorded: the next delta is a key delta.
        bool empty() const noexcept { return payload_.empty(); }
        std::uint64_t stream() const noexcept { return stream_; }
        /// Version of the last delta written (0: none yet).
        std::uint32_t version() const noexcept { return version_; }
        /// Bytes of the recorded payload copy.
        std::size_t recorded_bytes() const noexcept { return payload_.size(); }
        /// Forget the recorded state (the receiver lost it or asks for a
        /// resync): the next delta is a key delta. The version keeps counting,
        /// so deltas written before the reset never apply after it.
        void reset() noexcept {
            payload_.clear();
            holes_.clear();
        }
        /// reset() and return the copy's memory.
        void release() noexcept {
            reset();
            std::vector<char>().swap(payload_);
            std::vector<XBufferCore::WireHole>().swap(holes_);
            std::vector<XBufferCore::WireHole>().swap(scratch_holes_);
            std::vector<detail::DeltaRun>().swap(runs_);
        }

    private:
        friend struct detail::DeltaAccess;
        std::uint64_t stream_ = 0;
        std::uint32_t version_ = 0;
        std::uint32_t root_offset_ = 0;
        std::vector<char> payload_;                       // [0, used_end) as the receiver holds it ([0, 48) unused)
        std::vector<XBufferCore::WireHole> holes_;        // its hole table, by offset
        std::vector<XBufferCore::WireHole> scratch_holes_;
        std::vector<detail::DeltaRun> runs_;
    };

    /// Receiver side of one delta stream: which state the buffer next to it
    /// holds (stream, version, root type), plus retained scratch so that
    /// applying deltas does not allocate per message once it has grown.
    class XDeltaCursor {
    public:
        /// No key delta applied yet (or the last one was rejected): only a key
        /// delta applies.
        bool empty() const noexcept { return version_ == 0; }
        std::uint64_t stream() const noexcept { return stream_; }
        std::uint32_t version() const noexcept { return version_; }
        /// Forget the state: only a key delta applies next.
        void clear() noexcept {
            stream_ = 0;
            version_ = 0;
            schema_hash_ = 0;
            root_type_id_ = 0;
        }
        void release_scratch() noexcept {
            std::vector<XBufferCore::WireHole>().swap(holes_);
            std::vector<char>().swap(undo_);
        }

    private:
        friend struct detail::DeltaAccess;
        std::uint64_t stream_ = 0;
        std::uint32_t version_ = 0;
        std::uint64_t schema_hash_ = 0;
        std::uint64_t root_type_id_ = 0;
        std::vector<XBufferCore::WireHole> holes_;
        std::vector<char> undo_;
    };

    namespace detail {
        struct DeltaAccess {
            static constexpr std::size_t payload_begin = XWIRE_PAYLOAD_BEGIN;

            // ---- sender ---------------------------------------------------
            template <typename T>
            static XDeltaStats encode(XBuffer& xbuf, std::string& out, XDeltaBaseline& b, XDeltaOptions options) {
                XBufferCore& core = xbuf;
                const char* cur = static_cast<const char*>(core.get_address());
                const auto* sm = core.arena();
                const std::size_t used = static_cast<std::size_t>(sm->used_end);
                const auto root = static_cast<std::uint32_t>(sm->root_offset);
                if (used < payload_begin || used > XBufferCore::MAX_PAYLOAD || used > core.segment_size())
                    XOFFSET_THROW(XException(XErrc::invalid_state, "save_delta<T>(): payload extent out of range"));

                if (b.stream_ == 0) b.stream_ = new_delta_stream();
                if (b.version_ == UINT32_MAX) {
                    // Versions are exhausted: a new stream, starting with a key delta.
                    b.stream_ = new_delta_stream();
                    b.version_ = 0;
                    b.reset();
                }
                const bool key = b.payload_.empty();

                // The hole table, by offset.
                auto& holes = b.scratch_holes_;
                holes.clear();
                holes.reserve(core.free_block_count());
                core.for_each_free_block([&](XBufferCore::WireHole h) { holes.push_back(h); });
                std::sort(holes.begin(), holes.end(),
                          [](const XBufferCore::WireHole& x, const XBufferCore::WireHole& y) { return x.offset < y.offset; });
                const bool holes_changed = key || holes.size() != b.holes_.size() ||
                    (!holes.empty() &&
                     std::memcmp(holes.data(), b.holes_.data(), holes.size() * sizeof(XBufferCore::WireHole)) != 0);

                // The receiver's bytes past its used_end read as zero, so the
                // recorded copy grows with zeros (a key delta compares against
                // an all-zero payload).
                auto& base = b.payload_;
                const std::size_t base_used = base.size();
                if (key) base.assign(used, '\0');
                else if (used > base.size()) base.resize(used, '\0');

                auto& runs = b.runs_;
                runs.clear();
                delta_diff(cur, base.data(), payload_begin, used, options.merge_gap, runs);

                XDeltaStats stats;
                stats.key = key;
                stats.compared_bytes = used - payload_begin;
                if (!key && runs.empty() && !holes_changed && used == base_used && root == b.root_offset_) {
                    out.clear();
                    stats.empty = true;
                    return stats;
                }

                // Size, then one write.
                std::size_t size = sizeof(XDeltaHeader) + (key ? 16u : 0u) + (options.digest ? 8u : 0u);
                std::uint64_t run_bytes = 0;
                std::size_t prev = payload_begin;
                for (const auto& r : runs) {
                    size += delta_varint_size(static_cast<std::uint32_t>(r.offset - prev)) +
                            delta_varint_size(r.bytes) + r.bytes;
                    run_bytes += r.bytes;
                    prev = std::size_t(r.offset) + r.bytes;
                }
                if (holes_changed) {
                    if (holes.size() > UINT32_MAX)
                        XOFFSET_THROW(XException(XErrc::invalid_state, "save_delta<T>(): hole table too large"));
                    size += delta_varint_size(static_cast<std::uint32_t>(holes.size()));
                    std::size_t prev_hole = payload_begin;
                    for (const auto& h : holes) {
                        if (h.offset < prev_hole || (h.offset & 7u) != 0 || (h.bytes & 7u) != 0 || h.bytes == 0 ||
                            std::size_t(h.offset) + h.bytes > used)
                            XOFFSET_THROW(XException(XErrc::invalid_state, "save_delta<T>(): inconsistent free table"));
                        size += delta_varint_size(static_cast<std::uint32_t>((h.offset - prev_hole) / 8u)) +
                                delta_varint_size(h.bytes / 8u);
                        prev_hole = std::size_t(h.offset) + h.bytes;
                    }
                }
                if (size > out.max_size())
                    XOFFSET_THROW(std::length_error("save_delta<T>(): output size exceeds string limit"));
                out.resize(size);

                // Nothing below allocates: the baseline is updated as the runs
                // are written.
                char* p = out.data();
                XDeltaHeader header{};
                header.magic = XDELTA_MAGIC;
                header.flags = (key ? XDELTA_FLAG_KEY : 0u) | (holes_changed ? XDELTA_FLAG_HOLES : 0u) |
                               (options.digest ? XDELTA_FLAG_DIGEST : 0u);
                header.stream = b.stream_;
                header.version = b.version_ + 1u;
                header.used_bytes = static_cast<std::uint32_t>(used);
                header.root_offset = root;
                header.run_count = static_cast<std::uint32_t>(runs.size());
                std::memcpy(p, &header, sizeof(header));
                p += sizeof(header);
                if (key) {
                    const std::uint64_t fingerprint[2] = {wire_schema_hash<T>(), wire_root_type_id<T>()};
                    std::memcpy(p, fingerprint, 16);
                    p += 16;
                }
                if (options.digest) {
                    const std::uint64_t digest = xxh64(cur + payload_begin, used - payload_begin);
                    std::memcpy(p, &digest, 8);
                    p += 8;
                }
                prev = payload_begin;
                for (const auto& r : runs) {
                    p = delta_put_varint(p, static_cast<std::uint32_t>(r.offset - prev));
                    p = delta_put_varint(p, r.bytes);
                    std::memcpy(p, cur + r.offset, r.bytes);
                    std::memcpy(base.data() + r.offset, cur + r.offset, r.bytes);
                    p += r.bytes;
                    prev = std::size_t(r.offset) + r.bytes;
                }
                if (holes_changed) {
                    p = delta_put_varint(p, static_cast<std::uint32_t>(holes.size()));
                    std::size_t prev_hole = payload_begin;
                    for (const auto& h : holes) {
                        p = delta_put_varint(p, static_cast<std::uint32_t>((h.offset - prev_hole) / 8u));
                        p = delta_put_varint(p, h.bytes / 8u);
                        prev_hole = std::size_t(h.offset) + h.bytes;
                    }
                    b.holes_.swap(holes);
                }
                if (used < base.size()) base.resize(used); // shrinking never allocates
                b.version_ = header.version;
                b.root_offset_ = root;

                stats.holes = holes_changed;
                stats.runs = header.run_count;
                stats.run_bytes = run_bytes;
                stats.delta_bytes = size;
                return stats;
            }

            // ---- receiver -------------------------------------------------
            struct Parsed {
                XDeltaHeader header;
                const unsigned char* runs;       // first run
                const unsigned char* runs_end;   // end of the run section
                std::uint64_t digest = 0;
                std::uint64_t schema_hash = 0;
                std::uint64_t root_type_id = 0;
                std::size_t undo_bytes = 0;      // old bytes the runs overwrite below old_used
                bool key = false;
                bool holes = false;
                bool digest_present = false;
            };

            static std::string msg(const char* who, const char* what) {
                return std::string(who) + ": " + what;
            }

            // Header, run and hole sections; nothing is written to the buffer.
            // old_used: the replica's current used_end (0 for a key delta).
            template <typename T>
            static vstatus parse(std::span<const std::byte> data, const XDeltaCursor& cursor, std::size_t old_used,
                                 std::vector<XBufferCore::WireHole>& holes, Parsed& out, const char* who) {
                const auto* p = reinterpret_cast<const unsigned char*>(data.data());
                const std::size_t n = data.size_bytes();
                if (n < 2 || p[0] != 'X' || p[1] != 'D')
                    XOFFSET_VFAIL(XException, XErrc::bad_magic, msg(who, "not an XOffset delta (bad magic)"));
                if (n < 4) XOFFSET_VFAIL(XException, XErrc::truncated, msg(who, "delta shorter than its header"));
                if (p[2] != XDELTA_FORMAT_MAJOR || p[3] != XDELTA_FORMAT_MINOR)
                    XOFFSET_VFAIL(XException, XErrc::unsupported_version,
                                  std::string(who) + ": delta format " + std::to_string(unsigned(p[2])) + "." +
                                      std::to_string(unsigned(p[3])) + " is not supported; this build reads 1.0");
                if (n < sizeof(XDeltaHeader))
                    XOFFSET_VFAIL(XException, XErrc::truncated, msg(who, "delta shorter than its header"));
                auto& h = out.header;
                std::memcpy(&h, p, sizeof(h));
                if ((h.flags & ~XDELTA_FLAGS_KNOWN) != 0)
                    XOFFSET_VFAIL(XException, XErrc::unsupported_flags, msg(who, "unsupported delta flags"));
                out.key = (h.flags & XDELTA_FLAG_KEY) != 0;
                out.holes = (h.flags & XDELTA_FLAG_HOLES) != 0;
                out.digest_present = (h.flags & XDELTA_FLAG_DIGEST) != 0;
                const auto* q = p + sizeof(XDeltaHeader);
                const auto* const end = p + n;
                if (h.stream == 0 || h.version == 0)
                    XOFFSET_VFAIL(XException, XErrc::delta_base_mismatch, msg(who, "delta without stream or version"));
                if (out.key) {
                    if (end - q < 16) XOFFSET_VFAIL(XException, XErrc::truncated, msg(who, "delta shorter than its header"));
                    if (!out.holes)
                        XOFFSET_VFAIL(XException, XErrc::unsupported_flags, msg(who, "key delta without hole table"));
                    std::memcpy(&out.schema_hash, q, 8);
                    std::memcpy(&out.root_type_id, q + 8, 8);
                    q += 16;
                    if (out.root_type_id != wire_root_type_id<T>())
                        XOFFSET_VFAIL(XException, XErrc::root_type_mismatch, msg(who, "root type id mismatch"));
                    if (out.schema_hash != wire_schema_hash<T>())
                        XOFFSET_VFAIL(XException, XErrc::schema_mismatch, msg(who, "schema hash mismatch"));
                } else {
                    if (cursor.empty())
                        XOFFSET_VFAIL(XException, XErrc::delta_base_mismatch,
                                      msg(who, "no key delta applied yet (ask for a key delta)"));
                    if (cursor.root_type_id_ != wire_root_type_id<T>())
                        XOFFSET_VFAIL(XException, XErrc::root_type_mismatch, msg(who, "root type id mismatch"));
                    if (cursor.schema_hash_ != wire_schema_hash<T>())
                        XOFFSET_VFAIL(XException, XErrc::schema_mismatch, msg(who, "schema hash mismatch"));
                    if (h.stream != cursor.stream_ || h.version - 1u != cursor.version_)
                        XOFFSET_VFAIL(XException, XErrc::delta_base_mismatch,
                                      msg(who, "delta does not follow the state this replica holds (ask for a key delta)"));
                }
                if (out.digest_present) {
                    if (end - q < 8) XOFFSET_VFAIL(XException, XErrc::truncated, msg(who, "delta shorter than its header"));
                    std::memcpy(&out.digest, q, 8);
                    q += 8;
                }
                const std::size_t used = h.used_bytes;
                if (used < payload_begin || used > XBufferCore::MAX_PAYLOAD || (used & 7u) != 0)
                    XOFFSET_VFAIL(XException, XErrc::size_mismatch, msg(who, "used size mismatch"));
                if (h.root_offset < payload_begin || h.root_offset >= used)
                    XOFFSET_VFAIL(XException, XErrc::root_offset_out_of_bounds, msg(who, "root offset out of bounds"));

                // Runs: ascending, disjoint, non-empty, inside [48, used).
                out.runs = q;
                std::uint64_t prev = payload_begin;
                std::size_t undo = 0;
                for (std::uint32_t k = 0; k < h.run_count; ++k) {
                    std::uint32_t gap = 0, bytes = 0;
                    if (!delta_get_varint(q, end, gap) || !delta_get_varint(q, end, bytes))
                        XOFFSET_VFAIL(XException, XErrc::size_mismatch, msg(who, "delta run table truncated"));
                    const std::uint64_t offset = prev + gap;
                    if (bytes == 0 || offset + bytes > used)
                        XOFFSET_VFAIL(XException, XErrc::invalid_extent_table, msg(who, "delta run out of bounds"));
                    if (std::uint64_t(end - q) < bytes)
                        XOFFSET_VFAIL(XException, XErrc::size_mismatch, msg(who, "delta run table truncated"));
                    q += bytes;
                    if (offset < old_used) undo += static_cast<std::size_t>(std::min<std::uint64_t>(offset + bytes, old_used) - offset);
                    prev = offset + bytes;
                }
                out.runs_end = q;
                out.undo_bytes = undo;

                // Holes: whole new table (key deltas always carry one).
                holes.clear();
                if (out.holes) {
                    std::uint32_t count = 0;
                    if (!delta_get_varint(q, end, count))
                        XOFFSET_VFAIL(XException, XErrc::size_mismatch, msg(who, "delta hole table truncated"));
                    // Each entry takes at least 2 bytes and covers at least 8.
                    if (count > used / 8u || count > std::size_t(end - q) / 2u)
                        XOFFSET_VFAIL(XException, XErrc::size_mismatch, msg(who, "delta hole table truncated"));
                    holes.reserve(count);
                    std::uint64_t prev_hole = payload_begin;
                    for (std::uint32_t k = 0; k < count; ++k) {
                        std::uint32_t gap = 0, bytes = 0;
                        if (!delta_get_varint(q, end, gap) || !delta_get_varint(q, end, bytes))
                            XOFFSET_VFAIL(XException, XErrc::size_mismatch, msg(who, "delta hole table truncated"));
                        const std::uint64_t offset = prev_hole + std::uint64_t(gap) * 8u;
                        const std::uint64_t size = std::uint64_t(bytes) * 8u;
                        if (size == 0 || offset + size > used)
                            XOFFSET_VFAIL(XException, XErrc::invalid_payload, msg(who, "delta hole out of bounds"));
                        holes.push_back({static_cast<std::uint32_t>(offset), static_cast<std::uint32_t>(size)});
                        prev_hole = offset + size;
                    }
                }
                if (q != end) XOFFSET_VFAIL(XException, XErrc::size_mismatch, msg(who, "delta length mismatch"));
                return vok();
            }

            template <typename RunFn>
            static void for_each_run(const Parsed& parsed, RunFn&& fn) {
                const unsigned char* q = parsed.runs;
                std::size_t prev = payload_begin;
                for (std::uint32_t k = 0; k < parsed.header.run_count; ++k) {
                    std::uint32_t gap = 0, bytes = 0;
                    (void)delta_get_varint(q, parsed.runs_end, gap);
                    (void)delta_get_varint(q, parsed.runs_end, bytes);
                    const std::size_t offset = prev + gap;
                    fn(offset, reinterpret_cast<const char*>(q), std::size_t(bytes));
                    q += bytes;
                    prev = offset + bytes;
                }
            }

            template <typename T>
            static vstatus check_digest(const XBufferCore& core, const Parsed& parsed, const char* who) {
                if (!parsed.digest_present) return vok();
                const char* base = static_cast<const char*>(core.get_address());
                const std::size_t used = parsed.header.used_bytes;
                if (xxh64(base + payload_begin, used - payload_begin) != parsed.digest)
                    XOFFSET_VFAIL(XException, XErrc::digest_mismatch,
                                  msg(who, "result differs from the sender's digest (ask for a key delta)"));
                return vok();
            }

            template <typename T>
            static vstatus apply_key(XBuffer& xbuf, const Parsed& parsed, XDeltaCursor& cursor, const char* who) {
                XBufferCore& core = xbuf;
                const std::size_t used = parsed.header.used_bytes;
                cursor.clear();
                XOFFSET_ROLLBACK_BEGIN
                    std::size_t clean_from = 0;
                    XOFFSET_VTRY(core.reinit_for_overwrite(used, clean_from));
                    char* base = core.data();
                    if (clean_from > payload_begin)
                        std::memset(base + payload_begin, 0, std::min(clean_from, used) - payload_begin);
                    for_each_run(parsed, [&](std::size_t offset, const char* bytes, std::size_t n) {
                        std::memcpy(base + offset, bytes, n);
                    });
                    XBufferCore::write_payload_header(base, parsed.header.root_offset, used);
                    XOFFSET_VTRY(check_digest<T>(core, parsed, who));
                    XWireHeaderV2 header{};
                    header.used_bytes = parsed.header.used_bytes;
                    header.root_offset = parsed.header.root_offset;
                    header.hole_count = static_cast<std::uint32_t>(cursor.holes_.size());
                    XOFFSET_VTRY(xbuf.template finish_verified_load<T>(
                        header, reinterpret_cast<const char*>(cursor.holes_.data()), cursor.holes_.size(),
                        default_wire_upstream(), who, &xbuf.retained_reload_scratch()));
                XOFFSET_ROLLBACK_END(core.reset_arena(0))
                cursor.stream_ = parsed.header.stream;
                cursor.version_ = parsed.header.version;
                cursor.schema_hash_ = wire_schema_hash<T>();
                cursor.root_type_id_ = wire_root_type_id<T>();
                return vok();
            }

            // Restores what apply_incremental changed (bytes, used_end, root).
            static void roll_back(XBufferCore& core, const Parsed& parsed, const char* undo, std::size_t old_used,
                                  std::uint64_t old_root) noexcept {
                char* base = core.data();
                const std::size_t used = parsed.header.used_bytes;
                for_each_run(parsed, [&](std::size_t offset, const char*, std::size_t n) {
                    const std::size_t end = offset + n;
                    if (offset < old_used) {
                        const std::size_t kept = std::min(end, old_used) - offset;
                        std::memcpy(base + offset, undo, kept);
                        undo += kept;
                        if (end > old_used) std::memset(base + old_used, 0, end - old_used);
                    } else {
                        std::memset(base + offset, 0, n);
                    }
                });
                if (used < old_used) std::memcpy(base + used, undo, old_used - used);
                auto* sm = core.arena();
                sm->used_end = old_used;
                sm->root_offset = old_root;
            }

            template <typename T>
            static vstatus apply_incremental(XBuffer& xbuf, const Parsed& parsed, XDeltaCursor& cursor,
                                             const char* who) {
                XBufferCore& core = xbuf;
                const std::size_t used = parsed.header.used_bytes;
                const auto root = static_cast<std::uint64_t>(parsed.header.root_offset);
                const std::size_t old_used = static_cast<std::size_t>(core.arena()->used_end);
                const std::uint64_t old_root = core.arena()->root_offset;

                // Everything that can fail without verification fails first,
                // before a byte changes.
                auto& undo = cursor.undo_;
                const std::size_t undo_bytes = parsed.undo_bytes + (used < old_used ? old_used - used : 0u);
                XOFFSET_TRY {
                    if (undo.size() < undo_bytes) undo.resize(undo_bytes);
                    if (!parsed.holes) {
                        cursor.holes_.clear();
                        cursor.holes_.reserve(core.free_block_count());
                        core.for_each_free_block([&](XBufferCore::WireHole h) { cursor.holes_.push_back(h); });
                    }
                } XOFFSET_CATCH(const std::bad_alloc&) {
                    XOFFSET_VFAIL_BAD_ALLOC();
                }
                if (!parsed.holes && !core.free_table_complete())
                    XOFFSET_VFAIL(XException, XErrc::delta_base_mismatch,
                                  msg(who, "replica free table incomplete (ask for a key delta)"));
                if (used > core.segment_size() && !core.grow(used - core.segment_size()))
                    XOFFSET_VFAIL(XBadAlloc, XErrc::capacity_exceeded, arena_failure_message);

                // Save, then patch. Bytes at or past old_used are zero (N2).
                char* base = core.data();
                char* u = undo.data();
                for_each_run(parsed, [&](std::size_t offset, const char* bytes, std::size_t n) {
                    if (offset < old_used) {
                        const std::size_t kept = std::min(offset + n, old_used) - offset;
                        std::memcpy(u, base + offset, kept);
                        u += kept;
                    }
                    std::memcpy(base + offset, bytes, n);
                });
                if (used < old_used) {
                    std::memcpy(u, base + used, old_used - used);
                    std::memset(base + used, 0, old_used - used);
                }
                auto* sm = core.arena();
                sm->used_end = used;
                sm->root_offset = root;

                XOFFSET_ROLLBACK_BEGIN
                    XOFFSET_VTRY(check_digest<T>(core, parsed, who));
                    auto* root_ptr = find_root_at<T>(core, root);
                    if (!root_ptr) XOFFSET_VFAIL(XException, XErrc::root_mismatch, msg(who, "root not found"));
                    const auto actual = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(root_ptr) -
                                                                   reinterpret_cast<std::uintptr_t>(core.get_address()));
                    if (actual != root) XOFFSET_VFAIL(XException, XErrc::root_mismatch, msg(who, "root offset mismatch"));
                    XOFFSET_VTRY(verify_structural_wire_graph(*root_ptr, core, default_wire_upstream(),
                                                              cursor.holes_.data(), cursor.holes_.size(), true,
                                                              nullptr, &xbuf.retained_reload_scratch()));
                    if (parsed.holes) XOFFSET_VTRY(core.load_free_blocks(cursor.holes_.data(), cursor.holes_.size()));
                XOFFSET_ROLLBACK_END(roll_back(core, parsed, undo.data(), old_used, old_root))

                // Committed. A new epoch: element pointers and XHandles of the
                // previous content re-resolve.
                ++core.m_epoch;
                core.cache_root(find_root_at<T>(core, root));
                cursor.version_ = parsed.header.version;
                return vok();
            }

            template <typename T>
            static vstatus apply(XBuffer& xbuf, std::span<const std::byte> data, XDeltaCursor& cursor) {
                static_assert(is_v1_wire_admitted_v<T>,
                              "apply_delta_verified<T>() requires an admitted fixed-schema wire type.");
                constexpr const char* who = "apply_delta_verified<T>()";
                XBufferCore& core = xbuf;
                // A non-key delta patches a verified root of T that this
                // cursor's key delta produced.
                const bool has_state = core.typed_root_access_allowed_ && core.template root_type_matches<T>() &&
                                       core.arena() != nullptr;
                const std::size_t old_used = has_state ? static_cast<std::size_t>(core.arena()->used_end) : 0u;
                Parsed parsed{};
                XOFFSET_VTRY(parse<T>(data, cursor, old_used, cursor.holes_, parsed, who));
                if (parsed.key) return apply_key<T>(xbuf, parsed, cursor, who);
                if (!has_state)
                    XOFFSET_VFAIL(XException, XErrc::delta_base_mismatch,
                                  msg(who, "buffer holds no verified root of this type (ask for a key delta)"));
                return apply_incremental<T>(xbuf, parsed, cursor, who);
            }
        };
    } // namespace detail

    template <typename T>
    XDeltaStats XBuffer::save_delta_into(std::string& out, XDeltaBaseline& baseline, XDeltaOptions options) {
        static_assert(is_v1_wire_admitted_v<T>, "save_delta<T>() requires an admitted fixed-schema wire type.");
#ifdef XOFFSET_REJECT_IMPLICIT_PADDING
        static_assert(!has_implicit_padding_v<T>,
            "XOFFSET_REJECT_IMPLICIT_PADDING: the wire type has implicit padding (threat model T5/P3)");
#endif
        require_typed_root_access("save_delta<T>(): unverified payload cannot be relabeled as verified");
        require_schema_match<T>("save_delta<T>(): schema mismatch for requested root type");
        auto* root_ptr = detail::find_root<T>(*this);
        if (!root_ptr) XOFFSET_THROW(XException(XErrc::no_root, "save_delta<T>(): no root object of requested type"));
        ensure_hole_table(*root_ptr);
        return detail::DeltaAccess::encode<T>(*this, out, baseline, options);
    }

    template <typename T>
    std::string XBuffer::save_delta(XDeltaBaseline& baseline, XDeltaOptions options) {
        std::string out;
        (void)save_delta_into<T>(out, baseline, options);
        return out;
    }

    template <typename T>
    void XBuffer::apply_delta_verified(std::span<const std::byte> delta, XDeltaCursor& cursor) {
        XOFFSET_VREQUIRE(detail::DeltaAccess::apply<T>(*this, delta, cursor));
    }

    template <typename T>
    void XBuffer::apply_delta_verified(const std::string& delta, XDeltaCursor& cursor) {
        apply_delta_verified<T>(std::span<const std::byte>(reinterpret_cast<const std::byte*>(delta.data()),
                                                           delta.size()), cursor);
    }

    template <typename T>
    XStatus XBuffer::try_apply_delta_verified(std::span<const std::byte> delta, XDeltaCursor& cursor) {
#if XOFFSET_NO_EXCEPTIONS
        if (!detail::DeltaAccess::apply<T>(*this, delta, cursor)) return detail::take_pending_error();
        return {};
#else
        try {
            detail::DeltaAccess::apply<T>(*this, delta, cursor);
            return {};
        } catch (...) {
            return detail::current_exception_error();
        }
#endif
    }

    template <typename T>
    XStatus XBuffer::try_apply_delta_verified(const std::string& delta, XDeltaCursor& cursor) {
        return try_apply_delta_verified<T>(std::span<const std::byte>(
            reinterpret_cast<const std::byte*>(delta.data()), delta.size()), cursor);
    }

} // namespace XOffsetDatastructure
