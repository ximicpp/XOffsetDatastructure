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
#include <atomic>
#include <bit>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <memory>
#include <span>
#include <string>
#include <vector>
#if defined(__SSE2__) || defined(_M_X64) || defined(_M_AMD64)
#include <emmintrin.h>
#endif

#define XOFFSET_DELTA_SYNC_API 1

namespace XOffsetDatastructure {

    // Delta format 1.0 (docs/DELTA_SYNC.md). Little-endian. The header:
    // 'X' 'D', the format byte (major << 4 | minor), the flags byte, the
    // stream (u64), then varints: the version, with EXTENT (used_end - 48) / 8
    // and root_offset - 48, and the block count.
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
        in_place,  ///< besides those, bools and inline string heads, checked in place
        full,      ///< the whole payload was verified (structure, used_end, root or holes changed)
    };

    namespace detail {
        inline constexpr std::size_t delta_block = 64;  // block size of the block section

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

        // A block inside the payload goes whole (no mask) when the mask form
        // would not be shorter: mask bytes plus changed bytes of at least 64.
        inline bool delta_block_whole(std::uint64_t mask, std::size_t start, std::size_t used) noexcept {
            return start + delta_block <= used &&
                   std::popcount(delta_byte_flags(mask)) + std::popcount(mask) >= int(delta_block);
        }
        // Encoded size of one block after its skip varint: the selector byte,
        // the nonzero mask bytes, the changed bytes (or 64 bytes whole).
        inline std::size_t delta_block_size(std::uint64_t mask, bool whole) noexcept {
            return 1u + (whole ? delta_block
                               : std::size_t(std::popcount(delta_byte_flags(mask)) + std::popcount(mask)));
        }

        // The 64-byte blocks of [from, n) (the last may be shorter) where cur
        // differs from base (Update) or from zero (!Update, base unused), with
        // their masks. With Update, each block that differs is copied into
        // base on the way, so base equals cur on [from, n) afterwards.
        template <bool Update>
        DeltaBlockTotals delta_scan(const char* cur, char* base, std::size_t from, std::size_t n,
                                    DeltaBlockBuffer& blocks) {
            alignas(64) static constexpr char zeros[64] = {};
            DeltaBlockTotals totals;
            DeltaBlock* w = blocks.reset_for((n - from + delta_block - 1) / delta_block);
            std::size_t next = 0;  // index of the block after the last one recorded
            const auto record = [&](std::size_t start, std::uint64_t mask) {
                const std::size_t index = (start - from) / delta_block;
                const bool whole = delta_block_whole(mask, start, n);
                totals.encoded += delta_varint_size(static_cast<std::uint32_t>(index - next)) +
                                  delta_block_size(mask, whole);
                totals.changed += whole ? delta_block : std::size_t(std::popcount(mask));
                *w++ = {static_cast<std::uint32_t>(start), whole ? 1u : 0u, mask};
                next = index + 1;
            };
            std::size_t i = from;
            for (; n - i >= delta_block; i += delta_block) {
                const std::uint64_t m = delta_block_mask(cur + i, Update ? base + i : zeros);
                if (m == 0) continue;
                if constexpr (Update) std::memcpy(base + i, cur + i, delta_block);
                record(i, m);
            }
            if (i < n) {  // the last, shorter block
                std::uint64_t m = 0;
                std::size_t k = 0;
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
                if (m != 0) record(i, m);
            }
            blocks.commit(w);
            return totals;
        }

        inline DeltaBlockTotals delta_diff_update(const char* cur, char* base, std::size_t from, std::size_t n,
                                                  DeltaBlockBuffer& blocks) {
            return delta_scan<true>(cur, base, from, n, blocks);
        }

        // The blocks of [from, n) where p is nonzero (those of a key delta of p).
        inline DeltaBlockTotals delta_nonzero_blocks(const char* p, std::size_t from, std::size_t n,
                                                     DeltaBlockBuffer& blocks) {
            return delta_scan<false>(p, nullptr, from, n, blocks);
        }

        // Writes block bytes of `mask` from blk (word by word: a whole word is
        // one copy, other words byte by byte); returns the end.
        inline char* delta_put_masked(char* p, const char* blk, std::uint64_t mask) noexcept {
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
        // The inverse: writes the bytes of `mask` into blk from src; returns
        // the end of src.
        inline const char* delta_get_masked(char* blk, const char* src, std::uint64_t mask) noexcept {
            if (mask == ~std::uint64_t(0)) {
                std::memcpy(blk, src, delta_block);
                return src + delta_block;
            }
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
        // Per 64-byte block of the payload (block i: offsets 48 + 64 i to
        // 48 + 64 i + 63), bit k stands for byte 48 + 64 i + k. `checked`: the
        // verification of load_verified reads the byte: container heads,
        // string terminators, set and map keys with everything they reach,
        // bools, the length and padding of inline strings. Every other byte of
        // a verified payload may take any value without changing what
        // verification visits or decides. `local`: the checked bytes that are
        // checked again in place (bools, inline string heads). `heads`: the
        // first byte of each inline string head.
        struct DeltaCheckBits {
            std::uint64_t checked = 0;
            std::uint64_t local = 0;
            std::uint64_t heads = 0;
        };
        static_assert(sizeof(XFixedString) == 12, "inline string head layout");

        class DeltaCheckMarker {
        public:
            DeltaCheckMarker(const char* base, DeltaCheckBits* bits) noexcept : base_(base), bits_(bits) {}
            std::size_t offset_of(const void* p) const noexcept {
                return static_cast<std::size_t>(static_cast<const char*>(p) - base_);
            }
            void mark(const void* p, std::size_t n, bool local) noexcept { mark_at(offset_of(p), n, local); }
            // Everything marked lies in the verified payload, past the arena header.
            void mark_at(std::size_t offset, std::size_t n, bool local) noexcept {
                std::size_t i = offset - XWIRE_PAYLOAD_BEGIN;
                const std::size_t e = i + n;
                while (i < e) {
                    const std::size_t bit = i % delta_block;
                    const std::size_t take = std::min(delta_block - bit, e - i);
                    const std::uint64_t m =
                        (take == delta_block ? ~std::uint64_t(0) : (std::uint64_t(1) << take) - 1u) << bit;
                    DeltaCheckBits& b = bits_[i / delta_block];
                    b.checked |= m;
                    if (local) b.local |= m;
                    i += take;
                }
            }
            void clear_at(std::size_t offset, std::size_t n) noexcept {
                for (std::size_t i = offset - XWIRE_PAYLOAD_BEGIN, e = i + n; i < e; ++i) {
                    const std::uint64_t m = ~(std::uint64_t(1) << (i % delta_block));
                    bits_[i / delta_block].checked &= m;
                    bits_[i / delta_block].local &= m;
                }
            }
            void head_at(std::size_t offset) noexcept {
                const std::size_t i = offset - XWIRE_PAYLOAD_BEGIN;
                bits_[i / delta_block].heads |= std::uint64_t(1) << (i % delta_block);
            }
            // An inline string head: its length and padding are checked in
            // place, its characters are free.
            void inline_head_at(std::size_t offset) noexcept {
                const unsigned length = static_cast<unsigned char>(base_[offset + 11]) & 0x7Fu;
                head_at(offset);
                mark_at(offset + length, sizeof(XFixedString) - length, true);
            }

        private:
            const char* base_;
            DeltaCheckBits* bits_;
        };

        template <typename T>
        const T* delta_heap_data(const std::int32_t& delta) noexcept {
            return reinterpret_cast<const T*>(reinterpret_cast<const char*>(&delta) + delta);
        }

        template <typename T>
        void delta_mark_pinned(const T& value, DeltaCheckMarker& m);

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
                const auto& s = static_cast<const XFixedString&>(value);
                if (s.is_inline()) {
                    m.inline_head_at(m.offset_of(&s));
                } else {
                    m.mark(&s, sizeof(XFixedString), false);
                    if (s.capacity_ != 0) m.mark(delta_heap_data<char>(s.data_delta) + s.size_, 1, false);
                }
            } else if constexpr (is_fixed_flat_set_v<CleanT> || is_fixed_flat_map_v<CleanT> ||
                                 is_fixed_vector_v<CleanT>) {
                using Elem = typename CleanT::value_type;
                const auto& v = static_cast<const XFixedVector<Elem>&>(value);
                m.mark(&v, sizeof(v), false);
                if (v.capacity_ == 0) return;
                const Elem* data = delta_heap_data<Elem>(v.data_delta);
                for (std::uint32_t i = 0; i < v.size_; ++i) {
                    if constexpr (wire_value_needs_validation_v<Elem>) delta_mark_value(data[i], m);
                    // Keys are compared with each other.
                    if constexpr (is_fixed_flat_set_v<CleanT>) delta_mark_pinned(data[i], m);
                    else if constexpr (is_fixed_flat_map_v<CleanT>) delta_mark_pinned(data[i].first, m);
                }
            } else if constexpr (std::is_class_v<CleanT>) {
                (void)for_each_reflected_subobject_v<CleanT>(
                    [&](auto index) {
                        delta_mark_value(static_cast<const schema::base_t<CleanT, decltype(index)::value>&>(value), m);
                        return vok();
                    },
                    [&](auto index) {
                        using member = schema::member_t<CleanT, decltype(index)::value>;
                        delta_mark_value(member::get(value), m);
                        return vok();
                    });
            }
        }

        // Marks a set or map key and everything it reaches (what comparing
        // keys may read), all checked as a whole.
        template <typename T>
        void delta_mark_pinned(const T& value, DeltaCheckMarker& m) {
            using CleanT = std::remove_cv_t<T>;
            m.mark(&value, sizeof(CleanT), false);
            if constexpr (std::is_array_v<CleanT>) {
                if constexpr (wire_value_needs_validation_v<std::remove_extent_t<CleanT>>)
                    for (const auto& elem : value) delta_mark_pinned(elem, m);
            } else if constexpr (is_fixed_string_v<CleanT>) {
                const auto& s = static_cast<const XFixedString&>(value);
                if (!s.is_inline() && s.capacity_ != 0)
                    m.mark(delta_heap_data<char>(s.data_delta), std::size_t(s.size_) + 1u, false);
            } else if constexpr (is_fixed_flat_set_v<CleanT> || is_fixed_flat_map_v<CleanT> ||
                                 is_fixed_vector_v<CleanT>) {
                using Elem = typename CleanT::value_type;
                const auto& v = static_cast<const XFixedVector<Elem>&>(value);
                if (v.capacity_ == 0) return;
                const Elem* data = delta_heap_data<Elem>(v.data_delta);
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
        /// A key delta of the recorded state, at this stream and version, for a
        /// receiver that joins late (one baseline shared by several
        /// receivers): applied to any buffer, it makes the receiver hold what
        /// the others hold, so it accepts this baseline's next delta. The
        /// baseline is not advanced. Requires !empty().
        void key_delta_into(std::string& out, XDeltaOptions options = {}) const;
        std::string key_delta(XDeltaOptions options = {}) const {
            std::string out;
            key_delta_into(out, options);
            return out;
        }
        /// reset() and return the copy's memory.
        void release() noexcept {
            reset();
            std::vector<char>().swap(payload_);
            std::vector<XBufferCore::WireHole>().swap(holes_);
            std::vector<XBufferCore::WireHole>().swap(scratch_holes_);
            blocks_.release();
        }

    private:
        friend struct detail::DeltaAccess;
        std::uint64_t stream_ = 0;
        std::uint32_t version_ = 0;
        std::uint32_t root_offset_ = 0;
        std::uint64_t schema_hash_ = 0;                   // fingerprint of the recorded root type
        std::uint64_t root_type_id_ = 0;
        std::vector<char> payload_;                       // [0, used_end) as the receiver holds it ([0, 48) unused)
        std::vector<XBufferCore::WireHole> holes_;        // its hole table, by offset
        std::vector<XBufferCore::WireHole> scratch_holes_;
        detail::DeltaBlockBuffer blocks_;
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
        /// Frees the scratch and the map of checked bytes (the next delta is
        /// then verified in full, which rebuilds the map).
        void release_scratch() noexcept {
            std::vector<XBufferCore::WireHole>().swap(holes_);
            std::vector<char>().swap(undo_);
            std::vector<detail::DeltaBlockRef>().swap(blocks_);
            std::vector<detail::DeltaCheckBits>().swap(checks_);
            checks_valid_ = false;
        }

    private:
        friend struct detail::DeltaAccess;
        std::uint64_t stream_ = 0;
        std::uint32_t version_ = 0;
        std::uint64_t schema_hash_ = 0;
        std::uint64_t root_type_id_ = 0;
        std::vector<XBufferCore::WireHole> holes_;
        std::vector<char> undo_;
        std::vector<detail::DeltaBlockRef> blocks_;
        // The bytes the last verification read, valid for the buffer state
        // recorded with it (see DeltaAccess::checks_match).
        std::vector<detail::DeltaCheckBits> checks_;
        const void* checks_core_ = nullptr;
        const void* checks_base_ = nullptr;
        std::uint64_t checks_epoch_ = 0;
        std::uint64_t checks_used_ = 0;
        std::uint64_t checks_root_ = 0;
        std::size_t checks_free_ = 0;
        bool checks_free_complete_ = false;
        bool checks_valid_ = false;
        XDeltaCheck last_check_ = XDeltaCheck::key;
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
                // A baseline records one root type; another type starts over.
                constexpr std::uint64_t schema_hash = wire_schema_hash<T>();
                constexpr std::uint64_t root_type_id = wire_root_type_id<T>();
                if (b.schema_hash_ != schema_hash || b.root_type_id_ != root_type_id) {
                    b.reset();
                    b.schema_hash_ = schema_hash;
                    b.root_type_id_ = root_type_id;
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

                // The hole section is checked and sized before the baseline
                // changes.
                const std::size_t hole_section = holes_changed ? hole_section_size(holes, used) : 0u;

                // The receiver's bytes past its used_end read as zero, so the
                // recorded copy grows with zeros (a key delta compares against
                // an all-zero payload).
                auto& base = b.payload_;
                const std::size_t base_used = base.size();
                if (key) base.assign(used, '\0');
                else if (used > base.size()) base.resize(used, '\0');

                // The comparison brings the recorded copy up to date as it goes.
                // If anything fails before the delta is complete, the baseline
                // is dropped, so the next delta is a key delta.
                struct DropOnFailure {
                    XDeltaBaseline* b;
                    ~DropOnFailure() {
                        if (b) b->reset();
                    }
                } drop{&b};
                auto& blocks = b.blocks_;
                const DeltaBlockTotals totals = delta_diff_update(cur, base.data(), payload_begin, used, blocks);

                XDeltaStats stats;
                stats.key = key;
                stats.compared_bytes = used - payload_begin;
                if (!key && blocks.empty() && !holes_changed && used == base_used && root == b.root_offset_) {
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
                if (used < base.size()) base.resize(used); // shrinking never allocates
                b.version_ = header.version;
                b.root_offset_ = root;
                drop.b = nullptr;

                stats.holes = holes_changed;
                stats.blocks = header.block_count;
                stats.changed_bytes = totals.changed;
                stats.delta_bytes = out.size();
                return stats;
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
                if (size > out.max_size())
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
                        const std::size_t index = (blk.offset - payload_begin) / delta_block;
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
                        for (std::uint64_t s = sel; s != 0; s &= s - 1)
                            *p++ = static_cast<char>(blk.mask >> (8 * std::countr_zero(s)));
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
#if defined(__cpp_lib_string_resize_and_overwrite) && __cpp_lib_string_resize_and_overwrite >= 202110L
                out.resize_and_overwrite(size, [&](char* p, std::size_t) noexcept {
                    emit(p);
                    return size;
                });
#else
                out.resize(size);
                emit(out.data());
#endif
            }

            static void key_delta_into(const XDeltaBaseline& b, std::string& out, XDeltaOptions options) {
                if (b.payload_.empty() || b.version_ == 0)
                    XOFFSET_THROW(XException(XErrc::invalid_state, "XDeltaBaseline::key_delta_into(): no recorded state"));
                const std::size_t used = b.payload_.size();
                DeltaBlockBuffer blocks;
                const DeltaBlockTotals totals = delta_nonzero_blocks(b.payload_.data(), payload_begin, used, blocks);
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
            // old_used, old_root: the replica's current used_end and root (0
            // when it holds no verified root of T).
            template <typename T>
            static vstatus parse(std::span<const std::byte> data, const XDeltaCursor& cursor, std::size_t old_used,
                                 std::uint64_t old_root, std::vector<XBufferCore::WireHole>& holes,
                                 std::vector<DeltaBlockRef>& blocks, Parsed& out, const char* who) {
                const auto* p = reinterpret_cast<const unsigned char*>(data.data());
                out.data = p;
                const std::size_t n = data.size_bytes();
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
                for (std::uint32_t k = 0; k < h.block_count; ++k) {
                    std::uint32_t skip = 0;
                    if (!delta_get_varint(q, end, skip) || q == end)
                        XOFFSET_VFAIL(XException, XErrc::size_mismatch, msg(who, "delta block table truncated"));
                    const std::uint64_t index = next + skip;
                    const std::uint64_t start = payload_begin + index * delta_block;
                    if (start >= used)
                        XOFFSET_VFAIL(XException, XErrc::invalid_extent_table, msg(who, "delta block out of bounds"));
                    const unsigned sel = *q++;
                    std::uint64_t mask = ~std::uint64_t(0);
                    std::size_t bytes = delta_block;
                    if (sel == 0) {
                        if (start + delta_block > used)
                            XOFFSET_VFAIL(XException, XErrc::invalid_extent_table, msg(who, "delta block out of bounds"));
                    } else {
                        if (std::size_t(end - q) < std::size_t(std::popcount(sel)))
                            XOFFSET_VFAIL(XException, XErrc::size_mismatch, msg(who, "delta block table truncated"));
                        mask = 0;
                        for (unsigned s = sel; s != 0; s &= s - 1) {
                            const unsigned byte = *q++;
                            if (byte == 0)
                                XOFFSET_VFAIL(XException, XErrc::invalid_extent_table, msg(who, "delta block mask byte is zero"));
                            mask |= std::uint64_t(byte) << (8 * std::countr_zero(s));
                        }
                        if (start + 63u - unsigned(std::countl_zero(mask)) >= used)
                            XOFFSET_VFAIL(XException, XErrc::invalid_extent_table, msg(who, "delta block out of bounds"));
                        bytes = std::size_t(std::popcount(mask));
                    }
                    if (std::size_t(end - q) < bytes)
                        XOFFSET_VFAIL(XException, XErrc::size_mismatch, msg(who, "delta block table truncated"));
                    refs[k] = {static_cast<std::uint32_t>(start), static_cast<std::uint32_t>(q - p), mask};
                    q += bytes;
                    if (start < old_used) undo += std::min<std::size_t>(delta_block, old_used - std::size_t(start));
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
            // Room for the map of a payload of `used` bytes (before the payload
            // changes, so that rebuilding it cannot fail).
            static vstatus size_checks(XDeltaCursor& c, std::size_t used) {
                c.checks_valid_ = false;
                XOFFSET_TRY {
                    c.checks_.resize((used - payload_begin + delta_block - 1) / delta_block);
                } XOFFSET_CATCH(const std::bad_alloc&) {
                    XOFFSET_VFAIL_BAD_ALLOC();
                }
                return vok();
            }
            template <typename T>
            static void rebuild_checks(XDeltaCursor& c, XBufferCore& core, const T& root) {
                std::fill(c.checks_.begin(), c.checks_.end(), DeltaCheckBits{});
                DeltaCheckMarker m(static_cast<const char*>(core.get_address()), c.checks_.data());
                delta_mark_value(root, m);
                note_checks(c, core);
            }
            // The start of the inline string head that holds checked byte
            // `offset`, or 0 when the byte is a bool. Heads do not overlap, so
            // at most one starts in the 11 bytes before.
            static std::size_t inline_head_of(const DeltaCheckBits* bits, std::size_t offset) noexcept {
                const std::size_t i = offset - payload_begin;
                const std::size_t low = i >= sizeof(XFixedString) - 1 ? i - (sizeof(XFixedString) - 1) : 0;
                for (std::size_t j = i + 1; j-- > low;)
                    if ((bits[j / delta_block].heads >> (j % delta_block)) & 1u) return payload_begin + j;
                return 0;
            }
            // Calls fn(offset of the head) once for each inline string head the
            // delta wrote checked bytes of, and fn_bool(offset) for each bool.
            template <typename HeadFn, typename BoolFn>
            static vstatus for_each_checked_write(const Parsed& parsed, const DeltaCheckBits* bits, HeadFn&& on_head,
                                                  BoolFn&& on_bool) {
                std::size_t last_head = 0;
                for (std::size_t k = 0; k < parsed.block_count; ++k) {
                    const DeltaBlockRef& r = parsed.blocks[k];
                    const std::uint64_t written = r.mask & bits[(r.offset - payload_begin) / delta_block].checked;
                    for (std::uint64_t hit = written; hit != 0; hit &= hit - 1) {
                        const std::size_t offset = r.offset + static_cast<std::size_t>(std::countr_zero(hit));
                        const std::size_t head = inline_head_of(bits, offset);
                        if (head == 0) {
                            XOFFSET_VTRY(on_bool(offset));
                        } else if (head != last_head) {
                            last_head = head;
                            XOFFSET_VTRY(on_head(head));
                        }
                    }
                }
                return vok();
            }
            // The in-place checks: the bools and inline string heads the delta
            // wrote, as verification checks them. `full` when a head left the
            // inline form (its new block needs the whole verification).
            static vstatus check_in_place(const char* base, const Parsed& parsed, const DeltaCheckBits* bits,
                                          bool& full) {
                return for_each_checked_write(
                    parsed, bits,
                    [&](std::size_t head) -> vstatus {
                        if ((static_cast<unsigned char>(base[head + 11]) & 0x80u) == 0) {
                            full = true;
                            return vok();
                        }
                        return validate_inline_string_bytes(base + head);
                    },
                    [&](std::size_t offset) -> vstatus {
                        if (static_cast<unsigned char>(base[offset]) > 1u)
                            return wire_reject("wire validation: invalid bool representation");
                        return vok();
                    });
            }
            // After check_in_place: the heads it checked, marked for their new
            // lengths. (Iterating while marking is safe: a head is re-marked
            // from its own bytes, and marking it twice gives the same bits.)
            static void remark_in_place(XDeltaCursor& c, const char* base, const Parsed& parsed) noexcept {
                DeltaCheckMarker m(base, c.checks_.data());
                (void)for_each_checked_write(
                    parsed, c.checks_.data(),
                    [&](std::size_t head) {
                        m.clear_at(head, sizeof(XFixedString));
                        m.inline_head_at(head);
                        return vok();
                    },
                    [](std::size_t) { return vok(); });
            }
            // The scratch the whole verification needs: the replica's holes,
            // unless the delta carries them, and room for the new map.
            static vstatus prepare_full(XBufferCore& core, const Parsed& parsed, XDeltaCursor& cursor,
                                        const char* who) {
                XOFFSET_VTRY(size_checks(cursor, parsed.header.used_bytes));
                if (parsed.holes) return vok();
                XOFFSET_TRY {
                    cursor.holes_.clear();
                    cursor.holes_.reserve(core.free_block_count());
                    core.for_each_free_block([&](XBufferCore::WireHole h) { cursor.holes_.push_back(h); });
                } XOFFSET_CATCH(const std::bad_alloc&) {
                    XOFFSET_VFAIL_BAD_ALLOC();
                }
                if (!core.free_table_complete())
                    XOFFSET_VFAIL(XException, XErrc::delta_base_mismatch,
                                  msg(who, "replica free table incomplete (ask for a key delta)"));
                return vok();
            }

            template <typename T>
            static vstatus apply_key(XBuffer& xbuf, const Parsed& parsed, XDeltaCursor& cursor, const char* who) {
                XBufferCore& core = xbuf;
                const std::size_t used = parsed.header.used_bytes;
                cursor.clear();
                XOFFSET_VTRY(size_checks(cursor, used));
                XOFFSET_ROLLBACK_BEGIN
                    std::size_t clean_from = 0;
                    XOFFSET_VTRY(core.reinit_for_overwrite(used, clean_from));
                    char* base = core.data();
                    if (clean_from > payload_begin)
                        std::memset(base + payload_begin, 0, std::min(clean_from, used) - payload_begin);
                    // The bytes a key delta leaves out are zero, as they are here now.
                    for_each_block(parsed, [&](std::size_t offset, const char* bytes, std::uint64_t mask) {
                        delta_get_masked(base + offset, bytes, mask);
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
                rebuild_checks(cursor, core, *find_root_at<T>(core, parsed.header.root_offset));
                cursor.last_check_ = XDeltaCheck::key;
                return vok();
            }

            // Restores what apply_incremental changed (bytes, used_end, root).
            static void roll_back(XBufferCore& core, const Parsed& parsed, const char* undo, std::size_t old_used,
                                  std::uint64_t old_root) noexcept {
                char* base = core.data();
                const std::size_t used = parsed.header.used_bytes;
                for_each_block(parsed, [&](std::size_t offset, const char*, std::uint64_t) {
                    if (offset < old_used) {
                        const std::size_t kept = std::min(offset + delta_block, old_used) - offset;
                        std::memcpy(base + offset, undo, kept);
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
                                             const char* who) {
                XBufferCore& core = xbuf;
                const std::size_t used = parsed.header.used_bytes;
                const auto root = static_cast<std::uint64_t>(parsed.header.root_offset);
                const std::size_t old_used = static_cast<std::size_t>(core.arena()->used_end);
                const std::uint64_t old_root = core.arena()->root_offset;

                // What the result needs: no check when the delta writes only
                // bytes that verification does not read and keeps used_end, the
                // root and the holes; the in-place checks when it writes bools
                // and inline string heads besides; else the whole verification.
                XDeltaCheck check = XDeltaCheck::full;
                if (!parsed.holes && used == old_used && root == old_root && checks_match(cursor, core)) {
                    check = XDeltaCheck::none;
                    const DeltaCheckBits* bits = cursor.checks_.data();
                    for (std::size_t k = 0; k < parsed.block_count; ++k) {
                        const DeltaBlockRef& r = parsed.blocks[k];
                        const DeltaCheckBits& b = bits[(r.offset - payload_begin) / delta_block];
                        const std::uint64_t written = r.mask & b.checked;
                        if (written == 0) continue;
                        if ((written & ~b.local) != 0) {
                            check = XDeltaCheck::full;
                            break;
                        }
                        check = XDeltaCheck::in_place;
                    }
                }

                // Everything that can fail without verification fails first,
                // before a byte changes. Nothing can fail after an unchecked
                // delta without a digest, so it keeps no undo copy.
                auto& undo = cursor.undo_;
                const bool keep_undo = check != XDeltaCheck::none || parsed.digest_present;
                if (keep_undo) {
                    const std::size_t undo_bytes = parsed.undo_bytes + (used < old_used ? old_used - used : 0u);
                    XOFFSET_TRY {
                        if (undo.size() < undo_bytes) undo.resize(undo_bytes);
                    } XOFFSET_CATCH(const std::bad_alloc&) {
                        XOFFSET_VFAIL_BAD_ALLOC();
                    }
                }
                if (check == XDeltaCheck::full) XOFFSET_VTRY(prepare_full(core, parsed, cursor, who));
                if (used > core.segment_size() && !core.grow(used - core.segment_size()))
                    XOFFSET_VFAIL(XBadAlloc, XErrc::capacity_exceeded, arena_failure_message);

                // Save, then patch. Bytes at or past old_used are zero (N2).
                char* base = core.data();
                if (keep_undo) {
                    char* u = undo.data();
                    for_each_block(parsed, [&](std::size_t offset, const char* bytes, std::uint64_t mask) {
                        if (offset < old_used) {
                            const std::size_t kept = std::min(offset + delta_block, old_used) - offset;
                            if (kept == delta_block) std::memcpy(u, base + offset, delta_block);
                            else std::memcpy(u, base + offset, kept);
                            u += kept;
                        }
                        delta_get_masked(base + offset, bytes, mask);
                    });
                    if (used < old_used) {
                        std::memcpy(u, base + used, old_used - used);
                        std::memset(base + used, 0, old_used - used);
                    }
                } else {
                    for_each_block(parsed, [&](std::size_t offset, const char* bytes, std::uint64_t mask) {
                        delta_get_masked(base + offset, bytes, mask);
                    });
                }
                auto* sm = core.arena();
                sm->used_end = used;
                sm->root_offset = root;

                XOFFSET_ROLLBACK_BEGIN
                    XOFFSET_VTRY(check_digest<T>(core, parsed, who));
                    if (check == XDeltaCheck::in_place) {
                        bool full = false;
                        XOFFSET_VTRY(check_in_place(base, parsed, cursor.checks_.data(), full));
                        if (full) {
                            check = XDeltaCheck::full;
                            XOFFSET_VTRY(prepare_full(core, parsed, cursor, who));
                        }
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
                                                                  cursor.holes_.data(), cursor.holes_.size(), true,
                                                                  nullptr, &xbuf.retained_reload_scratch()));
                        if (parsed.holes)
                            XOFFSET_VTRY(core.load_free_blocks(cursor.holes_.data(), cursor.holes_.size()));
                    }
                XOFFSET_ROLLBACK_END(roll_back(core, parsed, undo.data(), old_used, old_root))

                // Committed. A new epoch: element pointers and XHandles of the
                // previous content re-resolve.
                ++core.m_epoch;
                T* root_ptr = find_root_at<T>(core, root);
                core.cache_root(root_ptr);
                cursor.version_ = parsed.header.version;
                if (check == XDeltaCheck::full) {
                    rebuild_checks(cursor, core, *root_ptr);
                } else {
                    if (check == XDeltaCheck::in_place) remark_in_place(cursor, base, parsed);
                    note_checks(cursor, core);
                }
                cursor.last_check_ = check;
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
                const std::uint64_t old_root = has_state ? core.arena()->root_offset : 0u;
                Parsed parsed{};
                XOFFSET_VTRY(parse<T>(data, cursor, old_used, old_root, cursor.holes_, cursor.blocks_, parsed, who));
                if (parsed.key) return apply_key<T>(xbuf, parsed, cursor, who);
                if (!has_state)
                    XOFFSET_VFAIL(XException, XErrc::delta_base_mismatch,
                                  msg(who, "buffer holds no verified root of this type (ask for a key delta)"));
                return apply_incremental<T>(xbuf, parsed, cursor, who);
            }
        };
    } // namespace detail

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
