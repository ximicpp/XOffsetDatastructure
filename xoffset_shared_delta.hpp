#ifndef X_OFFSET_SHARED_DELTA_HPP
#define X_OFFSET_SHARED_DELTA_HPP

// Shared deltas: record-relative deltas a server writes once per entity and
// sends to every connection that holds the entity (docs/SHARED_DELTA.md).
// The message format is XS 1.0, frozen within 3.x (format byte 0x01;
// tests/test_shared_delta_format.cpp pins its bytes). A client applies a
// message with XReceiver<XLink::from_server>::apply_sync
// (xoffset_receiver.hpp); this header has the sender side, the record codec
// and the receive engine apply_sync runs.
//
// A delta of xoffset_delta.hpp carries bytes at payload offsets, so it only
// applies to a replica with the sender's layout: one baseline per receiver.
// Here a change is written per record instead, as bytes at offsets inside the
// record, and applies to a copy of that record held anywhere (another buffer,
// another slot). A server compares each changed entity once per tick against
// one shared baseline of it and every connection that holds the entity gets
// the same bytes; the receiver keeps its own layout.
//
// What a body may write:
//   - the record's plain bytes: scalars, enums with a fixed underlying type,
//     bools (0 or 1), the same in nested records and arrays. Container heads
//     and padding are never written as bytes (their values depend on where
//     the containers' blocks lie in a buffer);
//   - its containers through container operations: a string is assigned whole
//     (STR); a vector of plain elements is replaced whole (VSET: the elements'
//     plain bytes) or, at the same length, patched (VPATCH: element bytes).
// Every write is a plain byte checked against the record type's byte classes
// or a library container operation in the receiver's own arena, so the
// receiver's buffer stays one that load_verified<T> accepts, without a check
// map or a whole verification.
//
// Body (record-relative):
//   inline := varint n, n x {varint skip, u8 selector (nonzero), mask bytes,
//             data bytes} -- the block section of delta format 1.0 with blocks
//             of 64 bytes counted from the record's first byte; every masked
//             byte is a plain byte of T
//   conts  := varint m, m x {u8 field << 2 | op, payload}, fields ascending;
//             present only when T has containers. field = the container's
//             index in schema order (nested records walked in place).
//             op 0 STR    varint len, len bytes
//             op 1 VSET   varint n, n x (plain bytes of E, in order)
//             op 2 VPATCH an inline section over the elements' bytes
//                         [0, size * sizeof(E)) at the current size
// Supported: scalars, bools, fixed-underlying enums, nested
// ordinary records and arrays of them, XString, and XVector of elements
// without containers. Sets, maps and vectors of records with containers are
// rejected at compile time.
//
// Encoding writes into an XSyncOut (storage that only grows, never zero
// filled); a server builds each message with an XSyncWriter, whose cursor
// stays in registers: one capacity check per op.

#include "xoffsetdatastructure.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>
#include <memory>
#include <new>
#include <span>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

#if defined(_MSC_VER) && !defined(__clang__)
#define XOFFSET_SYNC_INLINE __forceinline
#define XOFFSET_SYNC_LAMBDA_INLINE [[msvc::forceinline]]
#define XOFFSET_SYNC_COLD __declspec(noinline)
#elif defined(__GNUC__) || defined(__clang__)
#define XOFFSET_SYNC_INLINE inline __attribute__((always_inline))
#define XOFFSET_SYNC_LAMBDA_INLINE __attribute__((always_inline))
#define XOFFSET_SYNC_COLD __attribute__((noinline, cold))
#else
#define XOFFSET_SYNC_INLINE inline
#define XOFFSET_SYNC_LAMBDA_INLINE
#define XOFFSET_SYNC_COLD
#endif

namespace XOffsetDatastructure::xsync {

    // ---- varints (unsigned LEB128, at most 5 bytes, below 2^32) -------------

    // Writes v at b (at most 5 bytes); returns the length.
    inline unsigned encode_varint(char* b, std::uint32_t v) noexcept {
        if (v < 0x80) { b[0] = char(v); return 1; }
        if (v < 0x4000) { b[0] = char(v | 0x80); b[1] = char(v >> 7); return 2; }
        unsigned n = 0;
        while (v >= 0x80) { b[n++] = char(v | 0x80); v >>= 7; }
        b[n++] = char(v);
        return n;
    }
    // ---- output buffer ---------------------------------------------------------

    class XSyncWriter;

    // The bytes an encoder appends to. Storage only grows and is never zero
    // filled; clear() keeps it for the next message, so a server that reuses
    // one buffer per message pays one capacity check per write and no
    // allocation once it has grown.
    class XSyncOut {
    public:
        XSyncOut() = default;
        XSyncOut(const XSyncOut& o) { append(o.data(), o.size()); }
        XSyncOut& operator=(const XSyncOut& o) {
            if (this != &o) { len_ = 0; append(o.data(), o.size()); }
            return *this;
        }
        XSyncOut(XSyncOut&& o) noexcept : buf_(std::move(o.buf_)), len_(o.len_), cap_(o.cap_) { o.len_ = o.cap_ = 0; }
        XSyncOut& operator=(XSyncOut&& o) noexcept {
            buf_ = std::move(o.buf_); len_ = o.len_; cap_ = o.cap_; o.len_ = o.cap_ = 0;
            return *this;
        }

        void clear() noexcept { len_ = 0; }
        bool empty() const noexcept { return len_ == 0; }
        std::size_t size() const noexcept { return len_; }
        std::size_t capacity() const noexcept { return cap_; }
        char* data() noexcept { return buf_.get(); }
        const char* data() const noexcept { return buf_.get(); }
        std::string_view view() const noexcept { return {buf_.get(), len_}; }
        char& operator[](std::size_t i) noexcept { return buf_[i]; }
        char operator[](std::size_t i) const noexcept { return buf_[i]; }

        // At least n writable bytes after the end; returns where they start.
        // commit(k) then keeps the first k of them.
        char* tail(std::size_t n) {
            if (cap_ - len_ < n) grow(n);
            return buf_.get() + len_;
        }
        void commit(std::size_t k) noexcept { len_ += k; }
        void push_back(char c) { *tail(1) = c; ++len_; }
        void append(const char* p, std::size_t n) {
            if (n == 0) return;
            std::memcpy(tail(n), p, n);
            len_ += n;
        }
        // Drops the bytes from n on (n <= size()).
        void truncate(std::size_t n) noexcept { len_ = n; }
        void reserve(std::size_t n) { if (n > cap_) grow(n - len_); }

    private:
        friend class XSyncWriter;
        XOFFSET_DETAIL_NOINLINE void grow(std::size_t n) {
            std::size_t c = cap_ * 2;
            if (c < len_ + n) c = len_ + n;
            if (c < 256) c = 256;
            std::unique_ptr<char[]> b(new char[c]);
            if (len_) std::memcpy(b.get(), buf_.get(), len_);
            buf_ = std::move(b);
            cap_ = c;
        }
        std::unique_ptr<char[]> buf_;
        std::size_t len_ = 0, cap_ = 0;
    };

    // Appends at most max bytes to out: f(d) writes them at d and returns how
    // many it wrote. One capacity check, no call per byte.
    template <class F> inline void write_into(XSyncOut& out, std::size_t max, F&& f) {
        char* d = out.tail(max);
        out.commit(std::size_t(f(d)));
    }
    inline void put_varint(XSyncOut& out, std::uint32_t v) {
        write_into(out, 5, [&](char* d) { return encode_varint(d, v); });
    }
    namespace sd_detail {
        // A varint read out of line: where it ends and its value, ok false
        // when it is cut by end or longer than five bytes. Returned by value,
        // so the inline readers keep their position in a register.
        struct VarintAt {
            const char* p;
            std::uint32_t v;
            bool ok;
        };
        XOFFSET_DETAIL_NOINLINE inline VarintAt varint_slow(const char* p, const char* end) noexcept {
            std::uint32_t r = 0;
            for (unsigned s = 0; s < 35; s += 7) {
                if (p == end) return {p, 0, false};
                const auto b = static_cast<unsigned char>(*p++);
                if (s == 28 && b > 0x0f) return {p, 0, false};
                r |= std::uint32_t(b & 0x7f) << s;
                if (!(b & 0x80)) return {p, r, true};
            }
            return {p, 0, false};
        }
    } // namespace sd_detail
    namespace sd_detail {
        // A varint already checked (a body check() accepted, or the sender's):
        // one byte inline.
        XOFFSET_SYNC_INLINE std::uint32_t read_varint(const char*& p) noexcept {
            const auto b0 = static_cast<unsigned char>(*p);
            if (!(b0 & 0x80)) { ++p; return b0; }
            const VarintAt r = varint_slow(p, p + 5);
            p = r.p;
            return r.v;
        }
    } // namespace sd_detail
    // One and two bytes inline (ids below 2^14), the rest out of line.
    XOFFSET_SYNC_INLINE bool get_varint(const char*& p, const char* end, std::uint32_t& v) noexcept {
        if (p != end) {
            const auto b0 = static_cast<unsigned char>(p[0]);
            if (!(b0 & 0x80)) { v = b0; ++p; return true; }
            if (end - p >= 2) {
                const auto b1 = static_cast<unsigned char>(p[1]);
                if (!(b1 & 0x80)) { v = (b0 & 0x7fu) | std::uint32_t(b1) << 7; p += 2; return true; }
            }
        }
        const sd_detail::VarintAt r = sd_detail::varint_slow(p, end);
        p = r.p;
        if (r.ok) v = r.v;
        return r.ok;
    }

    namespace sd_detail {
        using XOffsetDatastructure::detail::fixed_size_scalar;
        using XOffsetDatastructure::detail::is_fixed_flat_map_v;
        using XOffsetDatastructure::detail::is_fixed_flat_set_v;
        using XOffsetDatastructure::detail::is_fixed_string_v;
        using XOffsetDatastructure::detail::is_fixed_vector_v;

        enum : std::uint8_t { kNone = 0, kPlain = 1, kBool = 2 };
        enum : std::uint8_t { kStr = 0, kVset = 1, kVpatch = 2 };

        template <class> inline constexpr bool always_false = false;

        template <class E> consteval bool enum_fixed() {
            if constexpr (!std::is_enum_v<E>) return false;
            else return requires(std::underlying_type_t<E> v) { E{v}; };
        }

        template <class U> constexpr bool is_container() {
            return is_fixed_string_v<U> || is_fixed_vector_v<U> || is_fixed_flat_set_v<U> || is_fixed_flat_map_v<U>;
        }

        // Byte classes of T at base: kPlain, kBool, or kNone (container heads, padding).
        template <class T>
        constexpr void classify(std::uint8_t* out, std::size_t base) {
            using U = std::remove_cv_t<T>;
            if constexpr (std::is_same_v<U, bool>) {
                out[base] = kBool;
            } else if constexpr (fixed_size_scalar<U>) {
                for (std::size_t i = 0; i < sizeof(U); ++i) out[base + i] = kPlain;
            } else if constexpr (std::is_enum_v<U>) {
                static_assert(enum_fixed<U>() && !std::is_same_v<std::underlying_type_t<U>, bool>,
                              "xsync: enums need a fixed, non-bool underlying type");
                for (std::size_t i = 0; i < sizeof(U); ++i) out[base + i] = kPlain;
            } else if constexpr (is_container<U>()) {
                // a head: written only through container operations
            } else if constexpr (std::is_array_v<U>) {
                using E = std::remove_extent_t<U>;
                for (std::size_t i = 0; i < std::extent_v<U>; ++i) classify<E>(out, base + i * sizeof(E));
            } else if constexpr (std::is_class_v<U>) {
                static_assert(schema::is_described_v<U> && schema::base_count_v<U> == 0,
                              "xsync: records must be described and have no bases");
                [&]<std::size_t... I>(std::index_sequence<I...>) {
                    (classify<typename schema::member_t<U, I>::type>(out, base + schema::member_t<U, I>::offset), ...);
                }(std::make_index_sequence<schema::member_count_v<U>>{});
            } else {
                static_assert(always_false<U>, "xsync: unsupported field type");
            }
        }

        // The containers of T, in schema order, with their offsets in T.
        template <class C, std::size_t Off> struct Slot { using type = C; static constexpr std::size_t offset = Off; };
        template <class... S> struct List { static constexpr std::size_t size = sizeof...(S); };
        template <class... L> struct Concat;
        template <> struct Concat<> { using type = List<>; };
        template <class... A> struct Concat<List<A...>> { using type = List<A...>; };
        template <class... A, class... B, class... R> struct Concat<List<A...>, List<B...>, R...> {
            using type = typename Concat<List<A..., B...>, R...>::type;
        };
        template <class T, std::size_t Base, class = void> struct ContainersOf;
        template <class U, std::size_t Base, class Seq> struct ArrayContainers;
        template <class U, std::size_t Base, std::size_t... I> struct ArrayContainers<U, Base, std::index_sequence<I...>> {
            using E = std::remove_extent_t<U>;
            using type = typename Concat<typename ContainersOf<E, Base + I * sizeof(E)>::type...>::type;
        };
        template <class U, std::size_t Base, class Seq> struct RecordContainers;
        template <class U, std::size_t Base, std::size_t... I> struct RecordContainers<U, Base, std::index_sequence<I...>> {
            using type = typename Concat<typename ContainersOf<typename schema::member_t<U, I>::type,
                                                               Base + schema::member_t<U, I>::offset>::type...>::type;
        };
        template <class T, std::size_t Base, class>
        struct ContainersOf {
            using U = std::remove_cv_t<T>;
            static constexpr auto pick() {
                if constexpr (is_fixed_string_v<U> || is_fixed_vector_v<U>) return List<Slot<U, Base>>{};
                else if constexpr (std::is_array_v<U>)
                    return typename ArrayContainers<U, Base, std::make_index_sequence<std::extent_v<U>>>::type{};
                else if constexpr (std::is_class_v<U> && !is_container<U>())
                    return typename RecordContainers<U, Base, std::make_index_sequence<schema::member_count_v<U>>>::type{};
                else return List<>{};
            }
            using type = decltype(pick());
        };

        template <class T> struct Info;

        template <class C> constexpr bool supported_container() {
            if constexpr (is_fixed_flat_set_v<C> || is_fixed_flat_map_v<C>) return false;
            else if constexpr (is_fixed_string_v<C>) return true;
            else {
                using E = typename C::value_type;
                return std::is_trivially_copyable_v<E> && ContainersOf<E, 0>::type::size == 0;
            }
        }

        template <class T> struct Info {
            static constexpr std::size_t size = sizeof(T);
            static constexpr std::size_t words = (size + 7) / 8;
            static constexpr std::array<std::uint8_t, size> classes = [] {
                std::array<std::uint8_t, size> a{};
                classify<T>(a.data(), 0);
                return a;
            }();
            static constexpr auto word_masks(std::uint8_t which) {
                std::array<std::uint8_t, words> m{};
                for (std::size_t i = 0; i < size; ++i)
                    if (classes[i] & which) m[i / 8] = std::uint8_t(m[i / 8] | (1u << (i % 8)));
                return m;
            }
            static constexpr std::array<std::uint8_t, words> plain = word_masks(kPlain | kBool);
            static constexpr std::array<std::uint8_t, words> bools = word_masks(kBool);
            static constexpr bool any_bool = [] { for (auto c : classes) if (c == kBool) return true; return false; }();
            // Runs of plain bytes: the dense form of VSET.
            static constexpr std::size_t run_count = [] {
                std::size_t n = 0;
                for (std::size_t i = 0; i < size; ++i) if (classes[i] && (i == 0 || !classes[i - 1])) ++n;
                return n;
            }();
            struct Run { std::uint32_t at, len; };
            static constexpr std::array<Run, run_count> runs = [] {
                std::array<Run, run_count> r{};
                std::size_t n = 0;
                for (std::size_t i = 0; i < size; ++i) {
                    if (!classes[i]) continue;
                    if (i == 0 || !classes[i - 1]) r[n++] = Run{std::uint32_t(i), 0};
                    ++r[n - 1].len;
                }
                return r;
            }();
            static constexpr std::size_t plain_bytes = [] { std::size_t n = 0; for (auto c : classes) n += c != 0; return n; }();
            using containers = typename ContainersOf<T, 0>::type;
            static constexpr std::size_t container_count = containers::size;
            static_assert(container_count < 64, "xsync: at most 63 containers per record");
            static_assert((size + 63) / 64 < 128, "xsync: records up to 8 KiB");
        };

        template <class S> constexpr bool slot_supported() { return supported_container<typename S::type>(); }
        template <class... S> constexpr bool all_supported(List<S...>) { return (slot_supported<S>() && ...); }

        template <class C, std::size_t Off, class T> C& at(T& rec) noexcept {
            return *std::launder(reinterpret_cast<C*>(reinterpret_cast<unsigned char*>(&rec) + Off));
        }
        template <class C, std::size_t Off, class T> const C& at(const T& rec) noexcept {
            return *std::launder(reinterpret_cast<const C*>(reinterpret_cast<const unsigned char*>(&rec) + Off));
        }

        // f.template operator()<C, Off>(index) for every container, in order.
        template <class... S, class F> void each(List<S...>, F&& f) {
            std::uint32_t k = 0;
            (f.template operator()<typename S::type, S::offset>(k++), ...);
        }
        // f.template operator()<C, Off>() for container k; false when k is out of range.
        template <class... S, class F> bool visit(List<S...>, std::uint32_t k, F&& f) {
            std::uint32_t i = 0;
            bool r = false;
            (void)((i++ == k ? (r = f.template operator()<typename S::type, S::offset>(), true) : false) || ...);
            return r;
        }

        // Bit b of the result: byte b of x is nonzero.
        inline unsigned byte_mask(std::uint64_t x) noexcept {
            // Top bit of each nonzero byte (its low seven bits carried up,
            // or its own top bit), then gathered into the top byte.
            const std::uint64_t h = (((x & 0x7f7f7f7f7f7f7f7full) + 0x7f7f7f7f7f7f7f7full) | x) & 0x8080808080808080ull;
            return unsigned((h * 0x0002040810204081ull) >> 56);
        }
        // Word a / 8 of p: rem = bytes left from a (zero fill past them).
        inline std::uint64_t load(const unsigned char* p, std::size_t rem) noexcept {
            std::uint64_t v = 0;
            if (rem >= 8) std::memcpy(&v, p, 8);
            else std::memcpy(&v, p, rem);
            return v;
        }
        // Byte b of kSpread[m] is 0xff when bit b of m is set.
        inline constexpr auto kSpread = [] {
            std::array<std::uint64_t, 256> t{};
            for (unsigned m = 0; m < 256; ++m)
                for (unsigned b = 0; b < 8; ++b)
                    if (m >> b & 1u) t[m] |= std::uint64_t(0xff) << (8 * b);
            return t;
        }();

        // Bits set in an 8-bit value (a table: std::popcount is a library call on
        // baseline x86-64 builds).
        inline constexpr auto kPop8 = [] {
            std::array<std::uint8_t, 256> t{};
            for (unsigned i = 0; i < 256; ++i) t[i] = std::uint8_t(std::popcount(i));
            return t;
        }();
        // Word a / 8 of the n bytes at p: whole words when N (n known at compile
        // time) is a multiple of 8, else zero filled past n.
        template <std::size_t N> inline std::uint64_t word_at(const unsigned char* p, std::size_t a, std::size_t n) noexcept {
            if constexpr (N != 0 && N % 8 == 0) { std::uint64_t v; std::memcpy(&v, p + a, 8); return v; }
            else return load(p + a, n - a);
        }

        // Writes the block section of [0, n) of cur against old (nullptr: all
        // zero). plain(w) gives the sendable bytes of word w. With sync the
        // sent bytes are copied into old. Returns the number of blocks.
        // N != 0: n is N, known at compile time (a record); whole words are
        // loaded and stored when N is a multiple of 8. One capacity check for
        // the whole section, then plain stores.
        // only (when not null): a block mask, bit b of only[b / 64]; the other
        // blocks are left out (neither compared nor synced).
        // Room put_blocks_at needs for n bytes: per block at most 5 + 1 + 8 +
        // 64 bytes; the masks and a run are stored as whole words (8 bytes of
        // slack).
        constexpr std::size_t blocks_room(std::size_t n) noexcept { return 1 + (n + 63) / 64 * (5 + 1 + 8 + 64) + 8; }

        // put_blocks into d, which has blocks_room(n) bytes: returns where the
        // section ends; count, the number of blocks.
        template <std::size_t N, class Plain>
        XOFFSET_SYNC_INLINE char* put_blocks_body(char* const d, const unsigned char* cur, unsigned char* old, std::size_t n,
                                                  Plain& plain, bool sync, const std::uint64_t* only, std::uint32_t& count_out) {
            if constexpr (N != 0) n = N;
            constexpr bool whole = N != 0 && N % 8 == 0;
            const std::size_t blocks = (n + 63) / 64;
            char* q = d + 1;
            std::uint32_t count = 0;
            std::size_t next = 0;
            for (std::size_t b = 0; b < blocks; ++b) {
                if (only && !(only[b / 64] >> (b % 64) & 1u)) continue;
                XOFFSET_INDETERMINATE std::uint64_t cw[8];
                XOFFSET_INDETERMINATE unsigned masks[8];
                unsigned sel = 0, nm = 0;
                std::uint64_t packed = 0;   // the block's masks, in order
                const std::size_t words = n - b * 64 >= 64 ? 8 : (n - b * 64 + 7) / 8;
                for (unsigned w = 0; w < words; ++w) {
                    const std::size_t a = b * 64 + w * 8;
                    const std::uint64_t c = word_at<N>(cur, a, n);
                    const std::uint64_t o = old ? word_at<N>(old, a, n) : 0;
                    const std::uint64_t x = c ^ o;
                    if (!x) continue;
                    const unsigned pl = plain(a / 8);
                    const unsigned m = byte_mask(x) & pl;
                    if (!m) continue;
                    cw[w] = c;
                    masks[w] = m;
                    sel |= 1u << w;
                    packed |= std::uint64_t(m) << (8 * nm++);
                    if (sync) {   // the sent bytes into old, the others kept
                        const std::uint64_t v = o ^ (x & kSpread[pl]);
                        std::memcpy(old + a, &v, whole || n - a >= 8 ? 8 : n - a);
                    }
                }
                if (!sel) continue;
                // One block: varint skip, selector, masks, data.
                q += encode_varint(q, std::uint32_t(b - next));
                *q++ = char(sel);
                std::memcpy(q, &packed, 8);
                q += nm;
                for (unsigned s = sel; s; s &= s - 1) {
                    const unsigned w = unsigned(std::countr_zero(s));
                    const unsigned m = masks[w];
                    const std::uint64_t c = cw[w];
                    const unsigned lo = unsigned(std::countr_zero(m));
                    const unsigned r = m >> lo;
                    if ((r & (r + 1)) == 0) {   // one run of bytes (a whole word included)
                        const std::uint64_t v = c >> (8 * lo);
                        std::memcpy(q, &v, 8);
                        q += kPop8[m];
                        continue;
                    }
                    for (unsigned t = m; t; t &= t - 1) *q++ = char(c >> (8 * std::countr_zero(t)));
                }
                next = b + 1;
                ++count;
            }
            d[0] = char(count);   // < 128 blocks: one varint byte
            count_out = count;
            return q;
        }
        template <std::size_t N, class Plain>
        XOFFSET_DETAIL_NOINLINE char* put_blocks_far(char* const d, const unsigned char* cur, unsigned char* old, std::size_t n,
                                                     Plain& plain, bool sync, const std::uint64_t* only, std::uint32_t& count_out) {
            return put_blocks_body<N>(d, cur, old, n, plain, sync, only, count_out);
        }
        // Records of one block are written inline (a message's small records:
        // its root fields, a tile, a march); larger ones and element arrays
        // share one out-of-line copy per type, so the code a server runs per
        // message stays small.
        template <std::size_t N = 0, class Plain>
        XOFFSET_SYNC_INLINE char* put_blocks_at(char* const d, const unsigned char* cur, unsigned char* old, std::size_t n,
                                                Plain&& plain, bool sync, const std::uint64_t* only, std::uint32_t& count_out) {
            if constexpr (N != 0 && N <= 64) return put_blocks_body<N>(d, cur, old, n, plain, sync, only, count_out);
            else return put_blocks_far<N>(d, cur, old, n, plain, sync, only, count_out);
        }
        template <std::size_t N = 0, class Plain>
        std::uint32_t put_blocks(XSyncOut& out, const unsigned char* cur, unsigned char* old, std::size_t n,
                                 Plain&& plain, bool sync, const std::uint64_t* only = nullptr) {
            if constexpr (N != 0) n = N;
            char* const d = out.tail(blocks_room(n));
            std::uint32_t count;
            char* const e = put_blocks_at<N>(d, cur, old, n, plain, sync, only, count);
            out.commit(std::size_t(e - d));
            return count;
        }

        // The bytes of word a / 8 that lie inside [0, n) (a < n).
        inline unsigned inside(std::size_t a, std::size_t n) noexcept {
            return n - a >= 8 ? 0xffu : (1u << (n - a)) - 1u;
        }

        // Checks a block section over [0, n): bounds, masks inside plain() and
        // inside [0, n), bool bytes 0 or 1 (only when AnyBool). Returns its end
        // or nullptr.
        template <bool AnyBool, class Plain, class Bools>
        const char* check_blocks(const char* p, const char* end, std::size_t n, Plain&& plain, Bools&& bools) noexcept {
            std::uint32_t count;
            if (!get_varint(p, end, count)) return nullptr;
            const std::size_t blocks = (n + 63) / 64;
            std::size_t next = 0;
            for (std::uint32_t e = 0; e < count; ++e) {
                std::uint32_t skip;
                if (!get_varint(p, end, skip)) return nullptr;
                const std::size_t b = next + skip;
                if (b >= blocks || p == end) return nullptr;
                const unsigned sel = static_cast<unsigned char>(*p++);
                if (!sel) return nullptr;
                const unsigned nm = kPop8[sel];
                if (std::size_t(end - p) < nm) return nullptr;
                const char* const masks = p;
                p += nm;
                std::size_t k = 0;
                unsigned i = 0;
                for (unsigned s = sel; s; s &= s - 1, ++i) {
                    const std::size_t a = b * 64 + unsigned(std::countr_zero(s)) * 8;
                    if (a >= n) return nullptr;
                    const unsigned m = static_cast<unsigned char>(masks[i]);
                    if (!m || (m & ~(plain(a / 8) & inside(a, n)))) return nullptr;
                    k += kPop8[m];
                }
                if (std::size_t(end - p) < k) return nullptr;
                if constexpr (AnyBool) {
                    const char* q = p;
                    i = 0;
                    for (unsigned s = sel; s; s &= s - 1, ++i) {
                        const unsigned m = static_cast<unsigned char>(masks[i]);
                        if (const unsigned bm = bools((b * 64 + unsigned(std::countr_zero(s)) * 8) / 8) & m) {
                            const char* r = q;
                            for (unsigned mm = m; mm; mm &= mm - 1, ++r)
                                if ((bm >> std::countr_zero(mm) & 1) && static_cast<unsigned char>(*r) > 1) return nullptr;
                        }
                        q += kPop8[m];
                    }
                }
                p += k;
                next = b + 1;
            }
            return p;
        }

        // Applies a checked block section to dst. Returns its end.
        XOFFSET_SYNC_INLINE const char* apply_blocks(const char* p, unsigned char* dst) noexcept {
            const std::uint32_t count = read_varint(p);
            std::size_t next = 0;
            for (std::uint32_t e = 0; e < count; ++e) {
                const std::size_t b = next + read_varint(p);
                const unsigned sel = static_cast<unsigned char>(*p++);
                const char* masks = p;
                p += kPop8[sel];
                for (unsigned s = sel; s; s &= s - 1) {
                    unsigned char* d = dst + b * 64 + unsigned(std::countr_zero(s)) * 8;
                    const unsigned m = static_cast<unsigned char>(*masks++);
                    if (m == 0xffu) { std::memcpy(d, p, 8); p += 8; continue; }
                    for (unsigned mm = m; mm; mm &= mm - 1) d[std::countr_zero(mm)] = static_cast<unsigned char>(*p++);
                }
                next = b + 1;
            }
            return p;
        }

        // Plain / bool word masks of an element array of E (word w of the elements' bytes).
        // The masks repeat every S / gcd(S, 8) words: one table per E and which.
        template <class E, std::uint8_t Which> inline constexpr auto kElemMasks = [] {
            constexpr std::size_t S = sizeof(E);
            constexpr std::size_t P = S / (S % 8 == 0 ? 8 : S % 4 == 0 ? 4 : S % 2 == 0 ? 2 : 1);
            std::array<std::uint8_t, P> t{};
            for (std::size_t w = 0; w < P; ++w)
                for (unsigned j = 0; j < 8; ++j)
                    if (Info<E>::classes[(w * 8 + j) % S] & Which) t[w] = std::uint8_t(t[w] | (1u << j));
            return t;
        }();
        template <class E> unsigned elem_mask(std::size_t w, std::uint8_t which) noexcept {
            constexpr auto& pb = kElemMasks<E, kPlain | kBool>;
            constexpr auto& b = kElemMasks<E, kBool>;
            if (which == (kPlain | kBool)) return pb[pb.size() == 1 ? 0 : w % pb.size()];
            if (which == kBool) return b[b.size() == 1 ? 0 : w % b.size()];
            unsigned m = 0;
            for (unsigned j = 0; j < 8; ++j)
                if (Info<E>::classes[(w * 8 + j) % sizeof(E)] & which) m |= 1u << j;
            return m;
        }
    } // namespace sd_detail

    /// A part of a record for XRecordDelta<T>::diff(now, before, out, fields):
    /// the 64-byte blocks of its plain bytes (bit b of blocks[b / 64]) and its
    /// containers (bit k, schema order). XRecordDelta<T>::fields(members)
    /// builds one from top-level members.
    struct XSyncFields {
        std::uint64_t blocks[2] = {0, 0};
        std::uint64_t containers = 0;
        constexpr XSyncFields& operator|=(const XSyncFields& o) noexcept {
            blocks[0] |= o.blocks[0]; blocks[1] |= o.blocks[1]; containers |= o.containers;
            return *this;
        }
    };

    namespace sd_detail {
        template <class... S> constexpr std::uint64_t containers_within(List<S...>, std::size_t lo, std::size_t hi) {
            std::uint64_t m = 0;
            std::uint32_t k = 0;
            ((m |= (S::offset >= lo && S::offset < hi ? std::uint64_t(1) << k : 0), ++k), ...);
            return m;
        }
    } // namespace sd_detail

    // Record codec for T (static functions).
    template <class T>
    struct XRecordDelta {
        using I = sd_detail::Info<T>;
        static_assert(sd_detail::all_supported(typename I::containers{}),
                      "xsync: containers must be strings or vectors of elements without containers");
        static constexpr std::size_t container_count = I::container_count;

        // Appends the change from before to now and copies it into before.
        // Appends nothing and returns false when nothing changed.
        static bool diff(const T& now, T& before, XSyncOut& out) { return diff_in(now, before, out, nullptr); }

        // The same over a part of the record only: the blocks and containers
        // fields names (fields(members)). The rest is neither compared nor
        // copied; when it is equal in now and before (the members a caller's
        // dirty flags leave out), the body is the one diff(now, before, out)
        // writes, for a fraction of the reads.
        static bool diff(const T& now, T& before, XSyncOut& out, const XSyncFields& fields) { return diff_in(now, before, out, &fields); }

        // Applies set(rec), which changes rec in place, and appends the change
        // it made to the blocks and containers fields names (the body
        // diff(now, before, out, fields) writes with before = rec as it was).
        // Appends nothing and returns false when nothing there changed. One
        // copy of the record instead of two (a baseline and a current image):
        // the named blocks and containers are copied aside before set runs.
        // set may change anything, but only the part fields names is
        // compared, and it must not move rec.
        template <class F> static bool update(T& rec, const XSyncFields& fields, XSyncOut& out, F&& set) {
            XOFFSET_INDETERMINATE alignas(8) unsigned char snap[sizeof(T)];
            const auto* r = reinterpret_cast<const unsigned char*>(&rec);
            constexpr std::size_t nblocks = (sizeof(T) + 63) / 64;
            // One copy of a constant size per hinted block (unrolled: a few
            // vector moves each, no length computed at run time).
            [&]<std::size_t... B>(std::index_sequence<B...>) {
                ((fields.blocks[B / 64] >> (B % 64) & 1u
                      ? (void)std::memcpy(snap + B * 64, r + B * 64, sizeof(T) - B * 64 >= 64 ? 64 : sizeof(T) - B * 64)
                      : (void)0),
                 ...);
            }(std::make_index_sequence<nblocks>{});
            [[maybe_unused]] std::array<std::uint32_t, container_count + 1> at{};
            [[maybe_unused]] std::array<std::uint32_t, container_count + 1> len{};
            XSyncOut& keep = scratch();
            if constexpr (container_count > 0) {
                keep.clear();
                sd_detail::each(typename I::containers{}, [&]<class C, std::size_t Off>(std::uint32_t k) {
                    if (!(fields.containers >> k & 1u)) return;
                    const C& c = sd_detail::at<C, Off>(rec);
                    at[k] = std::uint32_t(keep.size());
                    if constexpr (sd_detail::is_fixed_string_v<C>) {
                        len[k] = std::uint32_t(c.view().size());
                        keep.append(c.view().data(), c.view().size());
                    } else {
                        len[k] = std::uint32_t(c.size());
                        keep.append(reinterpret_cast<const char*>(c.data()), c.size() * sizeof(typename C::value_type));
                    }
                });
            }
            set(rec);
            const std::size_t start = out.size();
            const std::uint32_t nb = sd_detail::put_blocks<sizeof(T)>(
                out, reinterpret_cast<const unsigned char*>(&rec), snap, sizeof(T),
                [](std::size_t w) -> unsigned { return I::plain[w]; }, false, fields.blocks);
            std::uint32_t nc = 0;
            if constexpr (container_count > 0) {
                const std::size_t count_at = out.size();
                out.push_back(0);
                sd_detail::each(typename I::containers{}, [&]<class C, std::size_t Off>(std::uint32_t k) {
                    if (!(fields.containers >> k & 1u)) return;
                    const C& cur = sd_detail::at<C, Off>(rec);
                    unsigned char* old = reinterpret_cast<unsigned char*>(keep.data()) + at[k];
                    if constexpr (sd_detail::is_fixed_string_v<C>) {
                        const std::string_view v = cur.view();
                        if (v == std::string_view(reinterpret_cast<const char*>(old), len[k])) return;
                        put_str(out, k, v);
                        ++nc;
                    } else {
                        using E = typename C::value_type;
                        const std::size_t n = cur.size();
                        if (n != len[k] || n * sizeof(E) > 127 * 64) {
                            put_vset(out, k, cur);
                            ++nc;
                            return;
                        }
                        if (n == 0) return;
                        const std::size_t code_at = out.size();
                        out.push_back(char(k << 2 | sd_detail::kVpatch));
                        const std::uint32_t blocks = sd_detail::put_blocks(
                            out, reinterpret_cast<const unsigned char*>(cur.data()), old, n * sizeof(E),
                            [](std::size_t w) { return sd_detail::elem_mask<E>(w, sd_detail::kPlain | sd_detail::kBool); }, false);
                        if (blocks == 0) out.truncate(code_at);
                        else ++nc;
                    }
                });
                out[count_at] = char(nc);
            }
            if (nb == 0 && nc == 0) { out.truncate(start); return false; }
            return true;
        }

        /// Top-level members of T (std::size_t(-1) when T is not a described
        /// record): fields(m) covers the members whose bit is set in m (bit i =
        /// member i, declaration order; members from 64 on are not reachable).
        static constexpr std::size_t member_count = [] {
            if constexpr (std::is_class_v<T> && schema::is_described_v<T>) return std::size_t(schema::member_count_v<T>);
            else return std::size_t(-1);
        }();
        static constexpr XSyncFields member_fields(std::size_t i) { return kMemberFields[i]; }
        static constexpr XSyncFields fields(std::uint64_t members) noexcept {
            XSyncFields f;
            for (std::uint64_t m = members; m; m &= m - 1) {
                const unsigned i = unsigned(std::countr_zero(m));
                if (i < kMemberFields.size()) f |= kMemberFields[i];
            }
            return f;
        }

    private:
        static constexpr auto kMemberFields = [] {
            constexpr std::size_t n = member_count == std::size_t(-1) ? 0 : (member_count < 64 ? member_count : 64);
            std::array<XSyncFields, n> a{};
            if constexpr (n > 0) {
                [&]<std::size_t... M>(std::index_sequence<M...>) {
                    ((a[M] = [] {
                         using F = schema::member_t<T, M>;
                         XSyncFields f;
                         constexpr std::size_t lo = F::offset, hi = F::offset + sizeof(typename F::type);
                         for (std::size_t b = lo / 64; b <= (hi - 1) / 64; ++b) f.blocks[b / 64] |= std::uint64_t(1) << (b % 64);
                         f.containers = sd_detail::containers_within(typename I::containers{}, lo, hi);
                         return f;
                     }()), ...);
                }(std::make_index_sequence<n>{});
            }
            return a;
        }();

        static bool diff_in(const T& now, T& before, XSyncOut& out, const XSyncFields* only) {
            const std::size_t start = out.size();
            const std::uint32_t nb = sd_detail::put_blocks<sizeof(T)>(
                out, reinterpret_cast<const unsigned char*>(&now), reinterpret_cast<unsigned char*>(&before), sizeof(T),
                [](std::size_t w) -> unsigned { return I::plain[w]; }, true, only ? only->blocks : nullptr);
            std::uint32_t nc = 0;
            if constexpr (container_count > 0) {
                const std::size_t count_at = out.size();
                out.push_back(0);
                sd_detail::each(typename I::containers{}, [&]<class C, std::size_t Off>(std::uint32_t k) {
                    if (only && !(only->containers >> k & 1u)) return;
                    const C& cur = sd_detail::at<C, Off>(now);
                    C& old = sd_detail::at<C, Off>(before);
                    if constexpr (sd_detail::is_fixed_string_v<C>) {
                        const std::string_view v = cur.view();
                        if (v == old.view()) return;
                        put_str(out, k, v);
                        static_cast<XFixedString&>(old).assign(v);
                        ++nc;
                    } else {
                        using E = typename C::value_type;
                        const std::size_t n = cur.size();
                        if (n != old.size() || n * sizeof(E) > 127 * 64) {
                            put_vset(out, k, cur);
                            old.assign_range(std::span<const E>(cur.data(), n));
                            ++nc;
                            return;
                        }
                        if (n == 0) return;
                        const std::size_t code_at = out.size();
                        out.push_back(char(k << 2 | sd_detail::kVpatch));
                        const std::uint32_t blocks = sd_detail::put_blocks(
                            out, reinterpret_cast<const unsigned char*>(cur.data()), reinterpret_cast<unsigned char*>(old.data()),
                            n * sizeof(E), [](std::size_t w) { return sd_detail::elem_mask<E>(w, sd_detail::kPlain | sd_detail::kBool); }, true);
                        if (blocks == 0) out.truncate(code_at);
                        else ++nc;
                    }
                });
                out[count_at] = char(nc);
            }
            if (nb == 0 && nc == 0) { out.truncate(start); return false; }
            return true;
        }

    public:
        // Appends a body that turns a cleared record (clear()) into now.
        static void key(const T& now, XSyncOut& out) {
            sd_detail::put_blocks<sizeof(T)>(out, reinterpret_cast<const unsigned char*>(&now), nullptr, sizeof(T),
                                  [](std::size_t w) -> unsigned { return I::plain[w]; }, false);
            if constexpr (container_count > 0) {
                const std::size_t count_at = out.size();
                out.push_back(0);
                std::uint32_t nc = 0;
                sd_detail::each(typename I::containers{}, [&]<class C, std::size_t Off>(std::uint32_t k) {
                    const C& cur = sd_detail::at<C, Off>(now);
                    if constexpr (sd_detail::is_fixed_string_v<C>) {
                        if (cur.view().empty()) return;
                        put_str(out, k, cur.view());
                    } else {
                        if (cur.size() == 0) return;
                        put_vset(out, k, cur);
                    }
                    ++nc;
                });
                out[count_at] = char(nc);
            }
        }

        // Checks one body at p against the record it will apply to (dst ==
        // nullptr: a cleared record). Returns the body's end, or nullptr when it
        // is malformed or does not fit dst. Reads dst, never writes it.
        static const char* check(const char* p, const char* end, const T* dst) noexcept {
            p = sd_detail::check_blocks<I::any_bool>(p, end, sizeof(T), [](std::size_t w) -> unsigned { return I::plain[w]; },
                                        [](std::size_t w) -> unsigned { return I::bools[w]; });
            if (!p) return nullptr;
            if constexpr (container_count > 0) {
                std::uint32_t m = 0;
                if (!get_varint(p, end, m) || m > container_count) return nullptr;
                int last = -1;
                for (std::uint32_t i = 0; i < m; ++i) {
                    if (p == end) return nullptr;
                    const unsigned code = static_cast<unsigned char>(*p++);
                    const unsigned field = code >> 2, op = code & 3u;
                    if (int(field) <= last) return nullptr;
                    last = int(field);
                    const bool ok = sd_detail::visit(typename I::containers{}, field, [&]<class C, std::size_t Off>() -> bool {
                        if constexpr (sd_detail::is_fixed_string_v<C>) {
                            std::uint32_t len = 0;
                            if (op != sd_detail::kStr || !get_varint(p, end, len) || std::size_t(end - p) < len) return false;
                            p += len;
                            return true;
                        } else {
                            using E = typename C::value_type;
                            if (op == sd_detail::kVset) {
                                std::uint32_t n = 0;
                                if (!get_varint(p, end, n)) return false;
                                constexpr std::size_t P = sd_detail::Info<E>::plain_bytes;
                                if constexpr (P == 0) { if (n > (std::size_t(1) << 20)) return false; }
                                else if (std::size_t(end - p) / P < n) return false;
                                if constexpr (sd_detail::Info<E>::any_bool) {
                                    const char* q = p;
                                    for (std::uint32_t e = 0; e < n; ++e)
                                        for (const auto& r : sd_detail::Info<E>::runs)
                                            for (std::uint32_t j = 0; j < r.len; ++j, ++q)
                                                if (sd_detail::Info<E>::classes[r.at + j] == sd_detail::kBool &&
                                                    static_cast<unsigned char>(*q) > 1) return false;
                                }
                                p += std::size_t(n) * P;
                                return true;
                            }
                            if (op == sd_detail::kVpatch) {
                                const std::size_t n = dst ? sd_detail::at<C, Off>(*dst).size() : 0;
                                p = sd_detail::check_blocks<sd_detail::Info<E>::any_bool>(p, end, n * sizeof(E),
                                    [](std::size_t w) { return sd_detail::elem_mask<E>(w, sd_detail::kPlain | sd_detail::kBool); },
                                    [](std::size_t w) { return sd_detail::elem_mask<E>(w, sd_detail::kBool); });
                                return p != nullptr;
                            }
                            return false;
                        }
                    });
                    if (!ok) return nullptr;
                }
            }
            return p;
        }

        // Applies a body that check() accepted for dst. Returns its end.
        static const char* apply(const char* p, T& dst) {
            p = sd_detail::apply_blocks(p, reinterpret_cast<unsigned char*>(&dst));
            if constexpr (container_count > 0) {
                std::uint32_t m;
                get_varint(p, p + 5, m);
                for (std::uint32_t i = 0; i < m; ++i) {
                    const unsigned code = static_cast<unsigned char>(*p++);
                    sd_detail::visit(typename I::containers{}, code >> 2, [&]<class C, std::size_t Off>() -> bool {
                        C& c = sd_detail::at<C, Off>(dst);
                        if constexpr (sd_detail::is_fixed_string_v<C>) {
                            std::uint32_t len;
                            get_varint(p, p + 5, len);
                            static_cast<XFixedString&>(c).assign(std::string_view(p, len));
                            p += len;
                        } else {
                            using E = typename C::value_type;
                            if ((code & 3u) == sd_detail::kVset) {
                                std::uint32_t n;
                                get_varint(p, p + 5, n);
                                static thread_local std::vector<E> tmp;
                                tmp.resize(n);
                                if (n) std::memset(static_cast<void*>(tmp.data()), 0, std::size_t(n) * sizeof(E));
                                auto* d = reinterpret_cast<unsigned char*>(tmp.data());
                                for (std::uint32_t e = 0; e < n; ++e, d += sizeof(E))
                                    for (const auto& r : sd_detail::Info<E>::runs) { std::memcpy(d + r.at, p, r.len); p += r.len; }
                                c.assign_range(std::span<const E>(tmp.data(), n));
                            } else {
                                p = sd_detail::apply_blocks(p, reinterpret_cast<unsigned char*>(c.data()));
                            }
                        }
                        return true;
                    });
                }
            }
            return p;
        }

        // Clears a record: plain bytes zero, strings empty, vectors empty.
        static void clear(T& dst) {
            auto* d = reinterpret_cast<unsigned char*>(&dst);
            for (const auto& r : I::runs) std::memset(d + r.at, 0, r.len);
            if constexpr (container_count > 0) {
                sd_detail::each(typename I::containers{}, [&]<class C, std::size_t Off>(std::uint32_t) {
                    C& c = sd_detail::at<C, Off>(dst);
                    if constexpr (sd_detail::is_fixed_string_v<C>) { if (!c.view().empty()) c.clear(); }
                    else c.clear();
                });
            }
        }

        // Whether two records hold the same content (plain bytes and containers).
        static bool same(const T& a, const T& b) {
            const auto* x = reinterpret_cast<const unsigned char*>(&a);
            const auto* y = reinterpret_cast<const unsigned char*>(&b);
            for (const auto& r : I::runs) if (std::memcmp(x + r.at, y + r.at, r.len) != 0) return false;
            bool eq = true;
            if constexpr (container_count > 0) {
                sd_detail::each(typename I::containers{}, [&]<class C, std::size_t Off>(std::uint32_t) {
                    const C& u = sd_detail::at<C, Off>(a);
                    const C& v = sd_detail::at<C, Off>(b);
                    if constexpr (sd_detail::is_fixed_string_v<C>) { eq = eq && u.view() == v.view(); }
                    else {
                        using E = typename C::value_type;
                        if (u.size() != v.size()) { eq = false; return; }
                        for (std::size_t e = 0; e < u.size() && eq; ++e)
                            for (const auto& r : sd_detail::Info<E>::runs)
                                if (std::memcmp(reinterpret_cast<const unsigned char*>(&u.data()[e]) + r.at,
                                                reinterpret_cast<const unsigned char*>(&v.data()[e]) + r.at, r.len) != 0) { eq = false; break; }
                    }
                });
            }
            return eq;
        }

    private:
        static XSyncOut& scratch() {
            static thread_local XSyncOut s;
            return s;
        }
        static void put_str(XSyncOut& out, std::uint32_t k, std::string_view v) {
            out.push_back(char(k << 2 | sd_detail::kStr));
            put_varint(out, std::uint32_t(v.size()));
            out.append(v.data(), v.size());
        }
        template <class C> static void put_vset(XSyncOut& out, std::uint32_t k, const C& cur) {
            using E = typename C::value_type;
            out.push_back(char(k << 2 | sd_detail::kVset));
            const std::size_t n = cur.size();
            put_varint(out, std::uint32_t(n));
            const auto* s = reinterpret_cast<const unsigned char*>(cur.data());
            for (std::size_t e = 0; e < n; ++e, s += sizeof(E))
                for (const auto& r : sd_detail::Info<E>::runs) out.append(reinterpret_cast<const char*>(s + r.at), r.len);
        }
    };

    // ---- message framing (XS 1.0) ---------------------------------------------
    //   'X' 'S' u8 format(0x01) u8 flags [u64 stream LE if KEY] varint version varint count
    //   then count ops: u8 (table << 2 | kind), varint id, [body]
    //   kinds: enter 0 (body: the record from cleared), change 1 (body: the
    //   change), leave 2 (no body; keyed tables only). XSyncWriter writes the
    //   count as three varint bytes; a receiver takes any varint.
    inline constexpr std::uint8_t kFormat = 0x01;
    inline constexpr std::uint8_t kFlagKey = 0x01;
    enum XSyncKind : std::uint8_t { enter = 0, change = 1, leave = 2 };

    struct XSyncHeader {
        std::uint8_t flags = 0;
        std::uint64_t stream = 0;
        std::uint32_t version = 0;
        std::uint32_t count = 0;
    };

    inline void put_header(XSyncOut& out, const XSyncHeader& h) {
        write_into(out, 4 + 8 + 5 + 5, [&](char* d) {
            d[0] = 'X'; d[1] = 'S'; d[2] = char(kFormat); d[3] = char(h.flags);
            unsigned n = 4;
            if (h.flags & kFlagKey) { std::memcpy(d + n, &h.stream, 8); n += 8; }
            n += encode_varint(d + n, h.version);
            return n + encode_varint(d + n, h.count);
        });
    }
    inline void put_op(XSyncOut& out, unsigned table, XSyncKind kind, std::uint32_t id) {
        write_into(out, 6, [&](char* d) { d[0] = char(table << 2 | unsigned(kind)); return 1 + encode_varint(d + 1, id); });
    }
    // An op and its body (len bytes at body, already encoded): one write.
    inline void put_op(XSyncOut& out, unsigned table, XSyncKind kind, std::uint32_t id, const char* body, std::size_t len) {
        write_into(out, 6 + len, [&](char* d) {
            d[0] = char(table << 2 | unsigned(kind));
            const unsigned n = 1 + encode_varint(d + 1, id);
            std::memcpy(d + n, body, len);
            return n + len;
        });
    }
    // Builds one message in out: the header, then ops; finish() writes the op
    // count. The write cursor stays in the writer (a local of the encoding
    // function), so each op costs one capacity check and plain stores. The
    // count is a three-byte varint, so the header's length does not depend
    // on it.
    class XSyncWriter {
    public:
        XSyncWriter(XSyncOut& out, std::uint8_t flags, std::uint64_t stream, std::uint32_t version) : out_(&out) {
            out.clear();
            char* d = out.tail(4 + 8 + 5 + 3);
            d[0] = 'X'; d[1] = 'S'; d[2] = char(kFormat); d[3] = char(flags);
            std::size_t n = 4;
            if (flags & kFlagKey) { std::memcpy(d + n, &stream, 8); n += 8; }
            n += encode_varint(d + n, version);
            count_at_ = n;
            q_ = d + n + 3;
            end_ = out.data() + out.capacity();
        }
        XSyncWriter(const XSyncWriter&) = delete;
        XSyncWriter& operator=(const XSyncWriter&) = delete;

        // An op without a body (leave).
        void op(unsigned table, XSyncKind kind, std::uint32_t id) {
            char* d = need(6);
            d[0] = char(table << 2 | unsigned(kind));
            q_ = d + 1 + encode_varint(d + 1, id);
            ++count_;
        }
        // An op and its body (len bytes at body, already encoded).
        void op(unsigned table, XSyncKind kind, std::uint32_t id, const char* body, std::size_t len) {
            char* d = need(6 + len);
            d[0] = char(table << 2 | unsigned(kind));
            d += 1 + encode_varint(d + 1, id);
            std::memcpy(d, body, len);
            q_ = d + len;
            ++count_;
        }
        // A change op of a record diffed here (now against before, which it
        // syncs); nothing when they are equal. Returns whether it wrote one.
        template <class T> bool change(unsigned table, std::uint32_t id, const T& now, T& before) {
            if constexpr (XRecordDelta<T>::container_count == 0) {
                // A record of plain bytes only: its body is one block section,
                // written in place (the cursor never leaves the writer).
                char* d = need(6 + sd_detail::blocks_room(sizeof(T)));
                d[0] = char(table << 2 | unsigned(XSyncKind::change));
                std::uint32_t nb;
                char* const e = sd_detail::put_blocks_at<sizeof(T)>(
                    d + 1 + encode_varint(d + 1, id), reinterpret_cast<const unsigned char*>(&now), reinterpret_cast<unsigned char*>(&before),
                    sizeof(T), [](std::size_t w) -> unsigned { return sd_detail::Info<T>::plain[w]; }, true, nullptr, nb);
                if (!nb) return false;
                q_ = e;
                ++count_;
                return true;
            }
            char* d = need(6);
            d[0] = char(table << 2 | unsigned(XSyncKind::change));
            const std::size_t at = std::size_t(d - out_->data());
            out_->len_ = at + 1 + encode_varint(d + 1, id);
            const bool any = XRecordDelta<T>::diff(now, before, *out_);
            q_ = out_->data() + (any ? out_->len_ : at);
            end_ = out_->data() + out_->cap_;
            count_ += any;
            return any;
        }
        std::uint32_t count() const noexcept { return count_; }
        // Writes the count; out holds the message. Returns its length.
        std::size_t finish() noexcept {
            char* c = out_->data() + count_at_;
            c[0] = char(0x80 | (count_ & 0x7f));
            c[1] = char(0x80 | ((count_ >> 7) & 0x7f));
            c[2] = char((count_ >> 14) & 0x7f);
            out_->len_ = std::size_t(q_ - out_->data());
            return out_->len_;
        }

    private:
        struct Span { char* q; char* end; };
        // Room for n more bytes after used; the writer's own address never
        // leaves it, so its cursor can live in registers.
        XOFFSET_DETAIL_NOINLINE static Span grow(XSyncOut& out, std::size_t used, std::size_t n) {
            out.len_ = used;
            char* q = out.tail(n);
            return {q, out.data() + out.cap_};
        }
        char* need(std::size_t n) {
            if (std::size_t(end_ - q_) < n) {
                const Span s = grow(*out_, std::size_t(q_ - out_->data()), n);
                q_ = s.q;
                end_ = s.end;
            }
            return q_;
        }
        XSyncOut* out_;
        char* q_;
        char* end_;
        std::size_t count_at_;
        std::uint32_t count_ = 0;
    };

    inline const char* get_header(const char* p, const char* end, XSyncHeader& h) noexcept {
        if (end - p < 4 || p[0] != 'X' || p[1] != 'S' || static_cast<unsigned char>(p[2]) != kFormat) return nullptr;
        h.flags = static_cast<std::uint8_t>(p[3]);
        p += 4;
        if (h.flags & ~kFlagKey) return nullptr;
        if (h.flags & kFlagKey) {
            if (end - p < 8) return nullptr;
            std::memcpy(&h.stream, p, 8);
            p += 8;
        }
        if (!get_varint(p, end, h.version) || !get_varint(p, end, h.count)) return nullptr;
        return p;
    }

    // ---- receiving --------------------------------------------------------------

    class XSyncCursor;
    namespace sd_detail {
        struct SyncAccess;
        // One op of a message under the full check: where its body starts and
        // the handle its check found.
        struct SyncOp {
            const char* body;
            std::int64_t handle;
            std::uint32_t id;
            std::uint8_t code;
        };
    } // namespace sd_detail

    /// Receiver side of one XS stream: the stream and version of the last
    /// message applied, and the scratch the full check uses (kept, so applying
    /// messages does not allocate once it has grown). One cursor per stream,
    /// next to the records it keeps.
    class XSyncCursor {
    public:
        /// No key message applied yet, or the last message failed while it
        /// was being applied: only a key message applies.
        bool empty() const noexcept { return !keyed_; }
        std::uint64_t stream() const noexcept { return stream_; }
        std::uint32_t version() const noexcept { return version_; }
        /// Forget the state: only a key message applies next.
        void clear() noexcept { stream_ = 0; version_ = 0; keyed_ = false; }
        void release_scratch() noexcept {
            std::vector<sd_detail::SyncOp>().swap(ops_);
            std::vector<std::uint64_t>().swap(seen_);
            stamp_ = 0;
        }

    private:
        friend struct sd_detail::SyncAccess;
        std::uint64_t stream_ = 0;
        std::uint32_t version_ = 0;
        bool keyed_ = false;
        std::uint32_t stamp_ = 0;               // this message's mark in seen_
        std::vector<sd_detail::SyncOp> ops_;
        std::vector<std::uint64_t> seen_;      // (stamp << 38) | (table << 32 | id)
    };

    /// Whether msg is a key message: an XS 1.0 header with the KEY flag. A
    /// receiver empties its keyed tables before it applies one.
    inline bool is_key(std::string_view msg) noexcept {
        return msg.size() >= 4 && msg[0] == 'X' && msg[1] == 'S' && static_cast<unsigned char>(msg[2]) == kFormat &&
               (static_cast<unsigned char>(msg[3]) & kFlagKey) != 0;
    }
    inline bool is_key(std::span<const std::byte> msg) noexcept {
        return is_key(std::string_view(reinterpret_cast<const char*>(msg.data()), msg.size()));
    }

    // ---- client index -----------------------------------------------------------

    /// How XBasicSyncSlots picks a free slot for an id that enters: the one
    /// freed last (no ordering work), or the lowest (a heap; slots stay packed
    /// at the front, which keeps a per-client delta of the slots small).
    enum class XSlotReuse : std::uint8_t { last, lowest };

    /// A receiver's id -> slot index for one keyed table: the slot is where the
    /// receiver keeps the entry (an element of its own vector). Open
    /// addressing with linear probing and backward-shift erase, at most half
    /// full, so an id that enters or leaves allocates and frees nothing once
    /// the index has grown; a slot that leaves is reused before a new one is
    /// taken. Every 32-bit id is valid (an empty cell is marked by its slot).
    ///
    /// A target of XReceiver::apply_sync keeps one per keyed table:
    ///   find<K>(id)     -> slots.find(id)
    ///   enter<K>(id)    -> slots.enter(id, [&](std::int64_t s) { records.resize(s + 1); })   // s == records.size()
    ///   leave<K>(id, h) -> clear the record if the target wants that, then slots.leave(id, h)
    /// and calls clear() before it applies a key message. The calls are forced
    /// inline; a target's own find / enter / leave should be too (a large
    /// translation unit can reach the compiler's inline growth limit, past
    /// which whether they inline depends on unrelated code).
    template <XSlotReuse Reuse = XSlotReuse::last>
    class XBasicSyncSlots {
    public:
        /// The slot of id, or -1 when id is not held.
        XOFFSET_SYNC_INLINE std::int64_t find(std::uint32_t id) const noexcept {
            if (cells_.empty()) return -1;
            const Cell& c = cells_[probe(id)];
            return c.slot;   // the empty cell id would take has slot -1
        }

        /// Starts fetching the index cell id's lookup reads first (a receiver
        /// that knows a batch of ids prefetches them all, then looks them up).
        XOFFSET_SYNC_INLINE void prefetch(std::uint32_t id) const noexcept {
#if defined(__GNUC__) || defined(__clang__)
            if (!cells_.empty()) __builtin_prefetch(&cells_[home(id)]);
#else
            (void)id;
#endif
        }

        /// Gives id a slot and returns it, or -1 when id is already held (its
        /// slot unchanged). A free slot is reused first; with none,
        /// grow(new_slot) is called with the next unused slot (size() before
        /// the call; new_slot + 1 slots are given out after it), so the caller
        /// can make room for it.
        template <class Grow>
        XOFFSET_SYNC_INLINE std::int64_t enter(std::uint32_t id, Grow&& grow) {
            bool added;
            const std::int64_t s = take(id, static_cast<Grow&&>(grow), added);
            return added ? s : -1;
        }

        /// The slot of id, giving it one as enter does when it is not held
        /// (then added is true): one lookup either way.
        template <class Grow>
        XOFFSET_SYNC_INLINE std::int64_t take(std::uint32_t id, Grow&& grow, bool& added) {
            if (2 * (count_ + 1) > cells_.size()) [[unlikely]] rehash();
            Cell& c = cells_[probe(id)];
            if (c.slot >= 0) {
                added = false;
                return c.slot;
            }
            std::int32_t s;
            if (nfree_ != 0) {
                if constexpr (Reuse == XSlotReuse::lowest)
                    std::pop_heap(free_.begin(), free_.begin() + std::ptrdiff_t(nfree_), std::greater<>{});
                s = free_[--nfree_];
            } else {
                s = used_;
                grow(std::int64_t(s));   // may throw: then nothing is given out
                ++used_;
            }
            c = Cell{id, s};
            ++count_;
            added = true;
            return s;
        }

        /// Removes id, which holds slot (from find or enter); the slot becomes
        /// free. The caller clears or reuses its record as it likes.
        XOFFSET_SYNC_INLINE void leave(std::uint32_t id, std::int64_t slot) {
            erase(id);
            if (nfree_ == free_.size()) [[unlikely]] grow_free();
            free_[nfree_++] = std::int32_t(slot);
            if constexpr (Reuse == XSlotReuse::lowest)
                std::push_heap(free_.begin(), free_.begin() + std::ptrdiff_t(nfree_), std::greater<>{});
        }

        /// Holds nothing (before a key message): every slot given out so far
        /// is free, size() is unchanged and grow is only called past it again.
        /// The caller clears the records of the slots that were held.
        void clear() {
            for (Cell& c : cells_) c.slot = -1;
            if (free_.size() < std::size_t(used_)) free_.resize(std::size_t(used_));
            // The next enter takes slot 0 either way: lowest is a min-heap
            // (ascending is one), last pops from the back.
            nfree_ = std::size_t(used_);
            for (std::int32_t s = 0; s < used_; ++s)
                free_[std::size_t(s)] = Reuse == XSlotReuse::lowest ? s : used_ - 1 - s;
            count_ = 0;
        }

        /// Room for n ids held and n free slots without growing (a client
        /// that knows its view's size calls this before the first message).
        void reserve(std::size_t n) {
            while (cells_.size() < 2 * n) rehash();
            if (free_.size() < n) free_.resize(n);
        }

#ifdef SLG_XS2_DIAG
        // Diagnostic build only: evict every line of the index (clflush).
        void diag_flush() const noexcept {
            for (std::size_t i = 0; i < cells_.size() * sizeof(Cell); i += 64) __builtin_ia32_clflush(reinterpret_cast<const char*>(cells_.data()) + i);
            for (std::size_t i = 0; i < free_.size() * sizeof(std::int32_t); i += 64) __builtin_ia32_clflush(reinterpret_cast<const char*>(free_.data()) + i);
            __builtin_ia32_mfence();
        }
        // Diagnostic build only: read every cell and free-list entry (cache warm-up).
        std::uint64_t diag_touch(std::size_t stride) const noexcept {
            std::uint64_t h = 0;
            const auto* c = reinterpret_cast<const volatile unsigned char*>(cells_.data());
            for (std::size_t i = 0; i < cells_.size() * sizeof(Cell); i += stride) h += c[i];
            const auto* f = reinterpret_cast<const volatile unsigned char*>(free_.data());
            for (std::size_t i = 0; i < free_.size() * sizeof(std::int32_t); i += stride) h += f[i];
            return h + cells_.size() + free_.size();
        }
#endif
        /// Slots given out so far (held or free): the size the caller's records need.
        std::size_t size() const noexcept { return std::size_t(used_); }
        /// Ids held.
        std::size_t count() const noexcept { return count_; }
        /// Bytes the index keeps (cells and free list).
        std::size_t capacity_bytes() const noexcept {
            return cells_.capacity() * sizeof(Cell) + free_.capacity() * sizeof(std::int32_t);
        }

    private:
        struct Cell {
            std::uint32_t id;
            std::int32_t slot;   // < 0: empty
        };
        std::vector<Cell> cells_;
        // Free slots: the first nfree_ entries (a min-heap for lowest). The
        // vector's size is its capacity: a leave stores into it, no push_back
        // (which a large translation unit may not inline).
        std::vector<std::int32_t> free_;
        std::size_t nfree_ = 0;
        std::size_t count_ = 0;
        std::int32_t used_ = 0;
        unsigned shift_ = 32;

        XOFFSET_SYNC_INLINE std::size_t home(std::uint32_t id) const noexcept { return std::size_t((id * 0x9E3779B1u) >> shift_); }
        XOFFSET_SYNC_INLINE std::size_t mask() const noexcept { return cells_.size() - 1; }
        // The cell of id, or the empty cell where it would go.
        XOFFSET_SYNC_INLINE std::size_t probe(std::uint32_t id) const noexcept {
            std::size_t i = home(id);
            while (cells_[i].id != id && cells_[i].slot >= 0) i = (i + 1) & mask();
            // A stale id in an empty cell may equal id: then the cell is the empty one.
            return i;
        }
        // Doubles the free list's room, as push_back grows a vector (1, 2, 4, ...).
        XOFFSET_DETAIL_NOINLINE void grow_free() {
            std::vector<std::int32_t> next(free_.empty() ? 1 : free_.size() * 2);
            std::copy(free_.begin(), free_.end(), next.begin());
            free_.swap(next);
        }
        XOFFSET_DETAIL_NOINLINE void rehash() {
            std::vector<Cell> old(cells_.empty() ? 32 : cells_.size() * 2, Cell{0, -1});
            old.swap(cells_);
            shift_ = 32;
            for (std::size_t n = cells_.size(); n > 1; n >>= 1) --shift_;
            for (const Cell& c : old)
                if (c.slot >= 0) cells_[probe(c.id)] = c;
        }
        // Removes id (held): later cells of its run move back, so no tombstones are left.
        XOFFSET_SYNC_INLINE void erase(std::uint32_t id) noexcept {
            std::size_t i = probe(id);
            for (std::size_t j = (i + 1) & mask(); cells_[j].slot >= 0; j = (j + 1) & mask()) {
                const std::size_t h = home(cells_[j].id);
                // cells_[j] may fill the hole at i unless its home lies in (i, j].
                if (((j - h) & mask()) >= ((j - i) & mask())) {
                    cells_[i] = cells_[j];
                    i = j;
                }
            }
            cells_[i].slot = -1;
            --count_;
        }
    };

    /// The index a client of XReceiver::apply_sync keeps per keyed table.
    using XSyncSlots = XBasicSyncSlots<XSlotReuse::last>;

    namespace sd_detail {
        template <class Target, unsigned K>
        using sync_record_t = std::tuple_element_t<K, typename Target::tables>;

        template <class Target> inline constexpr unsigned sync_tables = unsigned(std::tuple_size_v<typename Target::tables>);

        inline constexpr const char* kUnknownTable = "shared delta: unknown table";
        inline constexpr const char* kNotHeld = "shared delta: an op on an entry the receiver does not hold";
        inline constexpr const char* kHeld = "shared delta: enter of an entry the receiver already holds";
        inline constexpr const char* kBadKind = "shared delta: invalid op kind for this table";
        inline constexpr const char* kBadBody = "shared delta: a record body does not check";
        inline constexpr const char* kRefused = "shared delta: the receiver could not add an entry";

        // f.template operator()<K>() for K == table (one jump; at most 64 tables).
        template <unsigned N, class F>
        XOFFSET_SYNC_INLINE const char* sync_switch(unsigned table, F&& f) {
            static_assert(N >= 1 && N <= 64, "XReceiver::apply_sync: a target has 1 to 64 tables");
            switch (table) {
#define XOFFSET_SYNC_CASE(k) case k: if constexpr ((k) < N) return f.template operator()<(k)>(); else break;
#define XOFFSET_SYNC_CASE8(k) XOFFSET_SYNC_CASE(k) XOFFSET_SYNC_CASE(k + 1) XOFFSET_SYNC_CASE(k + 2) XOFFSET_SYNC_CASE(k + 3) \
                              XOFFSET_SYNC_CASE(k + 4) XOFFSET_SYNC_CASE(k + 5) XOFFSET_SYNC_CASE(k + 6) XOFFSET_SYNC_CASE(k + 7)
            XOFFSET_SYNC_CASE8(0) XOFFSET_SYNC_CASE8(8) XOFFSET_SYNC_CASE8(16) XOFFSET_SYNC_CASE8(24)
            XOFFSET_SYNC_CASE8(32) XOFFSET_SYNC_CASE8(40) XOFFSET_SYNC_CASE8(48) XOFFSET_SYNC_CASE8(56)
#undef XOFFSET_SYNC_CASE8
#undef XOFFSET_SYNC_CASE
            default: break;
            }
            return kUnknownTable;
        }

        template <unsigned K, class Target>
        XOFFSET_SYNC_INLINE void sync_applied(Target& t, std::uint32_t id, std::int64_t h) {
            if constexpr (requires { t.template applied<K>(id, h); }) t.template applied<K>(id, h);
        }

        // XRecordDelta<R>::apply, with the block section inline for records
        // without containers.
        template <class R>
        XOFFSET_SYNC_INLINE const char* sync_apply(const char* p, R& r) {
            if constexpr (XRecordDelta<R>::container_count == 0) return apply_blocks(p, reinterpret_cast<unsigned char*>(&r));
            else return XRecordDelta<R>::apply(p, r);
        }

        // XRecordDelta<R>::clear, inline for records without containers.
        template <class R>
        XOFFSET_SYNC_INLINE void sync_clear(R& r) {
            if constexpr (XRecordDelta<R>::container_count == 0) {
                using I = Info<R>;
                auto* d = reinterpret_cast<unsigned char*>(&r);
                [&]<std::size_t... N>(std::index_sequence<N...>) XOFFSET_SYNC_LAMBDA_INLINE {
                    (std::memset(d + I::runs[N].at, 0, I::runs[N].len), ...);
                }(std::make_index_sequence<I::run_count>{});
            } else XRecordDelta<R>::clear(r);
        }

        // Where a body ends, or why the op is refused.
        struct SyncStep {
            const char* p;
            const char* err;
        };

        struct SyncAccess {
            using vstatus = ::XOffsetDatastructure::detail::vstatus;

            XOFFSET_SYNC_COLD static vstatus fail(XErrc code, const char* why) {
                XOFFSET_VFAIL(XException, code, why);
            }

            // The header, and the stream and version rules against cur: where
            // the ops start, or nullptr (header_error() says why).
            XOFFSET_SYNC_INLINE static const char* header(const char* p, const char* end, const XSyncCursor& cur,
                                                          XSyncHeader& h) noexcept {
                if (end - p < 6 || p[0] != 'X' || p[1] != 'S' || static_cast<unsigned char>(p[2]) != kFormat) return nullptr;
                h.flags = static_cast<std::uint8_t>(p[3]);
                p += 4;
                if (h.flags != 0) {
                    if (h.flags != kFlagKey || end - p < 8) return nullptr;
                    std::memcpy(&h.stream, p, 8);
                    p += 8;
                }
                if (!get_varint(p, end, h.version)) return nullptr;
                // The count: three varint bytes as XSyncWriter writes it, or any varint.
                if (end - p >= 3 && (static_cast<unsigned char>(p[0]) & static_cast<unsigned char>(p[1]) & 0x80) &&
                    !(static_cast<unsigned char>(p[2]) & 0x80)) {
                    h.count = (static_cast<unsigned char>(p[0]) & 0x7fu) | (static_cast<unsigned char>(p[1]) & 0x7fu) << 7 |
                              std::uint32_t(static_cast<unsigned char>(p[2])) << 14;
                    p += 3;
                } else if (!get_varint(p, end, h.count)) return nullptr;
                if (h.flags == 0 && (!cur.keyed_ || h.version != std::uint32_t(cur.version_ + 1))) return nullptr;
                // Every op takes at least two bytes.
                if (h.count > std::size_t(end - p) / 2) return nullptr;
                return p;
            }
            XOFFSET_SYNC_COLD static vstatus header_error(const char* p, const char* end, const XSyncCursor& cur) {
                const std::size_t n = std::size_t(end - p);
                if ((n >= 1 && p[0] != 'X') || (n >= 2 && p[1] != 'S'))
                    return fail(XErrc::bad_magic, "shared delta: not an XS message");
                if (n < 4) return fail(XErrc::truncated, "shared delta: message shorter than its header");
                if (static_cast<unsigned char>(p[2]) != kFormat)
                    return fail(XErrc::unsupported_version, "shared delta: unsupported XS format (this receiver reads XS 1.0)");
                const auto flags = static_cast<std::uint8_t>(p[3]);
                if (flags & ~kFlagKey) return fail(XErrc::unsupported_flags, "shared delta: unknown flags");
                p += 4;
                if (flags & kFlagKey) {
                    if (end - p < 8) return fail(XErrc::truncated, "shared delta: message shorter than its header");
                    p += 8;
                }
                std::uint32_t version, count;
                if (!get_varint(p, end, version) || !get_varint(p, end, count))
                    return fail(p == end ? XErrc::truncated : XErrc::invalid_payload, "shared delta: bad header varint");
                if (!(flags & kFlagKey)) {
                    if (!cur.keyed_) return fail(XErrc::delta_base_mismatch, "shared delta: no key message applied (ask for one)");
                    if (version != std::uint32_t(cur.version_ + 1))
                        return fail(XErrc::delta_base_mismatch, "shared delta: version gap (ask for a key message)");
                }
                return fail(XErrc::truncated, "shared delta: fewer bytes than ops");
            }

            static void done(XSyncCursor& cur, const XSyncHeader& h) noexcept {
                if (h.flags & kFlagKey) cur.stream_ = h.stream;
                cur.version_ = h.version;
                cur.keyed_ = true;
            }

            // Applies one op of a kind table K takes. h: the record's handle
            // (find's result; ignored for a keyed enter, which asks enter()).
            // An enter clears the record before its body applies; a leave
            // hands the entry back to the target.
            template <unsigned K, class Target>
            XOFFSET_SYNC_INLINE static SyncStep apply_op(Target& t, unsigned kind, std::uint32_t id, std::int64_t h,
                                                         const char* p) {
                using R = sync_record_t<Target, K>;
                if constexpr (Target::template keyed<K>) {
                    if (kind == XSyncKind::enter) {
                        h = t.template enter<K>(id);
                        if (h < 0) [[unlikely]] return {p, kRefused};
                    } else if (h < 0) [[unlikely]] return {p, kNotHeld};
                    else if (kind == XSyncKind::leave) {
                        t.template leave<K>(id, h);
                        return {p, nullptr};
                    }
                } else if (h < 0) [[unlikely]] return {p, kNotHeld};
                R& r = t.template record<K>(h);
                if (kind == XSyncKind::enter) sync_clear(r);
                p = sync_apply(p, r);
                sync_applied<K>(t, id, h);
                return {p, nullptr};
            }
            // The last kind table K takes: leave for keyed tables, change for fixed ones.
            template <unsigned K, class Target>
            static constexpr unsigned last_kind = Target::template keyed<K> ? unsigned(XSyncKind::leave) : unsigned(XSyncKind::change);

            // XCheck::admit: the header and the stream rules, then every op
            // in one pass. Ids are looked up (an op on an entry the receiver
            // does not hold is rejected) and tables and kinds checked; bodies
            // are the sender's. A message rejected after its header may be
            // partly applied: the cursor is then empty (ask for a key).
            template <class Target>
            static vstatus admit(const char* const begin, const char* const end, XSyncCursor& cur, Target& t) {
                XSyncHeader h;
                const char* p = header(begin, end, cur, h);
                if (!p) [[unlikely]] return header_error(begin, end, cur);
                cur.keyed_ = false;
                // The header bounds the first op; later ones start where the
                // sender's bodies end (bodies are not checked here).
                for (std::uint32_t n = h.count; n != 0; --n) {
                    std::uint32_t id;
                    const unsigned code = static_cast<unsigned char>(*p++);
                    if (!get_varint(p, end, id)) [[unlikely]] return fail(XErrc::truncated, "shared delta: message ends inside an op");
                    const char* e = sync_switch<sync_tables<Target>>(code >> 2, [&]<unsigned K>() XOFFSET_SYNC_LAMBDA_INLINE -> const char* {
                        const unsigned kind = code & 3u;
                        if (kind > last_kind<K, Target>) [[unlikely]] return kBadKind;
                        const std::int64_t h = Target::template keyed<K> && kind == XSyncKind::enter ? std::int64_t(-1)
                                                                                                    : t.template find<K>(id);
                        const SyncStep s = apply_op<K>(t, kind, id, h, p);
                        p = s.p;
                        return s.err;
                    });
                    if (e) [[unlikely]] return fail(XErrc::invalid_payload, e);
                }
                if (p != end) [[unlikely]] return fail(XErrc::invalid_payload, "shared delta: bytes after the last op");
                done(cur, h);
                return ::XOffsetDatastructure::detail::vok();
            }

            // Checks one enter or leave op (change takes the inline path).
            template <unsigned K, class Target>
            XOFFSET_SYNC_INLINE static SyncStep check_other(Target& t, unsigned kind, std::uint32_t id, const char* p,
                                                                const char* end, std::int64_t& handle) {
                using R = sync_record_t<Target, K>;
                if (kind > XSyncKind::leave || (!Target::template keyed<K> && kind == XSyncKind::leave)) return {p, kBadKind};
                handle = t.template find<K>(id);
                if (kind == XSyncKind::leave) return {p, handle < 0 ? kNotHeld : nullptr};
                if constexpr (Target::template keyed<K>) {
                    if (handle >= 0) return {p, kHeld};
                } else if (handle < 0) return {p, kNotHeld};
                p = XRecordDelta<R>::check(p, end, nullptr);
                return {p, p ? nullptr : kBadBody};
            }

            // XCheck::full: every op is checked against the records as they
            // are (an enter against a cleared record) before any applies; an
            // entry appears at most once per message. A rejected message
            // changes nothing. A failure while the checked ops apply (the
            // receiver could not add an entry, out of storage) leaves the
            // ops before it applied and the cursor empty.
            template <class Target>
            static vstatus full(const char* const begin, const char* const end, XSyncCursor& cur, Target& t) {
                XSyncHeader h;
                const char* p = header(begin, end, cur, h);
                if (!p) [[unlikely]] return header_error(begin, end, cur);
                if (cur.ops_.size() < h.count) cur.ops_.resize(h.count);
                SyncOp* const ops = cur.ops_.data();
                // Entries seen in this message. Up to 64 ops: a 256-bit filter
                // on the stack, and an op whose bit is set is compared with the
                // ops before it. More: open addressing over (table, id) in the
                // cursor, marked with the message's stamp (no clearing per
                // message).
                const bool small = h.count <= 64;
                std::uint64_t filter[4] = {0, 0, 0, 0};
                std::size_t slots = 0;
                std::uint64_t* seen = nullptr;
                std::uint64_t mark = 0;
                unsigned shift = 0;
                if (!small) {
                    slots = 16;
                    while (slots < std::size_t(h.count) * 2) slots *= 2;
                    if (cur.seen_.size() < slots) cur.seen_.assign(slots, 0);
                    if (++cur.stamp_ == (1u << 26)) {
                        cur.stamp_ = 1;
                        std::fill(cur.seen_.begin(), cur.seen_.end(), std::uint64_t(0));
                    }
                    seen = cur.seen_.data();
                    mark = std::uint64_t(cur.stamp_) << 38;
                    shift = unsigned(64 - std::countr_zero(slots));
                }
                for (std::uint32_t n = 0; n < h.count; ++n) {
                    std::uint32_t id;
                    if (p >= end) [[unlikely]] return fail(XErrc::truncated, "shared delta: message ends inside an op");
                    const unsigned code = static_cast<unsigned char>(*p++);
                    if (!get_varint(p, end, id)) [[unlikely]] return fail(XErrc::truncated, "shared delta: message ends inside an op");
                    const std::uint64_t key = (std::uint64_t(code >> 2) << 32) | id;
                    if (small) {
                        const unsigned bit = unsigned((key * 0x9E3779B97F4A7C15ull) >> 56);
                        const std::uint64_t m = std::uint64_t(1) << (bit & 63u);
                        if (filter[bit >> 6] & m) [[unlikely]] {
                            for (std::uint32_t k = 0; k < n; ++k)
                                if ((ops[k].code >> 2) == (code >> 2) && ops[k].id == id)
                                    return fail(XErrc::invalid_payload, "shared delta: an entry appears twice in one message");
                        }
                        filter[bit >> 6] |= m;
                    } else {
                        for (std::size_t s = std::size_t((key * 0x9E3779B97F4A7C15ull) >> shift);; s = (s + 1) & (slots - 1)) {
                            if ((seen[s] & ~((std::uint64_t(1) << 38) - 1)) != mark) { seen[s] = mark | key; break; }
                            if (seen[s] == (mark | key)) [[unlikely]]
                                return fail(XErrc::invalid_payload, "shared delta: an entry appears twice in one message");
                        }
                    }
                    const char* const body = p;
                    std::int64_t handle = -1;
                    const char* e = sync_switch<sync_tables<Target>>(code >> 2, [&]<unsigned K>() XOFFSET_SYNC_LAMBDA_INLINE -> const char* {
                        if ((code & 3u) == XSyncKind::change) [[likely]] {
                            handle = t.template find<K>(id);
                            if (handle < 0) [[unlikely]] return kNotHeld;
                            p = XRecordDelta<sync_record_t<Target, K>>::check(p, end, &t.template record<K>(handle));
                            return p ? nullptr : kBadBody;
                        }
                        const SyncStep s = check_other<K>(t, code & 3u, id, p, end, handle);
                        p = s.p;
                        return s.err;
                    });
                    if (e) [[unlikely]] return fail(XErrc::invalid_payload, e);
                    ops[n] = SyncOp{body, handle, id, std::uint8_t(code)};
                }
                if (p != end) [[unlikely]] return fail(XErrc::invalid_payload, "shared delta: bytes after the last op");
                cur.keyed_ = false;
                for (const SyncOp* o = ops; o != ops + h.count; ++o) {
                    const char* e = sync_switch<sync_tables<Target>>(unsigned(o->code >> 2), [&]<unsigned K>() XOFFSET_SYNC_LAMBDA_INLINE -> const char* {
                        return apply_op<K>(t, o->code & 3u, o->id, o->handle, o->body).err;
                    });
                    if (e) [[unlikely]] return fail(XErrc::invalid_payload, e);
                }
                done(cur, h);
                return ::XOffsetDatastructure::detail::vok();
            }
        };
    } // namespace sd_detail

} // namespace XOffsetDatastructure::xsync

#endif // X_OFFSET_SHARED_DELTA_HPP
