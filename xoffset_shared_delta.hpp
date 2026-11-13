#ifndef X_OFFSET_SHARED_DELTA_HPP
#define X_OFFSET_SHARED_DELTA_HPP

// Shared deltas: record bodies a server writes once per entity and tick and
// copies into every message whose connection holds the entity
// (docs/SHARED_DELTA.md). The message format is XS 2.0 (format byte 0x02;
// tests/test_shared_delta_format.cpp pins its bytes). A client applies a
// message with XReceiver<XLink::from_server>::apply_sync
// (xoffset_receiver.hpp); this header has the sender side, the record codec
// and the receive engine apply_sync runs.
//
// A delta of xoffset_delta.hpp carries bytes at payload offsets, so it only
// applies to a replica with the sender's layout: one baseline per receiver.
// Here a body is written per record, relative to the record, and applies to
// a copy of that record held anywhere (another buffer, another slot). The
// receiver keeps its own layout.
//
// Message (little endian):
//   'X' 'S' u8 format (0x02) u8 flags u32 version          8 bytes
//   u64 stream                                             KEY flag only
//   root                                                   table 0's whole body
//   sections, until the end of the message:
//     u32 (count << 8 | table << 2 | kind), then count ops of that table and kind
//   op:
//     leave   u32 id
//     enter   u32 id, whole body
//     change  u32 id, change body
// Table 0 is the stream's root record: a record without containers, carried
// whole by every message and named by no op. Tables 1 to 63 hold entries:
// keyed tables (entries enter, change and leave) and fixed ones (enter and
// change only). The id is what the receiver's find() takes: an entity id, or
// the slot the server gave this receiver for the entity (then the receiver
// needs no index; XSyncWriter::op writes a shared body under another id).
//
// Whole body of R: R's plain bytes (scalars, enums, bools; padding and
// container heads skipped), then, when R has containers, a container mask
// (one bit per container, schema order, nested records walked in place; u8,
// u16, u32 or u64 by count) and for each bit set: a string as u32 length and
// bytes, a vector as u32 count and count * sizeof(E) element bytes. A
// container whose bit is clear is empty: a whole body sets every plain byte
// and every container, so an enter needs no clear first.
// Change body of R: a member mask (one bit per member of R, declaration
// order; u8, u16, u32 or u64 by count), then the plain members whose bit is
// set, in order, each as its sizeof bytes, then the members with containers
// whose bit is set, in order: a string or vector as above, any other member
// (a nested record or array holding containers) as its own whole body.
//
// Supported: scalars, bools, enums with a fixed underlying type, nested
// records and arrays of them, XString, and XVector of elements without
// containers. Sets, maps and vectors of records with containers are rejected
// at compile time.
//
// A body source (an XSyncOut the server keeps its bodies in) and every
// message under construction keep kSyncSlack bytes past their end, so a short
// body is copied as two 16-byte moves.

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

    // ---- format -----------------------------------------------------------------

    inline constexpr std::uint8_t kFormat = 0x02;
    inline constexpr std::uint8_t kFlagKey = 0x01;
    /// Bytes of the head before the stream (or the root, without the KEY flag).
    inline constexpr std::size_t kSyncHead = 8;
    /// Bytes kept readable past a body source's end and writable past a
    /// message's end.
    inline constexpr std::size_t kSyncSlack = 32;
    /// Ops in one section at most (its count has 24 bits).
    inline constexpr std::uint32_t kSyncSectionMax = (std::uint32_t(1) << 24) - 1;
    /// Tables 0 (the root) to 63.
    inline constexpr unsigned kSyncTables = 64;
    enum XSyncKind : std::uint8_t { enter = 0, change = 1, leave = 2 };

    namespace sd_detail {
        template <class M> XOFFSET_SYNC_INLINE M load(const char* p) noexcept {
            M v;
            std::memcpy(&v, p, sizeof v);
            return v;
        }
        template <class M> XOFFSET_SYNC_INLINE void store(char* p, M v) noexcept { std::memcpy(p, &v, sizeof v); }

        // n bytes from s to d. Up to 32 bytes it moves 32: d keeps kSyncSlack
        // writable and s kSyncSlack readable past n.
        XOFFSET_SYNC_INLINE void copy_body(char* d, const char* s, std::size_t n) noexcept {
            if (n <= 32) {
                std::memcpy(d, s, 16);
                std::memcpy(d + 16, s + 16, 16);
            } else std::memcpy(d, s, n);
        }

        // The mask type of n bits (n <= 64).
        template <std::size_t N>
        using mask_t = std::conditional_t<(N <= 8), std::uint8_t,
                       std::conditional_t<(N <= 16), std::uint16_t,
                       std::conditional_t<(N <= 32), std::uint32_t, std::uint64_t>>>;
        template <std::size_t N>
        inline constexpr std::uint64_t low_bits = N >= 64 ? ~std::uint64_t(0) : (std::uint64_t(1) << N) - 1;
    } // namespace sd_detail

    // ---- output buffer ---------------------------------------------------------

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

    namespace sd_detail {
        using XOffsetDatastructure::detail::fixed_size_scalar;
        using XOffsetDatastructure::detail::is_fixed_flat_map_v;
        using XOffsetDatastructure::detail::is_fixed_flat_set_v;
        using XOffsetDatastructure::detail::is_fixed_string_v;
        using XOffsetDatastructure::detail::is_fixed_vector_v;

        enum : std::uint8_t { kNone = 0, kPlain = 1, kBool = 2 };

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

        template <class C> constexpr bool supported_container() {
            if constexpr (is_fixed_flat_set_v<C> || is_fixed_flat_map_v<C>) return false;
            else if constexpr (is_fixed_string_v<C>) return true;
            else {
                using E = typename C::value_type;
                return std::is_trivially_copyable_v<E> && ContainersOf<E, 0>::type::size == 0;
            }
        }
        template <class S> constexpr bool slot_supported() { return supported_container<typename S::type>(); }
        template <class... S> constexpr bool all_supported(List<S...>) { return (slot_supported<S>() && ...); }

        // T's byte classes, its plain runs and its containers (T a record or an array).
        template <class T> struct Info {
            static constexpr std::size_t size = sizeof(T);
            static constexpr std::array<std::uint8_t, size> classes = [] {
                std::array<std::uint8_t, size> a{};
                classify<T>(a.data(), 0);
                return a;
            }();
            // Runs of plain bytes (plain and bool), in order.
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
            // Bool bytes: their offsets in T, and in the runs laid end to end (a whole body).
            static constexpr std::size_t bool_count = [] { std::size_t n = 0; for (auto c : classes) n += c == kBool; return n; }();
            static constexpr std::array<std::uint32_t, bool_count> bools = [] {
                std::array<std::uint32_t, bool_count> a{};
                std::size_t n = 0;
                for (std::size_t i = 0; i < size; ++i) if (classes[i] == kBool) a[n++] = std::uint32_t(i);
                return a;
            }();
            static constexpr std::array<std::uint32_t, bool_count> packed_bools = [] {
                std::array<std::uint32_t, bool_count> a{};
                std::size_t n = 0, at = 0;
                for (std::size_t i = 0; i < size; ++i) {
                    if (classes[i] == kBool) a[n++] = std::uint32_t(at);
                    at += classes[i] != 0;
                }
                return a;
            }();
            using containers = typename ContainersOf<T, 0>::type;
            static constexpr std::size_t container_count = containers::size;
            static_assert(container_count <= 64, "xsync: at most 64 containers per record");
        };

        // Whether the bool bytes at the given offsets of p are 0 or 1.
        template <std::size_t N>
        XOFFSET_SYNC_INLINE bool bools_ok(const char* p, const std::array<std::uint32_t, N>& at) noexcept {
            unsigned bad = 0;
            for (std::size_t i = 0; i < N; ++i) bad |= static_cast<unsigned char>(p[at[i]]);
            return bad <= 1;
        }

        template <class C, std::size_t Off, class T> C& at(T& rec) noexcept {
            return *std::launder(reinterpret_cast<C*>(reinterpret_cast<unsigned char*>(&rec) + Off));
        }
        template <class C, std::size_t Off, class T> const C& at(const T& rec) noexcept {
            return *std::launder(reinterpret_cast<const C*>(reinterpret_cast<const unsigned char*>(&rec) + Off));
        }

        // f.template operator()<C, Off>(index) for every container, in order.
        template <class... S, class F> XOFFSET_SYNC_INLINE void each(List<S...>, F&& f) {
            std::uint32_t k = 0;
            (f.template operator()<typename S::type, S::offset>(k++), ...);
        }

        // ---- containers in a body: u32 count, then the bytes -----------------------

        template <class C> XOFFSET_SYNC_INLINE std::size_t container_bytes(const C& c) noexcept {
            if constexpr (is_fixed_string_v<C>) return c.view().size();
            else return c.size() * sizeof(typename C::value_type);
        }
        template <class C> XOFFSET_SYNC_INLINE bool container_empty(const C& c) noexcept {
            if constexpr (is_fixed_string_v<C>) return c.view().empty();
            else return c.size() == 0;
        }
        // Writes c at d (4 + container_bytes(c) bytes); returns the end.
        template <class C> XOFFSET_SYNC_INLINE char* put_container(char* d, const C& c) noexcept {
            if constexpr (is_fixed_string_v<C>) {
                const std::string_view v = c.view();
                store<std::uint32_t>(d, std::uint32_t(v.size()));
                if (!v.empty()) std::memcpy(d + 4, v.data(), v.size());
                return d + 4 + v.size();
            } else {
                const std::size_t b = c.size() * sizeof(typename C::value_type);
                store<std::uint32_t>(d, std::uint32_t(c.size()));
                if (b) std::memcpy(d + 4, c.data(), b);
                return d + 4 + b;
            }
        }
        // Sets c from the bytes at p (checked or the sender's); returns their end.
        template <class C> XOFFSET_SYNC_INLINE const char* apply_container(C& c, const char* p) {
            const std::uint32_t n = load<std::uint32_t>(p);
            p += 4;
            if constexpr (is_fixed_string_v<C>) {
                static_cast<XFixedString&>(c).assign(std::string_view(p, n));
                return p + n;
            } else {
                using E = typename C::value_type;
                c.assign_bytes(p, n);   // the wire's bytes need not be aligned for E: no E* to them
                return p + std::size_t(n) * sizeof(E);
            }
        }
        // Where the container at p ends, or nullptr when it does not fit in
        // [p, end) or an element's bool is not 0 or 1.
        template <class C> XOFFSET_SYNC_INLINE const char* check_container(const char* p, const char* end) noexcept {
            if (end - p < 4) return nullptr;
            const std::uint32_t n = load<std::uint32_t>(p);
            p += 4;
            const std::size_t left = std::size_t(end - p);
            if constexpr (is_fixed_string_v<C>) {
                if (left < n || n >= XBufferCore::MAX_PAYLOAD) return nullptr;
                return p + n;
            } else {
                using E = typename C::value_type;
                if (left / sizeof(E) < n || std::size_t(n) * sizeof(E) >= XBufferCore::MAX_PAYLOAD) return nullptr;
                if constexpr (Info<E>::bool_count != 0)
                    for (std::uint32_t e = 0; e < n; ++e)
                        if (!bools_ok(p + std::size_t(e) * sizeof(E), Info<E>::bools)) return nullptr;
                return p + std::size_t(n) * sizeof(E);
            }
        }
        template <class C> XOFFSET_SYNC_INLINE void clear_container(C& c) {
            if (!container_empty(c)) c.clear();
        }
        template <class C> bool same_container(const C& a, const C& b) noexcept {
            if constexpr (is_fixed_string_v<C>) return a.view() == b.view();
            else {
                using E = typename C::value_type;
                if (a.size() != b.size()) return false;
                for (std::size_t e = 0; e < a.size(); ++e)
                    for (const auto& r : Info<E>::runs)
                        if (std::memcmp(reinterpret_cast<const unsigned char*>(&a.data()[e]) + r.at,
                                        reinterpret_cast<const unsigned char*>(&b.data()[e]) + r.at, r.len) != 0) return false;
                return true;
            }
        }

        // ---- whole bodies --------------------------------------------------------

        // The whole body of T (a record or an array).
        template <class T> struct Whole {
            using I = Info<T>;
            static_assert(all_supported(typename I::containers{}),
                          "xsync: containers must be strings or vectors of elements without containers");
            static constexpr std::size_t C = I::container_count;
            using CMask = mask_t<C>;
            // Plain bytes only, no padding: the body is T's bytes.
            static constexpr bool flat = C == 0 && I::plain_bytes == sizeof(T);
            // Bytes of the body without the containers' own.
            static constexpr std::size_t fixed = I::plain_bytes + (C ? sizeof(CMask) : 0);

            // Bytes the body of r takes at most.
            XOFFSET_SYNC_INLINE static std::size_t room(const T& r) noexcept {
                std::size_t n = fixed;
                if constexpr (C > 0)
                    each(typename I::containers{}, [&]<class Ct, std::size_t Off>(std::uint32_t) XOFFSET_SYNC_LAMBDA_INLINE {
                        n += 4 + container_bytes(at<Ct, Off>(r));
                    });
                return n;
            }
            // Writes the body of r at d (room(r) bytes); returns its end.
            XOFFSET_SYNC_INLINE static char* put(char* d, const T& r) noexcept {
                const auto* s = reinterpret_cast<const char*>(&r);
                if constexpr (flat) {
                    std::memcpy(d, s, sizeof(T));
                    return d + sizeof(T);
                } else {
                    [&]<std::size_t... K>(std::index_sequence<K...>) XOFFSET_SYNC_LAMBDA_INLINE {
                        ((std::memcpy(d, s + I::runs[K].at, I::runs[K].len), d += I::runs[K].len), ...);
                    }(std::make_index_sequence<I::run_count>{});
                    if constexpr (C > 0) {
                        char* const cm = d;
                        d += sizeof(CMask);
                        CMask m = 0;
                        each(typename I::containers{}, [&]<class Ct, std::size_t Off>(std::uint32_t k) XOFFSET_SYNC_LAMBDA_INLINE {
                            const Ct& c = at<Ct, Off>(r);
                            if (container_empty(c)) return;
                            d = put_container(d, c);
                            m = CMask(m | (CMask(1) << k));
                        });
                        store<CMask>(cm, m);
                    }
                    return d;
                }
            }
            // Sets r from the body at p; returns its end.
            XOFFSET_SYNC_INLINE static const char* apply(const char* p, T& r) {
                auto* d = reinterpret_cast<char*>(&r);
                if constexpr (flat) {
                    std::memcpy(d, p, sizeof(T));
                    return p + sizeof(T);
                } else {
                    [&]<std::size_t... K>(std::index_sequence<K...>) XOFFSET_SYNC_LAMBDA_INLINE {
                        ((std::memcpy(d + I::runs[K].at, p, I::runs[K].len), p += I::runs[K].len), ...);
                    }(std::make_index_sequence<I::run_count>{});
                    if constexpr (C > 0) {
                        const CMask m = load<CMask>(p);
                        p += sizeof(CMask);
                        each(typename I::containers{}, [&]<class Ct, std::size_t Off>(std::uint32_t k) XOFFSET_SYNC_LAMBDA_INLINE {
                            Ct& c = at<Ct, Off>(r);
                            if (m >> k & 1u) p = apply_container(c, p);
                            else clear_container(c);
                        });
                    }
                    return p;
                }
            }
            // Where the body at p ends, or nullptr when it does not check:
            // cut by end, a bool not 0 or 1, a mask bit past the containers,
            // a container that does not fit.
            static const char* check(const char* p, const char* end) noexcept {
                if (std::size_t(end - p) < fixed) return nullptr;
                if constexpr (I::bool_count != 0) if (!bools_ok(p, I::packed_bools)) return nullptr;
                p += I::plain_bytes;
                if constexpr (C > 0) {
                    const std::uint64_t m = load<CMask>(p);
                    p += sizeof(CMask);
                    if (m & ~low_bits<C>) return nullptr;
                    each(typename I::containers{}, [&]<class Ct, std::size_t Off>(std::uint32_t k) XOFFSET_SYNC_LAMBDA_INLINE {
                        if (p && (m >> k & 1u)) p = check_container<Ct>(p, end);
                    });
                }
                return p;
            }
            static void clear(T& r) {
                auto* d = reinterpret_cast<unsigned char*>(&r);
                for (const auto& run : I::runs) std::memset(d + run.at, 0, run.len);
                if constexpr (C > 0)
                    each(typename I::containers{}, [&]<class Ct, std::size_t Off>(std::uint32_t) { clear_container(at<Ct, Off>(r)); });
            }
            static bool same(const T& a, const T& b) noexcept {
                const auto* x = reinterpret_cast<const unsigned char*>(&a);
                const auto* y = reinterpret_cast<const unsigned char*>(&b);
                for (const auto& run : I::runs) if (std::memcmp(x + run.at, y + run.at, run.len) != 0) return false;
                bool eq = true;
                if constexpr (C > 0)
                    each(typename I::containers{}, [&]<class Ct, std::size_t Off>(std::uint32_t) {
                        eq = eq && same_container(at<Ct, Off>(a), at<Ct, Off>(b));
                    });
                return eq;
            }
        };

        // ---- change bodies: T's members ------------------------------------------

        enum : std::uint8_t { kMemberPlain = 0, kMemberStr = 1, kMemberVec = 2, kMemberWhole = 3 };

        template <class T> struct Members {
            static constexpr std::size_t N = schema::member_count_v<T>;
            static_assert(N >= 1 && N <= 64, "xsync: a record has 1 to 64 members (group more in nested records)");
            using Mask = mask_t<N>;
            template <std::size_t K> using type = std::remove_cv_t<typename schema::member_t<T, K>::type>;
            template <std::size_t K> static constexpr std::size_t offset = schema::member_t<T, K>::offset;
            template <std::size_t K> static constexpr std::uint8_t kind_of() {
                using U = type<K>;
                if constexpr (is_fixed_string_v<U>) return kMemberStr;
                else if constexpr (is_fixed_vector_v<U>) return kMemberVec;
                else if constexpr (ContainersOf<U, 0>::type::size != 0) return kMemberWhole;
                else return kMemberPlain;
            }
            template <std::size_t K> static constexpr std::uint8_t kind = kind_of<K>();
            template <std::size_t K> static constexpr std::size_t size = sizeof(type<K>);
            // Offsets of member K's bool bytes in the member.
            template <std::size_t K> static constexpr auto bools = [] {
                constexpr std::size_t lo = offset<K>, hi = offset<K> + sizeof(type<K>);
                constexpr std::size_t n = [] {
                    std::size_t c = 0;
                    for (std::size_t i = lo; i < hi; ++i) c += Info<T>::classes[i] == kBool;
                    return c;
                }();
                std::array<std::uint32_t, n> a{};
                std::size_t c = 0;
                for (std::size_t i = lo; i < hi; ++i) if (Info<T>::classes[i] == kBool) a[c++] = std::uint32_t(i - lo);
                return a;
            }();
            static constexpr std::uint64_t plain_mask = [] {
                std::uint64_t m = 0;
                [&]<std::size_t... K>(std::index_sequence<K...>) {
                    ((m |= kind<K> == kMemberPlain ? std::uint64_t(1) << K : 0), ...);
                }(std::make_index_sequence<N>{});
                return m;
            }();
            static constexpr std::uint64_t all = low_bits<N>;
            static constexpr std::uint64_t container_mask = all & ~plain_mask;

            // f.template operator()<K>() for member k (k < N).
            template <class F> XOFFSET_SYNC_INLINE static void member(unsigned k, F&& f) {
                [&]<std::size_t... K>(std::index_sequence<K...>) XOFFSET_SYNC_LAMBDA_INLINE {
                    (void)((k == K ? (f.template operator()<K>(), true) : false) || ...);
                }(std::make_index_sequence<N>{});
            }
        };
    } // namespace sd_detail

    /// Record codec for T (static functions): whole bodies (enter) and change
    /// bodies (the members a mask names), as the message format above has
    /// them. A server encodes a body once per entity and tick and copies it
    /// into every message that carries the entity (put_whole_op and friends
    /// below); XSyncWriter can also encode in place.
    template <class T>
    struct XRecordBody {
        using whole_body = sd_detail::Whole<T>;
        static constexpr std::size_t container_count = whole_body::C;
        /// No containers: diff() and put_diff apply.
        static constexpr bool plain = container_count == 0;

        // ---- whole bodies (enter, and the root) ----

        /// Bytes the whole body of r takes at most.
        XOFFSET_SYNC_INLINE static std::size_t whole_room(const T& r) noexcept { return whole_body::room(r); }
        /// Writes the whole body of r at d (whole_room(r) writable bytes); returns its end.
        XOFFSET_SYNC_INLINE static char* put_whole(char* d, const T& r) noexcept { return whole_body::put(d, r); }
        /// Sets every plain byte and container of r from the whole body at p
        /// (checked, or the sender's); returns the body's end.
        XOFFSET_SYNC_INLINE static const char* apply_whole(const char* p, T& r) { return whole_body::apply(p, r); }
        /// Where the whole body at p ends, or nullptr when it does not check.
        static const char* check_whole(const char* p, const char* end) noexcept { return whole_body::check(p, end); }

        // ---- change bodies ----

        using members = sd_detail::Members<T>;

        /// Bytes the change body of the members in mask of r takes at most.
        XOFFSET_SYNC_INLINE static std::size_t change_room(const T& r, std::uint64_t mask) noexcept {
            using M = members;
            std::size_t n = sizeof(typename M::Mask);
            [&]<std::size_t... K>(std::index_sequence<K...>) XOFFSET_SYNC_LAMBDA_INLINE {
                ((mask >> K & 1u ? (void)(n += member_room<K>(r)) : (void)0), ...);
            }(std::make_index_sequence<M::N>{});
            return n;
        }
        /// Writes the change body of the members in mask (bits past the
        /// members ignored) of r at d (change_room(r, mask) writable bytes);
        /// returns its end.
        XOFFSET_SYNC_INLINE static char* put_change(char* d, const T& r, std::uint64_t mask) noexcept {
            using M = members;
            mask &= M::all;
            sd_detail::store<typename M::Mask>(d, typename M::Mask(mask));
            d += sizeof(typename M::Mask);
            const auto* s = reinterpret_cast<const char*>(&r);
            // One test per plain member, each a copy of a compile-time size.
            [&]<std::size_t... K>(std::index_sequence<K...>) XOFFSET_SYNC_LAMBDA_INLINE {
                ((M::template kind<K> == sd_detail::kMemberPlain && (mask >> K & 1u)
                      ? (void)(std::memcpy(d, s + M::template offset<K>, M::template size<K>), d += M::template size<K>)
                      : (void)0),
                 ...);
            }(std::make_index_sequence<M::N>{});
            if constexpr (M::container_mask != 0) {
                for (std::uint64_t m = mask & M::container_mask; m; m &= m - 1)
                    M::member(unsigned(std::countr_zero(m)), [&]<std::size_t K>() XOFFSET_SYNC_LAMBDA_INLINE {
                        d = put_member<K>(d, r);
                    });
            }
            return d;
        }
        /// The members of now that differ from prev (plain bytes compared as
        /// bytes, containers by content).
        static std::uint64_t changed(const T& now, const T& prev) noexcept {
            using M = members;
            std::uint64_t m = 0;
            [&]<std::size_t... K>(std::index_sequence<K...>) {
                ((m |= member_same<K>(now, prev) ? 0 : std::uint64_t(1) << K), ...);
            }(std::make_index_sequence<M::N>{});
            return m;
        }
        /// Records without containers: bytes put_diff writes at most.
        static constexpr std::size_t diff_room = sizeof(sd_detail::mask_t<schema::member_count_v<T>>) + sizeof(T);
        /// Records without containers: the change body of the members of now
        /// that differ from prev, written at d (diff_room bytes), and prev
        /// takes them. Returns the end, or nullptr when nothing differs (then
        /// prev is unchanged and d holds nothing).
        XOFFSET_SYNC_INLINE static char* put_diff(char* d, const T& now, T& prev) noexcept requires plain {
            using M = members;
            char* const md = d;
            d += sizeof(typename M::Mask);
            typename M::Mask mask = 0;
            const auto* s = reinterpret_cast<const char*>(&now);
            auto* o = reinterpret_cast<char*>(&prev);
            [&]<std::size_t... K>(std::index_sequence<K...>) XOFFSET_SYNC_LAMBDA_INLINE {
                ((std::memcmp(s + M::template offset<K>, o + M::template offset<K>, M::template size<K>) != 0
                      ? (void)(std::memcpy(d, s + M::template offset<K>, M::template size<K>),
                               std::memcpy(o + M::template offset<K>, s + M::template offset<K>, M::template size<K>),
                               d += M::template size<K>, mask = typename M::Mask(mask | (typename M::Mask(1) << K)))
                      : (void)0),
                 ...);
            }(std::make_index_sequence<M::N>{});
            if (!mask) return nullptr;
            sd_detail::store<typename M::Mask>(md, mask);
            return d;
        }
        /// Sets the members the change body at p names (checked, or the
        /// sender's); returns the body's end.
        XOFFSET_SYNC_INLINE static const char* apply_change(const char* p, T& r) {
            using M = members;
            const std::uint64_t mask = sd_detail::load<typename M::Mask>(p);
            p += sizeof(typename M::Mask);
            auto* d = reinterpret_cast<char*>(&r);
            [&]<std::size_t... K>(std::index_sequence<K...>) XOFFSET_SYNC_LAMBDA_INLINE {
                ((M::template kind<K> == sd_detail::kMemberPlain && (mask >> K & 1u)
                      ? (void)(std::memcpy(d + M::template offset<K>, p, M::template size<K>), p += M::template size<K>)
                      : (void)0),
                 ...);
            }(std::make_index_sequence<M::N>{});
            if constexpr (M::container_mask != 0) {
                for (std::uint64_t m = mask & M::container_mask; m; m &= m - 1)
                    M::member(unsigned(std::countr_zero(m)), [&]<std::size_t K>() XOFFSET_SYNC_LAMBDA_INLINE {
                        p = apply_member<K>(p, r);
                    });
            }
            return p;
        }
        /// Where the change body at p ends, or nullptr when it does not
        /// check: cut by end, a mask bit past the members, a bool not 0 or 1,
        /// a container that does not fit.
        static const char* check_change(const char* p, const char* end) noexcept {
            using M = members;
            if (std::size_t(end - p) < sizeof(typename M::Mask)) return nullptr;
            const std::uint64_t mask = sd_detail::load<typename M::Mask>(p);
            p += sizeof(typename M::Mask);
            if (mask & ~M::all) return nullptr;
            bool ok = true;
            [&]<std::size_t... K>(std::index_sequence<K...>) {
                ((ok = ok && (M::template kind<K> != sd_detail::kMemberPlain || !(mask >> K & 1u) || check_plain<K>(p, end))), ...);
            }(std::make_index_sequence<M::N>{});
            if (!ok) return nullptr;
            if constexpr (M::container_mask != 0) {
                for (std::uint64_t m = mask & M::container_mask; m && p; m &= m - 1)
                    M::member(unsigned(std::countr_zero(m)), [&]<std::size_t K>() { p = check_member<K>(p, end); });
            }
            return p;
        }

        /// Plain bytes zero, containers empty.
        static void clear(T& r) { whole_body::clear(r); }
        /// Whether a and b hold the same plain bytes and container contents.
        static bool same(const T& a, const T& b) noexcept { return whole_body::same(a, b); }

    private:
        template <std::size_t K> using member_t = typename members::template type<K>;
        template <std::size_t K> static constexpr std::size_t member_off = members::template offset<K>;

        template <std::size_t K> XOFFSET_SYNC_INLINE static std::size_t member_room(const T& r) noexcept {
            constexpr auto kind = members::template kind<K>;
            if constexpr (kind == sd_detail::kMemberPlain) return sizeof(member_t<K>);
            else if constexpr (kind == sd_detail::kMemberWhole) return sd_detail::Whole<member_t<K>>::room(sd_detail::at<member_t<K>, member_off<K>>(r));
            else return 4 + sd_detail::container_bytes(sd_detail::at<member_t<K>, member_off<K>>(r));
        }
        template <std::size_t K> XOFFSET_SYNC_INLINE static char* put_member(char* d, const T& r) noexcept {
            const auto& m = sd_detail::at<member_t<K>, member_off<K>>(r);
            if constexpr (members::template kind<K> == sd_detail::kMemberWhole) return sd_detail::Whole<member_t<K>>::put(d, m);
            else if constexpr (members::template kind<K> != sd_detail::kMemberPlain) return sd_detail::put_container(d, m);
            else return d;
        }
        template <std::size_t K> XOFFSET_SYNC_INLINE static const char* apply_member(const char* p, T& r) {
            auto& m = sd_detail::at<member_t<K>, member_off<K>>(r);
            if constexpr (members::template kind<K> == sd_detail::kMemberWhole) return sd_detail::Whole<member_t<K>>::apply(p, m);
            else if constexpr (members::template kind<K> != sd_detail::kMemberPlain) return sd_detail::apply_container(m, p);
            else return p;
        }
        template <std::size_t K> static bool check_plain(const char*& p, const char* end) noexcept {
            constexpr std::size_t n = sizeof(member_t<K>);
            if (std::size_t(end - p) < n) return false;
            if constexpr (members::template bools<K>.size() != 0)
                if (!sd_detail::bools_ok(p, members::template bools<K>)) return false;
            p += n;
            return true;
        }
        template <std::size_t K> static const char* check_member(const char* p, const char* end) noexcept {
            if constexpr (members::template kind<K> == sd_detail::kMemberWhole) return sd_detail::Whole<member_t<K>>::check(p, end);
            else if constexpr (members::template kind<K> != sd_detail::kMemberPlain) return sd_detail::check_container<member_t<K>>(p, end);
            else return p;
        }
        template <std::size_t K> static bool member_same(const T& a, const T& b) noexcept {
            const auto& x = sd_detail::at<member_t<K>, member_off<K>>(a);
            const auto& y = sd_detail::at<member_t<K>, member_off<K>>(b);
            if constexpr (members::template kind<K> == sd_detail::kMemberPlain) return std::memcmp(&x, &y, sizeof x) == 0;
            else if constexpr (members::template kind<K> == sd_detail::kMemberWhole) return sd_detail::Whole<member_t<K>>::same(x, y);
            else return sd_detail::same_container(x, y);
        }
    };

    // ---- shared ops: a server's body store ----------------------------------------
    //
    // An op (u32 id, then the body) appended to out, which keeps kSyncSlack
    // bytes past it: XSyncWriter::op copies it into a message under the id a
    // receiver knows the entity by. Each returns the op's length (where it
    // starts: out.size() before the call).

    /// An enter op: id and the whole body of r.
    template <class T> std::size_t put_whole_op(XSyncOut& out, std::uint32_t id, const T& r) {
        char* const q = out.tail(4 + XRecordBody<T>::whole_room(r) + kSyncSlack);
        sd_detail::store<std::uint32_t>(q, id);
        const std::size_t n = std::size_t(XRecordBody<T>::put_whole(q + 4, r) - q);
        out.commit(n);
        return n;
    }
    /// A change op: id and the change body of the members in mask of r.
    template <class T> std::size_t put_change_op(XSyncOut& out, std::uint32_t id, const T& r, std::uint64_t mask) {
        char* const q = out.tail(4 + XRecordBody<T>::change_room(r, mask) + kSyncSlack);
        sd_detail::store<std::uint32_t>(q, id);
        const std::size_t n = std::size_t(XRecordBody<T>::put_change(q + 4, r, mask) - q);
        out.commit(n);
        return n;
    }
    /// Records without containers: a change op of the members of now that
    /// differ from prev (prev takes them), or nothing (returns 0) when none do.
    template <class T> std::size_t put_diff_op(XSyncOut& out, std::uint32_t id, const T& now, T& prev) {
        char* const q = out.tail(4 + XRecordBody<T>::diff_room + kSyncSlack);
        sd_detail::store<std::uint32_t>(q, id);
        const char* const e = XRecordBody<T>::put_diff(q + 4, now, prev);
        if (!e) return 0;
        out.commit(std::size_t(e - q));
        return std::size_t(e - q);
    }

    // ---- writer -----------------------------------------------------------------

    /// Builds one message in out: the head and the root record, then
    /// sections (begin, ops, end; an empty section is dropped), then
    /// finish(). The write cursor stays in the writer (a local of the
    /// encoding function): one capacity check per op, or one per section
    /// after reserve().
    class XSyncWriter {
    public:
        /// A delta: the stream's next version (the receiver's cursor holds the stream).
        template <class Root>
        XSyncWriter(XSyncOut& out, const Root& root, std::uint32_t version) : out_(&out) {
            start(out, root, version, 0, nullptr);
        }
        /// A key message of stream: every entry the receiver keeps enters.
        template <class Root>
        XSyncWriter(XSyncOut& out, const Root& root, std::uint32_t version, std::uint64_t stream) : out_(&out) {
            start(out, root, version, kFlagKey, &stream);
        }
        XSyncWriter(const XSyncWriter&) = delete;
        XSyncWriter& operator=(const XSyncWriter&) = delete;

        /// A section of leaves of n ids (one capacity check; nothing when n is 0).
        template <class Id> XOFFSET_SYNC_INLINE void leaves(unsigned table, const Id* ids, std::size_t n) {
            if (n > kSyncSectionMax) [[unlikely]] too_many();
            char* d = need(4 + 4 * n);
            sd_detail::store<std::uint32_t>(d, std::uint32_t(n) << 8 | code(table, XSyncKind::leave));
            for (std::size_t i = 0; i < n; ++i) sd_detail::store<std::uint32_t>(d + 4 + 4 * i, std::uint32_t(ids[i]));
            q_ = d + (n ? 4 + 4 * n : 0);
        }
        /// Starts a section of ops of one table (1 to 63) and kind.
        XOFFSET_SYNC_INLINE void begin(unsigned table, XSyncKind kind) {
            char* d = need(4);
            sec_ = d;
            cur_ = code(table, kind);
            n_ = 0;
            q_ = d + 4;
        }
        /// Ends the section (at most kSyncSectionMax ops); an empty one is dropped.
        XOFFSET_SYNC_INLINE void end() {
            if (n_ > kSyncSectionMax) [[unlikely]] too_many();
            sd_detail::store<std::uint32_t>(sec_, n_ << 8 | cur_);
            q_ = n_ ? q_ : sec_;
        }
        /// A leave op of id (in a leave section).
        XOFFSET_SYNC_INLINE void leave(std::uint32_t id) {
            char* d = need(4);
            sd_detail::store<std::uint32_t>(d, id);
            q_ = d + 4;
            ++n_;
        }
        /// A shared op: len bytes at op (its id, then its body), with
        /// kSyncSlack bytes readable past them.
        XOFFSET_SYNC_INLINE void op(const char* op, std::size_t len) {
            char* d = need(len + kSyncSlack);
            sd_detail::copy_body(d, op, len);
            q_ = d + len;
            ++n_;
        }
        /// The same under id: the op's own id is replaced (the slot this
        /// receiver keeps the entity in, say).
        XOFFSET_SYNC_INLINE void op(const char* op, std::size_t len, std::uint32_t id) {
            char* d = need(len + kSyncSlack);
            sd_detail::copy_body(d, op, len);
            sd_detail::store<std::uint32_t>(d, id);
            q_ = d + len;
            ++n_;
        }
        /// A shared op of len bytes, at most N (len 0: no op), with N bytes
        /// readable at op: N bytes are copied either way, no branch on len.
        template <std::size_t N> XOFFSET_SYNC_INLINE void op_upto(const char* op, std::size_t len) {
            char* d = need(N + kSyncSlack);
            std::memcpy(d, op, N);
            q_ = d + len;
            n_ += len != 0;
        }
        template <std::size_t N> XOFFSET_SYNC_INLINE void op_upto(const char* op, std::size_t len, std::uint32_t id) {
            char* d = need(N + kSyncSlack);
            std::memcpy(d, op, N);
            sd_detail::store<std::uint32_t>(d, id);   // past the op when len is 0: overwritten later
            q_ = d + len;
            n_ += len != 0;
        }
        /// Room for n more bytes now: ops written with op_in_room then skip
        /// the capacity check (each takes its len + kSyncSlack of the room).
        XOFFSET_SYNC_INLINE void reserve(std::size_t n) { (void)need(n); }
        XOFFSET_SYNC_INLINE void op_in_room(const char* op, std::size_t len, std::uint32_t id) {
            char* d = q_;
            sd_detail::copy_body(d, op, len);
            sd_detail::store<std::uint32_t>(d, id);
            q_ = d + len;
            ++n_;
        }
        /// An enter op of r encoded here.
        template <class R> XOFFSET_SYNC_INLINE void whole(std::uint32_t id, const R& r) {
            char* d = need(4 + XRecordBody<R>::whole_room(r) + kSyncSlack);
            sd_detail::store<std::uint32_t>(d, id);
            q_ = XRecordBody<R>::put_whole(d + 4, r);
            ++n_;
        }
        /// A change op of the members in mask of r encoded here; nothing when
        /// mask is 0. Returns whether it wrote one.
        template <class R> XOFFSET_SYNC_INLINE bool change(std::uint32_t id, const R& r, std::uint64_t mask) {
            if (!(mask & XRecordBody<R>::members::all)) return false;
            char* d = need(4 + XRecordBody<R>::change_room(r, mask) + kSyncSlack);
            sd_detail::store<std::uint32_t>(d, id);
            q_ = XRecordBody<R>::put_change(d + 4, r, mask);
            ++n_;
            return true;
        }
        /// Records without containers: a change op of the members of now that
        /// differ from prev (prev takes them); nothing when none do. Returns
        /// whether it wrote one.
        template <class R> XOFFSET_SYNC_INLINE bool diff(std::uint32_t id, const R& now, R& prev) {
            char* d = need(4 + XRecordBody<R>::diff_room + kSyncSlack);
            sd_detail::store<std::uint32_t>(d, id);
            char* const e = XRecordBody<R>::put_diff(d + 4, now, prev);
            if (!e) return false;
            q_ = e;
            ++n_;
            return true;
        }
        /// Ends the message (after the last section's end()); returns its length.
        std::size_t finish() noexcept {
            out_->commit(std::size_t(q_ - out_->data()) - out_->size());
            return out_->size();
        }
        /// The same, with the head's version written now in place of the
        /// one given at construction (a sender that reads its connection's
        /// state late, after the ops).
        std::size_t finish(std::uint32_t version) noexcept {
            sd_detail::store<std::uint32_t>(out_->data() + 4, version);
            return finish();
        }

    private:
        template <class Root>
        void start(XSyncOut& out, const Root& root, std::uint32_t version, std::uint8_t flags, const std::uint64_t* stream) {
            static_assert(XRecordBody<Root>::container_count == 0, "xsync: the root record (table 0) has no containers");
            out.clear();
            char* d = out.tail(kSyncHead + 8 + XRecordBody<Root>::whole_body::fixed + kSyncSlack);
            d[0] = 'X'; d[1] = 'S'; d[2] = char(kFormat); d[3] = char(flags);
            sd_detail::store<std::uint32_t>(d + 4, version);
            d += kSyncHead;
            if (stream) { sd_detail::store<std::uint64_t>(d, *stream); d += 8; }
            q_ = XRecordBody<Root>::put_whole(d, root);
            end_ = out.data() + out.capacity();
        }
        static constexpr std::uint32_t code(unsigned table, XSyncKind kind) noexcept { return std::uint32_t(table << 2 | unsigned(kind)); }
        [[noreturn]] XOFFSET_SYNC_COLD static void too_many() {
            XOFFSET_THROW(std::length_error("xsync: more than 2^24 - 1 ops in one section"));
        }
        // The writer's address never leaves it (grow is static), so its
        // cursor can live in registers.
        XOFFSET_SYNC_INLINE char* need(std::size_t n) {
            if (std::size_t(end_ - q_) < n) [[unlikely]] {
                char* const old = out_->data();
                const Span sp = grow(*out_, std::size_t(q_ - old), n);
                if (sec_) sec_ = sp.base + (sec_ - old);
                q_ = sp.base + (q_ - old);
                end_ = sp.end;
            }
            return q_;
        }
        struct Span { char* base; char* end; };
        XOFFSET_DETAIL_NOINLINE static Span grow(XSyncOut& out, std::size_t used, std::size_t n) {
            out.commit(used - out.size());
            out.tail(n);
            return {out.data(), out.data() + out.capacity()};
        }
        XSyncOut* out_;
        char* q_ = nullptr;
        char* end_ = nullptr;
        char* sec_ = nullptr;
        std::uint32_t cur_ = 0;
        std::uint32_t n_ = 0;
    };

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
            std::uint8_t code;   // table << 2 | kind
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

    /// Whether msg is a key message: an XS 2.0 head with the KEY flag. A
    /// receiver empties its keyed tables before it applies one.
    inline bool is_key(std::string_view msg) noexcept {
        return msg.size() >= kSyncHead && msg[0] == 'X' && msg[1] == 'S' && static_cast<unsigned char>(msg[2]) == kFormat &&
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
        inline constexpr const char* kNoRoot = "shared delta: the receiver holds no root record (table 0, id 0)";
        inline constexpr const char* kTwice = "shared delta: an entry appears twice in one message";
        inline constexpr const char* kCut = "shared delta: message ends inside a section";

        // f.template operator()<K>() for K == table (one jump; at most 64 tables).
        template <unsigned N, class F>
        XOFFSET_SYNC_INLINE const char* sync_switch(unsigned table, F&& f) {
            static_assert(N >= 1 && N <= kSyncTables, "XReceiver::apply_sync: a target has 1 to 64 tables");
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

        // The head, as read.
        struct SyncHead {
            std::uint64_t stream;
            std::uint32_t version;
            std::uint8_t flags;
        };

        struct SyncAccess {
            using vstatus = ::XOffsetDatastructure::detail::vstatus;

            XOFFSET_SYNC_COLD static vstatus fail(XErrc code, const char* why) {
                XOFFSET_VFAIL(XException, code, why);
            }
            XOFFSET_SYNC_COLD static vstatus fail_op(const char* why) {
                return fail(why == kCut ? XErrc::truncated : XErrc::invalid_payload, why);
            }

            template <class Target> static constexpr void check_target() {
                static_assert(!Target::template keyed<0>, "XReceiver::apply_sync: table 0 is the root record, not keyed");
                static_assert(XRecordBody<sync_record_t<Target, 0>>::container_count == 0,
                              "XReceiver::apply_sync: the root record (table 0) has no containers");
            }

            // The head and the stream and version rules against cur, with
            // room for the root (root bytes): where the root starts, or
            // nullptr (head_error() says why).
            XOFFSET_SYNC_INLINE static const char* head(const char* p, const char* end, const XSyncCursor& cur, SyncHead& h,
                                                        std::size_t root) noexcept {
                if (std::size_t(end - p) < kSyncHead + root || p[0] != 'X' || p[1] != 'S' ||
                    static_cast<unsigned char>(p[2]) != kFormat) return nullptr;
                h.flags = static_cast<std::uint8_t>(p[3]);
                h.version = load<std::uint32_t>(p + 4);
                h.stream = cur.stream_;
                p += kSyncHead;
                if (h.flags != 0) {
                    if (h.flags != kFlagKey || std::size_t(end - p) < 8 + root) return nullptr;
                    h.stream = load<std::uint64_t>(p);
                    p += 8;
                } else if (!cur.keyed_ || h.version != std::uint32_t(cur.version_ + 1)) return nullptr;
                return p;
            }
            XOFFSET_SYNC_COLD static vstatus head_error(const char* p, const char* end, const XSyncCursor& cur, std::size_t root) {
                const std::size_t n = std::size_t(end - p);
                if ((n >= 1 && p[0] != 'X') || (n >= 2 && p[1] != 'S'))
                    return fail(XErrc::bad_magic, "shared delta: not an XS message");
                if (n >= 3 && static_cast<unsigned char>(p[2]) != kFormat)
                    return fail(XErrc::unsupported_version, "shared delta: unsupported XS format (this receiver reads XS 2.0)");
                if (n < kSyncHead) return fail(XErrc::truncated, "shared delta: message shorter than its head");
                const auto flags = static_cast<std::uint8_t>(p[3]);
                if (flags & ~kFlagKey) return fail(XErrc::unsupported_flags, "shared delta: unknown flags");
                if (!(flags & kFlagKey)) {
                    if (!cur.keyed_) return fail(XErrc::delta_base_mismatch, "shared delta: no key message applied (ask for one)");
                    if (load<std::uint32_t>(p + 4) != std::uint32_t(cur.version_ + 1))
                        return fail(XErrc::delta_base_mismatch, "shared delta: version gap (ask for a key message)");
                }
                (void)root;
                return fail(XErrc::truncated, "shared delta: message ends inside its head or root record");
            }

            static void done(XSyncCursor& cur, const SyncHead& h) noexcept {
                cur.stream_ = h.stream;
                cur.version_ = h.version;
                cur.keyed_ = true;
            }

            // The last kind table K takes: leave for keyed tables, change for fixed ones.
            template <unsigned K, class Target>
            static constexpr unsigned last_kind = Target::template keyed<K> ? unsigned(XSyncKind::leave) : unsigned(XSyncKind::change);

            // XCheck::admit: the head and the stream rules, then every
            // section in one pass: tables and kinds checked, ids looked up (an
            // op on an entry the receiver does not hold is rejected, an enter
            // of a keyed table asks enter()), each op's id within the message.
            // Bodies are the sender's. A message rejected after its head may
            // be partly applied: the cursor is then empty (ask for a key).
            template <class Target>
            static vstatus admit(const char* const begin, const char* const end, XSyncCursor& cur, Target& t) {
                check_target<Target>();
                using R0 = sync_record_t<Target, 0>;
                constexpr std::size_t root = XRecordBody<R0>::whole_body::fixed;
                SyncHead h;
                const char* p = head(begin, end, cur, h, root);
                if (!p) [[unlikely]] return head_error(begin, end, cur, root);
                cur.keyed_ = false;
                {
                    const std::int64_t rh = t.template find<0>(0);
                    if (rh < 0) [[unlikely]] return fail(XErrc::invalid_payload, kNoRoot);
                    p = XRecordBody<R0>::apply_whole(p, t.template record<0>(rh));
                    sync_applied<0>(t, 0, rh);
                }
                while (p != end) {
                    if (end - p < 4) [[unlikely]] return fail_op(kCut);
                    const std::uint32_t w = load<std::uint32_t>(p);
                    p += 4;
                    const unsigned kind = w & 3u;
                    std::uint32_t n = w >> 8;
                    const char* e = sync_switch<sync_tables<Target>>((w >> 2) & 63u, [&]<unsigned K>() XOFFSET_SYNC_LAMBDA_INLINE -> const char* {
                        if constexpr (K == 0) return kBadKind;
                        else {
                            using R = sync_record_t<Target, K>;
                            if (kind == XSyncKind::change) {
                                for (; n; --n) {
                                    if (end - p < 4) [[unlikely]] return kCut;
                                    const std::uint32_t id = load<std::uint32_t>(p);
                                    const std::int64_t hd = t.template find<K>(id);
                                    if (hd < 0) [[unlikely]] return kNotHeld;
                                    p = XRecordBody<R>::apply_change(p + 4, t.template record<K>(hd));
                                    sync_applied<K>(t, id, hd);
                                }
                                return nullptr;
                            }
                            if (kind == XSyncKind::enter) {
                                for (; n; --n) {
                                    if (end - p < 4) [[unlikely]] return kCut;
                                    const std::uint32_t id = load<std::uint32_t>(p);
                                    std::int64_t hd;
                                    if constexpr (Target::template keyed<K>) {
                                        hd = t.template enter<K>(id);
                                        if (hd < 0) [[unlikely]] return kRefused;
                                    } else {
                                        hd = t.template find<K>(id);
                                        if (hd < 0) [[unlikely]] return kNotHeld;
                                    }
                                    p = XRecordBody<R>::apply_whole(p + 4, t.template record<K>(hd));
                                    sync_applied<K>(t, id, hd);
                                }
                                return nullptr;
                            }
                            if constexpr (Target::template keyed<K>) {
                                if (kind == XSyncKind::leave) {
                                    if (std::size_t(end - p) / 4 < n) [[unlikely]] return kCut;
                                    for (; n; --n) {
                                        const std::uint32_t id = load<std::uint32_t>(p);
                                        p += 4;
                                        const std::int64_t hd = t.template find<K>(id);
                                        if (hd < 0) [[unlikely]] return kNotHeld;
                                        t.template leave<K>(id, hd);
                                    }
                                    return nullptr;
                                }
                            }
                            return kBadKind;
                        }
                    });
                    if (e) [[unlikely]] return fail_op(e);
                }
                done(cur, h);
                return ::XOffsetDatastructure::detail::vok();
            }

            // Entries seen in this message. The first 64 ops: a 256-bit
            // filter on the stack, and an op whose bit is set is compared with
            // the ops before it. Past them: open addressing over (table, id)
            // in the cursor, marked with the message's stamp (no clearing per
            // message).
            struct Seen {
                std::uint64_t filter[4] = {0, 0, 0, 0};
                std::uint64_t* cells = nullptr;
                std::size_t size = 0;
                std::uint64_t mark = 0;
            };
            static constexpr std::uint64_t kSeenKey = (std::uint64_t(1) << 38) - 1;
            static std::uint64_t op_key(const SyncOp& o) noexcept { return std::uint64_t(o.code >> 2) << 32 | o.id; }
            static std::size_t seen_home(std::uint64_t key, std::size_t size) noexcept {
                return std::size_t((key * 0x9E3779B97F4A7C15ull) >> (64 - std::countr_zero(size)));
            }
            // Adds key to the table; false when it is there already.
            static bool seen_add(Seen& s, std::uint64_t key) noexcept {
                for (std::size_t i = seen_home(key, s.size);; i = (i + 1) & (s.size - 1)) {
                    if ((s.cells[i] & ~kSeenKey) != s.mark) { s.cells[i] = s.mark | key; return true; }
                    if (s.cells[i] == (s.mark | key)) return false;
                }
            }
            // The table for ops 0..n-1 (n >= 64) and their keys in it.
            XOFFSET_DETAIL_NOINLINE static void seen_grow(XSyncCursor& cur, Seen& s, const SyncOp* ops, std::size_t n) {
                if (s.size == 0) {
                    // This message's first use: a new stamp.
                    if (++cur.stamp_ == (1u << 26)) {
                        cur.stamp_ = 1;
                        std::fill(cur.seen_.begin(), cur.seen_.end(), std::uint64_t(0));
                    }
                    s.mark = std::uint64_t(cur.stamp_) << 38;
                }
                std::size_t size = std::max<std::size_t>(cur.seen_.size(), 256);
                while (size < 2 * (n + 1)) size *= 2;
                if (cur.seen_.size() < size) cur.seen_.assign(size, 0);
                s.cells = cur.seen_.data();
                s.size = cur.seen_.size();
                for (std::size_t k = 0; k < n; ++k) (void)seen_add(s, op_key(ops[k]));
            }
            // Whether entry key, op n's, is one of ops 0..n-1's; notes it.
            XOFFSET_SYNC_INLINE static bool seen_before(XSyncCursor& cur, Seen& s, const SyncOp* ops, std::size_t n, std::uint64_t key) {
                if (n < 64) {
                    const unsigned bit = unsigned((key * 0x9E3779B97F4A7C15ull) >> 56);
                    const std::uint64_t m = std::uint64_t(1) << (bit & 63u);
                    if (s.filter[bit >> 6] & m) [[unlikely]] {
                        for (std::size_t k = 0; k < n; ++k)
                            if (op_key(ops[k]) == key) return true;
                    }
                    s.filter[bit >> 6] |= m;
                    return false;
                }
                if (s.size < 2 * (n + 1)) [[unlikely]] seen_grow(cur, s, ops, n);
                return !seen_add(s, key);
            }

            // Applies one op checked by full(). h: find's result (a keyed
            // enter asks enter() instead).
            template <unsigned K, class Target>
            XOFFSET_SYNC_INLINE static const char* apply_op(Target& t, const SyncOp& o) {
                using R = sync_record_t<Target, K>;
                const unsigned kind = o.code & 3u;
                std::int64_t h = o.handle;
                if (kind == XSyncKind::change) {
                    XRecordBody<R>::apply_change(o.body, t.template record<K>(h));
                } else if (kind == XSyncKind::enter) {
                    if constexpr (Target::template keyed<K>) {
                        h = t.template enter<K>(o.id);
                        if (h < 0) [[unlikely]] return kRefused;
                    }
                    XRecordBody<R>::apply_whole(o.body, t.template record<K>(h));
                } else {
                    if constexpr (Target::template keyed<K>) t.template leave<K>(o.id, h);
                    return nullptr;
                }
                sync_applied<K>(t, o.id, h);
                return nullptr;
            }

            // XCheck::full: the head, the root and every op are checked
            // against the records as they are before any applies: tables and
            // kinds, ids held (a keyed enter: not held), every body against
            // its record type, each entry at most once per message. A
            // rejected message changes nothing. A failure while the checked
            // ops apply (the receiver could not add an entry, out of storage)
            // leaves the ops before it applied and the cursor empty.
            template <class Target>
            static vstatus full(const char* const begin, const char* const end, XSyncCursor& cur, Target& t) {
                check_target<Target>();
                using R0 = sync_record_t<Target, 0>;
                constexpr std::size_t root = XRecordBody<R0>::whole_body::fixed;
                SyncHead h;
                const char* p = head(begin, end, cur, h, root);
                if (!p) [[unlikely]] return head_error(begin, end, cur, root);
                const std::int64_t rh = t.template find<0>(0);
                if (rh < 0) [[unlikely]] return fail(XErrc::invalid_payload, kNoRoot);
                const char* const root_body = p;
                p = XRecordBody<R0>::check_whole(p, end);
                if (!p) [[unlikely]] return fail(XErrc::invalid_payload, kBadBody);
                Seen seen;
                std::size_t n = 0;
                while (p != end) {
                    if (end - p < 4) [[unlikely]] return fail_op(kCut);
                    const std::uint32_t w = load<std::uint32_t>(p);
                    p += 4;
                    const unsigned kind = w & 3u, table = (w >> 2) & 63u;
                    const std::uint32_t count = w >> 8;
                    // Every op takes at least four bytes.
                    if (std::size_t(end - p) / 4 < count) [[unlikely]] return fail_op(kCut);
                    if (cur.ops_.size() < n + count) cur.ops_.resize(std::max(n + count, 2 * cur.ops_.size()));
                    SyncOp* const ops = cur.ops_.data();
                    const char* e = sync_switch<sync_tables<Target>>(table, [&]<unsigned K>() XOFFSET_SYNC_LAMBDA_INLINE -> const char* {
                        if constexpr (K == 0) return kBadKind;
                        else {
                            using R = sync_record_t<Target, K>;
                            if (kind > last_kind<K, Target>) [[unlikely]] return kBadKind;
                            const std::uint8_t code = std::uint8_t(K << 2 | kind);
                            for (std::uint32_t i = 0; i < count; ++i, ++n) {
                                if (end - p < 4) [[unlikely]] return kCut;
                                const std::uint32_t id = load<std::uint32_t>(p);
                                p += 4;
                                if (seen_before(cur, seen, ops, n, std::uint64_t(K) << 32 | id)) [[unlikely]] return kTwice;
                                const std::int64_t hd = t.template find<K>(id);
                                const char* const body = p;
                                if (kind == XSyncKind::change) {
                                    if (hd < 0) [[unlikely]] return kNotHeld;
                                    p = XRecordBody<R>::check_change(p, end);
                                } else if (kind == XSyncKind::enter) {
                                    if (Target::template keyed<K> ? hd >= 0 : hd < 0) [[unlikely]]
                                        return Target::template keyed<K> ? kHeld : kNotHeld;
                                    p = XRecordBody<R>::check_whole(p, end);
                                } else if (hd < 0) [[unlikely]] return kNotHeld;
                                if (!p) [[unlikely]] return kBadBody;
                                ops[n] = SyncOp{body, hd, id, code};
                            }
                            return nullptr;
                        }
                    });
                    if (e) [[unlikely]] return fail_op(e);
                }
                cur.keyed_ = false;
                XRecordBody<R0>::apply_whole(root_body, t.template record<0>(rh));
                sync_applied<0>(t, 0, rh);
                const SyncOp* const ops = cur.ops_.data();
                for (std::size_t i = 0; i < n; ++i) {
                    const char* e = sync_switch<sync_tables<Target>>(unsigned(ops[i].code >> 2), [&]<unsigned K>() XOFFSET_SYNC_LAMBDA_INLINE -> const char* {
                        if constexpr (K == 0) return kBadKind;
                        else return apply_op<K>(t, ops[i]);
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
