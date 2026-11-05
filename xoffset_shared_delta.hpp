#ifndef X_OFFSET_SHARED_DELTA_HPP
#define X_OFFSET_SHARED_DELTA_HPP

// EXPERIMENTAL (D3 prototype, 2026-11-04): record-relative shared deltas.
// Not part of the public API; names, format and behaviour may change or go.
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
// Supported in the prototype: scalars, bools, fixed-underlying enums, nested
// ordinary records and arrays of them, XString, and XVector of elements
// without containers. Sets, maps and vectors of records with containers are
// rejected at compile time.
//
// Encoding writes into an XSyncOut (storage that only grows, never zero
// filled); a server builds each message with an XSyncWriter, whose cursor
// stays in registers: one capacity check per op.

#include "xoffsetdatastructure.hpp"

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <new>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

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
    inline bool get_varint(const char*& p, const char* end, std::uint32_t& v) noexcept {
        if (p != end && !(static_cast<unsigned char>(*p) & 0x80)) { v = static_cast<unsigned char>(*p++); return true; }
        std::uint32_t r = 0;
        for (unsigned s = 0; s < 35; s += 7) {
            if (p == end) return false;
            const auto b = static_cast<unsigned char>(*p++);
            if (s == 28 && b > 0x0f) return false;
            r |= std::uint32_t(b & 0x7f) << s;
            if (!(b & 0x80)) { v = r; return true; }
        }
        return false;
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
                              "xsync prototype: enums need a fixed, non-bool underlying type");
                for (std::size_t i = 0; i < sizeof(U); ++i) out[base + i] = kPlain;
            } else if constexpr (is_container<U>()) {
                // a head: written only through container operations
            } else if constexpr (std::is_array_v<U>) {
                using E = std::remove_extent_t<U>;
                for (std::size_t i = 0; i < std::extent_v<U>; ++i) classify<E>(out, base + i * sizeof(E));
            } else if constexpr (std::is_class_v<U>) {
                static_assert(schema::is_described_v<U> && schema::base_count_v<U> == 0,
                              "xsync prototype: records must be described and have no bases");
                [&]<std::size_t... I>(std::index_sequence<I...>) {
                    (classify<typename schema::member_t<U, I>::type>(out, base + schema::member_t<U, I>::offset), ...);
                }(std::make_index_sequence<schema::member_count_v<U>>{});
            } else {
                static_assert(always_false<U>, "xsync prototype: unsupported field type");
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
            static_assert(container_count < 64, "xsync prototype: at most 63 containers per record");
            static_assert((size + 63) / 64 < 128, "xsync prototype: records up to 8 KiB");
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
            x |= x >> 4; x |= x >> 2; x |= x >> 1;
            x &= 0x0101010101010101ull;
            return unsigned((x * 0x0102040810204080ull) >> 56);
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
        template <std::size_t N = 0, class Plain>
        std::uint32_t put_blocks(XSyncOut& out, const unsigned char* cur, unsigned char* old, std::size_t n,
                                 Plain&& plain, bool sync) {
            if constexpr (N != 0) n = N;
            constexpr bool whole = N != 0 && N % 8 == 0;
            const std::size_t blocks = (n + 63) / 64;
            char* const d = out.tail(1 + blocks * (5 + 1 + 8 + 64));
            char* q = d + 1;
            std::uint32_t count = 0;
            std::size_t next = 0;
            for (std::size_t b = 0; b < blocks; ++b) {
                unsigned masks[8];
                unsigned sel = 0;
                const std::size_t words = n - b * 64 >= 64 ? 8 : (n - b * 64 + 7) / 8;
                for (unsigned w = 0; w < words; ++w) {
                    const std::size_t a = b * 64 + w * 8;
                    const std::uint64_t x = word_at<N>(cur, a, n) ^ (old ? word_at<N>(old, a, n) : 0);
                    if (!x) continue;
                    const unsigned m = byte_mask(x) & plain(a / 8);
                    masks[w] = m;
                    sel |= unsigned(m != 0) << w;
                }
                if (!sel) continue;
                // One block: varint skip, selector, masks, data.
                q += encode_varint(q, std::uint32_t(b - next));
                *q++ = char(sel);
                for (unsigned s = sel; s; s &= s - 1) *q++ = char(masks[std::countr_zero(s)]);
                for (unsigned s = sel; s; s &= s - 1) {
                    const unsigned w = unsigned(std::countr_zero(s));
                    const std::size_t a = b * 64 + w * 8;
                    const unsigned m = masks[w];
                    const std::uint64_t c = word_at<N>(cur, a, n);
                    if (m == 0xffu) {   // a whole word
                        std::memcpy(q, &c, 8);
                        q += 8;
                        if (sync) std::memcpy(old + a, &c, 8);
                        continue;
                    }
                    for (unsigned r = m; r; r &= r - 1) *q++ = char(c >> (8 * std::countr_zero(r)));
                    if (sync) {   // the sent bytes into old, the others kept
                        const std::uint64_t keep = kSpread[m];
                        const std::uint64_t o = (word_at<N>(old, a, n) & ~keep) | (c & keep);
                        std::memcpy(old + a, &o, whole || n - a >= 8 ? 8 : n - a);
                    }
                }
                next = b + 1;
                ++count;
            }
            d[0] = char(count);   // < 128 blocks: one varint byte
            out.commit(std::size_t(q - d));
            return count;
        }

        // Bits set in an 8-bit value (a table: std::popcount is a library call on
        // baseline x86-64 builds).
        inline constexpr auto kPop8 = [] {
            std::array<std::uint8_t, 256> t{};
            for (unsigned i = 0; i < 256; ++i) t[i] = std::uint8_t(std::popcount(i));
            return t;
        }();
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
        inline const char* apply_blocks(const char* p, unsigned char* dst) noexcept {
            std::uint32_t count;   // the varints were checked: p + 5 bounds nothing
            get_varint(p, p + 5, count);
            std::size_t next = 0;
            for (std::uint32_t e = 0; e < count; ++e) {
                std::uint32_t skip;
                get_varint(p, p + 5, skip);
                const std::size_t b = next + skip;
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
        template <class E> unsigned elem_mask(std::size_t w, std::uint8_t which) noexcept {
            constexpr std::size_t S = sizeof(E);
            unsigned m = 0;
            for (unsigned j = 0; j < 8; ++j)
                if (Info<E>::classes[(w * 8 + j) % S] & which) m |= 1u << j;
            return m;
        }
    } // namespace sd_detail

    // Record codec for T (static functions).
    template <class T>
    struct XRecordDelta {
        using I = sd_detail::Info<T>;
        static_assert(sd_detail::all_supported(typename I::containers{}),
                      "xsync prototype: containers must be strings or vectors of elements without containers");
        static constexpr std::size_t container_count = I::container_count;

        // Appends the change from before to now and copies it into before.
        // Appends nothing and returns false when nothing changed.
        static bool diff(const T& now, T& before, XSyncOut& out) {
            const std::size_t start = out.size();
            const std::uint32_t nb = sd_detail::put_blocks<sizeof(T)>(
                out, reinterpret_cast<const unsigned char*>(&now), reinterpret_cast<unsigned char*>(&before), sizeof(T),
                [](std::size_t w) -> unsigned { return I::plain[w]; }, true);
            std::uint32_t nc = 0;
            if constexpr (container_count > 0) {
                const std::size_t count_at = out.size();
                out.push_back(0);
                sd_detail::each(typename I::containers{}, [&]<class C, std::size_t Off>(std::uint32_t k) {
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
                std::uint32_t m;
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
                            std::uint32_t len;
                            if (op != sd_detail::kStr || !get_varint(p, end, len) || std::size_t(end - p) < len) return false;
                            p += len;
                            return true;
                        } else {
                            using E = typename C::value_type;
                            if (op == sd_detail::kVset) {
                                std::uint32_t n;
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

    // ---- message framing (XS 0.1) ---------------------------------------------
    //   'X' 'S' u8 format(0x01) u8 flags [u64 stream if KEY] varint version varint count
    //   then count ops: u8 (table << 2 | kind), varint id, [body]
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

} // namespace XOffsetDatastructure::xsync

#endif // X_OFFSET_SHARED_DELTA_HPP
