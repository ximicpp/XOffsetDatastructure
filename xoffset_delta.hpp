#pragma once
// Delta sync: send only what changed to a receiver that already holds a copy.
//
// A sender keeps one XDeltaBaseline per receiver (or per group of receivers
// that always get the same deltas): a copy of the payload as that receiver
// holds it. XBuffer::save_delta_into<T> compares the buffer with it in
// 64-byte blocks and writes, for each block with a changed byte, a byte mask
// and the changed bytes (and the hole table when it changed), then advances
// the baseline. The receiver keeps an XDeltaCursor next to its buffer;
// XBuffer::apply_delta_verified<T> writes the bytes into the buffer, verifies
// the patched payload exactly as load_verified<T> verifies a wire, and rolls
// the bytes back if it is rejected. Nothing is encoded or decoded per field:
// the bytes are raw payload bytes at payload offsets, which mean the same
// thing on every 64-bit little-endian target.
//
// The first delta of a baseline (and the first after reset()) is a key
// delta: the difference from an all-zero payload, which any buffer accepts.
// Format and rules: docs/DELTA_SYNC.md.
//
// Included at the end of xoffsetdatastructure.hpp.

#include <algorithm>
#include <tuple>
#include <cstddef>
#include <atomic>
#include <bit>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <memory>
#include <new>
#include <span>
#include <string>
#include <vector>
#if defined(__SSE2__) || defined(_M_X64) || defined(_M_AMD64)
#include <emmintrin.h>
#endif
// Byte gather and scatter of the change blocks: SSSE3 (checked at run time)
// on x86-64, NEON on AArch64, plain code elsewhere.
#if defined(__x86_64__) || defined(_M_X64) || defined(_M_AMD64)
#include <tmmintrin.h>
#if defined(_MSC_VER) && !defined(__clang__)
#include <intrin.h>
#else
#include <cpuid.h>
#endif
#define XOFFSET_DELTA_SHUFFLE_X86 1
#elif defined(__aarch64__) || defined(_M_ARM64)
#include <arm_neon.h>
#define XOFFSET_DELTA_SHUFFLE_NEON 1
#endif

#define XOFFSET_DELTA_SYNC_API 1

// A hint to bring the cache line at p in (a line about to be written): a
// delta touches a few lines scattered over large buffers, and fetching them
// all at once overlaps their misses.
#if defined(__GNUC__) || defined(__clang__)
#define XOFFSET_DELTA_PREFETCH(p) __builtin_prefetch((p), 1, 3)
#elif defined(_MSC_VER) && (defined(_M_X64) || defined(_M_AMD64) || defined(_M_IX86))
#define XOFFSET_DELTA_PREFETCH(p) _mm_prefetch(reinterpret_cast<const char*>(p), _MM_HINT_T0)
#elif defined(_MSC_VER) && defined(_M_ARM64)
#define XOFFSET_DELTA_PREFETCH(p) __prefetch(p)
#else
#define XOFFSET_DELTA_PREFETCH(p) ((void)(p))
#endif

#if defined(_MSC_VER) && !defined(__clang__)
#define XOFFSET_DELTA_INLINE __forceinline
#define XOFFSET_DELTA_COLD __declspec(noinline)
#elif defined(__GNUC__) || defined(__clang__)
#define XOFFSET_DELTA_INLINE inline __attribute__((always_inline))
#define XOFFSET_DELTA_COLD __attribute__((noinline, cold))
#else
#define XOFFSET_DELTA_INLINE inline
#define XOFFSET_DELTA_COLD
#endif

namespace XOffsetDatastructure {

    // Delta format 1.0 (docs/DELTA_SYNC.md). Little-endian. The header:
    // 'X' 'D', the format byte (major << 4 | minor), the flags byte, the
    // stream (u64), then varints: the version, with EXTENT (used_end - 48) / 8
    // and root_offset - 48, and the block count. Block k of the block
    // section is payload bytes [64k, 64k + 64); no delta writes the arena
    // header [0, 48).
    inline constexpr std::uint32_t XDELTA_FORMAT_MAJOR = 1;
    inline constexpr std::uint32_t XDELTA_FORMAT_MINOR = 0;
    inline constexpr std::uint32_t XDELTA_FLAG_KEY = 1u;    // apply to an all-zero payload; carries the fingerprint
    inline constexpr std::uint32_t XDELTA_FLAG_HOLES = 2u;  // carries the whole new hole table
    inline constexpr std::uint32_t XDELTA_FLAG_DIGEST = 4u; // carries the digest of the resulting payload
    inline constexpr std::uint32_t XDELTA_FLAG_EXTENT = 8u; // carries used_end and the root offset (else unchanged)
    inline constexpr std::uint32_t XDELTA_FLAGS_KNOWN = 15u;
    inline constexpr std::size_t XDELTA_HEADER_FIXED = 12;  // bytes before the varints

    /// What the receiver checked for the last delta it applied
    /// (XDeltaCursor::last_check()).
    enum class XDeltaCheck : std::uint8_t {
        key,       ///< a key delta: the whole payload was verified
        none,      ///< the delta wrote only bytes that verification does not read
        in_place,  ///< besides those, what it changed was checked on its own: bools, string heads,
                   ///< vector sizes, heap strings, sets and maps, and containers of plain elements
                   ///< that moved, got a block or lost it (with the coverage of live blocks and holes)
        full,      ///< the whole payload was verified (the root or a vector of records moved, ...)
        admit,     ///< trust tier admit (XReceiver on a trusted link, xoffset_receiver.hpp): the delta's
                   ///< framing and bounds and the root's place only; the payload was not verified
    };

    namespace detail {
        inline constexpr std::size_t delta_block = 64;  // block size of the block section

        // A baseline's copy of the payload: 64-byte aligned (each block in
        // one cache line, like the payload it copies), zero-filled as it
        // grows, with room to grow (a key delta reserves a quarter more, and
        // growth past the room doubles it), moved with memcpy.
        class DeltaCopy {
        public:
            DeltaCopy() noexcept = default;
            DeltaCopy(const DeltaCopy& other) { *this = other; }
            DeltaCopy(DeltaCopy&& other) noexcept { swap(other); }
            DeltaCopy& operator=(const DeltaCopy& other) {
                if (this == &other) return *this;
                if (other.size_ > cap_) reallocate(other.size_, 0);
                if (other.size_ != 0) std::memcpy(data_, other.data_, other.size_);
                size_ = other.size_;
                return *this;
            }
            DeltaCopy& operator=(DeltaCopy&& other) noexcept {
                DeltaCopy(std::move(other)).swap(*this);
                return *this;
            }
            ~DeltaCopy() { release(); }
            void swap(DeltaCopy& other) noexcept {
                std::swap(data_, other.data_);
                std::swap(size_, other.size_);
                std::swap(cap_, other.cap_);
            }
            char* data() noexcept { return data_; }
            const char* data() const noexcept { return data_; }
            std::size_t size() const noexcept { return size_; }
            bool empty() const noexcept { return size_ == 0; }
            void clear() noexcept { size_ = 0; }
            // n zero bytes (the old ones are not kept).
            void assign_zero(std::size_t n) {
                if (n > cap_) reallocate(n + n / 4, 0);
                if (n != 0) std::memset(data_, 0, n);
                size_ = n;
            }
            // Grows with zeros, or shrinks (never allocating).
            void resize(std::size_t n) {
                if (n > cap_) reallocate(std::max(n, 2 * cap_), size_);
                if (n > size_) std::memset(data_ + size_, 0, n - size_);
                size_ = n;
            }
            void release() noexcept {
                if (data_) ::operator delete(data_, std::align_val_t{delta_block});
                data_ = nullptr;
                size_ = cap_ = 0;
            }

        private:
            void reallocate(std::size_t cap, std::size_t keep) {
                cap = (cap + delta_block - 1) / delta_block * delta_block;
                char* fresh = static_cast<char*>(::operator new(cap, std::align_val_t{delta_block}));
                if (keep != 0) std::memcpy(fresh, data_, keep);
                if (data_) ::operator delete(data_, std::align_val_t{delta_block});
                data_ = fresh;
                cap_ = cap;
            }
            char* data_ = nullptr;
            std::size_t size_ = 0, cap_ = 0;
        };

        // A delta header, decoded.
        struct DeltaHeader {
            std::uint32_t flags = 0;        // XDELTA_FLAG_*
            std::uint64_t stream = 0;       // the sender's baseline stream (nonzero)
            std::uint32_t version = 0;      // version after this delta (>= 1); a non-key delta applies to version - 1
            std::uint32_t used_bytes = 0;   // payload used_end after this delta
            std::uint32_t root_offset = 0;  // root offset after this delta
            std::uint32_t block_count = 0;  // entries of the block section
        };

        // A block the sender found: its payload offset, whether it is sent
        // whole, and the mask of its changed bytes (bit k: byte offset + k).
        struct DeltaBlock {
            std::uint32_t offset;
            std::uint32_t whole;
            std::uint64_t mask;
        };
        // A block of a received delta: payload offset, where its bytes start
        // in the delta, and the mask of the bytes it writes.
        struct DeltaBlockRef {
            std::uint32_t offset;
            std::uint32_t src;
            std::uint64_t mask;
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
            if (v < 0x80u) [[likely]] {
                *p = static_cast<char>(v);
                return p + 1;
            }
            while (v >= 0x80u) {
                *p++ = static_cast<char>((v & 0x7Fu) | 0x80u);
                v >>= 7;
            }
            *p++ = static_cast<char>(v);
            return p;
        }
        // False on truncation, a value of 2^32 or more, or more than 5 bytes.
        inline bool delta_get_varint(const unsigned char*& p, const unsigned char* end, std::uint32_t& out) noexcept {
            if (p != end && *p < 0x80u) [[likely]] {
                out = *p++;
                return true;
            }
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

        // Bit k is set when byte k of x (in memory order) is nonzero.
        inline std::uint64_t delta_byte_flags(std::uint64_t x) noexcept {
            constexpr std::uint64_t lows = 0x7F7F7F7F7F7F7F7Full;
            const std::uint64_t high = (((x & lows) + lows) | x) & ~lows;  // 0x80 in each nonzero byte
            return ((high >> 7) * 0x0102040810204080ull) >> 56;
        }

        // Bit k is set when a[k] != b[k], k in [0, 64). Portable version.
        inline std::uint64_t delta_block_mask_words(const char* a, const char* b) noexcept {
            std::uint64_t x[8];
            std::uint64_t any = 0;
            for (std::size_t k = 0; k < 8; ++k) {
                x[k] = delta_load64(a + 8 * k) ^ delta_load64(b + 8 * k);
                any |= x[k];
            }
            if (any == 0) return 0;
            std::uint64_t m = 0;
            for (std::size_t k = 0; k < 8; ++k) m |= delta_byte_flags(x[k]) << (8 * k);
            return m;
        }

        // Bit k is set when a[k] != b[k], k in [0, 64).
        inline std::uint64_t delta_block_mask(const char* a, const char* b) noexcept {
#if defined(__SSE2__) || defined(_M_X64) || defined(_M_AMD64)
            const auto load = [](const char* p) { return _mm_loadu_si128(reinterpret_cast<const __m128i*>(p)); };
            const __m128i e0 = _mm_cmpeq_epi8(load(a), load(b));
            const __m128i e1 = _mm_cmpeq_epi8(load(a + 16), load(b + 16));
            const __m128i e2 = _mm_cmpeq_epi8(load(a + 32), load(b + 32));
            const __m128i e3 = _mm_cmpeq_epi8(load(a + 48), load(b + 48));
            if (_mm_movemask_epi8(_mm_and_si128(_mm_and_si128(e0, e1), _mm_and_si128(e2, e3))) == 0xFFFF) return 0;
            const std::uint64_t equal = std::uint64_t(std::uint32_t(_mm_movemask_epi8(e0))) |
                                        (std::uint64_t(std::uint32_t(_mm_movemask_epi8(e1))) << 16) |
                                        (std::uint64_t(std::uint32_t(_mm_movemask_epi8(e2))) << 32) |
                                        (std::uint64_t(std::uint32_t(_mm_movemask_epi8(e3))) << 48);
            return ~equal;
#else
            return delta_block_mask_words(a, b);
#endif
        }

        // Encoded size of a block section and the payload bytes it carries.
        struct DeltaBlockTotals {
            std::size_t encoded = 0;
            std::uint64_t changed = 0;
            std::uint64_t compared = 0;  // payload bytes compared
        };

        // The blocks a sender found: a growable array written through a
        // pointer. Copies start empty (it is scratch).
        class DeltaBlockBuffer {
        public:
            DeltaBlockBuffer() = default;
            DeltaBlockBuffer(const DeltaBlockBuffer&) noexcept {}
            DeltaBlockBuffer& operator=(const DeltaBlockBuffer&) noexcept {
                size_ = 0;
                return *this;
            }
            DeltaBlockBuffer(DeltaBlockBuffer&&) noexcept = default;
            DeltaBlockBuffer& operator=(DeltaBlockBuffer&&) noexcept = default;

            const DeltaBlock* begin() const noexcept { return data_.get(); }
            const DeltaBlock* end() const noexcept { return data_.get() + size_; }
            std::size_t size() const noexcept { return size_; }
            bool empty() const noexcept { return size_ == 0; }
            const DeltaBlock& operator[](std::size_t k) const noexcept { return data_[k]; }
            void release() noexcept {
                data_.reset();
                size_ = cap_ = 0;
            }
            // Room for n blocks from the start; returns the start.
            DeltaBlock* reset_for(std::size_t n) {
                size_ = 0;
                if (cap_ < n) {
                    const std::size_t cap = std::max<std::size_t>(n, std::max<std::size_t>(16, cap_ * 2));
                    data_.reset(new DeltaBlock[cap]);
                    cap_ = cap;
                }
                return data_.get();
            }
            void commit(const DeltaBlock* end) noexcept { size_ = std::size_t(end - data_.get()); }

        private:
            std::unique_ptr<DeltaBlock[]> data_;
            std::size_t size_ = 0, cap_ = 0;
        };

        // Bits set in x. Without a population count instruction (baseline
        // x86-64 builds) std::popcount is a library routine or a long
        // sequence; this is one multiply.
        inline unsigned delta_popcount(std::uint64_t x) noexcept {
#if defined(__POPCNT__) || defined(__aarch64__) || defined(_M_ARM64)
            return static_cast<unsigned>(std::popcount(x));
#else
            x = x - ((x >> 1) & 0x5555555555555555ull);
            x = (x & 0x3333333333333333ull) + ((x >> 2) & 0x3333333333333333ull);
            x = (x + (x >> 4)) & 0x0F0F0F0F0F0F0F0Full;
            return static_cast<unsigned>((x * 0x0101010101010101ull) >> 56);
#endif
        }
        // A block inside the payload goes whole (no mask) when the mask form
        // would not be shorter: mask bytes plus changed bytes of at least 64.
        // Its encoded size after its skip varint: the selector byte, the
        // nonzero mask bytes, the changed bytes (or 64 bytes whole).
        struct DeltaBlockForm {
            bool whole;
            std::size_t size;     // encoded, after the skip varint
            std::size_t changed;  // bytes carried
        };
        // inside: all 64 bytes of the block lie in the range sent.
        inline DeltaBlockForm delta_block_form(std::uint64_t mask, bool inside) noexcept {
            const std::size_t changed = delta_popcount(mask);
            const std::size_t masked = delta_popcount(delta_byte_flags(mask)) + changed;
            if (inside && masked >= delta_block) return {true, 1u + delta_block, delta_block};
            return {false, 1u + masked, changed};
        }
        // The bytes of block `index` that lie in [from, n).
        inline std::uint64_t delta_range_mask(std::size_t index, std::size_t from, std::size_t n) noexcept {
            const std::size_t start = index * delta_block;
            if (n <= start || from >= start + delta_block || from >= n) return 0;
            const std::size_t lo = from > start ? from - start : 0u;
            const std::size_t hi = std::min(n - start, delta_block);
            const std::uint64_t below_hi = hi == delta_block ? ~std::uint64_t(0) : (std::uint64_t(1) << hi) - 1u;
            return below_hi & (~std::uint64_t(0) << lo);
        }
        // Appends block `index` with the bytes `mask` (of [from, n)) to the
        // blocks at w, adding its encoded size (its skip from `next`, the
        // index after the last block appended) to totals.
        inline void delta_append_block(DeltaBlock*& w, DeltaBlockTotals& totals, std::size_t& next, std::size_t index,
                                       std::uint64_t mask, std::size_t from, std::size_t n, bool whole_blocks) noexcept {
            const std::size_t start = index * delta_block;
            const bool inside = start >= from && start + delta_block <= n;
            const DeltaBlockForm form = whole_blocks && inside ? DeltaBlockForm{true, 1u + delta_block, delta_block}
                                                               : delta_block_form(mask, inside);
            totals.encoded += delta_varint_size(static_cast<std::uint32_t>(index - next)) + form.size;
            totals.changed += form.changed;
            *w++ = {static_cast<std::uint32_t>(start), form.whole ? 1u : 0u, mask};
            next = index + 1;
        }

        // The blocks (block k: payload bytes [64k, 64k + 64), one cache line
        // of a line-aligned payload) holding bytes of [from, n) where cur
        // differs from base (Update) or from zero (!Update, base unused),
        // with the masks of those bytes (bytes below from or past n are
        // never in a mask), among the blocks next_run picks (next_run(k):
        // the next run of picked block indexes as a pair [first, last) with
        // first >= k and last > first, or first past the blocks when none is
        // left; last may run past them). With Update, each block that
        // differs is copied into base on the way, so base equals cur on the
        // selected bytes afterwards. whole_blocks: every block that differs
        // and lies inside [from, n) goes whole.
        template <bool Update, typename NextRun>
        DeltaBlockTotals delta_scan(const char* cur, char* base, std::size_t from, std::size_t n,
                                    DeltaBlockBuffer& blocks, NextRun&& next_run, bool whole_blocks = false) {
            alignas(64) static constexpr char zeros[64] = {};
            DeltaBlockTotals totals;
            const std::size_t count = (n + delta_block - 1) / delta_block;
            const std::size_t full = n / delta_block;  // blocks that end by n (all but a shorter last)
            const std::size_t first = from / delta_block;  // the block from falls in
            const std::uint64_t first_mask = ~std::uint64_t(0) << (from % delta_block);
            DeltaBlock* w = blocks.reset_for(count - std::min(first, count));
            std::size_t next = 0;  // index of the block after the last one recorded
            const auto record = [&](std::size_t index, std::uint64_t mask) {
                delta_append_block(w, totals, next, index, mask, from, n, whole_blocks);
            };
            for (std::pair<std::size_t, std::size_t> run = next_run(first); run.first < count;
                 run = next_run(std::min(run.second, count))) {
                std::size_t index = std::max(run.first, first);
                const std::size_t last = std::min(run.second, full);
                if (index < last) totals.compared += (last - index) * delta_block;
                if (index == first && index < last) {
                    // The block from falls in: its bytes below from are not compared.
                    totals.compared -= from % delta_block;
                    const std::size_t i = index * delta_block;
                    const std::uint64_t m = delta_block_mask(cur + i, Update ? base + i : zeros) & first_mask;
                    if (m != 0) {
                        if constexpr (Update) std::memcpy(base + from, cur + from, i + delta_block - from);
                        record(index, m);
                    }
                    ++index;
                }
                for (; index < last; ++index) {
                    const std::size_t i = index * delta_block;
                    const std::uint64_t m = delta_block_mask(cur + i, Update ? base + i : zeros);
                    if (m == 0) [[likely]] continue;
                    if constexpr (Update) std::memcpy(base + i, cur + i, delta_block);
                    record(index, m);
                }
                if (full == count || run.second <= full) continue;
                // The last, shorter block.
                const std::size_t lo = full == first ? from : full * delta_block;
                const std::size_t i = full * delta_block;
                totals.compared += n - lo;
                std::uint64_t m = 0;
                std::size_t k = lo - i;
                for (; k % 8 != 0 && i + k < n; ++k) {
                    if (cur[i + k] != (Update ? base[i + k] : char(0))) {
                        m |= std::uint64_t(1) << k;
                        if constexpr (Update) base[i + k] = cur[i + k];
                    }
                }
                for (; n - (i + k) >= 8; k += 8) {
                    const std::uint64_t x = Update ? delta_load64(cur + i + k) ^ delta_load64(base + i + k)
                                                   : delta_load64(cur + i + k);
                    if (x == 0) continue;
                    m |= delta_byte_flags(x) << k;
                    if constexpr (Update) std::memcpy(base + i + k, cur + i + k, 8);
                }
                for (; i + k < n; ++k) {
                    if (cur[i + k] != (Update ? base[i + k] : char(0))) {
                        m |= std::uint64_t(1) << k;
                        if constexpr (Update) base[i + k] = cur[i + k];
                    }
                }
                if (m != 0) record(full, m);
            }
            blocks.commit(w);
            return totals;
        }
        struct DeltaEveryBlock {
            constexpr std::pair<std::size_t, std::size_t> operator()(std::size_t index) const noexcept {
                return {index, SIZE_MAX};
            }
        };

        inline DeltaBlockTotals delta_diff_update(const char* cur, char* base, std::size_t from, std::size_t n,
                                                  DeltaBlockBuffer& blocks, bool whole_blocks = false) {
            return delta_scan<true>(cur, base, from, n, blocks, DeltaEveryBlock{}, whole_blocks);
        }

        // The blocks of [from, n) where p is nonzero (those of a key delta of p).
        inline DeltaBlockTotals delta_nonzero_blocks(const char* p, std::size_t from, std::size_t n,
                                                     DeltaBlockBuffer& blocks, bool whole_blocks = false) {
            return delta_scan<false>(p, nullptr, from, n, blocks, DeltaEveryBlock{}, whole_blocks);
        }

        // Byte shuffles per mask byte m (bit j: byte j of a word): `gather`
        // byte i is the position of the i-th set bit (0x80 past popcount(m),
        // which reads as zero); `scatter` byte j is the rank of bit j among
        // the set bits when it is set, else 8 + j (the old byte, from the
        // upper half of the shuffled pair).
        struct DeltaShuffleTables {
            std::uint64_t gather[256];
            std::uint64_t scatter[256];
            unsigned char count[256];
        };
        constexpr DeltaShuffleTables make_delta_shuffle_tables() noexcept {
            DeltaShuffleTables t{};
            for (unsigned m = 0; m < 256; ++m) {
                std::uint64_t g = 0, sc = 0;
                unsigned k = 0;
                for (unsigned j = 0; j < 8; ++j) {
                    if ((m >> j) & 1u) {
                        g |= std::uint64_t(j) << (8 * k);
                        sc |= std::uint64_t(k) << (8 * j);
                        ++k;
                    } else {
                        sc |= std::uint64_t(8 + j) << (8 * j);
                    }
                }
                for (unsigned i = k; i < 8; ++i) g |= std::uint64_t(0x80) << (8 * i);
                t.gather[m] = g;
                t.scatter[m] = sc;
                t.count[m] = static_cast<unsigned char>(k);
            }
            return t;
        }
        inline constexpr DeltaShuffleTables delta_shuffle = make_delta_shuffle_tables();

        // Plain versions: a whole word is one copy, other words byte by byte.
        inline char* delta_put_masked_plain(char* p, const char* blk, std::uint64_t mask) noexcept {
            for (std::uint64_t sel = delta_byte_flags(mask); sel != 0; sel &= sel - 1) {
                const unsigned w = static_cast<unsigned>(std::countr_zero(sel));
                const unsigned bits = static_cast<unsigned>(mask >> (8 * w)) & 0xFFu;
                const char* word = blk + 8 * w;
                if (bits == 0xFFu) {
                    std::memcpy(p, word, 8);
                    p += 8;
                } else {
                    for (unsigned b = bits; b != 0; b &= b - 1) *p++ = word[std::countr_zero(b)];
                }
            }
            return p;
        }
        inline const char* delta_get_masked_plain(char* blk, const char* src, std::uint64_t mask) noexcept {
            for (std::uint64_t sel = delta_byte_flags(mask); sel != 0; sel &= sel - 1) {
                const unsigned w = static_cast<unsigned>(std::countr_zero(sel));
                const unsigned bits = static_cast<unsigned>(mask >> (8 * w)) & 0xFFu;
                char* word = blk + 8 * w;
                if (bits == 0xFFu) {
                    std::memcpy(word, src, 8);
                    src += 8;
                } else {
                    for (unsigned b = bits; b != 0; b &= b - 1) word[std::countr_zero(b)] = *src++;
                }
            }
            return src;
        }

        // Shuffle versions, one shuffle per word with a changed byte. The
        // gather stores 8 bytes per word (the output has 8 bytes of room past
        // its end); the scatter loads 8 bytes per word (src has 8 readable
        // bytes past the block's bytes).
#if defined(XOFFSET_DELTA_SHUFFLE_X86)
#if defined(_MSC_VER) && !defined(__clang__)
#define XOFFSET_DELTA_SSSE3
#else
#define XOFFSET_DELTA_SSSE3 __attribute__((target("ssse3")))
#endif
        inline bool delta_shuffle_supported() noexcept {
#if defined(__SSSE3__)
            return true;
#else
            static const bool supported = [] {
#if defined(_MSC_VER) && !defined(__clang__)
                int r[4] = {};
                __cpuid(r, 1);
                return (r[2] & (1 << 9)) != 0;
#else
                unsigned a = 0, b = 0, c = 0, d = 0;
                return __get_cpuid(1, &a, &b, &c, &d) != 0 && (c & (1u << 9)) != 0;
#endif
            }();
            return supported;
#endif
        }
        XOFFSET_DELTA_SSSE3 inline char* delta_put_masked_shuffle(char* p, const char* blk, std::uint64_t mask) noexcept {
            for (std::uint64_t sel = delta_byte_flags(mask); sel != 0; sel &= sel - 1) {
                const unsigned w = static_cast<unsigned>(std::countr_zero(sel));
                const unsigned bits = static_cast<unsigned>(mask >> (8 * w)) & 0xFFu;
                const __m128i v = _mm_loadl_epi64(reinterpret_cast<const __m128i*>(blk + 8 * w));
                const __m128i idx = _mm_loadl_epi64(reinterpret_cast<const __m128i*>(&delta_shuffle.gather[bits]));
                _mm_storel_epi64(reinterpret_cast<__m128i*>(p), _mm_shuffle_epi8(v, idx));
                p += delta_shuffle.count[bits];
            }
            return p;
        }
        XOFFSET_DELTA_SSSE3 inline const char* delta_get_masked_shuffle(char* blk, const char* src,
                                                                         std::uint64_t mask) noexcept {
            for (std::uint64_t sel = delta_byte_flags(mask); sel != 0; sel &= sel - 1) {
                const unsigned w = static_cast<unsigned>(std::countr_zero(sel));
                const unsigned bits = static_cast<unsigned>(mask >> (8 * w)) & 0xFFu;
                char* word = blk + 8 * w;
                const __m128i pair = _mm_unpacklo_epi64(_mm_loadl_epi64(reinterpret_cast<const __m128i*>(src)),
                                                        _mm_loadl_epi64(reinterpret_cast<const __m128i*>(word)));
                const __m128i idx = _mm_loadl_epi64(reinterpret_cast<const __m128i*>(&delta_shuffle.scatter[bits]));
                _mm_storel_epi64(reinterpret_cast<__m128i*>(word), _mm_shuffle_epi8(pair, idx));
                src += delta_shuffle.count[bits];
            }
            return src;
        }
        XOFFSET_DELTA_SSSE3 inline const char* delta_set_masked_shuffle(char* blk, const char* src, std::uint64_t mask,
                                                                         std::size_t words) noexcept {
            for (std::size_t w = 0; w < words; ++w) {
                const unsigned bits = static_cast<unsigned>(mask >> (8 * w)) & 0xFFu;
                // Bytes outside the mask come from the zero upper half.
                const __m128i idx = _mm_loadl_epi64(reinterpret_cast<const __m128i*>(&delta_shuffle.scatter[bits]));
                const __m128i v = _mm_shuffle_epi8(_mm_loadl_epi64(reinterpret_cast<const __m128i*>(src)), idx);
                _mm_storel_epi64(reinterpret_cast<__m128i*>(blk + 8 * w), v);
                src += delta_shuffle.count[bits];
            }
            return src;
        }
        // The mask of a block from its selector and the popcount(sel) mask
        // bytes at q (8 readable), and its bit count; false when one of the
        // mask bytes is zero.
        XOFFSET_DELTA_SSSE3 inline bool delta_expand_mask_shuffle(const unsigned char* q, unsigned sel,
                                                                  std::uint64_t& mask, std::size_t& bits) noexcept {
            const __m128i zeros = _mm_setzero_si128();
            const __m128i idx = _mm_loadl_epi64(reinterpret_cast<const __m128i*>(&delta_shuffle.scatter[sel]));
            // Index bytes 8 + j (no mask byte) pick zeros from the upper half.
            const __m128i v = _mm_shuffle_epi8(_mm_loadl_epi64(reinterpret_cast<const __m128i*>(q)), idx);
            const unsigned zero = static_cast<unsigned>(_mm_movemask_epi8(_mm_cmpeq_epi8(v, zeros)));
            mask = static_cast<std::uint64_t>(_mm_cvtsi128_si64(v));
            const __m128i nibbles = _mm_set1_epi8(0x0F);
            const __m128i counts = _mm_setr_epi8(0, 1, 1, 2, 1, 2, 2, 3, 1, 2, 2, 3, 2, 3, 3, 4);
            const __m128i per_byte =
                _mm_add_epi8(_mm_shuffle_epi8(counts, _mm_and_si128(v, nibbles)),
                             _mm_shuffle_epi8(counts, _mm_and_si128(_mm_srli_epi16(v, 4), nibbles)));
            bits = static_cast<std::size_t>(_mm_cvtsi128_si32(_mm_sad_epu8(per_byte, zeros)) & 0xFFFF);
            return (zero & sel) == 0;
        }
#elif defined(XOFFSET_DELTA_SHUFFLE_NEON)
        inline bool delta_shuffle_supported() noexcept { return true; }
        inline char* delta_put_masked_shuffle(char* p, const char* blk, std::uint64_t mask) noexcept {
            for (std::uint64_t sel = delta_byte_flags(mask); sel != 0; sel &= sel - 1) {
                const unsigned w = static_cast<unsigned>(std::countr_zero(sel));
                const unsigned bits = static_cast<unsigned>(mask >> (8 * w)) & 0xFFu;
                const uint8x8_t v = vld1_u8(reinterpret_cast<const std::uint8_t*>(blk + 8 * w));
                const uint8x8_t idx = vcreate_u8(delta_shuffle.gather[bits]);
                vst1_u8(reinterpret_cast<std::uint8_t*>(p), vtbl1_u8(v, idx));
                p += delta_shuffle.count[bits];
            }
            return p;
        }
        inline const char* delta_get_masked_shuffle(char* blk, const char* src, std::uint64_t mask) noexcept {
            for (std::uint64_t sel = delta_byte_flags(mask); sel != 0; sel &= sel - 1) {
                const unsigned w = static_cast<unsigned>(std::countr_zero(sel));
                const unsigned bits = static_cast<unsigned>(mask >> (8 * w)) & 0xFFu;
                auto* word = reinterpret_cast<std::uint8_t*>(blk + 8 * w);
                const uint8x16_t pair = vcombine_u8(vld1_u8(reinterpret_cast<const std::uint8_t*>(src)), vld1_u8(word));
                vst1_u8(word, vqtbl1_u8(pair, vcreate_u8(delta_shuffle.scatter[bits])));
                src += delta_shuffle.count[bits];
            }
            return src;
        }
        inline const char* delta_set_masked_shuffle(char* blk, const char* src, std::uint64_t mask,
                                                    std::size_t words) noexcept {
            for (std::size_t w = 0; w < words; ++w) {
                const unsigned bits = static_cast<unsigned>(mask >> (8 * w)) & 0xFFu;
                const uint8x16_t pair = vcombine_u8(vld1_u8(reinterpret_cast<const std::uint8_t*>(src)), vdup_n_u8(0));
                vst1_u8(reinterpret_cast<std::uint8_t*>(blk + 8 * w),
                        vqtbl1_u8(pair, vcreate_u8(delta_shuffle.scatter[bits])));
                src += delta_shuffle.count[bits];
            }
            return src;
        }
        inline bool delta_expand_mask_shuffle(const unsigned char* q, unsigned sel, std::uint64_t& mask,
                                              std::size_t& bits) noexcept {
            const uint8x16_t pair = vcombine_u8(vld1_u8(q), vdup_n_u8(0));
            const uint8x8_t v = vqtbl1_u8(pair, vcreate_u8(delta_shuffle.scatter[sel]));
            mask = vget_lane_u64(vreinterpret_u64_u8(v), 0);
            bits = vaddv_u8(vcnt_u8(v));
            // Bit k: byte k of v is zero.
            const std::uint64_t zero = vget_lane_u64(vreinterpret_u64_u8(vceq_u8(v, vdup_n_u8(0))), 0);
            return (delta_byte_flags(zero) & sel) == 0;
        }
#else
        inline bool delta_shuffle_supported() noexcept { return false; }
        inline char* delta_put_masked_shuffle(char* p, const char* blk, std::uint64_t mask) noexcept {
            return delta_put_masked_plain(p, blk, mask);
        }
        inline const char* delta_get_masked_shuffle(char* blk, const char* src, std::uint64_t mask) noexcept {
            return delta_get_masked_plain(blk, src, mask);
        }
        inline const char* delta_set_masked_shuffle(char* blk, const char* src, std::uint64_t mask,
                                                    std::size_t words) noexcept {
            std::memset(blk, 0, 8 * words);
            return delta_get_masked_plain(blk, src, mask);
        }
        inline bool delta_expand_mask_shuffle(const unsigned char*, unsigned, std::uint64_t&, std::size_t&) noexcept {
            return false;
        }
#endif

        // Writes block bytes of `mask` from blk; returns the end. The output
        // has 8 bytes of room past the bytes written.
        inline char* delta_put_masked(char* p, const char* blk, std::uint64_t mask) noexcept {
            return delta_shuffle_supported() ? delta_put_masked_shuffle(p, blk, mask)
                                             : delta_put_masked_plain(p, blk, mask);
        }
        // The inverse: writes the bytes of `mask` into blk from src (src_end:
        // the end of the readable input); returns the end of src.
        inline const char* delta_get_masked(char* blk, const char* src, std::uint64_t mask,
                                            const char* src_end) noexcept {
            if (mask == ~std::uint64_t(0)) {
                std::memcpy(blk, src, delta_block);
                return src + delta_block;
            }
            if (src_end - src >= std::ptrdiff_t(delta_block + 8) && delta_shuffle_supported())
                return delta_get_masked_shuffle(blk, src, mask);
            return delta_get_masked_plain(blk, src, mask);
        }
        // Like delta_get_masked into the first `words` words of blk, with
        // the bytes outside the mask set to zero (the mask lies in them).
        inline const char* delta_set_masked(char* blk, const char* src, std::uint64_t mask, std::size_t words,
                                            const char* src_end) noexcept {
            if (mask == ~std::uint64_t(0)) {
                std::memcpy(blk, src, delta_block);
                return src + delta_block;
            }
            if (src_end - src >= std::ptrdiff_t(delta_block + 8) && delta_shuffle_supported())
                return delta_set_masked_shuffle(blk, src, mask, words);
            std::memset(blk, 0, 8 * words);
            return delta_get_masked_plain(blk, src, mask);
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

        // ---- receiver: the bytes that verification reads --------------------
        // Per 64-byte block of the payload (block i: offsets 64 i to
        // 64 i + 63), bit k stands for byte 64 i + k. `checked`: the
        // verification of load_verified reads the byte: container heads,
        // string terminators, set and map keys with everything they reach,
        // bools, the length and padding of inline strings. Every other byte of
        // a verified payload may take any value without changing what
        // verification visits or decides. `local`: the checked bytes that are
        // checked again in place (bools; the length and padding of inline
        // string heads; the size of the heads of vectors whose elements hold
        // nothing verification reads, whose block and capacity stay while
        // their other head bytes stay). `heads`: the first byte of each
        // inline or empty string head and of each such vector head; the last
        // byte of a string head is local, that of a vector head is not.
        struct DeltaCheckBits {
            std::uint64_t checked = 0;
            std::uint64_t local = 0;
            std::uint64_t heads = 0;
        };
        static_assert(sizeof(XFixedString) == 12, "inline string head layout");
        static_assert(sizeof(XFixedVector<std::uint32_t>) == 12, "vector head layout");

        // A container whose change is checked on its own: a heap string, a
        // set or map whose elements hold nothing verification reads but their
        // keys, or a vector whose elements hold nothing verification reads.
        // Its checked bytes are its head and bytes of its block (the
        // terminator, the keys); a change of them is checked against the
        // container alone. When its capacity or block changes (it moved, was
        // allocated or was freed), its new block is checked against the
        // receiver's map of live blocks, and the hole table against both, so
        // the payload's coverage is checked without the whole verification.
        enum DeltaNodeKind : std::uint32_t {
            delta_node_string,
            delta_node_set,
            delta_node_map,
            delta_node_vector,
            delta_node_records,  // a vector of elements that verification reads: grows within its block, or
                                 // has all its elements verified again when it moves or shrinks
            delta_node_dead,     // a heap string that became inline or empty, or a node inside the old
                                 // elements of a rebuilt vector of records: no ranges
        };
        struct DeltaVerifyContext;
        struct DeltaCheckNode {
            std::uint32_t head = 0;      // offset of the head
            DeltaNodeKind kind = delta_node_string;
            std::uint32_t capacity = 0;  // capacity_ and data_delta when checked
            std::int32_t data_delta = 0;
            std::uint32_t size = 0;      // size_ when checked
            std::uint32_t element = 0;   // set, map: element size
            std::uint32_t key_size = 0;  // set, map: the key, at the start of an element
            std::uint32_t stamp = 0;     // scratch: last delta that touched it
            std::uint32_t moved = 0;     // scratch: last delta that changed its capacity or block
            bool (*keys_ordered)(const char* data, std::uint32_t size) = nullptr;  // set, map
            // records: verifies elements [from, to) of the block at `data` and
            // marks them (DeltaVerifyContext).
            bool (*grow)(DeltaVerifyContext& v, std::size_t data, std::uint32_t from, std::uint32_t to) = nullptr;
        };
        // A block that a delta freed and one that it made live (bytes rounded
        // to 8; 0: none), for node `node` (none: a string head that left the
        // inline or empty form).
        struct DeltaMove {
            static constexpr std::uint32_t new_node = ~std::uint32_t(0);
            std::uint32_t node = new_node;
            std::uint32_t head = 0;
            std::size_t old_begin = 0, old_bytes = 0;
            std::size_t new_begin = 0, new_bytes = 0;
            std::uint32_t size = 0;  // rebuild: the vector's new size
            bool grow = false;     // a vector of records grew in its block: elements [old_begin, new_begin)
            bool rebuild = false;  // a vector of records moved or shrank: all its elements verified again
            bool dropped = false;  // a node inside the old elements of a rebuilt vector: its block freed
        };
        // Bits [from, from + n) of a bit map.
        inline void delta_bits_set(std::uint64_t* w, std::size_t from, std::size_t n) noexcept {
            for (std::size_t e = from + n; from < e;) {
                const std::size_t bit = from % 64, take = std::min<std::size_t>(64 - bit, e - from);
                w[from / 64] |= (take == 64 ? ~std::uint64_t(0) : ((std::uint64_t(1) << take) - 1u)) << bit;
                from += take;
            }
        }
        inline void delta_bits_clear(std::uint64_t* w, std::size_t from, std::size_t n) noexcept {
            for (std::size_t e = from + n; from < e;) {
                const std::size_t bit = from % 64, take = std::min<std::size_t>(64 - bit, e - from);
                w[from / 64] &= ~((take == 64 ? ~std::uint64_t(0) : ((std::uint64_t(1) << take) - 1u)) << bit);
                from += take;
            }
        }
        inline bool delta_bits_any(const std::uint64_t* w, std::size_t from, std::size_t n) noexcept {
            for (std::size_t e = from + n; from < e;) {
                const std::size_t bit = from % 64, take = std::min<std::size_t>(64 - bit, e - from);
                if (w[from / 64] & ((take == 64 ? ~std::uint64_t(0) : ((std::uint64_t(1) << take) - 1u)) << bit))
                    return true;
                from += take;
            }
            return false;
        }
        // A range of checked bytes that belongs to one node; ranges do not overlap.
        struct DeltaNodeRange {
            std::uint32_t begin = 0, end = 0, node = 0;
        };

        // Keys strictly ordered, as verification checks them.
        template <typename Elem, bool Map>
        bool delta_keys_ordered(const char* raw, std::uint32_t size) {
            const Elem* data = reinterpret_cast<const Elem*>(raw);
            bool unordered = false;
            for (std::uint32_t i = 1; i < size; ++i) {
                if constexpr (Map) unordered |= !fixed_detail::ordered_less(data[i - 1].first, data[i].first);
                else unordered |= !fixed_detail::ordered_less(data[i - 1], data[i]);
            }
            return !unordered;
        }

        class DeltaCheckMarker {
        public:
            DeltaCheckMarker(const char* base, DeltaCheckBits* bits, std::vector<DeltaCheckNode>* nodes = nullptr,
                             std::vector<DeltaNodeRange>* ranges = nullptr, std::uint64_t* live = nullptr) noexcept
                : base_(base), bits_(bits), nodes_(nodes), ranges_(ranges), live_(live) {}
            const char* base() const noexcept { return base_; }
            // A live block of `bytes` (rounded to 8) at `p`, in the map of
            // live blocks (one bit per 8 bytes) when one is built.
            void live(const void* p, std::size_t bytes) noexcept { live_at(offset_of(p), bytes); }
            void live_at(std::size_t offset, std::size_t bytes) noexcept {
                if (live_) delta_bits_set(live_, offset / 8u, (bytes + 7u) / 8u);
            }
            // Records a node (when nodes are collected) with its head and block ranges.
            void node(const DeltaCheckNode& n, std::size_t block_begin, std::size_t block_bytes) {
                if (!nodes_) return;
                const auto index = static_cast<std::uint32_t>(nodes_->size());
                nodes_->push_back(n);
                ranges_->push_back({n.head, n.head + static_cast<std::uint32_t>(sizeof(XFixedString)), index});
                if (block_bytes != 0)
                    ranges_->push_back({static_cast<std::uint32_t>(block_begin),
                                        static_cast<std::uint32_t>(block_begin + block_bytes), index});
            }
            std::size_t offset_of(const void* p) const noexcept {
                return static_cast<std::size_t>(static_cast<const char*>(p) - base_);
            }
            void mark(const void* p, std::size_t n, bool local) noexcept { mark_at(offset_of(p), n, local); }
            // Checked as a whole: never checked in place.
            void pin(const void* p, std::size_t n) noexcept {
                for_bits(offset_of(p), n, [](DeltaCheckBits& b, std::uint64_t m) {
                    b.checked |= m;
                    b.local &= ~m;
                    b.heads &= ~m;
                });
            }
            // Everything marked lies in the verified payload, past the arena header.
            void mark_at(std::size_t offset, std::size_t n, bool local) noexcept {
                for_bits(offset, n, [local](DeltaCheckBits& b, std::uint64_t m) {
                    b.checked |= m;
                    if (local) b.local |= m;
                });
            }
            void clear_at(std::size_t offset, std::size_t n) noexcept {
                for_bits(offset, n, [](DeltaCheckBits& b, std::uint64_t m) {
                    b.checked &= ~m;
                    b.local &= ~m;
                });
            }
            // Every mark in [offset, offset + n), head marks included.
            void clear_all_at(std::size_t offset, std::size_t n) noexcept {
                for_bits(offset, n, [](DeltaCheckBits& b, std::uint64_t m) {
                    b.checked &= ~m;
                    b.local &= ~m;
                    b.heads &= ~m;
                });
            }
            // A head that left the inline or empty form: its bits and its head mark.
            void clear_head_at(std::size_t offset) noexcept {
                clear_at(offset, sizeof(XFixedString));
                bits_[offset / delta_block].heads &= ~(std::uint64_t(1) << (offset % delta_block));
            }
            // The head of a vector whose elements hold nothing verification
            // reads: its size is checked in place (against its capacity); its
            // block and capacity are checked by the whole verification.
            void vector_head_at(std::size_t offset) noexcept {
                const std::size_t i = offset;
                const std::size_t bit = i % delta_block;
                DeltaCheckBits& b = bits_[i / delta_block];
                b.heads |= std::uint64_t(1) << bit;
                if (bit + sizeof(XFixedString) <= delta_block) {  // within one block (most heads)
                    b.checked |= std::uint64_t(0xFFF) << bit;
                    b.local |= std::uint64_t(0xF) << (bit + 4);
                    return;
                }
                mark_at(offset, sizeof(XFixedString), false);
                mark_at(offset + 4, 4, true);
            }
            // An inline string head: its length and padding are checked in
            // place, its characters are free. An empty head (no block, all
            // zero) reads as length 0: all its bytes are checked in place.
            void inline_head_at(std::size_t offset) noexcept {
                const unsigned length = static_cast<unsigned char>(base_[offset + 11]) & 0x7Fu;  // at most 10
                const std::size_t i = offset;
                const std::size_t bit = i % delta_block;
                DeltaCheckBits& b = bits_[i / delta_block];
                b.heads |= std::uint64_t(1) << bit;
                if (bit + sizeof(XFixedString) <= delta_block) {  // within one block (most heads)
                    const std::uint64_t m = ((std::uint64_t(1) << (sizeof(XFixedString) - length)) - 1u)
                                            << (bit + length);
                    b.checked |= m;
                    b.local |= m;
                    return;
                }
                mark_at(offset + length, sizeof(XFixedString) - length, true);
            }

        private:
            // fn(bits of a block, mask of the bytes of [offset, offset + n) in it).
            template <typename Fn>
            void for_bits(std::size_t offset, std::size_t n, Fn&& fn) noexcept {
                std::size_t i = offset;
                const std::size_t bit = i % delta_block;
                if (bit + n <= delta_block) {  // within one block (most marks)
                    const std::uint64_t m =
                        (n == delta_block ? ~std::uint64_t(0) : (std::uint64_t(1) << n) - 1u) << bit;
                    if (n != 0) fn(bits_[i / delta_block], m);
                    return;
                }
                const std::size_t e = i + n;
                while (i < e) {
                    const std::size_t at = i % delta_block;
                    const std::size_t take = std::min(delta_block - at, e - i);
                    const std::uint64_t m =
                        (take == delta_block ? ~std::uint64_t(0) : (std::uint64_t(1) << take) - 1u) << at;
                    fn(bits_[i / delta_block], m);
                    i += take;
                }
            }

            const char* base_;
            DeltaCheckBits* bits_;
            std::vector<DeltaCheckNode>* nodes_;
            std::vector<DeltaNodeRange>* ranges_;
            std::uint64_t* live_;
        };

        template <typename T>
        const T* delta_heap_data(const std::int32_t& delta) noexcept {
            return reinterpret_cast<const T*>(reinterpret_cast<const char*>(&delta) + delta);
        }

        template <typename T>
        void delta_mark_pinned(const T& value, DeltaCheckMarker& m);
        template <typename Elem>
        bool delta_grow_records(DeltaVerifyContext& v, std::size_t data, std::uint32_t from, std::uint32_t to);

        // A string with a block: its head, its terminator, and a node.
        XOFFSET_DELTA_COLD inline void delta_mark_heap_string(const XFixedString& s, DeltaCheckMarker& m) {
            m.mark(&s, sizeof(XFixedString), false);
            m.mark(delta_heap_data<char>(s.data_delta) + s.size_, 1, false);
            DeltaCheckNode n;
            n.head = static_cast<std::uint32_t>(m.offset_of(&s));
            n.kind = delta_node_string;
            n.capacity = s.capacity_;
            n.data_delta = s.data_delta;
            n.size = s.size_;
            m.node(n, m.offset_of(delta_heap_data<char>(s.data_delta)), std::size_t(s.capacity_) + 1u);
            m.live(delta_heap_data<char>(s.data_delta), std::size_t(s.capacity_) + 1u);
        }
        XOFFSET_DELTA_INLINE void delta_mark_string(const XFixedString& s, DeltaCheckMarker& m) {
            // Inline or empty (all 12 bytes zero, as verification requires
            // of a head without a block): checked in place.
            if (s.is_inline() || s.capacity_ == 0) [[likely]]
                m.inline_head_at(m.offset_of(&s));
            else
                delta_mark_heap_string(s, m);
        }

        // Marks what validate_wire_value(value) reads, on a verified payload
        // (the same branches in the same order).
        template <typename T>
        void delta_mark_value(const T& value, DeltaCheckMarker& m) {
            using CleanT = std::remove_cv_t<T>;
            if constexpr (!wire_value_needs_validation_v<CleanT>) {
                (void)value;
                (void)m;
            } else if constexpr (std::is_same_v<CleanT, bool>) {
                m.mark(&value, 1, true);
            } else if constexpr (std::is_array_v<CleanT>) {
                for (const auto& elem : value) delta_mark_value(elem, m);
            } else if constexpr (is_fixed_string_v<CleanT>) {
                delta_mark_string(static_cast<const XFixedString&>(value), m);
            } else if constexpr (is_fixed_flat_set_v<CleanT> || is_fixed_flat_map_v<CleanT> ||
                                 is_fixed_vector_v<CleanT>) {
                using Elem = typename CleanT::value_type;
                const auto& v = static_cast<const XFixedVector<Elem>&>(value);
                if constexpr (!is_fixed_flat_set_v<CleanT> && !is_fixed_flat_map_v<CleanT> &&
                              !wire_value_needs_validation_v<Elem>) {
                    m.vector_head_at(m.offset_of(&v));
                    if constexpr (alignof(Elem) <= 8) {  // a node: its block is checked on its own when it changes
                        DeltaCheckNode n;
                        n.head = static_cast<std::uint32_t>(m.offset_of(&v));
                        n.kind = delta_node_vector;
                        n.capacity = v.capacity_;
                        n.data_delta = v.data_delta;
                        n.size = v.size_;
                        n.element = sizeof(Elem);
                        m.node(n, 0, 0);
                    }
                    if (v.capacity_ != 0) m.live(delta_heap_data<Elem>(v.data_delta), std::size_t(v.capacity_) * sizeof(Elem));
                    return;  // its elements hold nothing verification reads
                }
                m.mark(&v, sizeof(v), false);
                if constexpr (!is_fixed_flat_set_v<CleanT> && !is_fixed_flat_map_v<CleanT> && alignof(Elem) <= 8) {
                    // A vector of records: a node (its head only) whose growth
                    // within its block is verified element by element.
                    DeltaCheckNode n;
                    n.head = static_cast<std::uint32_t>(m.offset_of(&v));
                    n.kind = delta_node_records;
                    n.capacity = v.capacity_;
                    n.data_delta = v.data_delta;
                    n.size = v.size_;
                    n.element = sizeof(Elem);
                    n.grow = &delta_grow_records<Elem>;
                    m.node(n, 0, 0);
                }
                if constexpr (!wire_value_needs_validation_v<Elem>) {  // a set or map
                    DeltaCheckNode n;
                    n.head = static_cast<std::uint32_t>(m.offset_of(&v));
                    n.capacity = v.capacity_;
                    n.data_delta = v.data_delta;
                    n.size = v.size_;
                    n.element = sizeof(Elem);
                    std::size_t keys_bytes = 0;  // the block range holds checked bytes only for keys
                    if constexpr (is_fixed_flat_set_v<CleanT>) {
                        n.kind = delta_node_set;
                        n.key_size = sizeof(Elem);
                        n.keys_ordered = &delta_keys_ordered<Elem, false>;
                        keys_bytes = std::size_t(v.capacity_) * sizeof(Elem);
                    } else if constexpr (is_fixed_flat_map_v<CleanT>) {
                        static_assert(offsetof(Elem, first) == 0, "map element layout");
                        n.kind = delta_node_map;
                        n.key_size = sizeof(typename CleanT::key_type);
                        n.keys_ordered = &delta_keys_ordered<Elem, true>;
                        keys_bytes = std::size_t(v.capacity_) * sizeof(Elem);
                    }
                    m.node(n, keys_bytes != 0 ? m.offset_of(delta_heap_data<Elem>(v.data_delta)) : 0, keys_bytes);
                }
                if (v.capacity_ == 0) return;
                const Elem* data = delta_heap_data<Elem>(v.data_delta);
                m.live(data, std::size_t(v.capacity_) * sizeof(Elem));
                for (std::uint32_t i = 0; i < v.size_; ++i) {
                    // Keys are compared with each other: everything they reach
                    // is checked as a whole (which covers what verifying them
                    // reads), with no part checked in place or on its own.
                    if constexpr (is_fixed_flat_set_v<CleanT>) {
                        delta_mark_pinned(data[i], m);
                    } else if constexpr (is_fixed_flat_map_v<CleanT>) {
                        delta_mark_pinned(data[i].first, m);
                        if constexpr (wire_value_needs_validation_v<typename CleanT::mapped_type>)
                            delta_mark_value(data[i].second, m);
                    } else if constexpr (wire_value_needs_validation_v<Elem>) {
                        delta_mark_value(data[i], m);
                    }
                }
            } else if constexpr (std::is_class_v<CleanT>) {
                (void)for_each_reflected_subobject_v<CleanT>(
                    [&](auto index) {
                        delta_mark_value(static_cast<const schema::base_t<CleanT, decltype(index)::value>&>(value), m);
                        return vok();
                    },
                    [&](auto index) {
                        using member = schema::member_t<CleanT, decltype(index)::value>;
                        const auto& field = member::get(value);
                        if constexpr (is_fixed_string_v<std::remove_cvref_t<decltype(field)>>)
                            delta_mark_string(static_cast<const XFixedString&>(field), m);
                        else
                            delta_mark_value(field, m);
                        return vok();
                    });
            }
        }

        // Marks a set or map key and everything it reaches (what comparing
        // keys may read), all checked as a whole.
        template <typename T>
        void delta_mark_pinned(const T& value, DeltaCheckMarker& m) {
            using CleanT = std::remove_cv_t<T>;
            m.pin(&value, sizeof(CleanT));
            if constexpr (std::is_array_v<CleanT>) {
                if constexpr (wire_value_needs_validation_v<std::remove_extent_t<CleanT>>)
                    for (const auto& elem : value) delta_mark_pinned(elem, m);
            } else if constexpr (is_fixed_string_v<CleanT>) {
                const auto& s = static_cast<const XFixedString&>(value);
                if (!s.is_inline() && s.capacity_ != 0) {
                    m.pin(delta_heap_data<char>(s.data_delta), std::size_t(s.size_) + 1u);
                    m.live(delta_heap_data<char>(s.data_delta), std::size_t(s.capacity_) + 1u);
                }
            } else if constexpr (is_fixed_flat_set_v<CleanT> || is_fixed_flat_map_v<CleanT> ||
                                 is_fixed_vector_v<CleanT>) {
                using Elem = typename CleanT::value_type;
                const auto& v = static_cast<const XFixedVector<Elem>&>(value);
                if (v.capacity_ == 0) return;
                const Elem* data = delta_heap_data<Elem>(v.data_delta);
                m.live(data, std::size_t(v.capacity_) * sizeof(Elem));
                for (std::uint32_t i = 0; i < v.size_; ++i) delta_mark_pinned(data[i], m);
            } else if constexpr (std::is_class_v<CleanT> && wire_value_needs_validation_v<CleanT>) {
                (void)for_each_reflected_subobject_v<CleanT>(
                    [&](auto index) {
                        delta_mark_pinned(static_cast<const schema::base_t<CleanT, decltype(index)::value>&>(value), m);
                        return vok();
                    },
                    [&](auto index) {
                        using member = schema::member_t<CleanT, decltype(index)::value>;
                        delta_mark_pinned(member::get(value), m);
                        return vok();
                    });
            }
        }

        // Verifies values a delta brought into a verified payload (the new
        // elements of a vector of records), as validate_wire_value does, with
        // every block they own claimed in the map of live blocks (it must be
        // free; the delta's freed blocks are out of the map already). A
        // failure only sends the delta to the whole verification, which then
        // decides (and gives the error).
        struct DeltaVerifyContext {
            const char* base;
            std::size_t used;
            std::uint64_t* live;
            std::size_t claimed = 0;  // granules claimed
            DeltaCheckMarker* marker;  // marks what it verified (with nodes)
            bool claim(std::size_t begin, std::size_t bytes) noexcept {
                if (delta_bits_any(live, begin / 8u, bytes / 8u)) return false;
                delta_bits_set(live, begin / 8u, bytes / 8u);
                claimed += bytes / 8u;
                return true;
            }
            // The block of a container head (as head_block in DeltaAccess),
            // claimed: false when it fails.
            bool block(const void* head, bool string, std::uint32_t element, std::size_t& begin,
                       std::size_t& bytes) noexcept {
                const std::size_t at = static_cast<std::size_t>(static_cast<const char*>(head) - base);
                std::int32_t data_delta;
                std::uint32_t size, capacity;
                std::memcpy(&data_delta, base + at, 4);
                std::memcpy(&size, base + at + 4, 4);
                std::memcpy(&capacity, base + at + 8, 4);
                bytes = 0;
                if (size > capacity) return false;
                if (string ? capacity >= XBufferCore::MAX_PAYLOAD : capacity > (XBufferCore::MAX_PAYLOAD - 8u) / element)
                    return false;
                if (capacity == 0) return size == 0 && data_delta == 0;
                const std::size_t raw = string ? std::size_t(capacity) + 1u : std::size_t(capacity) * element;
                const std::size_t block = (raw + 7u) & ~std::size_t(7u);
                const std::int64_t where = std::int64_t(at) + data_delta;
                if (where < 0 || (where & 7) != 0 || std::uint64_t(where) > used || block > used - std::size_t(where))
                    return false;
                begin = static_cast<std::size_t>(where);
                bytes = block;
                if (string && base[begin + size] != '\0') return false;
                return claim(begin, bytes);
            }
        };

        template <typename T>
        bool delta_verify_value(const T& value, DeltaVerifyContext& v) {
            using CleanT = std::remove_cv_t<T>;
            if constexpr (!wire_value_needs_validation_v<CleanT>) {
                (void)value;
                (void)v;
                return true;
            } else if constexpr (std::is_same_v<CleanT, bool>) {
                unsigned char bits;
                std::memcpy(&bits, &value, 1);
                return bits <= 1u;
            } else if constexpr (std::is_array_v<CleanT>) {
                for (const auto& elem : value)
                    if (!delta_verify_value(elem, v)) return false;
                return true;
            } else if constexpr (is_fixed_string_v<CleanT>) {
                const auto& s = static_cast<const XFixedString&>(value);
                if (s.is_inline() || (s.capacity_ == 0 && s.size_ == 0 && s.data_delta == 0)) {
                    std::uint64_t low;
                    std::uint32_t high;
                    std::memcpy(&low, &s, 8);
                    std::memcpy(&high, reinterpret_cast<const char*>(&s) + 8, 4);
                    if ((low | high) == 0) return true;
                    const unsigned length = (high >> 24) & 0x7Fu;
                    return (high & XFixedString::inline_flag) != 0 && length <= XFixedString::inline_max &&
                           ((low & inline_pad_masks.low[length]) | (high & inline_pad_masks.high[length])) == 0;
                }
                std::size_t begin, bytes;
                return v.block(&s, true, 1, begin, bytes);
            } else if constexpr (is_fixed_flat_set_v<CleanT> || is_fixed_flat_map_v<CleanT> ||
                                 is_fixed_vector_v<CleanT>) {
                using Elem = typename CleanT::value_type;
                if constexpr (alignof(Elem) > 8) {
                    return false;  // the whole verification decides
                } else {
                    const auto& vec = static_cast<const XFixedVector<Elem>&>(value);
                    std::size_t begin, bytes;
                    if (!v.block(&vec, false, sizeof(Elem), begin, bytes)) return false;
                    if (bytes == 0) return true;
                    const Elem* data = reinterpret_cast<const Elem*>(v.base + begin);
                    if constexpr (wire_value_needs_validation_v<Elem>)
                        for (std::uint32_t i = 0; i < vec.size_; ++i)
                            if (!delta_verify_value(data[i], v)) return false;
                    if constexpr (is_fixed_flat_set_v<CleanT>) return delta_keys_ordered<Elem, false>(v.base + begin, vec.size_);
                    else if constexpr (is_fixed_flat_map_v<CleanT>) return delta_keys_ordered<Elem, true>(v.base + begin, vec.size_);
                    else return true;
                }
            } else if constexpr (std::is_class_v<CleanT>) {
                bool ok = true;
                (void)for_each_reflected_subobject_v<CleanT>(
                    [&](auto index) {
                        if (ok) ok = delta_verify_value(static_cast<const schema::base_t<CleanT, decltype(index)::value>&>(value), v);
                        return vok();
                    },
                    [&](auto index) {
                        using member = schema::member_t<CleanT, decltype(index)::value>;
                        if (ok) ok = delta_verify_value(member::get(value), v);
                        return vok();
                    });
                return ok;
            } else {
                return false;
            }
        }

        template <typename Elem>
        bool delta_grow_records(DeltaVerifyContext& v, std::size_t data, std::uint32_t from, std::uint32_t to) {
            const Elem* elems = reinterpret_cast<const Elem*>(v.base + data);
            for (std::uint32_t i = from; i < to; ++i)
                if (!delta_verify_value(elems[i], v)) return false;
            for (std::uint32_t i = from; i < to; ++i) delta_mark_value(elems[i], *v.marker);
            return true;
        }

        // What writing one delta needs only while it runs: the buffer's hole
        // table, the blocks that changed and, for touched_only, the blocks to
        // compare. A thread writes one delta at a time, so its baselines
        // share one (see DeltaReceiveScratch).
        struct DeltaSendScratch {
            std::vector<XBufferCore::WireHole> holes;
            DeltaBlockBuffer blocks;
            std::vector<std::uint8_t> want;
            std::vector<std::uint64_t> masks;  // XDeltaRecord::touches: bytes to send per block
            void release() noexcept {
                std::vector<XBufferCore::WireHole>().swap(holes);
                blocks.release();
                std::vector<std::uint8_t>().swap(want);
                std::vector<std::uint64_t>().swap(masks);
            }
        };
        // What applying one delta needs only while it runs: the delta copied
        // with room past its end, the old bytes it overwrites, its blocks and
        // the replica's holes. A thread applies one delta at a time, so its
        // cursors share one: a replica's next delta finds it in cache, and a
        // thousand replicas do not keep a thousand copies.
        struct DeltaReceiveScratch {
            std::vector<XBufferCore::WireHole> holes;
            std::vector<char> undo;
            std::vector<char> input;  // a small delta, copied with room past its end
            std::vector<DeltaBlockRef> blocks;
            void release() noexcept {
                std::vector<XBufferCore::WireHole>().swap(holes);
                std::vector<char>().swap(undo);
                std::vector<char>().swap(input);
                std::vector<DeltaBlockRef>().swap(blocks);
            }
        };
#if XOFFSET_THREAD_CACHES
        inline bool& delta_thread_scratch_available() noexcept {
            static thread_local bool available = true;  // trivial: outlives the scratch
            return available;
        }
        struct DeltaThreadScratch {
            DeltaSendScratch send;
            DeltaReceiveScratch receive;
            ~DeltaThreadScratch() { delta_thread_scratch_available() = false; }
        };
        // Null once the thread's scratch is destroyed (a delta written or
        // applied from a later thread_local destructor uses its baseline's or
        // cursor's own).
        inline DeltaThreadScratch* delta_thread_scratch() noexcept {
            if (!delta_thread_scratch_available()) return nullptr;
            static thread_local DeltaThreadScratch scratch;
            return &scratch;
        }
#endif
    } // namespace detail

    /// What a baseline records of the state its receiver holds.
    enum class XDeltaRecord : std::uint8_t {
        /// A copy of the payload. A delta carries exactly the bytes that
        /// differ from it (found by comparing, or among the touched blocks
        /// with XDeltaOptions::touched_only). Memory: one copy of the
        /// payload's used bytes.
        copy,
        /// No copy: a delta carries every byte the buffer's touch() marks
        /// name since the baseline's last delta from it (touched_only is
        /// implied), plus the bytes past the receiver's used size and those
        /// of holes that came or went, compared with nothing. The sender
        /// touches everything it changes; bytes touched but unchanged are
        /// sent too. Where the touches do not say what changed (the first
        /// delta, a buffer whose content was replaced, a touch log that
        /// started over), the delta is a key delta. For senders whose
        /// changes are few and touched exactly: no copy to keep or read.
        touches,
    };

    /// Sender side of one delta stream: the state one receiver (or a group of
    /// receivers that always get the same deltas) holds, by default as a copy
    /// of its payload (see XDeltaRecord).
    class XDeltaBaseline {
    public:
        XDeltaBaseline() = default;
        /// A stream id chosen by the caller (nonzero); by default one is drawn
        /// on the first delta.
        explicit XDeltaBaseline(std::uint64_t stream) noexcept : stream_(stream) {}
        explicit XDeltaBaseline(XDeltaRecord record, std::uint64_t stream = 0) noexcept
            : stream_(stream), record_(record) {}

        /// No state recorded: the next delta is a key delta.
        bool empty() const noexcept { return used_ == 0; }
        XDeltaRecord record() const noexcept { return record_; }
        std::uint64_t stream() const noexcept { return stream_; }
        /// Version of the last delta written (0: none yet).
        std::uint32_t version() const noexcept { return version_; }
        /// Bytes of the recorded payload copy (0 for XDeltaRecord::touches).
        std::size_t recorded_bytes() const noexcept { return payload_.size(); }
        /// Forget the recorded state (the receiver lost it or asks for a
        /// resync): the next delta is a key delta. The version keeps counting,
        /// so deltas written before the reset never apply after it.
        void reset() noexcept {
            payload_.clear();
            holes_.clear();
            holes_gen_ = 0;
            touch_log_ = 0;
            used_ = 0;
        }
        /// A key delta of the recorded state, at this stream and version, for a
        /// receiver that joins late (one baseline shared by several
        /// receivers): applied to any buffer, it makes the receiver hold what
        /// the others hold, so it accepts this baseline's next delta. The
        /// baseline is not advanced. Requires !empty() and
        /// XDeltaRecord::copy.
        void key_delta_into(std::string& out, XDeltaOptions options = {}) const;
        std::string key_delta(XDeltaOptions options = {}) const {
            std::string out;
            key_delta_into(out, options);
            return out;
        }
        /// reset() and return the copy's memory, and the scratch the calling
        /// thread's baselines share.
        void release() noexcept {
            reset();
            payload_.release();
            std::vector<XBufferCore::WireHole>().swap(holes_);
            own_scratch_.release();
#if XOFFSET_THREAD_CACHES
            if (detail::DeltaThreadScratch* s = detail::delta_thread_scratch()) s->send.release();
#endif
        }

    private:
        friend struct detail::DeltaAccess;
        std::uint64_t stream_ = 0;
        std::uint32_t version_ = 0;
        std::uint32_t root_offset_ = 0;
        std::uint64_t schema_hash_ = 0;                   // fingerprint of the recorded root type
        std::uint64_t root_type_id_ = 0;
        std::size_t used_ = 0;                            // the receiver's used_end (0: nothing recorded)
        XDeltaRecord record_ = XDeltaRecord::copy;
        detail::DeltaCopy payload_;  // [0, used_end) as the receiver holds it ([0, 48) unused)
        std::vector<XBufferCore::WireHole> holes_;        // its hole table, by offset
        std::uint64_t holes_gen_ = 0;                     // the free-block table's generation it was taken at
        // touched_only: the touch log of the buffer the last delta came from
        // (0: none) and the stamp it had then (touches since have a later one).
        std::uint64_t touch_log_ = 0;
        std::uint32_t touch_stamp_ = 0;
        // Used instead of the thread's shared scratch where there is none
        // (XOFFSET_THREAD_CACHES 0, or after the thread's is destroyed).
        detail::DeltaSendScratch own_scratch_;
    };

    /// Receiver side of one delta stream: which state the buffer next to it
    /// holds (stream, version, root type) and the map of bytes its checks
    /// read. The scratch a delta needs while it is applied is shared by the
    /// thread's cursors and kept, so applying deltas does not allocate per
    /// message once it has grown.
    class XDeltaCursor {
    public:
        /// No key delta applied yet (or the last one was rejected): only a key
        /// delta applies.
        bool empty() const noexcept { return version_ == 0; }
        std::uint64_t stream() const noexcept { return stream_; }
        std::uint32_t version() const noexcept { return version_; }
        /// What was checked for the last delta applied.
        XDeltaCheck last_check() const noexcept { return last_check_; }
        /// Forget the state: only a key delta applies next.
        void clear() noexcept {
            stream_ = 0;
            version_ = 0;
            schema_hash_ = 0;
            root_type_id_ = 0;
            checks_valid_ = false;
        }
        /// Frees the map of checked bytes (the next delta is then verified in
        /// full, which rebuilds the map) and the scratch, including the one
        /// the calling thread's cursors share.
        void release_scratch() noexcept {
            own_scratch_.release();
#if XOFFSET_THREAD_CACHES
            if (detail::DeltaThreadScratch* s = detail::delta_thread_scratch()) s->receive.release();
#endif
            std::vector<detail::DeltaCheckBits>().swap(checks_);
            std::vector<detail::DeltaCheckNode>().swap(nodes_);
            std::vector<detail::DeltaNodeRange>().swap(ranges_);
            std::vector<detail::DeltaNodeRange>().swap(range_scratch_);
            std::vector<std::uint32_t>().swap(range_index_);
            std::vector<std::uint32_t>().swap(touched_);
            std::vector<std::uint64_t>().swap(live_);
            std::vector<detail::DeltaMove>().swap(moves_);
            checks_valid_ = false;
        }

    private:
        friend struct detail::DeltaAccess;
        std::uint64_t stream_ = 0;
        std::uint32_t version_ = 0;
        std::uint64_t schema_hash_ = 0;
        std::uint64_t root_type_id_ = 0;
        // The bytes the last verification read, valid for the buffer state
        // recorded with it (see DeltaAccess::checks_match).
        std::vector<detail::DeltaCheckBits> checks_;
        std::vector<detail::DeltaCheckNode> nodes_;
        std::vector<detail::DeltaNodeRange> ranges_;  // by begin
        std::vector<detail::DeltaNodeRange> range_scratch_;
        std::vector<std::uint32_t> range_index_;      // per block of checks_: the ranges that begin before it
        std::vector<std::uint32_t> touched_;          // nodes the current delta re-checks
        std::vector<std::uint64_t> live_;             // per 8 payload bytes: in a live block (header, root, blocks)
        std::size_t live_granules_ = 0;               // bits set in live_
        std::vector<detail::DeltaMove> moves_;        // blocks the current delta frees and makes live
        std::uint32_t stamp_ = 0;
        const void* checks_core_ = nullptr;
        const void* checks_base_ = nullptr;
        std::uint64_t checks_epoch_ = 0;
        std::uint64_t checks_used_ = 0;
        std::uint64_t checks_root_ = 0;
        std::size_t checks_free_ = 0;
        bool checks_free_complete_ = false;
        bool checks_valid_ = false;  // the recorded state is a verified one
        bool checks_built_ = false;  // checks_, nodes_ and ranges_ describe it (built when first needed)
        XDeltaCheck last_check_ = XDeltaCheck::key;
        // Used instead of the thread's shared scratch where there is none
        // (XOFFSET_THREAD_CACHES 0, or after the thread's is destroyed).
        detail::DeltaReceiveScratch own_scratch_;
    };

    namespace detail {
        struct DeltaAccess {
            static constexpr std::size_t payload_begin = XWIRE_PAYLOAD_BEGIN;
            static DeltaSendScratch& send_scratch(XDeltaBaseline& b) noexcept {
#if XOFFSET_THREAD_CACHES
                if (DeltaThreadScratch* s = delta_thread_scratch()) return s->send;
#endif
                return b.own_scratch_;
            }
            static DeltaReceiveScratch& receive_scratch(XDeltaCursor& c) noexcept {
#if XOFFSET_THREAD_CACHES
                if (DeltaThreadScratch* s = delta_thread_scratch()) return s->receive;
#endif
                return c.own_scratch_;
            }
            // Deltas up to delta_padded_max bytes are read from a copy with
            // delta_padding bytes of room (a whole block plus a word).
            static constexpr std::size_t delta_padded_max = 4096;
            static constexpr std::size_t delta_padding = delta_block + 8;
            static constexpr std::size_t delta_touch_group = 16;  // blocks per DeltaTouchLog::groups entry

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
                // A baseline records one root type; another type starts over.
                constexpr std::uint64_t schema_hash = wire_schema_hash<T>();
                constexpr std::uint64_t root_type_id = wire_root_type_id<T>();
                if (b.schema_hash_ != schema_hash || b.root_type_id_ != root_type_id) {
                    b.reset();
                    b.schema_hash_ = schema_hash;
                    b.root_type_id_ = root_type_id;
                }
                // A touches baseline whose touches no longer say what changed
                // starts over with a key delta.
                const bool touches = b.record_ == XDeltaRecord::touches;
                if (touches && !b.empty() && !touches_known(xbuf, b)) b.reset();
                const bool key = b.empty();
                // The free-block table as of the baseline's last delta (the
                // same generation): the same holes, not listed again.
                const std::uint64_t holes_gen = core.free_block_gen();
                const bool holes_same = !key && b.holes_gen_ == holes_gen;
                // The delta is written into out's storage; the old hole table
                // is compared: their lines are fetched while the rest runs.
                XOFFSET_DELTA_PREFETCH(out.data());
                if (!holes_same && !b.holes_.empty()) XOFFSET_DELTA_PREFETCH(b.holes_.data());

                // The hole table, by offset.
                DeltaSendScratch& scratch = send_scratch(b);
                auto& holes = scratch.holes;
                holes.clear();
                if (!holes_same) {
                    holes.reserve(core.free_block_count());
                    core.for_each_free_block([&](XBufferCore::WireHole h) { holes.push_back(h); });
                    std::sort(holes.begin(), holes.end(), [](const XBufferCore::WireHole& x, const XBufferCore::WireHole& y) {
                        return x.offset < y.offset;
                    });
                }
                const bool holes_changed = !holes_same &&
                    (key || holes.size() != b.holes_.size() ||
                     (!holes.empty() &&
                      std::memcmp(holes.data(), b.holes_.data(), holes.size() * sizeof(XBufferCore::WireHole)) != 0));

                // The hole section is checked and sized before the baseline
                // changes.
                const std::size_t hole_section = holes_changed ? hole_section_size(holes, used) : 0u;

                // The receiver's bytes past its used_end read as zero, so the
                // recorded copy grows with zeros (a key delta compares against
                // an all-zero payload).
                const std::size_t base_used = b.used_;
                auto& base = b.payload_;
                if (!touches) {
                    if (key) base.assign_zero(used);
                    else if (used > base.size()) base.resize(used);
                }

                // The comparison brings the recorded copy up to date as it goes.
                // If anything fails before the delta is complete, the baseline
                // is dropped, so the next delta is a key delta.
                struct DropOnFailure {
                    XDeltaBaseline* b;
                    ~DropOnFailure() {
                        if (b) b->reset();
                    }
                } drop{&b};
                auto& blocks = scratch.blocks;
                DeltaTouchLog* const log = xbuf.touch_log_.get();
                DeltaBlockTotals totals;
                if (touches) {
                    totals = key ? delta_nonzero_blocks(cur, payload_begin, used, blocks, options.whole_blocks)
                                 : touched_bytes(b, scratch, *log, cur, holes, holes_changed, base_used, used,
                                                 options.whole_blocks);
                } else if (options.touched_only && !key && touches_known(xbuf, b)) {
                    const std::size_t count = (used + delta_block - 1) / delta_block;
                    const std::size_t grown =
                        used > base_used ? std::max(base_used, payload_begin) / delta_block : count;
                    // Blocks the touches decide (below both used sizes): by the
                    // list, or by the stamps (none past them was stamped).
                    const std::size_t below =
                        std::min((std::min(base_used, used) + delta_block - 1) / delta_block, grown);
                    if (holes_changed) {
                        select_touched(b, scratch.want, *log, holes, base_used, used);
                        const std::uint8_t* want = scratch.want.data();
                        totals = delta_scan<true>(
                            cur, base.data(), payload_begin, used, blocks,
                            [=](std::size_t i) {
                                while (i < count && want[i] == 0) ++i;
                                std::size_t j = i;
                                while (j < count && want[j] != 0) ++j;
                                return std::pair<std::size_t, std::size_t>(i, j);
                            },
                            options.whole_blocks);
                    } else if (listed_since(*log, b.touch_stamp_)) {
                        // The blocks listed since the baseline's stamp, or past
                        // its used size.
                        sort_touch_list(*log);
                        const std::uint32_t* list = log->blocks;
                        const std::uint32_t listed = log->listed;
                        for (std::uint32_t q = 0; q < listed && list[q] < below; ++q)
                            XOFFSET_DELTA_PREFETCH(base.data() + std::size_t(list[q]) * delta_block);
                        std::uint32_t k = 0;
                        totals = delta_scan<true>(
                            cur, base.data(), payload_begin, used, blocks,
                            [=, &k](std::size_t i) {
                                if (i >= grown) return std::pair<std::size_t, std::size_t>(i, SIZE_MAX);
                                while (k < listed && list[k] < i) ++k;
                                if (k < listed && list[k] < below) {
                                    const std::size_t j = list[k];
                                    std::size_t e = j + 1;
                                    for (++k; k < listed && list[k] == e && e < below; ++k) ++e;
                                    return std::pair<std::size_t, std::size_t>(j, e);
                                }
                                return std::pair<std::size_t, std::size_t>(grown, SIZE_MAX);
                            },
                            options.whole_blocks);
                    } else {
                        // Touched since the baseline's stamp (groups of 16
                        // blocks untouched since are skipped whole), or past
                        // its used size.
                        const std::uint32_t* stamps = log->stamps.data();
                        const std::uint32_t* groups = log->groups.data();
                        const std::uint32_t since = b.touch_stamp_;
                        const std::size_t stamped = std::min(below, log->stamps.size());
                        totals = delta_scan<true>(
                            cur, base.data(), payload_begin, used, blocks,
                            [=](std::size_t i) {
                                if (i >= grown) return std::pair<std::size_t, std::size_t>(i, SIZE_MAX);
                                while (i < stamped) {
                                    if (i % delta_touch_group == 0 && groups[i / delta_touch_group] <= since) {
                                        i += delta_touch_group;
                                        continue;
                                    }
                                    if (stamps[i] > since) {
                                        std::size_t j = i + 1;
                                        while (j < stamped && stamps[j] > since) ++j;
                                        return std::pair<std::size_t, std::size_t>(i, j);
                                    }
                                    ++i;
                                }
                                // From grown on (a skipped group may reach past it), every block.
                                return std::pair<std::size_t, std::size_t>(grown, SIZE_MAX);
                            },
                            options.whole_blocks);
                    }
                } else {
                    totals = delta_diff_update(cur, base.data(), payload_begin, used, blocks, options.whole_blocks);
                }

                XDeltaStats stats;
                stats.key = key;
                stats.compared_bytes = totals.compared;
                const bool pair = options.touched_only || touches;
                if (!key && blocks.empty() && !holes_changed && used == base_used && root == b.root_offset_) {
                    if (pair) pair_touch_log(xbuf, b);
                    b.holes_gen_ = holes_gen;
                    drop.b = nullptr;
                    out.clear();
                    stats.empty = true;
                    return stats;
                }

                DeltaHeader header{};
                header.flags = (key ? XDELTA_FLAG_KEY : 0u) | (holes_changed ? XDELTA_FLAG_HOLES : 0u) |
                               (options.digest ? XDELTA_FLAG_DIGEST : 0u) |
                               (key || used != base_used || root != b.root_offset_ ? XDELTA_FLAG_EXTENT : 0u);
                header.stream = b.stream_;
                header.version = b.version_ + 1u;
                header.used_bytes = static_cast<std::uint32_t>(used);
                header.root_offset = root;
                header.block_count = static_cast<std::uint32_t>(blocks.size());
                write_delta(out, header, b.schema_hash_, b.root_type_id_, cur, blocks, totals, holes, hole_section);
                if (holes_changed) b.holes_.swap(holes);
                b.holes_gen_ = holes_gen;
                if (used < base.size()) base.resize(used); // shrinking never allocates
                b.used_ = used;
                b.version_ = header.version;
                b.root_offset_ = root;
                // A key delta also pairs with a touch log the buffer already
                // keeps, so the baseline's next touched_only delta compares
                // only what was touched since.
                if (pair || (key && xbuf.touch_log_)) pair_touch_log(xbuf, b);
                drop.b = nullptr;

                stats.holes = holes_changed;
                stats.blocks = header.block_count;
                stats.changed_bytes = totals.changed;
                stats.delta_bytes = out.size();
                return stats;
            }

            // Whether the buffer's touch log describes its content and holds
            // what was touched since the baseline's last delta from it (in its
            // list, or in stamps kept since then).
            static bool touches_known(const XBuffer& xbuf, const XDeltaBaseline& b) noexcept {
                const XBufferCore& core = xbuf;
                const DeltaTouchLog* log = xbuf.touch_log_.get();
                return log && !log->lost && log->epoch == core.epoch() && b.touch_log_ == log->id &&
                       (listed_since(*log, b.touch_stamp_) || stamped_since(*log, b.touch_stamp_));
            }
            // Whether the log's list holds every block touched since period
            // `since` (only the current period came since, and it did not
            // overflow).
            static bool listed_since(const DeltaTouchLog& log, std::uint32_t since) noexcept {
                return since + 1u == log.now && !log.overflow;
            }
            // Whether the stamps hold every touch since period `since`.
            static bool stamped_since(const DeltaTouchLog& log, std::uint32_t since) noexcept {
                return since + 1u >= log.stamps_from;
            }
            // The list by block (it is short, and mostly in order).
            static void sort_touch_list(DeltaTouchLog& log) noexcept {
                std::uint32_t* blocks = log.blocks;
                std::uint64_t* masks = log.masks;
                const bool with_masks = log.masks_on;
                for (std::uint32_t i = 1; i < log.listed; ++i) {
                    if (blocks[i - 1] < blocks[i]) continue;
                    const std::uint32_t b = blocks[i];
                    const std::uint64_t m = with_masks ? masks[i] : 0u;
                    std::uint32_t j = i;
                    for (; j > 0 && blocks[j - 1] > b; --j) {
                        blocks[j] = blocks[j - 1];
                        if (with_masks) masks[j] = masks[j - 1];
                    }
                    blocks[j] = b;
                    if (with_masks) masks[j] = m;
                }
            }
            // The blocks of a delta from an XDeltaRecord::touches baseline: the
            // bytes touched since its stamp (whole blocks where the list does
            // not hold them), those past its used size where nonzero, those
            // of holes that came or went.
            static DeltaBlockTotals touched_bytes(const XDeltaBaseline& b, DeltaSendScratch& scratch, DeltaTouchLog& log,
                                                  const char* cur, const std::vector<XBufferCore::WireHole>& holes,
                                                  bool holes_changed, std::size_t base_used, std::size_t used,
                                                  bool whole_blocks) {
                DeltaBlockBuffer& blocks = scratch.blocks;
                DeltaBlockTotals totals;
                const std::size_t count = (used + delta_block - 1) / delta_block;
                const bool listed = listed_since(log, b.touch_stamp_);
                const auto listed_mask = [&](std::uint32_t k) {
                    return log.masks_on ? log.masks[k] : ~std::uint64_t(0);
                };
                std::size_t next = 0;
                if (listed && !holes_changed && used <= base_used) {
                    // The usual case: the listed blocks, in order.
                    sort_touch_list(log);
                    DeltaBlock* w = blocks.reset_for(log.listed);
                    for (std::uint32_t k = 0; k < log.listed; ++k) {
                        const std::uint32_t block = log.blocks[k];
                        if (block >= count) break;
                        const std::uint64_t m = listed_mask(k) & delta_range_mask(block, payload_begin, used);
                        if (m != 0) delta_append_block(w, totals, next, block, m, payload_begin, used, whole_blocks);
                    }
                    blocks.commit(w);
                    return totals;
                }
                // Otherwise the bytes to send per block.
                auto& masks = scratch.masks;
                masks.assign(count, 0);
                std::uint64_t* m = masks.data();
                if (listed) {
                    for (std::uint32_t k = 0; k < log.listed; ++k)
                        if (log.blocks[k] < count) m[log.blocks[k]] |= listed_mask(k);
                } else {
                    const std::size_t stamped = std::min(log.stamps.size(), count);
                    const std::uint32_t since = b.touch_stamp_;
                    // Only the current period came since and it kept its bytes:
                    // those; otherwise whole blocks.
                    const bool kept = log.masks_period == log.now && since + 1u == log.now && !log.shared &&
                                      log.block_masks.size() >= stamped;
                    for (std::size_t i = 0; i < stamped; ++i)
                        if (log.stamps[i] > since) m[i] = kept ? log.block_masks[i] : ~std::uint64_t(0);
                }
                const auto mark = [&](std::size_t from, std::size_t to) {
                    to = std::min(to, used);
                    for (std::size_t i = from / delta_block; from < to && i * delta_block < to; ++i)
                        m[i] |= delta_range_mask(i, from, to);
                };
                if (holes_changed) for_each_hole_change(b.holes_, holes, mark);
                if (used > base_used) {
                    // Past the receiver's used size its bytes are zero: the nonzero ones.
                    (void)delta_nonzero_blocks(cur, std::max(base_used, payload_begin), used, blocks);
                    for (const DeltaBlock& blk : blocks) m[blk.offset / delta_block] |= blk.mask;
                }
                DeltaBlock* w = blocks.reset_for(count);
                for (std::size_t i = 0; i < count; ++i) {
                    const std::uint64_t bytes = m[i] == 0 ? 0 : m[i] & delta_range_mask(i, payload_begin, used);
                    if (bytes != 0) delta_append_block(w, totals, next, i, bytes, payload_begin, used, whole_blocks);
                }
                blocks.commit(w);
                return totals;
            }
            // fn(from, to) for the bytes of each hole in one table and not the
            // other (both by offset).
            template <typename Fn>
            static void for_each_hole_change(const std::vector<XBufferCore::WireHole>& old,
                                             const std::vector<XBufferCore::WireHole>& holes, Fn&& fn) {
                std::size_t i = 0, j = 0;
                while (i < old.size() || j < holes.size()) {
                    if (j == holes.size() || (i < old.size() && old[i].offset < holes[j].offset)) {
                        fn(std::size_t(old[i].offset), std::size_t(old[i].offset) + old[i].bytes);
                        ++i;
                    } else if (i == old.size() || holes[j].offset < old[i].offset) {
                        fn(std::size_t(holes[j].offset), std::size_t(holes[j].offset) + holes[j].bytes);
                        ++j;
                    } else {
                        if (old[i].bytes != holes[j].bytes)
                            fn(std::size_t(old[i].offset), std::size_t(old[i].offset) + std::max(old[i].bytes, holes[j].bytes));
                        ++i;
                        ++j;
                    }
                }
            }

            // ---- sender: touch marks (XDeltaOptions::touched_only) -----------
            // The buffer's touch log, made or started over when it no longer
            // describes the content (or lost a touch); null when it cannot be
            // made (then every delta compares everything).
            static DeltaTouchLog* touch_log_for(XBuffer& xbuf) noexcept {
                const XBufferCore& core = xbuf;
                auto& log = xbuf.touch_log_;
                if (!log) {
                    log.reset(new (std::nothrow) DeltaTouchLog);
                    if (!log) return nullptr;
                    log->id = new_delta_stream();
                    log->epoch = core.epoch();
                } else if (log->epoch != core.epoch() || log->lost) {
                    restart_touch_log(*log, core);
                }
                return log.get();
            }
            static void restart_touch_log(DeltaTouchLog& log, const XBufferCore& core) noexcept {
                log.copy = 0;
                log.stamps.clear();  // regrown with zeros when stamps are needed
                log.groups.clear();
                log.id = new_delta_stream();
                log.epoch = core.epoch();
                log.now = 1;
                log.listed = 0;
                log.seen = 0;
                log.lost = false;
                log.overflow = false;
                log.shared = false;
                log.masks_on = false;
                log.stamps_from = UINT32_MAX;
                log.owner = nullptr;
                log.masks_period = 0;
            }
            // Where touches go: the log and the payload they describe.
            struct TouchSink {
                DeltaTouchLog* log = nullptr;
                const char* base = nullptr;
                std::size_t used = 0, segment = 0;
            };
            static bool touch_sink(XBuffer& xbuf, TouchSink& sink) noexcept {
                const XBufferCore& core = xbuf;
                sink.log = touch_log_for(xbuf);
                if (!sink.log) return false;
                sink.base = static_cast<const char*>(core.get_address());
                const XArena* sm = core.arena();
                sink.used = sm ? static_cast<std::size_t>(sm->used_end) : 0u;
                sink.segment = core.segment_size();
                return true;
            }
            // Room in the stamps for block `last` (and every block below
            // used); false (the log lost a touch) when there is none.
            static bool reserve_touch_blocks(DeltaTouchLog& log, std::size_t last, std::size_t used = 0) noexcept {
                if (log.stamps.size() > last && (log.masks_period == 0 || log.block_masks.size() > last)) return true;
                XOFFSET_TRY {
                    const std::size_t blocks = std::max(last + 1, (used + delta_block - 1) / delta_block);
                    if (log.stamps.size() < blocks) {
                        log.stamps.resize(blocks, 0u);
                        log.groups.resize((blocks + delta_touch_group - 1) / delta_touch_group, 0u);
                    }
                    if (log.masks_period != 0 && log.block_masks.size() < log.stamps.size())
                        log.block_masks.resize(log.stamps.size(), 0u);
                } XOFFSET_CATCH(const std::bad_alloc&) {
                    log.lost = true;
                    return false;
                }
                return true;
            }
            static void stamp_block(DeltaTouchLog& log, std::size_t i) noexcept {
                if (!reserve_touch_blocks(log, i)) return;
                log.stamps[i] = log.now;
                log.groups[i / delta_touch_group] = log.now;
            }
            // The listed blocks into the stamps: from now on this period's
            // touches are all stamped.
            static void stamp_touch_list(DeltaTouchLog& log) noexcept {
                // With masks and one baseline the listed bytes go along, and
                // the period's later touches keep theirs (block_masks).
                log.masks_period = log.masks_on && !log.shared ? log.now : 0u;
                for (std::uint32_t k = 0; k < log.listed; ++k) {
                    stamp_block(log, log.blocks[k]);
                    if (log.masks_period != 0 && log.block_masks.size() > log.blocks[k])
                        log.block_masks[log.blocks[k]] = log.masks[k];
                }
                log.stamps_from = std::min(log.stamps_from, log.now);
            }
            // A stamped touch of the bytes `mask` of block i.
            static void stamp_touch(DeltaTouchLog& log, std::size_t i, std::uint64_t mask) noexcept {
                if (log.masks_period != log.now || log.shared) {
                    stamp_block(log, i);
                    return;
                }
                if (!reserve_touch_blocks(log, i)) return;
                std::uint64_t& kept = log.block_masks[i];
                kept = log.stamps[i] == log.now ? kept | mask : mask;
                log.stamps[i] = log.now;
                log.groups[i / delta_touch_group] = log.now;
            }
            // Records a touch of the bytes `mask` of block i.
            XOFFSET_DELTA_INLINE static void touch_block(DeltaTouchLog& log, std::size_t i, std::uint64_t mask) noexcept {
                if (!log.overflow) [[likely]] {
                    const auto block = static_cast<std::uint32_t>(i);
                    const auto bit = static_cast<std::uint16_t>(1u << (block % 16u));
                    std::uint32_t k = 0;
                    if (log.seen & bit) {  // touched before in this period?
                        k = log.listed;
                        while (k > 0 && log.blocks[k - 1] != block) --k;
                    }
                    if (k > 0) {
                        if (log.masks_on) log.masks[k - 1] |= mask;
                    } else if (log.listed < DeltaTouchLog::list_capacity) {
                        log.seen = static_cast<std::uint16_t>(log.seen | bit);
                        log.blocks[log.listed] = block;
                        if (log.masks_on) log.masks[log.listed] = mask;
                        // The next delta compares this block with the baseline's copy.
                        if (log.copy) XOFFSET_DELTA_PREFETCH(reinterpret_cast<const char*>(log.copy + i * delta_block));
                        ++log.listed;
                    } else {
                        log.overflow = true;  // stamped from here on
                        stamp_touch_list(log);
                    }
                }
                if (log.shared || log.overflow) [[unlikely]] stamp_touch(log, i, mask);
            }
            // Records a touch of payload bytes [offset, end) (end past used is cut).
            static void stamp_touch_log(DeltaTouchLog& log, std::size_t offset, std::size_t end,
                                        std::size_t used) noexcept {
                end = std::min(end, used);
                if (end <= offset) return;
                const std::size_t first = offset / delta_block;
                const std::size_t last = (end - 1) / delta_block;
                if (first == last) [[likely]] {  // most touches: one field, one block
                    const std::size_t n = end - offset;
                    touch_block(log, first,
                                (n == delta_block ? ~std::uint64_t(0) : (std::uint64_t(1) << n) - 1u) << (offset % delta_block));
                    return;
                }
                for (std::size_t i = first; i <= last; ++i) {
                    const std::size_t lo = i == first ? offset % delta_block : 0u;
                    const std::size_t hi = i == last ? (end - 1) % delta_block + 1u : delta_block;
                    const std::uint64_t ones = hi - lo == delta_block ? ~std::uint64_t(0)
                                                                      : (std::uint64_t(1) << (hi - lo)) - 1u;
                    touch_block(log, i, ones << lo);
                }
            }
            static void touch_bytes(TouchSink& sink, const void* p, std::size_t bytes) noexcept {
                if (bytes == 0) return;
                const char* at = static_cast<const char*>(p);
                // Bytes outside the payload are not this buffer's (nothing to
                // compare them with): every block is compared next time.
                if (!sink.base || at < sink.base + payload_begin || std::size_t(at - sink.base) > sink.segment ||
                    bytes > sink.segment - std::size_t(at - sink.base)) {
                    sink.log->lost = true;
                    return;
                }
                const auto offset = static_cast<std::size_t>(at - sink.base);
                stamp_touch_log(*sink.log, offset, offset + bytes, sink.used);
            }
            static void touch(XBuffer& xbuf, const void* p, std::size_t bytes) noexcept {
                TouchSink sink;
                if (touch_sink(xbuf, sink)) touch_bytes(sink, p, bytes);
            }
            // A touch of the bytes of [p, p + bytes) (bytes <= 8) that changed:
            // bit k of `changed` set when byte k did.
            static void touch_changed(XBuffer& xbuf, const void* p, std::size_t bytes, std::uint64_t changed) noexcept {
                if (changed == 0) return;
                TouchSink sink;
                if (!touch_sink(xbuf, sink)) return;
                const char* at = static_cast<const char*>(p);
                if (!sink.base || at < sink.base + payload_begin || std::size_t(at - sink.base) > sink.segment ||
                    bytes > sink.segment - std::size_t(at - sink.base)) {
                    sink.log->lost = true;
                    return;
                }
                const auto offset = static_cast<std::size_t>(at - sink.base);
                if (offset + bytes > sink.used) {
                    stamp_touch_log(*sink.log, offset, offset + bytes, sink.used);
                    return;
                }
                const std::size_t first = offset / delta_block, lo = offset % delta_block;
                if (const std::uint64_t low = changed << lo) touch_block(*sink.log, first, low);
                if (lo + bytes > delta_block) {
                    if (const std::uint64_t high = changed >> (delta_block - lo)) touch_block(*sink.log, first + 1, high);
                }
            }
            // value's bytes and the blocks of every string and container it reaches.
            template <typename T>
            static void touch_value(XBuffer& xbuf, const T& value) noexcept {
                TouchSink sink;
                if (!touch_sink(xbuf, sink)) return;
                touch_bytes(sink, &value, sizeof(T));
                touch_reached(sink, value);
            }
            template <typename T>
            static void touch_reached(TouchSink& sink, const T& value) noexcept {
                using CleanT = std::remove_cv_t<T>;
                if constexpr (!wire_value_needs_validation_v<CleanT>) {
                    (void)sink;
                    (void)value;
                } else if constexpr (std::is_array_v<CleanT>) {
                    for (const auto& elem : value) touch_reached(sink, elem);
                } else if constexpr (is_fixed_string_v<CleanT>) {
                    const auto& str = static_cast<const XFixedString&>(value);
                    // The whole block, slack included (a block freed and
                    // reused may leave its old bytes zeroed in the slack).
                    if (!str.is_inline() && str.capacity_ != 0)
                        touch_bytes(sink, delta_heap_data<char>(str.data_delta),
                                    XBufferCore::arena_header::block_size(str.capacity_ + 1u));
                } else if constexpr (is_fixed_flat_set_v<CleanT> || is_fixed_flat_map_v<CleanT> ||
                                     is_fixed_vector_v<CleanT>) {
                    using Elem = typename CleanT::value_type;
                    const auto& v = static_cast<const XFixedVector<Elem>&>(value);
                    if (v.capacity_ == 0) return;
                    const Elem* data = delta_heap_data<Elem>(v.data_delta);
                    touch_bytes(sink, data,
                                XBufferCore::arena_header::block_size(
                                    static_cast<std::uint32_t>(std::size_t(v.capacity_) * sizeof(Elem))));
                    if constexpr (wire_value_needs_validation_v<Elem>)
                        for (std::uint32_t i = 0; i < v.size_; ++i) touch_reached(sink, data[i]);
                } else if constexpr (std::is_class_v<CleanT>) {
                    (void)for_each_reflected_subobject_v<CleanT>(
                        [&](auto index) {
                            touch_reached(sink, static_cast<const schema::base_t<CleanT, decltype(index)::value>&>(value));
                            return vok();
                        },
                        [&](auto index) {
                            using member = schema::member_t<CleanT, decltype(index)::value>;
                            touch_reached(sink, member::get(value));
                            return vok();
                        });
                }
            }
            // After a delta from xbuf: the baseline holds what the buffer held
            // at the log's current stamp; later touches are newer.
            static void pair_touch_log(XBuffer& xbuf, XDeltaBaseline& b) noexcept {
                DeltaTouchLog* log = touch_log_for(xbuf);
                if (!log) {
                    b.touch_log_ = 0;
                    return;
                }
                if (log->owner != nullptr && log->owner != &b && !log->shared) {
                    // A second baseline: from now on every touch is stamped,
                    // so each finds what was touched since its own last delta.
                    log->shared = true;
                    stamp_touch_list(*log);
                    log->masks_period = 0;
                }
                log->owner = &b;
                log->copy = !log->shared && b.record_ == XDeltaRecord::copy && !b.payload_.empty()
                                ? reinterpret_cast<std::uintptr_t>(b.payload_.data())
                                : 0u;
                b.touch_log_ = log->id;
                b.touch_stamp_ = log->now;
                if (log->now == UINT32_MAX) {
                    restart_touch_log(*log, xbuf);  // the next delta compares everything
                    return;
                }
                ++log->now;
                log->listed = 0;
                log->seen = 0;
                log->overflow = false;
                // The only baseline needs no stamps beyond the list.
                if (!log->shared) log->stamps_from = UINT32_MAX;
                // A touches baseline needs the bytes, from the period that starts now.
                if (b.record_ == XDeltaRecord::touches) log->masks_on = true;
            }
            // The blocks a touched_only delta compares when the holes changed
            // (want, per block): those touched since the
            // baseline's stamp, those past its used size, those of holes that
            // came or went.
            // Blocks below both used sizes that the log has stamps for.
            static std::size_t touched_blocks(const DeltaTouchLog& log, std::size_t base_used, std::size_t used) noexcept {
                const std::size_t common = std::min(base_used, used);
                return std::min((common + delta_block - 1) / delta_block,
                                log.stamps.size());
            }
            static void select_touched(const XDeltaBaseline& b, std::vector<std::uint8_t>& want,
                                       const DeltaTouchLog& log, const std::vector<XBufferCore::WireHole>& holes,
                                       std::size_t base_used, std::size_t used) {
                const std::size_t count = (used + delta_block - 1) / delta_block;
                want.assign(count, 0);
                const auto mark = [&](std::size_t from, std::size_t to) {
                    from = std::max(from, payload_begin);
                    to = std::min(to, used);
                    if (to <= from) return;
                    std::fill(want.begin() + std::ptrdiff_t(from / delta_block),
                              want.begin() + std::ptrdiff_t((to - 1) / delta_block + 1), 1);
                };
                const std::size_t stamped = touched_blocks(log, base_used, used);
                const std::uint32_t since = b.touch_stamp_;
                std::uint8_t* w = want.data();
                if (listed_since(log, since)) {
                    for (std::uint32_t k = 0; k < log.listed; ++k)
                        if (log.blocks[k] < count) w[log.blocks[k]] = 1;
                } else {
                    const std::uint32_t* stamps = log.stamps.data();
                    for (std::size_t i = 0; i < stamped; ++i) w[i] = stamps[i] > since ? 1 : 0;
                }
                mark(base_used, used);
                for_each_hole_change(b.holes_, holes, mark);
            }

            // Size of the hole section for a table by offset; an inconsistent
            // table (overlap, misalignment, past used) is a library error.
            static std::size_t hole_section_size(const std::vector<XBufferCore::WireHole>& holes, std::size_t used) {
                if (holes.size() > UINT32_MAX)
                    XOFFSET_THROW(XException(XErrc::invalid_state, "save_delta<T>(): hole table too large"));
                std::size_t size = delta_varint_size(static_cast<std::uint32_t>(holes.size()));
                std::size_t prev = payload_begin;
                for (const auto& h : holes) {
                    if (h.offset < prev || (h.offset & 7u) != 0 || (h.bytes & 7u) != 0 || h.bytes == 0 ||
                        std::size_t(h.offset) + h.bytes > used)
                        XOFFSET_THROW(XException(XErrc::invalid_state, "save_delta<T>(): inconsistent free table"));
                    size += delta_varint_size(static_cast<std::uint32_t>((h.offset - prev) / 8u)) +
                            delta_varint_size(h.bytes / 8u);
                    prev = std::size_t(h.offset) + h.bytes;
                }
                return size;
            }

            // Writes the delta `header` announces: the fingerprint (KEY), the
            // digest of src (DIGEST), the blocks with their bytes from src
            // (`totals` is their encoded size), the holes (HOLES,
            // hole_section bytes).
            static std::size_t header_size(const DeltaHeader& h) noexcept {
                std::size_t size = XDELTA_HEADER_FIXED + delta_varint_size(h.version) + delta_varint_size(h.block_count);
                if (h.flags & XDELTA_FLAG_EXTENT)
                    size += delta_varint_size((h.used_bytes - payload_begin) / 8u) +
                            delta_varint_size(h.root_offset - static_cast<std::uint32_t>(payload_begin));
                return size;
            }
            static char* put_header(char* p, const DeltaHeader& h) noexcept {
                p[0] = 'X';
                p[1] = 'D';
                p[2] = static_cast<char>((XDELTA_FORMAT_MAJOR << 4) | XDELTA_FORMAT_MINOR);
                p[3] = static_cast<char>(h.flags);
                std::memcpy(p + 4, &h.stream, 8);
                p += XDELTA_HEADER_FIXED;
                p = delta_put_varint(p, h.version);
                if (h.flags & XDELTA_FLAG_EXTENT) {
                    p = delta_put_varint(p, (h.used_bytes - payload_begin) / 8u);
                    p = delta_put_varint(p, h.root_offset - static_cast<std::uint32_t>(payload_begin));
                }
                return delta_put_varint(p, h.block_count);
            }

            static void write_delta(std::string& out, const DeltaHeader& header, std::uint64_t schema_hash,
                                    std::uint64_t root_type_id, const char* src, const DeltaBlockBuffer& blocks,
                                    DeltaBlockTotals totals, const std::vector<XBufferCore::WireHole>& holes,
                                    std::size_t hole_section) {
                const bool key = (header.flags & XDELTA_FLAG_KEY) != 0;
                const bool digest = (header.flags & XDELTA_FLAG_DIGEST) != 0;
                const bool with_holes = (header.flags & XDELTA_FLAG_HOLES) != 0;
                const std::size_t size = header_size(header) + (key ? 16u : 0u) + (digest ? 8u : 0u) +
                                         totals.encoded + (with_holes ? hole_section : 0u);
                if (size > out.max_size() - 8u)
                    XOFFSET_THROW(std::length_error("save_delta<T>(): output size exceeds string limit"));
                const auto emit = [&](char* p) noexcept {
                    p = put_header(p, header);
                    if (key) {
                        const std::uint64_t fingerprint[2] = {schema_hash, root_type_id};
                        std::memcpy(p, fingerprint, 16);
                        p += 16;
                    }
                    if (digest) {
                        const std::uint64_t d = xxh64(src + payload_begin, header.used_bytes - payload_begin);
                        std::memcpy(p, &d, 8);
                        p += 8;
                    }
                    std::size_t next = 0;
                    for (const auto& blk : blocks) {
                        const std::size_t index = blk.offset / delta_block;
                        p = delta_put_varint(p, static_cast<std::uint32_t>(index - next));
                        next = index + 1;
                        if (blk.whole) {
                            *p++ = 0;
                            std::memcpy(p, src + blk.offset, delta_block);
                            p += delta_block;
                            continue;
                        }
                        const std::uint64_t sel = delta_byte_flags(blk.mask);
                        *p++ = static_cast<char>(sel);
                        // The nonzero mask bytes: the mask is a word whose bytes sel selects.
                        p = delta_put_masked(p, reinterpret_cast<const char*>(&blk.mask), sel);
                        p = delta_put_masked(p, src + blk.offset, blk.mask);
                    }
                    if (with_holes) {
                        p = delta_put_varint(p, static_cast<std::uint32_t>(holes.size()));
                        std::size_t prev_hole = payload_begin;
                        for (const auto& h : holes) {
                            p = delta_put_varint(p, static_cast<std::uint32_t>((h.offset - prev_hole) / 8u));
                            p = delta_put_varint(p, h.bytes / 8u);
                            prev_hole = std::size_t(h.offset) + h.bytes;
                        }
                    }
                };
                // The gather of the block bytes stores 8 bytes at a time: 8
                // bytes of room past the end.
#if defined(__cpp_lib_string_resize_and_overwrite) && __cpp_lib_string_resize_and_overwrite >= 202110L
                out.resize_and_overwrite(size + 8u, [&](char* p, std::size_t) noexcept {
                    emit(p);
                    return size;
                });
#else
                out.resize(size + 8u);
                emit(out.data());
                out.resize(size);
#endif
            }

            static void key_delta_into(const XDeltaBaseline& b, std::string& out, XDeltaOptions options) {
                if (b.used_ == 0 || b.version_ == 0 || b.record_ != XDeltaRecord::copy)
                    XOFFSET_THROW(XException(XErrc::invalid_state, "XDeltaBaseline::key_delta_into(): no recorded state"));
                const std::size_t used = b.payload_.size();
                DeltaBlockBuffer blocks;
                const DeltaBlockTotals totals =
                    delta_nonzero_blocks(b.payload_.data(), payload_begin, used, blocks, options.whole_blocks);
                DeltaHeader header{};
                header.flags = XDELTA_FLAG_KEY | XDELTA_FLAG_HOLES | XDELTA_FLAG_EXTENT |
                               (options.digest ? XDELTA_FLAG_DIGEST : 0u);
                header.stream = b.stream_;
                header.version = b.version_;
                header.used_bytes = static_cast<std::uint32_t>(used);
                header.root_offset = b.root_offset_;
                header.block_count = static_cast<std::uint32_t>(blocks.size());
                write_delta(out, header, b.schema_hash_, b.root_type_id_, b.payload_.data(), blocks, totals, b.holes_,
                            hole_section_size(b.holes_, used));
            }

            // ---- receiver -------------------------------------------------
            struct Parsed {
                DeltaHeader header;
                const unsigned char* data;       // the delta
                const unsigned char* end;        // its end
                const unsigned char* readable_end = nullptr;  // end of the readable bytes (end, or past it)
                const DeltaBlockRef* blocks;     // its blocks, decoded
                std::size_t block_count;
                std::uint64_t digest = 0;
                std::uint64_t schema_hash = 0;
                std::uint64_t root_type_id = 0;
                std::size_t undo_bytes = 0;      // old bytes of the blocks written, below old_used
                bool key = false;
                bool holes = false;
                bool digest_present = false;
            };

            static std::string msg(const char* who, const char* what) {
                return std::string(who) + ": " + what;
            }

            // Header, block and hole sections; nothing is written to the buffer.
            // room: readable bytes past the end of data. old_used, old_root:
            // the replica's current used_end and root (0 when it holds no
            // verified root of T).
            template <typename T>
            // replica: the payload a non-key delta will patch (null: none);
            // its lines and those of the cursor's check map that the blocks
            // need are fetched while the rest is parsed.
            static vstatus parse(std::span<const std::byte> data, std::size_t room, const XDeltaCursor& cursor,
                                 std::size_t old_used, std::uint64_t old_root,
                                 std::vector<XBufferCore::WireHole>& holes, std::vector<DeltaBlockRef>& blocks,
                                 Parsed& out, const char* who, const char* replica = nullptr) {
                const auto* p = reinterpret_cast<const unsigned char*>(data.data());
                out.data = p;
                const std::size_t n = data.size_bytes();
                out.end = p + n;
                out.readable_end = out.end + room;
                if (n < 2 || p[0] != 'X' || p[1] != 'D')
                    XOFFSET_VFAIL(XException, XErrc::bad_magic, msg(who, "not an XOffset delta (bad magic)"));
                if (n < 3) XOFFSET_VFAIL(XException, XErrc::truncated, msg(who, "delta shorter than its header"));
                if ((p[2] >> 4) != XDELTA_FORMAT_MAJOR || (p[2] & 15u) != XDELTA_FORMAT_MINOR)
                    XOFFSET_VFAIL(XException, XErrc::unsupported_version,
                                  std::string(who) + ": delta format " + std::to_string(unsigned(p[2] >> 4)) + "." +
                                      std::to_string(unsigned(p[2] & 15u)) + " is not supported; this build reads 1.0");
                if (n < XDELTA_HEADER_FIXED)
                    XOFFSET_VFAIL(XException, XErrc::truncated, msg(who, "delta shorter than its header"));
                auto& h = out.header;
                h.flags = p[3];
                if ((h.flags & ~XDELTA_FLAGS_KNOWN) != 0)
                    XOFFSET_VFAIL(XException, XErrc::unsupported_flags, msg(who, "unsupported delta flags"));
                std::memcpy(&h.stream, p + 4, 8);
                out.key = (h.flags & XDELTA_FLAG_KEY) != 0;
                out.holes = (h.flags & XDELTA_FLAG_HOLES) != 0;
                out.digest_present = (h.flags & XDELTA_FLAG_DIGEST) != 0;
                const bool extent = (h.flags & XDELTA_FLAG_EXTENT) != 0;
                const auto* q = p + XDELTA_HEADER_FIXED;
                const auto* const end = p + n;
                std::uint32_t used_words = 0, root_from_begin = 0;
                if (!delta_get_varint(q, end, h.version) ||
                    (extent && (!delta_get_varint(q, end, used_words) || !delta_get_varint(q, end, root_from_begin))) ||
                    !delta_get_varint(q, end, h.block_count))
                    XOFFSET_VFAIL(XException, XErrc::truncated, msg(who, "delta shorter than its header"));
                if (out.key && !extent)
                    XOFFSET_VFAIL(XException, XErrc::unsupported_flags, msg(who, "key delta without extent"));
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
                std::uint64_t used = old_used, root = old_root;
                if (extent) {
                    used = payload_begin + std::uint64_t(used_words) * 8u;
                    root = payload_begin + std::uint64_t(root_from_begin);
                } else if (old_used == 0) {
                    XOFFSET_VFAIL(XException, XErrc::delta_base_mismatch,
                                  msg(who, "buffer holds no verified root of this type (ask for a key delta)"));
                }
                if (used > XBufferCore::MAX_PAYLOAD)
                    XOFFSET_VFAIL(XException, XErrc::size_mismatch, msg(who, "used size mismatch"));
                if (root >= used)
                    XOFFSET_VFAIL(XException, XErrc::root_offset_out_of_bounds, msg(who, "root offset out of bounds"));
                h.used_bytes = static_cast<std::uint32_t>(used);
                h.root_offset = static_cast<std::uint32_t>(root);

                // Blocks: ascending; a mask byte that is present is nonzero; every
                // byte written lies inside [48, used). Each takes at least 3 bytes.
                if (h.block_count > std::size_t(end - q) / 3u)
                    XOFFSET_VFAIL(XException, XErrc::size_mismatch, msg(who, "delta block table truncated"));
                XOFFSET_TRY {
                    if (blocks.size() < h.block_count) blocks.resize(h.block_count);
                } XOFFSET_CATCH(const std::bad_alloc&) {
                    XOFFSET_VFAIL_BAD_ALLOC();
                }
                DeltaBlockRef* refs = blocks.data();
                std::uint64_t next = 0;
                std::size_t undo = 0;
                const bool fetch = replica != nullptr && !out.key;
                const DeltaCheckBits* const checks = cursor.checks_.data();
                const std::size_t check_count = cursor.checks_valid_ ? cursor.checks_.size() : 0u;
                // Mask bytes are expanded with one shuffle where 8 bytes are
                // readable (before the end of the input, or in its room).
                const bool shuffle = delta_shuffle_supported();
                const auto* const readable = out.readable_end;
                for (std::uint32_t k = 0; k < h.block_count; ++k) {
                    std::uint32_t skip = 0;
                    if (!delta_get_varint(q, end, skip) || q == end)
                        XOFFSET_VFAIL(XException, XErrc::size_mismatch, msg(who, "delta block table truncated"));
                    const std::uint64_t index = next + skip;
                    const std::uint64_t start = index * delta_block;
                    if (start >= used)
                        XOFFSET_VFAIL(XException, XErrc::invalid_extent_table, msg(who, "delta block out of bounds"));
                    const unsigned sel = *q++;
                    // Block 0 starts in the arena header, which no delta writes.
                    if (start < payload_begin && sel == 0)
                        XOFFSET_VFAIL(XException, XErrc::invalid_extent_table, msg(who, "delta block writes the header"));
                    std::uint64_t mask = ~std::uint64_t(0);
                    std::size_t bytes = delta_block;
                    if (sel == 0) {
                        if (start + delta_block > used)
                            XOFFSET_VFAIL(XException, XErrc::invalid_extent_table, msg(who, "delta block out of bounds"));
                    } else {
                        const std::size_t present = delta_shuffle.count[sel];
                        if (std::size_t(end - q) < present)
                            XOFFSET_VFAIL(XException, XErrc::size_mismatch, msg(who, "delta block table truncated"));
                        bool nonzero = true;
                        if (shuffle && readable - q >= 8) {
                            nonzero = delta_expand_mask_shuffle(q, sel, mask, bytes);
                            q += present;
                        } else {
                            mask = 0;
                            bytes = 0;
                            for (unsigned s = sel; s != 0; s &= s - 1) {
                                const unsigned byte = *q++;
                                nonzero &= byte != 0;
                                bytes += delta_shuffle.count[byte];
                                mask |= std::uint64_t(byte) << (8 * std::countr_zero(s));
                            }
                        }
                        if (!nonzero)
                            XOFFSET_VFAIL(XException, XErrc::invalid_extent_table, msg(who, "delta block mask byte is zero"));
                        if (start + 63u - unsigned(std::countl_zero(mask)) >= used)
                            XOFFSET_VFAIL(XException, XErrc::invalid_extent_table, msg(who, "delta block out of bounds"));
                        if (start < payload_begin && start + unsigned(std::countr_zero(mask)) < payload_begin)
                            XOFFSET_VFAIL(XException, XErrc::invalid_extent_table, msg(who, "delta block writes the header"));
                    }
                    if (std::size_t(end - q) < bytes)
                        XOFFSET_VFAIL(XException, XErrc::size_mismatch, msg(who, "delta block table truncated"));
                    refs[k] = {static_cast<std::uint32_t>(start), static_cast<std::uint32_t>(q - p), mask};
                    if (fetch) {
                        XOFFSET_DELTA_PREFETCH(replica + start);
                        if (index < check_count) XOFFSET_DELTA_PREFETCH(checks + index);
                    }
                    q += bytes;
                    const std::size_t lo = std::max<std::size_t>(std::size_t(start), payload_begin);
                    if (lo < old_used) undo += std::min<std::size_t>(std::size_t(start) + delta_block, old_used) - lo;
                    next = index + 1;
                }
                out.blocks = refs;
                out.block_count = h.block_count;
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

            template <typename BlockFn>
            static void for_each_block(const Parsed& parsed, BlockFn&& fn) {
                for (std::size_t k = 0; k < parsed.block_count; ++k) {
                    const DeltaBlockRef& r = parsed.blocks[k];
                    fn(std::size_t(r.offset), reinterpret_cast<const char*>(parsed.data + r.src), r.mask);
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

            // ---- receiver: the map of checked bytes --------------------------
            // The map holds for the buffer state recorded with it. A buffer that
            // was moved, reloaded, reset, swapped, grown into new storage, or
            // that allocated or freed since, no longer matches, and the next
            // delta is verified in full. (A replica is not changed in place
            // between deltas: docs/DELTA_SYNC.md.)
            static bool checks_match(const XDeltaCursor& c, const XBufferCore& core) noexcept {
                const XArena* sm = core.arena();
                return c.checks_valid_ && c.checks_core_ == &core && c.checks_base_ == core.get_address() &&
                       c.checks_epoch_ == core.epoch() && c.checks_used_ == sm->used_end &&
                       c.checks_root_ == sm->root_offset && c.checks_free_ == core.free_block_count() &&
                       c.checks_free_complete_ == core.free_table_complete();
            }
            static void note_checks(XDeltaCursor& c, const XBufferCore& core) noexcept {
                c.checks_core_ = &core;
                c.checks_base_ = core.get_address();
                c.checks_epoch_ = core.epoch();
                c.checks_used_ = core.arena()->used_end;
                c.checks_root_ = core.arena()->root_offset;
                c.checks_free_ = core.free_block_count();
                c.checks_free_complete_ = core.free_table_complete();
                c.checks_valid_ = true;
            }
            // Room for at least n elements, growing by half again at least: a
            // map that grows a little with each delta reallocates (and copies
            // itself) only now and then, not at every growth.
            template <typename V>
            static void reserve_room(V& v, std::size_t n) {
                if (v.capacity() < n) v.reserve(std::max(n, v.capacity() + v.capacity() / 2));
            }
            // Room for the map of a payload of `used` bytes (before the payload
            // changes, so that rebuilding it cannot fail), with a quarter more
            // for the payload to grow into.
            static vstatus size_checks(XDeltaCursor& c, std::size_t used) {
                c.checks_valid_ = false;
                XOFFSET_TRY {
                    const std::size_t blocks = (used + delta_block - 1) / delta_block;
                    reserve_room(c.checks_, blocks + blocks / 4);
                    reserve_room(c.live_, live_words(used + used / 4));
                    c.checks_.resize(blocks);
                    c.live_.resize(live_words(used));
                } XOFFSET_CATCH(const std::bad_alloc&) {
                    XOFFSET_VFAIL_BAD_ALLOC();
                }
                return vok();
            }
            // Room for the map of a payload grown to `used` bytes (the new
            // bytes checked by nothing, in no live block yet).
            static bool grow_checks(XDeltaCursor& c, std::size_t used) noexcept {
                const std::size_t blocks = (used + delta_block - 1) / delta_block;
                XOFFSET_TRY {
                    if (c.checks_.size() < blocks) {
                        reserve_room(c.checks_, blocks);
                        c.checks_.resize(blocks);
                    }
                    if (c.live_.size() < live_words(used)) {
                        reserve_room(c.live_, live_words(used));
                        c.live_.resize(live_words(used));
                    }
                } XOFFSET_CATCH(const std::bad_alloc&) {
                    return false;
                }
                return true;
            }
            // Words of the map of live blocks of a payload of `used` bytes.
            static constexpr std::size_t live_words(std::size_t used) noexcept { return (used + 511u) / 512u; }
            // After a whole verification of an incremental delta: the state is
            // recorded; the map is built when a delta first needs it (a
            // replica whose deltas keep needing the whole verification, like
            // a store that receives saves, does not build it each time).
            static void defer_checks(XDeltaCursor& c, const XBufferCore& core) noexcept {
                c.checks_built_ = false;
                note_checks(c, core);
            }
            // The map of the recorded state (which the buffer still holds);
            // false when it cannot be built (the delta is then verified in
            // full).
            template <typename T>
            static bool ensure_checks(XDeltaCursor& c, XBufferCore& core, std::uint64_t root_offset) {
                if (c.checks_built_) return true;
                const T* root = find_root_at<T>(core, root_offset);
                if (!root) {
                    c.checks_valid_ = false;
                    return false;
                }
                std::fill(c.checks_.begin(), c.checks_.end(), DeltaCheckBits{});
                std::fill(c.live_.begin(), c.live_.end(), std::uint64_t(0));
                c.nodes_.clear();
                c.ranges_.clear();
                XOFFSET_TRY {
                    DeltaCheckMarker m(static_cast<const char*>(core.get_address()), c.checks_.data(), &c.nodes_,
                                       &c.ranges_, c.live_.data());
                    m.live_at(0, payload_begin);  // the arena header
                    m.live(root, sizeof(T));
                    delta_mark_value(*root, m);
                    index_ranges(c);
                    c.touched_.reserve(c.nodes_.size());
                } XOFFSET_CATCH(const std::bad_alloc&) {
                    c.checks_valid_ = false;
                    return false;
                }
                std::size_t live = 0;
                for (const std::uint64_t w : c.live_) live += static_cast<std::size_t>(delta_popcount(w));
                c.live_granules_ = live;
                c.checks_built_ = true;
                return true;
            }
            // For tests: the map kept from delta to delta equals the one built
            // afresh for the buffer's current state (marks, live blocks and
            // nodes); true when no map is kept. what: the first difference.
            template <typename T>
            static bool checks_agree(const XDeltaCursor& c, XBufferCore& core, const char*& what) {
                what = "";
                if (!c.checks_valid_ || !c.checks_built_ || !checks_match(c, core)) return true;
                const std::size_t used = static_cast<std::size_t>(core.arena()->used_end);
                const T* root = find_root_at<T>(core, core.arena()->root_offset);
                if (!root) return (what = "root not found"), false;
                std::vector<DeltaCheckBits> bits((used + delta_block - 1) / delta_block);
                std::vector<std::uint64_t> live(live_words(used));
                std::vector<DeltaCheckNode> nodes;
                std::vector<DeltaNodeRange> ranges;
                DeltaCheckMarker m(static_cast<const char*>(core.get_address()), bits.data(), &nodes, &ranges, live.data());
                m.live_at(0, payload_begin);
                m.live(root, sizeof(T));
                delta_mark_value(*root, m);
                if (c.checks_.size() < bits.size()) return (what = "map shorter than the payload"), false;
                for (std::size_t b = 0; b < c.checks_.size(); ++b) {
                    const DeltaCheckBits want = b < bits.size() ? bits[b] : DeltaCheckBits{};
                    if (c.checks_[b].checked != want.checked) return (what = "checked bytes differ"), false;
                    if (c.checks_[b].local != want.local) return (what = "bytes checked in place differ"), false;
                    if (c.checks_[b].heads != want.heads) return (what = "head marks differ"), false;
                }
                if (c.live_.size() < live.size()) return (what = "live map shorter than the payload"), false;
                std::size_t granules = 0;
                for (std::size_t w = 0; w < c.live_.size(); ++w) {
                    if (c.live_[w] != (w < live.size() ? live[w] : 0u)) return (what = "live blocks differ"), false;
                    granules += static_cast<std::size_t>(delta_popcount(c.live_[w]));
                }
                if (granules != c.live_granules_) return (what = "live block count differs"), false;
                const auto key = [](const DeltaCheckNode& n) {  // a vector's size is checked in place, not kept
                    return std::tuple(n.head, std::uint32_t(n.kind), n.capacity, n.data_delta,
                                      n.kind == delta_node_vector ? 0u : n.size, n.element, n.key_size);
                };
                std::vector<decltype(key(nodes[0]))> have, want;
                for (const DeltaCheckNode& n : c.nodes_)
                    if (n.kind != delta_node_dead) have.push_back(key(n));
                for (const DeltaCheckNode& n : nodes) want.push_back(key(n));
                std::sort(have.begin(), have.end());
                std::sort(want.begin(), want.end());
                if (have != want) return (what = "nodes differ"), false;
                std::size_t r = 0;  // the node ranges: the same set, by begin
                if (c.ranges_.size() != ranges.size()) return (what = "node ranges differ"), false;
                std::vector<std::pair<std::uint32_t, std::uint32_t>> hr, wr;
                for (const DeltaNodeRange& x : c.ranges_) hr.emplace_back(x.begin, x.end);
                for (const DeltaNodeRange& x : ranges) wr.emplace_back(x.begin, x.end);
                std::sort(wr.begin(), wr.end());
                for (; r < hr.size(); ++r)
                    if (hr[r] != wr[r]) return (what = "node ranges differ"), false;
                return true;
            }
            // Sorts the node ranges by begin (they do not overlap), by block
            // first (a counting sort; then the few in one block), and indexes
            // them: range_index_[b] = the number of ranges that begin before
            // block b of the map.
            static void index_ranges(XDeltaCursor& c) {
                const std::size_t blocks = c.checks_.size();
                auto& index = c.range_index_;
                index.assign(blocks + 1, 0u);
                for (const DeltaNodeRange& r : c.ranges_) ++index[r.begin / delta_block + 1];
                for (std::size_t b = 1; b <= blocks; ++b) index[b] += index[b - 1];
                auto& sorted = c.range_scratch_;
                sorted.resize(c.ranges_.size());
                for (const DeltaNodeRange& r : c.ranges_) sorted[index[r.begin / delta_block]++] = r;
                for (std::size_t b = blocks; b > 0; --b) index[b] = index[b - 1];  // back to bucket starts
                index[0] = 0;
                for (std::size_t i = 1; i < sorted.size(); ++i) {  // moves stay within a block
                    const DeltaNodeRange r = sorted[i];
                    std::size_t j = i;
                    for (; j > 0 && sorted[j - 1].begin > r.begin; --j) sorted[j] = sorted[j - 1];
                    sorted[j] = r;
                }
                c.ranges_.swap(sorted);
            }
            // The node whose range holds `offset`, or none: the last range
            // that begins at or before it, from its block or before.
            static const DeltaNodeRange* node_range_of(const XDeltaCursor& c, std::size_t offset) noexcept {
                const std::size_t b = offset / delta_block;
                if (b + 1 >= c.range_index_.size()) return nullptr;
                const DeltaNodeRange* r = c.ranges_.data();
                std::size_t k = c.range_index_[b];
                const std::size_t e = c.range_index_[b + 1];
                const DeltaNodeRange* hit = k > 0 ? &r[k - 1] : nullptr;
                for (; k < e && r[k].begin <= offset; ++k) hit = &r[k];
                return hit && offset < hit->end ? hit : nullptr;
            }
            // Before the patch: the nodes owning the checked bytes outside
            // bools and inline string heads that the delta writes; false when
            // some byte belongs to none (the whole verification is needed).
            static bool collect_nodes(XDeltaCursor& c, const Parsed& parsed) noexcept {
                c.touched_.clear();
                if (++c.stamp_ == 0) {
                    for (auto& n : c.nodes_) n.stamp = n.moved = 0;
                    c.stamp_ = 1;
                }
                for (std::size_t k = 0; k < parsed.block_count; ++k) {
                    const DeltaBlockRef& r = parsed.blocks[k];
                    const DeltaCheckBits& b = c.checks_[r.offset / delta_block];
                    for (std::uint64_t hit = r.mask & b.checked & ~b.local; hit != 0;) {
                        const std::size_t offset = r.offset + static_cast<std::size_t>(std::countr_zero(hit));
                        const DeltaNodeRange* range = node_range_of(c, offset);
                        if (!range) return false;
                        DeltaCheckNode& n = c.nodes_[range->node];
                        if (n.stamp != c.stamp_) {
                            n.stamp = c.stamp_;
                            c.touched_.push_back(range->node);  // reserved for every node
                        }
                        // The rest of the range in this block belongs to the same node.
                        const std::size_t past = range->end - r.offset;
                        hit = past >= delta_block ? 0 : hit & ~((std::uint64_t(1) << past) - 1u);
                    }
                }
                return true;
            }
            // An inline or empty string head that passes verification.
            static bool inline_head_ok(const char* head) noexcept {
                std::uint64_t low;
                std::uint32_t high;
                std::memcpy(&low, head, 8);
                std::memcpy(&high, head + 8, 4);
                const unsigned length = (high >> 24) & 0x7Fu;
                if ((low | high) == 0) return true;  // empty: no block
                return (high & XFixedString::inline_flag) != 0 && length <= XFixedString::inline_max &&
                       ((low & inline_pad_masks.low[length]) | (high & inline_pad_masks.high[length])) == 0;
            }
            // Bytes of the block of a container of `capacity` (rounded to 8).
            static std::size_t node_block_bytes(DeltaNodeKind kind, std::uint32_t capacity,
                                                std::uint32_t element) noexcept {
                const std::size_t raw =
                    kind == delta_node_string ? std::size_t(capacity) + 1u : std::size_t(capacity) * element;
                return (raw + 7u) & ~std::size_t(7u);
            }
            // The head of a heap string, vector, set or map at `head` after the
            // patch, checked as verification checks it (its block in
            // [0, used), 8-aligned; a string's terminator), not counting
            // overlaps: false when it fails. bytes: its block (0: none).
            static bool head_block(const char* base, std::size_t used, std::size_t head, DeltaNodeKind kind,
                                   std::uint32_t element, std::size_t& begin, std::size_t& bytes) noexcept {
                std::int32_t data_delta;
                std::uint32_t size, capacity;
                std::memcpy(&data_delta, base + head, 4);
                std::memcpy(&size, base + head + 4, 4);
                std::memcpy(&capacity, base + head + 8, 4);
                bytes = 0;
                if (size > capacity) return false;
                if (kind == delta_node_string ? capacity >= XBufferCore::MAX_PAYLOAD
                                              : capacity > (XBufferCore::MAX_PAYLOAD - 8u) / element)
                    return false;
                if (capacity == 0) return size == 0 && data_delta == 0;
                const std::size_t block = node_block_bytes(kind, capacity, element);
                const std::int64_t at = std::int64_t(head) + data_delta;
                if (at < 0 || (at & 7) != 0 || std::uint64_t(at) > used || block > used - std::size_t(at)) return false;
                begin = static_cast<std::size_t>(at);
                bytes = block;
                return kind != delta_node_string || base[begin + size] == '\0';
            }
            // After the patch: 0 when touched node `n` fails what verification
            // checks of it, 1 when it passes with the same capacity and block,
            // 2 when its capacity or block changed (it moved, was allocated or
            // freed, or a heap string became inline or empty) and its head and
            // new block pass on their own (mv: the block freed and the block
            // made live; overlaps are checked against the map later). A vector
            // of records: 2 when it grew within its block (its new elements
            // are verified by cover), 3 when it moved or shrank and its head
            // and new block pass (all its elements are verified again, and
            // what its old elements owned is freed: drop_old_elements).
            static int node_check(const char* base, std::size_t used, const DeltaCheckNode& n, DeltaMove& mv) noexcept {
                const char* head = base + n.head;
                std::int32_t data_delta;
                std::uint32_t size, capacity;
                std::memcpy(&data_delta, head, 4);
                std::memcpy(&size, head + 4, 4);
                std::memcpy(&capacity, head + 8, 4);
                if (n.kind == delta_node_records) {
                    mv.head = n.head;
                    if (capacity == n.capacity && data_delta == n.data_delta && size >= n.size) {
                        if (size > capacity) return 0;
                        if (size == n.size) return 1;
                        mv.grow = true;
                        mv.old_begin = n.size;
                        mv.new_begin = size;
                        return 2;
                    }
                    mv.rebuild = true;
                    mv.size = size;
                    if (n.capacity != 0) {
                        mv.old_begin = static_cast<std::size_t>(std::int64_t(n.head) + n.data_delta);
                        mv.old_bytes = node_block_bytes(n.kind, n.capacity, n.element);
                    }
                    return head_block(base, used, n.head, n.kind, n.element, mv.new_begin, mv.new_bytes) ? 3 : 0;
                }
                if (capacity == n.capacity && data_delta == n.data_delta) {
                    if (size > capacity) return 0;
                    if (capacity == 0) return size == 0 ? 1 : 0;
                    if (n.kind == delta_node_string) return head[data_delta + std::int64_t(size)] == '\0' ? 1 : 0;
                    return !n.keys_ordered || n.keys_ordered(head + data_delta, size) ? 1 : 0;
                }
                mv.head = n.head;
                if (n.capacity != 0) {
                    mv.old_begin = static_cast<std::size_t>(std::int64_t(n.head) + n.data_delta);
                    mv.old_bytes = node_block_bytes(n.kind, n.capacity, n.element);
                }
                if (n.kind == delta_node_string && (capacity & XFixedString::inline_flag) != 0)
                    return inline_head_ok(head) ? 2 : 0;
                if (!head_block(base, used, n.head, n.kind, n.element, mv.new_begin, mv.new_bytes)) return 0;
                if (mv.new_bytes != 0 && n.keys_ordered && !n.keys_ordered(base + mv.new_begin, size)) return 0;
                return 2;
            }
            // After the commit: a touched node's checked bytes for its new size.
            static void remark_node(DeltaCheckMarker& m, DeltaCheckNode& n) noexcept {
                const char* head = m.base() + n.head;
                std::uint32_t size;
                std::memcpy(&size, head + 4, 4);
                if (n.capacity != 0 && n.kind != delta_node_vector && n.kind != delta_node_records) {
                    const std::size_t data = n.head + static_cast<std::size_t>(std::int64_t(n.data_delta));
                    if (n.kind == delta_node_string) {
                        m.clear_at(data + n.size, 1);
                        m.mark_at(data + size, 1, false);
                    } else {
                        // The keys of elements [0, size) are marked (at fixed
                        // places in the same block): only those that came or
                        // went change.
                        for (std::uint32_t i = n.size; i < size; ++i)
                            m.mark_at(data + std::size_t(i) * n.element, n.key_size, false);
                        for (std::uint32_t i = size; i < n.size; ++i)
                            m.clear_at(data + std::size_t(i) * n.element, n.key_size);
                    }
                }
                n.size = size;
            }
            // The start of the head (a bit of DeltaCheckBits::*Which) that holds
            // checked byte `offset`, at most `reach` bytes before it, or 0.
            // Heads do not overlap: at most one starts that close.
            template <std::uint64_t DeltaCheckBits::*Which>
            static std::size_t head_before(const DeltaCheckBits* bits, std::size_t offset, std::size_t reach) noexcept {
                const std::size_t block = offset / delta_block, bit = offset % delta_block;  // reach < 64
                std::uint64_t in = (bits[block].*Which) & (bit == delta_block - 1 ? ~std::uint64_t(0)
                                                                                 : (std::uint64_t(2) << bit) - 1u);
                if (bit >= reach) in &= ~((std::uint64_t(1) << (bit - reach)) - 1u);
                if (in != 0) return block * delta_block + 63u - unsigned(std::countl_zero(in));
                if (bit < reach && block > 0) {  // the end of the block before
                    const unsigned from = unsigned(delta_block - (reach - bit));
                    const std::uint64_t before = (bits[block - 1].*Which) >> from;
                    if (before != 0)
                        return (block - 1) * delta_block + from + 63u - unsigned(std::countl_zero(before));
                }
                return 0;
            }
            // Calls on_vector(offset of the head) once for each vector head
            // whose size the delta wrote, on_head(offset of the head) once for
            // each inline string head it wrote checked bytes of, and
            // on_bool(offset) for each bool it wrote; stops at the first false.
            template <typename VectorFn, typename HeadFn, typename BoolFn>
            static bool for_each_checked_write(const Parsed& parsed, const DeltaCheckBits* bits, VectorFn&& on_vector,
                                               HeadFn&& on_head, BoolFn&& on_bool) noexcept {
                std::size_t last_head = 0;
                for (std::size_t k = 0; k < parsed.block_count; ++k) {
                    const DeltaBlockRef& r = parsed.blocks[k];
                    const std::uint64_t written = r.mask & bits[r.offset / delta_block].local;
                    for (std::uint64_t hit = written; hit != 0;) {
                        const std::size_t offset = r.offset + static_cast<std::size_t>(std::countr_zero(hit));
                        const std::size_t head =
                            head_before<&DeltaCheckBits::heads>(bits, offset, sizeof(XFixedString) - 1);
                        if (head != 0) {
                            // A vector head's last byte is not local, a string head's is.
                            const std::size_t e = head + sizeof(XFixedString) - 1;
                            const bool vector = ((bits[e / delta_block].local >> (e % delta_block)) & 1u) == 0;
                            if (head != last_head && !(vector ? on_vector(head) : on_head(head))) return false;
                        } else {
                            if (!on_bool(offset)) return false;
                            hit &= hit - 1;
                            continue;
                        }
                        // The rest of the head in this block is done.
                        last_head = head;
                        const std::size_t past = head + sizeof(XFixedString) - r.offset;
                        hit = past >= delta_block ? 0 : hit & ~((std::uint64_t(1) << past) - 1u);
                    }
                }
                return true;
            }
            // After the patch, with the moves of the delta found: their freed
            // blocks leave the map of live blocks, their new blocks enter it
            // (each must have been free), no live block lies past used_end,
            // and the hole table (the delta's, or the replica's when the delta
            // carries none) tiles the rest: every hole valid and free, live
            // and hole bytes adding up to used_end. This is the coverage that
            // the whole verification checks. False: it does not hold (the map
            // is then rebuilt by the whole verification).
            static bool cover(XDeltaCursor& c, const XBufferCore& core, const char* base, const Parsed& parsed,
                              const std::vector<XBufferCore::WireHole>& holes, std::size_t used,
                              std::size_t old_used) {
                std::uint64_t* live = c.live_.data();
                std::size_t count = c.live_granules_;
                for (const DeltaMove& mv : c.moves_) {
                    if (mv.grow || mv.old_bytes == 0) continue;
                    if (mv.old_begin + mv.old_bytes > old_used) return false;  // cannot happen: it was verified
                    for (std::size_t g = mv.old_begin / 8u, e = g + mv.old_bytes / 8u; g < e; ++g)
                        count -= (live[g / 64] >> (g % 64)) & 1u;
                    delta_bits_clear(live, mv.old_begin / 8u, mv.old_bytes / 8u);
                }
                for (const DeltaMove& mv : c.moves_) {
                    if (mv.grow || mv.new_bytes == 0) continue;
                    if (delta_bits_any(live, mv.new_begin / 8u, mv.new_bytes / 8u)) return false;
                    delta_bits_set(live, mv.new_begin / 8u, mv.new_bytes / 8u);
                    count += mv.new_bytes / 8u;
                }
                // The checked bytes of the blocks moved nodes freed are
                // cleared first: a new element's block may lie where one of
                // them was, and its marks come next.
                {
                    DeltaCheckMarker m(base, c.checks_.data());
                    for (const DeltaMove& mv : c.moves_) {
                        if (mv.node == DeltaMove::new_node || mv.grow || mv.rebuild || mv.dropped) continue;
                        const DeltaCheckNode& n = c.nodes_[mv.node];
                        if (n.capacity == 0 || n.kind == delta_node_vector) continue;
                        const std::size_t data = static_cast<std::size_t>(std::int64_t(n.head) + n.data_delta);
                        if (n.kind == delta_node_string) {
                            m.clear_at(data + n.size, 1);
                        } else {
                            for (std::uint32_t i = 0; i < n.size; ++i)
                                m.clear_at(data + std::size_t(i) * n.element, n.key_size);
                        }
                    }
                }
                // New elements of vectors of records, and all the elements
                // of rebuilt ones: verified, their blocks claimed, and marked
                // (nodes appended; the ranges are rebuilt after the commit).
                bool grew = false;
                XOFFSET_TRY {
                    DeltaCheckMarker m(base, c.checks_.data(), &c.nodes_, &c.ranges_, live);
                    for (std::size_t k = 0; k < c.moves_.size(); ++k) {
                        const DeltaMove mv = c.moves_[k];
                        if (!mv.grow && !mv.rebuild) continue;
                        const DeltaCheckNode n = c.nodes_[mv.node];  // a copy: nodes may be appended
                        DeltaVerifyContext v{base, used, live, 0, &m};
                        const bool ok = mv.rebuild
                                            ? n.grow(v, mv.new_begin, 0, mv.size)
                                            : n.grow(v, static_cast<std::size_t>(std::int64_t(n.head) + n.data_delta),
                                                     static_cast<std::uint32_t>(mv.old_begin),
                                                     static_cast<std::uint32_t>(mv.new_begin));
                        if (!ok) return false;
                        count += v.claimed;
                        grew = true;
                    }
                    if (grew) {
                        const std::size_t nodes = c.nodes_.size();
                        reserve_room(c.touched_, nodes);
                        reserve_room(c.ranges_, 2 * nodes);
                        reserve_room(c.range_scratch_, 2 * nodes);
                    }
                } XOFFSET_CATCH(const std::bad_alloc&) {
                    return false;
                }
                if (used < old_used && delta_bits_any(live, used / 8u, (old_used - used) / 8u)) return false;
                std::uint64_t free = 0;
                const auto hole_ok = [&](XBufferCore::WireHole h) noexcept {
                    if (h.bytes == 0 || (h.bytes & 7u) != 0 || (h.offset & 7u) != 0 || h.offset < payload_begin ||
                        h.offset > used || h.bytes > used - h.offset)
                        return false;
                    if (delta_bits_any(live, h.offset / 8u, h.bytes / 8u)) return false;
                    free += h.bytes;
                    return true;
                };
                if (parsed.holes) {
                    for (const XBufferCore::WireHole& h : holes)
                        if (!hole_ok(h)) return false;
                } else {
                    if (!core.free_table_complete()) return false;
                    bool ok = true;
                    core.for_each_free_block([&](XBufferCore::WireHole h) { ok = ok && hole_ok(h); });
                    if (!ok) return false;
                }
                if (std::uint64_t(count) * 8u + free != used) return false;
                c.live_granules_ = count;
                return true;
            }
            // A vector of records that moved or shrank (a rebuild move): the
            // nodes inside its old elements, and inside theirs, become dead,
            // their marks cleared and their blocks freed (moves with
            // `dropped`); its old elements are unmarked. cover then verifies
            // all its elements in the new block as it verifies new elements,
            // which claims their blocks and marks them again. A rebuilt
            // vector inside another one is dropped with it. (Before cover
            // marks anything; a check that fails after this sends the delta
            // to the whole verification, which rebuilds the map.)
            static void drop_old_elements(XDeltaCursor& c) {
                DeltaCheckMarker m(nullptr, c.checks_.data());
                auto& regions = c.range_scratch_;  // old elements, [begin, end)
                regions.clear();
                const auto elements = [](const DeltaCheckNode& n) {
                    return static_cast<std::size_t>(std::int64_t(n.head) + n.data_delta);
                };
                const auto add_region = [&](const DeltaCheckNode& n, std::uint32_t index) {
                    if (n.capacity == 0 || n.size == 0) return;
                    const std::size_t data = elements(n);
                    regions.push_back({static_cast<std::uint32_t>(data),
                                       static_cast<std::uint32_t>(data + std::size_t(n.size) * n.element), index});
                };
                for (const DeltaMove& mv : c.moves_)
                    if (mv.rebuild) add_region(c.nodes_[mv.node], mv.node);
                const auto inside = [&](std::uint32_t head) noexcept {
                    for (const DeltaNodeRange& r : regions)
                        if (head >= r.begin && head < r.end) return true;
                    return false;
                };
                for (bool more = !regions.empty(); more;) {
                    more = false;
                    for (std::size_t i = 0; i < c.nodes_.size(); ++i) {
                        DeltaCheckNode& n = c.nodes_[i];
                        if (n.kind == delta_node_dead || !inside(n.head)) continue;
                        const auto index = static_cast<std::uint32_t>(i);
                        if (n.capacity != 0) {
                            const std::size_t data = elements(n);
                            DeltaMove mv;
                            mv.node = index;
                            mv.head = n.head;
                            mv.dropped = true;
                            mv.old_begin = data;
                            mv.old_bytes = node_block_bytes(n.kind, n.capacity, n.element);
                            c.moves_.push_back(mv);
                            if (n.kind == delta_node_string) {
                                m.clear_at(data + n.size, 1);
                            } else if (n.kind != delta_node_vector) {  // keys, or the elements of records
                                m.clear_all_at(data, std::size_t(n.size) * n.element);
                                if (n.kind == delta_node_records && n.size != 0) {
                                    add_region(n, index);
                                    more = true;
                                }
                            }
                        }
                        n.kind = delta_node_dead;
                        n.moved = c.stamp_;
                    }
                }
                // Moves of nodes dropped with a rebuilt vector (a vector of
                // records inside one that grew or was rebuilt itself) go.
                c.moves_.erase(std::remove_if(c.moves_.begin(), c.moves_.end(),
                                              [&](const DeltaMove& mv) {
                                                  return !mv.dropped && mv.node != DeltaMove::new_node &&
                                                         c.nodes_[mv.node].kind == delta_node_dead;
                                              }),
                               c.moves_.end());
                for (const DeltaMove& mv : c.moves_) {
                    if (!mv.rebuild) continue;
                    const DeltaCheckNode& n = c.nodes_[mv.node];
                    if (n.capacity != 0) m.clear_all_at(elements(n), std::size_t(n.size) * n.element);
                }
            }
            // The in-place checks: the containers, vector sizes, bools and inline
            // or empty string heads the delta wrote pass what verification
            // checks of them; a container whose capacity or block changed, and
            // a string head that left the inline or empty form, has its head
            // and new block checked on its own and the coverage checked
            // against the map of live blocks (cover). `structural`: the delta
            // changes used_end or the hole table. False when a check does not
            // hold, or the delta changes something these checks do not cover:
            // the whole verification then decides (so a delta is accepted or
            // rejected, with the same error, as the whole verification would).
            static bool holds_incremental(XDeltaCursor& c, const XBufferCore& core, const char* base,
                                          const Parsed& parsed, const std::vector<XBufferCore::WireHole>& holes,
                                          std::size_t used, std::size_t old_used, bool structural) {
                c.moves_.clear();
                XOFFSET_TRY {
                    // Vectors of records first: what the old elements of one
                    // that moved or shrank owned is dropped before the other
                    // nodes are checked (the nodes inside are verified again
                    // with it).
                    bool rebuilt = false;
                    for (const std::uint32_t index : c.touched_) {
                        DeltaCheckNode& n = c.nodes_[index];
                        if (n.kind != delta_node_records) continue;
                        DeltaMove mv;
                        const int r = node_check(base, used, n, mv);
                        if (r == 0) return false;
                        if (r >= 2) {
                            mv.node = index;
                            n.moved = c.stamp_;
                            c.moves_.push_back(mv);
                            rebuilt = rebuilt || r == 3;
                        }
                    }
                    if (rebuilt) drop_old_elements(c);
                    for (const std::uint32_t index : c.touched_) {
                        DeltaCheckNode& n = c.nodes_[index];
                        if (n.kind == delta_node_records || n.kind == delta_node_dead) continue;
                        DeltaMove mv;
                        const int r = node_check(base, used, n, mv);
                        if (r == 0) return false;
                        if (r == 2) {
                            mv.node = index;
                            n.moved = c.stamp_;
                            c.moves_.push_back(mv);
                        }
                    }
                    const bool held = for_each_checked_write(
                        parsed, c.checks_.data(),
                        [&](std::size_t head) {
                            std::uint32_t size, capacity;  // block and capacity: the vector's node, if any
                            std::memcpy(&size, base + head + 4, 4);
                            std::memcpy(&capacity, base + head + 8, 4);
                            return size <= capacity;
                        },
                        [&](std::size_t head) {
                            if (inline_head_ok(base + head)) return true;
                            // A heap string now: its head and block on their own.
                            std::uint32_t capacity;
                            std::memcpy(&capacity, base + head + 8, 4);
                            if ((capacity & XFixedString::inline_flag) != 0) return false;
                            DeltaMove mv;
                            mv.head = static_cast<std::uint32_t>(head);
                            if (!head_block(base, used, head, delta_node_string, 1, mv.new_begin, mv.new_bytes) ||
                                mv.new_bytes == 0)
                                return false;
                            c.moves_.push_back(mv);
                            return true;
                        },
                        [&](std::size_t offset) { return static_cast<unsigned char>(base[offset]) <= 1u; });
                    if (!held) return false;
                    if (c.moves_.empty() && !structural) return true;
                    // Room for the map after the commit, which cannot fail.
                    std::size_t nodes = c.nodes_.size();
                    for (const DeltaMove& mv : c.moves_) nodes += mv.node == DeltaMove::new_node;
                    reserve_room(c.nodes_, nodes);
                    reserve_room(c.touched_, nodes);
                    reserve_room(c.ranges_, 2 * nodes);
                    reserve_room(c.range_scratch_, 2 * nodes);
                    reserve_room(c.range_index_, c.checks_.size() + 1);
                } XOFFSET_CATCH(const std::bad_alloc&) {
                    return false;
                }
                return cover(c, core, base, parsed, holes, used, old_used);
            }
            // The node ranges, from the nodes (after nodes came, went or moved);
            // nodes that are no longer containers are dropped.
            static void rebuild_ranges(XDeltaCursor& c) noexcept {
                c.nodes_.erase(std::remove_if(c.nodes_.begin(), c.nodes_.end(),
                                              [](const DeltaCheckNode& n) { return n.kind == delta_node_dead; }),
                               c.nodes_.end());
                c.ranges_.clear();
                for (std::size_t i = 0; i < c.nodes_.size(); ++i) {
                    const DeltaCheckNode& n = c.nodes_[i];
                    if (n.kind == delta_node_dead) continue;
                    const auto index = static_cast<std::uint32_t>(i);
                    c.ranges_.push_back({n.head, n.head + static_cast<std::uint32_t>(sizeof(XFixedString)), index});
                    if (n.capacity == 0 || n.kind == delta_node_vector || n.kind == delta_node_records) continue;
                    const auto data = static_cast<std::uint32_t>(std::int64_t(n.head) + n.data_delta);
                    const std::size_t bytes =
                        n.kind == delta_node_string ? std::size_t(n.capacity) + 1u : std::size_t(n.capacity) * n.element;
                    c.ranges_.push_back({data, data + static_cast<std::uint32_t>(bytes), index});
                }
                index_ranges(c);  // within the room reserved by holds_incremental
            }
            // After the commit: the containers and heads it checked, marked for
            // their new sizes and blocks. (Iterating while marking is safe: a
            // head is re-marked from its own bytes, and marking it twice gives
            // the same bits.)
            static void remark_in_place(XDeltaCursor& c, const char* base, const Parsed& parsed,
                                        std::size_t used) noexcept {
                if (c.checks_.size() > (used + delta_block - 1) / delta_block) {  // used_end came down
                    c.checks_.resize((used + delta_block - 1) / delta_block);
                    c.live_.resize(live_words(used));
                }
                DeltaCheckMarker m(base, c.checks_.data(), &c.nodes_, &c.ranges_);
                bool ranges_changed = false;
                // (The checked bytes of the freed blocks were cleared by
                // cover, before it marked new elements.)
                for (const DeltaMove& mv : c.moves_) {
                    if (mv.node == DeltaMove::new_node) continue;
                    DeltaCheckNode& n = c.nodes_[mv.node];
                    if (mv.dropped) {  // dead: dropped by rebuild_ranges
                        ranges_changed = true;
                        continue;
                    }
                    if (mv.grow) {  // its new elements were marked by cover
                        n.size = static_cast<std::uint32_t>(mv.new_begin);
                        ranges_changed = true;
                        continue;
                    }
                    const char* head = base + n.head;
                    std::int32_t data_delta;
                    std::uint32_t size, capacity;
                    std::memcpy(&data_delta, head, 4);
                    std::memcpy(&size, head + 4, 4);
                    std::memcpy(&capacity, head + 8, 4);
                    if (mv.rebuild) {  // its elements were marked by cover
                        n.capacity = capacity;
                        n.data_delta = data_delta;
                        n.size = size;
                        ranges_changed = true;
                        continue;
                    }
                    if (n.kind != delta_node_vector) ranges_changed = true;
                    if (n.kind == delta_node_string && (capacity == 0 || (capacity & XFixedString::inline_flag) != 0)) {
                        // Inline or empty now: checked in place, no longer a node.
                        n.kind = delta_node_dead;
                        m.clear_at(n.head, sizeof(XFixedString));
                        m.inline_head_at(n.head);
                        continue;
                    }
                    n.capacity = capacity;
                    n.data_delta = data_delta;
                    n.size = size;
                    if (capacity == 0 || n.kind == delta_node_vector) continue;
                    const std::size_t data = static_cast<std::size_t>(std::int64_t(n.head) + data_delta);
                    if (n.kind == delta_node_string) {
                        m.mark_at(data + size, 1, false);
                    } else {
                        for (std::uint32_t i = 0; i < size; ++i)
                            m.mark_at(data + std::size_t(i) * n.element, n.key_size, false);
                    }
                }
                for (const std::uint32_t index : c.touched_) {
                    DeltaCheckNode& n = c.nodes_[index];
                    if (n.moved != c.stamp_) remark_node(m, n);
                }
                (void)for_each_checked_write(
                    parsed, c.checks_.data(), [](std::size_t) { return true; },
                    [&](std::size_t head) {
                        if (inline_head_ok(base + head)) {
                            m.clear_at(head, sizeof(XFixedString));
                            m.inline_head_at(head);
                        } else {  // a heap string now: a node (within the room reserved)
                            m.clear_head_at(head);
                            delta_mark_heap_string(*reinterpret_cast<const XFixedString*>(base + head), m);
                            ranges_changed = true;
                        }
                        return true;
                    },
                    [](std::size_t) { return true; });
                if (ranges_changed) rebuild_ranges(c);
            }
            // The scratch the whole verification needs: the replica's holes,
            // unless the delta carries them, and room for the new map.
            static vstatus prepare_full(XBufferCore& core, const Parsed& parsed, XDeltaCursor& cursor,
                                        std::vector<XBufferCore::WireHole>& holes, const char* who) {
                XOFFSET_VTRY(size_checks(cursor, parsed.header.used_bytes));
                if (parsed.holes) return vok();
                XOFFSET_TRY {
                    holes.clear();
                    holes.reserve(core.free_block_count());
                    core.for_each_free_block([&](XBufferCore::WireHole h) { holes.push_back(h); });
                } XOFFSET_CATCH(const std::bad_alloc&) {
                    XOFFSET_VFAIL_BAD_ALLOC();
                }
                if (!core.free_table_complete())
                    XOFFSET_VFAIL(XException, XErrc::delta_base_mismatch,
                                  msg(who, "replica free table incomplete (ask for a key delta)"));
                return vok();
            }

            // Admit: the key delta's payload is taken like XReceiver::reload<T,
            // admit> takes a wire (finish_admitted_load: the root object inside
            // the used payload; holes are inside it by the parse), and no map
            // of checked bytes is built.
            template <typename T, bool Admit = false>
            static vstatus apply_key(XBuffer& xbuf, const Parsed& parsed, XDeltaCursor& cursor,
                                     DeltaReceiveScratch& scratch, const char* who) {
                XBufferCore& core = xbuf;
                const std::size_t used = parsed.header.used_bytes;
                cursor.clear();
                if constexpr (!Admit) XOFFSET_VTRY(size_checks(cursor, used));
                XOFFSET_ROLLBACK_BEGIN
                    std::size_t clean_from = 0;
                    XOFFSET_VTRY(core.reinit_for_overwrite(used, clean_from));
                    char* base = core.data();
                    // The bytes a key delta leaves out are zero. Bytes from
                    // clean_from on are zero already; below, each byte is
                    // written once (a block's words with their zeros, the
                    // gaps between blocks zeroed).
                    const std::size_t stale_end = std::min(clean_from, used);
                    std::size_t written = payload_begin;
                    const char* const src_end = reinterpret_cast<const char*>(parsed.readable_end);
                    for_each_block(parsed, [&](std::size_t offset, const char* bytes, std::uint64_t mask) {
                        if (written < std::min(offset, stale_end))
                            std::memset(base + written, 0, std::min(offset, stale_end) - written);
                        const std::size_t words = std::min<std::size_t>(delta_block, used - offset) / 8u;
                        // Block 0 starts in the arena header: its words from payload_begin.
                        const std::size_t skip = offset < payload_begin ? payload_begin - offset : 0u;
                        delta_set_masked(base + offset + skip, bytes, mask >> skip, words - skip / 8u, src_end);
                        written = offset + 8 * words;
                    });
                    if (written < stale_end) std::memset(base + written, 0, stale_end - written);
                    XBufferCore::write_payload_header(base, parsed.header.root_offset, used);
                    XOFFSET_VTRY(check_digest<T>(core, parsed, who));
                    XWireHeaderV2 header{};
                    header.used_bytes = parsed.header.used_bytes;
                    header.root_offset = parsed.header.root_offset;
                    header.hole_count = static_cast<std::uint32_t>(scratch.holes.size());
                    if constexpr (Admit) {
                        XOFFSET_VTRY(xbuf.template finish_admitted_load<T>(
                            header, reinterpret_cast<const char*>(scratch.holes.data()), scratch.holes.size(), who));
                    } else {
                        XOFFSET_VTRY(xbuf.template finish_verified_load<T>(
                            header, reinterpret_cast<const char*>(scratch.holes.data()), scratch.holes.size(),
                            default_wire_upstream(), who, &xbuf.retained_reload_scratch()));
                    }
                XOFFSET_ROLLBACK_END(core.reset_arena(0))
                cursor.stream_ = parsed.header.stream;
                cursor.version_ = parsed.header.version;
                cursor.schema_hash_ = wire_schema_hash<T>();
                cursor.root_type_id_ = wire_root_type_id<T>();
                if constexpr (Admit) {
                    cursor.last_check_ = XDeltaCheck::admit;  // no map: checks_valid_ stays false
                } else {
                    // Deltas follow a key delta: its map is built now.
                    defer_checks(cursor, core);
                    (void)ensure_checks<T>(cursor, core, parsed.header.root_offset);
                    cursor.last_check_ = XDeltaCheck::key;
                }
                return vok();
            }

            // Restores what apply_incremental changed (bytes, used_end, root).
            static void roll_back(XBufferCore& core, const Parsed& parsed, const char* undo, std::size_t old_used,
                                  std::uint64_t old_root) noexcept {
                char* base = core.data();
                const std::size_t used = parsed.header.used_bytes;
                for_each_block(parsed, [&](std::size_t offset, const char*, std::uint64_t) {
                    const std::size_t lo = std::max(offset, payload_begin);  // block 0: past the arena header
                    if (lo < old_used) {
                        const std::size_t kept = std::min(offset + delta_block, old_used) - lo;
                        std::memcpy(base + lo, undo, kept);
                        undo += kept;
                    }
                    // Bytes at or past old_used were zero (N2); the block wrote
                    // only below used.
                    const std::size_t from = std::max(offset, old_used);
                    const std::size_t to = std::min(offset + delta_block, used);
                    if (to > from) std::memset(base + from, 0, to - from);
                });
                if (used < old_used) std::memcpy(base + used, undo, old_used - used);
                auto* sm = core.arena();
                sm->used_end = old_used;
                sm->root_offset = old_root;
            }

            template <typename T>
            static vstatus apply_incremental(XBuffer& xbuf, const Parsed& parsed, XDeltaCursor& cursor,
                                             DeltaReceiveScratch& scratch, const char* who) {
                XBufferCore& core = xbuf;
                const std::size_t used = parsed.header.used_bytes;
                const auto root = static_cast<std::uint64_t>(parsed.header.root_offset);
                const std::size_t old_used = static_cast<std::size_t>(core.arena()->used_end);
                const std::uint64_t old_root = core.arena()->root_offset;
                const bool touch_log_current = xbuf.touch_log_ && xbuf.touch_log_->epoch == core.epoch();

                // What the result needs: no check when the delta writes only
                // bytes that verification does not read and keeps used_end, the
                // root and the holes; the in-place checks when it writes bools,
                // inline string heads, vector sizes or containers checked on
                // their own, or changes used_end or the holes (the coverage is
                // then checked against the map of live blocks); else the whole
                // verification.
                const bool structural = parsed.holes || used != old_used;
                XDeltaCheck check = XDeltaCheck::full;
                if (root == old_root && checks_match(cursor, core) && ensure_checks<T>(cursor, core, old_root) &&
                    (!structural || grow_checks(cursor, used))) {
                    check = structural ? XDeltaCheck::in_place : XDeltaCheck::none;
                    const DeltaCheckBits* bits = cursor.checks_.data();
                    for (std::size_t k = 0; check == XDeltaCheck::none && k < parsed.block_count; ++k) {
                        const DeltaBlockRef& r = parsed.blocks[k];
                        const DeltaCheckBits& b = bits[r.offset / delta_block];
                        if ((r.mask & b.checked) != 0) check = XDeltaCheck::in_place;
                    }
                    if (check == XDeltaCheck::in_place && !collect_nodes(cursor, parsed)) check = XDeltaCheck::full;
                }

                // Everything that can fail without verification fails first,
                // before a byte changes. Nothing can fail after an unchecked
                // delta without a digest, so it keeps no undo copy.
                auto& undo = scratch.undo;
                const bool keep_undo = check != XDeltaCheck::none || parsed.digest_present;
                if (keep_undo) {
                    const std::size_t undo_bytes = parsed.undo_bytes + (used < old_used ? old_used - used : 0u);
                    XOFFSET_TRY {
                        if (undo.size() < undo_bytes) undo.resize(undo_bytes);
                    } XOFFSET_CATCH(const std::bad_alloc&) {
                        XOFFSET_VFAIL_BAD_ALLOC();
                    }
                }
                if (check == XDeltaCheck::full) XOFFSET_VTRY(prepare_full(core, parsed, cursor, scratch.holes, who));
                if (used > core.segment_size() && !core.grow(used - core.segment_size()))
                    XOFFSET_VFAIL(XBadAlloc, XErrc::capacity_exceeded, arena_failure_message);

                // Save, then patch. Bytes at or past old_used are zero (N2).
                char* base = core.data();
                const char* const src_end = reinterpret_cast<const char*>(parsed.readable_end);
                if (keep_undo) {
                    char* u = undo.data();
                    for_each_block(parsed, [&](std::size_t offset, const char* bytes, std::uint64_t mask) {
                        const std::size_t lo = std::max(offset, payload_begin);  // block 0: past the arena header
                        if (lo < old_used) {
                            const std::size_t kept = std::min(offset + delta_block, old_used) - lo;
                            if (kept == delta_block) std::memcpy(u, base + offset, delta_block);
                            else std::memcpy(u, base + lo, kept);
                            u += kept;
                        }
                        delta_get_masked(base + offset, bytes, mask, src_end);
                    });
                    if (used < old_used) {
                        std::memcpy(u, base + used, old_used - used);
                        std::memset(base + used, 0, old_used - used);
                    }
                } else {
                    for_each_block(parsed, [&](std::size_t offset, const char* bytes, std::uint64_t mask) {
                        delta_get_masked(base + offset, bytes, mask, src_end);
                    });
                }
                auto* sm = core.arena();
                sm->used_end = used;
                sm->root_offset = root;

                XOFFSET_ROLLBACK_BEGIN
                    XOFFSET_VTRY(check_digest<T>(core, parsed, who));
                    if (check == XDeltaCheck::in_place &&
                        !holds_incremental(cursor, core, base, parsed, scratch.holes, used, old_used, structural)) {
                        check = XDeltaCheck::full;
                        XOFFSET_VTRY(prepare_full(core, parsed, cursor, scratch.holes, who));
                    }
                    if (check == XDeltaCheck::full) {
                        auto* root_ptr = find_root_at<T>(core, root);
                        if (!root_ptr) XOFFSET_VFAIL(XException, XErrc::root_mismatch, msg(who, "root not found"));
                        const auto actual = static_cast<std::uint64_t>(
                            reinterpret_cast<std::uintptr_t>(root_ptr) -
                            reinterpret_cast<std::uintptr_t>(core.get_address()));
                        if (actual != root)
                            XOFFSET_VFAIL(XException, XErrc::root_mismatch, msg(who, "root offset mismatch"));
                        XOFFSET_VTRY(verify_structural_wire_graph(*root_ptr, core, default_wire_upstream(),
                                                                  scratch.holes.data(), scratch.holes.size(), true,
                                                                  nullptr, &xbuf.retained_reload_scratch()));
                        if (parsed.holes)
                            XOFFSET_VTRY(core.load_free_blocks(scratch.holes.data(), scratch.holes.size()));
                    } else if (parsed.holes) {
                        XOFFSET_VTRY(core.load_free_blocks(scratch.holes.data(), scratch.holes.size()));
                    }
                // The map of live blocks may have changed: rebuilt by the next
                // delta's whole verification.
                XOFFSET_ROLLBACK_END(roll_back(core, parsed, undo.data(), old_used, old_root);
                                     cursor.checks_valid_ = false)

                // Committed. A new epoch: element pointers and XHandles of the
                // previous content re-resolve.
                core.advance_epoch();
                // A buffer that also sends deltas (touched_only) gets the
                // blocks written as touches.
                if (touch_log_current) {
                    DeltaTouchLog& log = *xbuf.touch_log_;
                    for_each_block(parsed, [&](std::size_t offset, const char*, std::uint64_t mask) {
                        touch_block(log, offset / delta_block, mask);
                    });
                    log.epoch = core.epoch();
                }
                T* root_ptr = find_root_at<T>(core, root);
                core.cache_root(root_ptr);
                cursor.version_ = parsed.header.version;
                if (check == XDeltaCheck::full) {
                    defer_checks(cursor, core);
                } else {
                    if (check == XDeltaCheck::in_place) remark_in_place(cursor, base, parsed, used);
                    note_checks(cursor, core);
                }
                cursor.last_check_ = check;
                return vok();
            }

            // Trust tier admit of a non-key delta (XReceiver<L>::apply_delta<T,
            // admit>, trust tier design P2). What the parse checks stays (format,
            // stream and version, root type and fingerprint, every block and hole
            // inside [48, used), the root below used) and the root object must lie
            // inside the used payload, aligned; the payload itself is the sender's
            // promise. No map of checked bytes is kept: the cursor's map no longer
            // matches, so a later verified delta on this cursor verifies the whole
            // payload (and rebuilds the map), and the map is never rebuilt over a
            // payload that was only admitted. An undo copy is kept only for a
            // digest or a hole table, the two steps that can fail after the patch.
            template <typename T>
            static vstatus apply_incremental_admit(XBuffer& xbuf, const Parsed& parsed, XDeltaCursor& cursor,
                                                   DeltaReceiveScratch& scratch, const char* who) {
                XBufferCore& core = xbuf;
                const std::size_t used = parsed.header.used_bytes;
                const auto root = static_cast<std::uint64_t>(parsed.header.root_offset);
                const std::size_t old_used = static_cast<std::size_t>(core.arena()->used_end);
                const std::uint64_t old_root = core.arena()->root_offset;
                const bool touch_log_current = xbuf.touch_log_ && xbuf.touch_log_->epoch == core.epoch();
                using Storage = std::conditional_t<has_wire_allocator_ctor<T>, T, ReflectRoot<T>>;
                if (sizeof(Storage) > used - root || root % alignof(Storage) != 0)
                    XOFFSET_VFAIL(XException, XErrc::root_mismatch, msg(who, "root not found"));
                cursor.checks_valid_ = false;

                auto& undo = scratch.undo;
                const bool keep_undo = parsed.digest_present || parsed.holes;
                if (keep_undo) {
                    const std::size_t undo_bytes = parsed.undo_bytes + (used < old_used ? old_used - used : 0u);
                    XOFFSET_TRY {
                        if (undo.size() < undo_bytes) undo.resize(undo_bytes);
                    } XOFFSET_CATCH(const std::bad_alloc&) {
                        XOFFSET_VFAIL_BAD_ALLOC();
                    }
                }
                if (used > core.segment_size() && !core.grow(used - core.segment_size()))
                    XOFFSET_VFAIL(XBadAlloc, XErrc::capacity_exceeded, arena_failure_message);

                // Bytes at or past old_used are zero (N2).
                char* base = core.data();
                const char* const src_end = reinterpret_cast<const char*>(parsed.readable_end);
                if (keep_undo) {
                    char* u = undo.data();
                    for_each_block(parsed, [&](std::size_t offset, const char* bytes, std::uint64_t mask) {
                        const std::size_t lo = std::max(offset, payload_begin);  // block 0: past the arena header
                        if (lo < old_used) {
                            const std::size_t kept = std::min(offset + delta_block, old_used) - lo;
                            if (kept == delta_block) std::memcpy(u, base + offset, delta_block);
                            else std::memcpy(u, base + lo, kept);
                            u += kept;
                        }
                        delta_get_masked(base + offset, bytes, mask, src_end);
                    });
                    if (used < old_used) {
                        std::memcpy(u, base + used, old_used - used);
                        std::memset(base + used, 0, old_used - used);
                    }
                } else {
                    for_each_block(parsed, [&](std::size_t offset, const char* bytes, std::uint64_t mask) {
                        delta_get_masked(base + offset, bytes, mask, src_end);
                    });
                    if (used < old_used) std::memset(base + used, 0, old_used - used);
                }
                auto* sm = core.arena();
                sm->used_end = used;
                sm->root_offset = root;
                if (keep_undo) {
                    XOFFSET_ROLLBACK_BEGIN
                        XOFFSET_VTRY(check_digest<T>(core, parsed, who));
                        if (parsed.holes)
                            XOFFSET_VTRY(core.load_free_blocks(scratch.holes.data(), scratch.holes.size()));
                    XOFFSET_ROLLBACK_END(roll_back(core, parsed, undo.data(), old_used, old_root))
                }

                // Committed: a new epoch, as for a verified delta.
                core.advance_epoch();
                if (touch_log_current) {
                    DeltaTouchLog& log = *xbuf.touch_log_;
                    for_each_block(parsed, [&](std::size_t offset, const char*, std::uint64_t mask) {
                        touch_block(log, offset / delta_block, mask);
                    });
                    log.epoch = core.epoch();
                }
                core.cache_root(find_root_at<T>(core, root));
                cursor.version_ = parsed.header.version;
                cursor.last_check_ = XDeltaCheck::admit;
                return vok();
            }

            // Admit selects the trust tier admit (apply_incremental_admit,
            // apply_key<T, true>); false is apply_delta_verified<T>.
            template <typename T, bool Admit = false>
            static vstatus apply(XBuffer& xbuf, std::span<const std::byte> data, XDeltaCursor& cursor) {
                static_assert(is_v1_wire_admitted_v<T>,
                              "apply_delta_verified<T>() requires an admitted fixed-schema wire type.");
                constexpr const char* who = Admit ? "XReceiver::apply_delta<T, admit>()" : "apply_delta_verified<T>()";
                XBufferCore& core = xbuf;
                // The cursor's fields are read throughout: fetched while the
                // buffer's state is looked up.
                XOFFSET_DELTA_PREFETCH(&cursor);
                XOFFSET_DELTA_PREFETCH(reinterpret_cast<const char*>(&cursor) + 64);
                XOFFSET_DELTA_PREFETCH(reinterpret_cast<const char*>(&cursor) + 128);
                // A non-key delta patches a verified root of T that this
                // cursor's key delta produced.
                const bool has_state = core.typed_root_access_allowed_ && core.template root_type_matches<T>() &&
                                       core.arena() != nullptr;
                const std::size_t old_used = has_state ? static_cast<std::size_t>(core.arena()->used_end) : 0u;
                const std::uint64_t old_root = has_state ? core.arena()->root_offset : 0u;
                // A small delta is read from a copy with room past its end, so
                // that every block's bytes are scattered a word at a time.
                DeltaReceiveScratch& scratch = receive_scratch(cursor);
                std::size_t room = 0;
                if (data.size() <= delta_padded_max) {
                    XOFFSET_TRY {
                        auto& input = scratch.input;
                        if (input.size() < data.size() + delta_padding) input.resize(delta_padded_max + delta_padding);
                        if (!data.empty()) std::memcpy(input.data(), data.data(), data.size());
                        data = std::span<const std::byte>(reinterpret_cast<const std::byte*>(input.data()),
                                                          data.size());
                        room = delta_padding;
                    } XOFFSET_CATCH(const std::bad_alloc&) {
                        room = 0;  // read in place
                    }
                }
                Parsed parsed{};
                XOFFSET_VTRY(parse<T>(data, room, cursor, old_used, old_root, scratch.holes, scratch.blocks, parsed, who,
                                      has_state ? static_cast<const char*>(core.get_address()) : nullptr));
                if (parsed.key) return apply_key<T, Admit>(xbuf, parsed, cursor, scratch, who);
                if (!has_state)
                    XOFFSET_VFAIL(XException, XErrc::delta_base_mismatch,
                                  msg(who, "buffer holds no verified root of this type (ask for a key delta)"));
                if constexpr (Admit) return apply_incremental_admit<T>(xbuf, parsed, cursor, scratch, who);
                else return apply_incremental<T>(xbuf, parsed, cursor, scratch, who);
            }
        };
    } // namespace detail

    template <typename T>
    void XBuffer::touch(const T& value) noexcept {
        static_assert(!std::is_pointer_v<T>, "touch(value) marks the object itself; use touch(p, bytes) for raw bytes");
        detail::DeltaAccess::touch_value(*this, value);
    }
    inline void XBuffer::touch(const void* p, std::size_t bytes) noexcept { detail::DeltaAccess::touch(*this, p, bytes); }
    template <typename T, typename V>
    bool XBuffer::touch_set(T& field, const V& value) {
        if (field == value) return false;
        if constexpr (std::is_trivially_copyable_v<T> && !std::is_class_v<T> && !std::is_array_v<T> &&
                      sizeof(T) <= 8) {
            // A scalar: only the bytes that changed are touched (what a delta
            // from an XDeltaRecord::touches baseline then sends).
            std::uint64_t before = 0, after = 0;
            std::memcpy(&before, &field, sizeof(T));
            field = value;
            std::memcpy(&after, &field, sizeof(T));
            detail::DeltaAccess::touch_changed(*this, &field, sizeof(T), detail::delta_byte_flags(before ^ after));
        } else {
            field = value;
            touch(field);
        }
        return true;
    }

    inline void XDeltaBaseline::key_delta_into(std::string& out, XDeltaOptions options) const {
        detail::DeltaAccess::key_delta_into(*this, out, options);
    }

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
