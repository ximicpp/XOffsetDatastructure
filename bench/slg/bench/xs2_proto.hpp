// XS 2.0 shared-delta message format: PROTOTYPE (4.0 W2 step 1, unmerged).
//
// Message:
//   'X' 'S' u8 format(0x02) u8 flags u32 version      (8 bytes, LE)
//   [u64 stream]                                       (KEY flag only)
//   root record                                        (sizeof(tables[0]) raw bytes; plain, no containers)
//   sections until the end:
//     u32 (count << 8 | table << 2 | kind), then count ops of that table and kind
//   ops:
//     leave   u32 id
//     enter   u32 id, whole body
//     change  u32 id, change body
// Whole body of T: its plain byte runs (compile-time ranges, padding and
// container heads skipped), then a container mask (one bit per container,
// schema order; u8..u64 by count) and per set bit: string u32 len + bytes,
// vector u32 n + n * sizeof(E) raw element bytes. Containers not in the mask
// are empty.
// Change body of T: member mask (u8/u16/u32/u64 by member count), then per
// set bit in member order: a plain member's raw bytes (sizeof member), a
// string u32 len + bytes, a vector u32 n + raw elements.
// The id is part of every shared body, so a connection's op is one copy.
#pragma once
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string_view>
#include <tuple>
#include <type_traits>
#include "xoffset_shared_delta.hpp"

namespace xs2 {
namespace xsd = XOffsetDatastructure::xsync::sd_detail;
namespace xsc = XOffsetDatastructure::schema;
using XOffsetDatastructure::xsync::XSyncOut;

#if defined(__GNUC__) || defined(__clang__)
#define XS2_INLINE inline __attribute__((always_inline))
#define XS2_LAMBDA_INLINE __attribute__((always_inline))
#define XS2_NOINLINE __attribute__((noinline))
#else
#define XS2_INLINE inline
#define XS2_LAMBDA_INLINE
#define XS2_NOINLINE
#endif

inline constexpr std::uint8_t kFormat = 0x02;
inline constexpr std::uint8_t kFlagKey = 0x01;
enum Kind : std::uint8_t { enter = 0, change = 1, leave = 2 };
inline constexpr std::size_t kHead = 8;
// Slack every body source and message keeps past its end, so small copies
// can move whole 16-byte words.
inline constexpr std::size_t kSlack = 32;

// n bytes from s to d; may write up to 32 bytes past d + n when n <= 32 (the
// destination keeps kSlack) and read up to 32 past s + n (sources keep it too).
XS2_INLINE void copy_body(char* d, const char* s, std::size_t n) noexcept {
    if (n <= 32) {
        std::memcpy(d, s, 16);
        std::memcpy(d + 16, s + 16, 16);
    } else std::memcpy(d, s, n);
}
// A plain member of n bytes (1, 2, 4, 8, 16 or other).
XS2_INLINE void copy_member(char* d, const char* s, std::uint32_t n) noexcept {
    switch (n) {
    case 1: *d = *s; break;
    case 2: std::memcpy(d, s, 2); break;
    case 4: std::memcpy(d, s, 4); break;
    case 8: std::memcpy(d, s, 8); break;
    case 16: std::memcpy(d, s, 16); break;
    default: std::memcpy(d, s, n); break;
    }
}
template <class M> XS2_INLINE M load(const char* p) noexcept { M v; std::memcpy(&v, p, sizeof v); return v; }
template <class M> XS2_INLINE void store(char* p, M v) noexcept { std::memcpy(p, &v, sizeof v); }

template <class U> constexpr bool is_str() { return xsd::is_fixed_string_v<std::remove_cv_t<U>>; }
template <class U> constexpr bool is_vec() { return xsd::is_fixed_vector_v<std::remove_cv_t<U>>; }

template <class T> struct Rec {
    static constexpr std::size_t N = xsc::member_count_v<T>;
    static_assert(N >= 1 && N <= 64, "xs2 prototype: 1 to 64 members");
    using Mask = std::conditional_t<(N <= 8), std::uint8_t,
                 std::conditional_t<(N <= 16), std::uint16_t, std::conditional_t<(N <= 32), std::uint32_t, std::uint64_t>>>;
    enum : std::uint8_t { kPlain = 0, kStr = 1, kVec = 2 };
    struct Member { std::uint32_t off, size; std::uint8_t kind; };
    static constexpr std::array<Member, N> members = [] {
        std::array<Member, N> a{};
        [&]<std::size_t... I>(std::index_sequence<I...>) {
            ((a[I] = [] {
                 using F = xsc::member_t<T, I>;
                 using U = std::remove_cv_t<typename F::type>;
                 if constexpr (is_str<U>()) return Member{std::uint32_t(F::offset), 0, kStr};
                 else if constexpr (is_vec<U>()) {
                     using E = typename U::value_type;
                     static_assert(std::is_trivially_copyable_v<E> && xsd::ContainersOf<E, 0>::type::size == 0);
                     return Member{std::uint32_t(F::offset), std::uint32_t(sizeof(E)), kVec};
                 } else {
                     static_assert(xsd::ContainersOf<U, 0>::type::size == 0, "xs2 prototype: nested containers");
                     return Member{std::uint32_t(F::offset), std::uint32_t(sizeof(U)), kPlain};
                 }
             }()), ...);
        }(std::make_index_sequence<N>{});
        return a;
    }();
    static constexpr std::uint64_t plain_members = [] {
        std::uint64_t m = 0;
        for (std::size_t i = 0; i < N; ++i) if (members[i].kind == kPlain) m |= std::uint64_t(1) << i;
        return m;
    }();
    static constexpr std::uint64_t container_members = ~plain_members & (N == 64 ? ~std::uint64_t(0) : (std::uint64_t(1) << N) - 1);
    using I = xsd::Info<T>;
    static constexpr std::size_t C = I::container_count;
    using CMask = std::conditional_t<(C <= 8), std::uint8_t,
                  std::conditional_t<(C <= 16), std::uint16_t, std::conditional_t<(C <= 32), std::uint32_t, std::uint64_t>>>;
    // A record whose plain bytes are the whole record (no padding, no containers).
    static constexpr bool flat = C == 0 && I::plain_bytes == sizeof(T);

    // f.template operator()<I>() for member I (compile-time index), I in 0..N-1.
    template <class F> XS2_INLINE static void member(unsigned i, F&& f) {
        [&]<std::size_t... K>(std::index_sequence<K...>) XS2_LAMBDA_INLINE {
            (void)((i == K ? (f.template operator()<K>(), true) : false) || ...);
        }(std::make_index_sequence<N>{});
    }

    // ---- encoding ---------------------------------------------------------

    // Room a whole body of r needs (an upper bound).
    static std::size_t whole_room(const T& r) {
        std::size_t n = I::plain_bytes + sizeof(CMask) + kSlack;
        if constexpr (C > 0) {
            xsd::each(typename I::containers{}, [&]<class Ct, std::size_t Off>(std::uint32_t) {
                const Ct& c = xsd::at<Ct, Off>(r);
                if constexpr (xsd::is_fixed_string_v<Ct>) n += 4 + c.view().size();
                else n += 4 + c.size() * sizeof(typename Ct::value_type);
            });
        }
        return n;
    }
    // Writes the whole body of r at d (whole_room bytes); returns its end.
    static char* put_whole(char* d, const T& r) {
        const auto* s = reinterpret_cast<const char*>(&r);
        if constexpr (flat) {
            std::memcpy(d, s, sizeof(T));
            return d + sizeof(T);
        } else {
            [&]<std::size_t... K>(std::index_sequence<K...>) {
                ((std::memcpy(d, s + I::runs[K].at, I::runs[K].len), d += I::runs[K].len), ...);
            }(std::make_index_sequence<I::run_count>{});
            if constexpr (C > 0) {
                char* const cm = d;
                d += sizeof(CMask);
                CMask m = 0;
                xsd::each(typename I::containers{}, [&]<class Ct, std::size_t Off>(std::uint32_t k) {
                    const Ct& c = xsd::at<Ct, Off>(r);
                    if constexpr (xsd::is_fixed_string_v<Ct>) {
                        const std::string_view v = c.view();
                        if (v.empty()) return;
                        store<std::uint32_t>(d, std::uint32_t(v.size()));
                        std::memcpy(d + 4, v.data(), v.size());
                        d += 4 + v.size();
                    } else {
                        if (c.size() == 0) return;
                        const std::size_t b = c.size() * sizeof(typename Ct::value_type);
                        store<std::uint32_t>(d, std::uint32_t(c.size()));
                        std::memcpy(d + 4, c.data(), b);
                        d += 4 + b;
                    }
                    m = CMask(m | (CMask(1) << k));
                });
                store<CMask>(cm, m);
            }
            return d;
        }
    }
    // Room a change body of the members in mask of r needs.
    static std::size_t change_room(const T& r, std::uint64_t mask) {
        std::size_t n = sizeof(Mask) + kSlack;
        for (std::uint64_t m = mask; m; m &= m - 1) {
            const unsigned i = unsigned(std::countr_zero(m));
            if (members[i].kind == kPlain) n += members[i].size;
            else n += 4 + container_bytes(r, i);
        }
        return n;
    }
    static std::size_t container_bytes(const T& r, unsigned i) {
        std::size_t n = 0;
        member(i, [&]<std::size_t K>() XS2_LAMBDA_INLINE {
            using U = std::remove_cv_t<typename xsc::member_t<T, K>::type>;
            if constexpr (is_str<U>()) n = xsd::at<U, xsc::member_t<T, K>::offset>(r).view().size();
            else if constexpr (is_vec<U>()) n = xsd::at<U, xsc::member_t<T, K>::offset>(r).size() * sizeof(typename U::value_type);
        });
        return n;
    }
    // Writes the change body (mask, then the masked members of r) at d; returns its end.
    static char* put_change(char* d, const T& r, std::uint64_t mask) {
        store<Mask>(d, Mask(mask));
        d += sizeof(Mask);
        const auto* s = reinterpret_cast<const char*>(&r);
        // One test per plain member, each a copy of a compile-time size (a
        // branch per member, each predictable on its own).
        [&]<std::size_t... K>(std::index_sequence<K...>) XS2_LAMBDA_INLINE {
            ((members[K].kind == kPlain && (mask >> K & 1u)
                  ? (void)(std::memcpy(d, s + members[K].off, members[K].size), d += members[K].size)
                  : (void)0),
             ...);
        }(std::make_index_sequence<N>{});
        if constexpr (C > 0) {
            // Containers come after the plain members of a lower index too:
            // members in order, so the receiver reads them in the same order.
            // (The receiver applies plain members first, then containers.)
            for (std::uint64_t m = mask & container_members; m; m &= m - 1) {
                member(unsigned(std::countr_zero(m)), [&]<std::size_t K>() XS2_LAMBDA_INLINE {
                    using U = std::remove_cv_t<typename xsc::member_t<T, K>::type>;
                    constexpr std::size_t Off = xsc::member_t<T, K>::offset;
                    if constexpr (is_str<U>()) {
                        const std::string_view v = xsd::at<U, Off>(r).view();
                        store<std::uint32_t>(d, std::uint32_t(v.size()));
                        std::memcpy(d + 4, v.data(), v.size());
                        d += 4 + v.size();
                    } else if constexpr (is_vec<U>()) {
                        const U& c = xsd::at<U, Off>(r);
                        const std::size_t b = c.size() * sizeof(typename U::value_type);
                        store<std::uint32_t>(d, std::uint32_t(c.size()));
                        if (b) std::memcpy(d + 4, c.data(), b);
                        d += 4 + b;
                    }
                });
            }
        }
        return d;
    }
    // Plain records (no containers): the members of now that differ from
    // prev, as a change body at d (prev takes them). Returns the end, or
    // nullptr when nothing differs.
    static char* put_diff(char* d, const T& now, T& prev) requires (C == 0) {
        char* const md = d;
        d += sizeof(Mask);
        Mask mask = 0;
        const auto* s = reinterpret_cast<const char*>(&now);
        auto* o = reinterpret_cast<char*>(&prev);
        [&]<std::size_t... K>(std::index_sequence<K...>) XS2_LAMBDA_INLINE {
            ((std::memcmp(s + members[K].off, o + members[K].off, members[K].size) != 0
                  ? (void)(std::memcpy(d, s + members[K].off, members[K].size), std::memcpy(o + members[K].off, s + members[K].off, members[K].size),
                           d += members[K].size, mask = Mask(mask | (Mask(1) << K)))
                  : (void)0),
             ...);
        }(std::make_index_sequence<N>{});
        if (!mask) return nullptr;
        store<Mask>(md, mask);
        return d;
    }

    // ---- applying (bodies trusted) -------------------------------------------

    static const char* apply_whole(const char* p, T& r) {
        auto* d = reinterpret_cast<char*>(&r);
        if constexpr (flat) {
            std::memcpy(d, p, sizeof(T));
            return p + sizeof(T);
        } else {
            [&]<std::size_t... K>(std::index_sequence<K...>) XS2_LAMBDA_INLINE {
                ((std::memcpy(d + I::runs[K].at, p, I::runs[K].len), p += I::runs[K].len), ...);
            }(std::make_index_sequence<I::run_count>{});
            if constexpr (C > 0) {
                const CMask m = load<CMask>(p);
                p += sizeof(CMask);
                xsd::each(typename I::containers{}, [&]<class Ct, std::size_t Off>(std::uint32_t k) {
                    Ct& c = xsd::at<Ct, Off>(r);
                    if (!(m >> k & 1u)) {
                        if constexpr (xsd::is_fixed_string_v<Ct>) { if (!c.view().empty()) c.clear(); }
                        else { if (c.size()) c.clear(); }
                        return;
                    }
                    p = put_container(c, p);
                });
            }
            return p;
        }
    }
    template <class Ct> XS2_INLINE static const char* put_container(Ct& c, const char* p) {
        const std::uint32_t n = load<std::uint32_t>(p);
        p += 4;
        if constexpr (xsd::is_fixed_string_v<Ct>) {
            static_cast<XOffsetDatastructure::XFixedString&>(c).assign(std::string_view(p, n));
            return p + n;
        } else {
            using E = typename Ct::value_type;
            c.assign_range(std::span<const E>(reinterpret_cast<const E*>(p), n));
            return p + std::size_t(n) * sizeof(E);
        }
    }
    static const char* apply_change(const char* p, T& r) {
        const std::uint64_t mask = load<Mask>(p);
        p += sizeof(Mask);
        auto* d = reinterpret_cast<char*>(&r);
        [&]<std::size_t... K>(std::index_sequence<K...>) XS2_LAMBDA_INLINE {
            ((members[K].kind == kPlain && (mask >> K & 1u)
                  ? (void)(std::memcpy(d + members[K].off, p, members[K].size), p += members[K].size)
                  : (void)0),
             ...);
        }(std::make_index_sequence<N>{});
        if constexpr (C > 0) {
            for (std::uint64_t m = mask & container_members; m; m &= m - 1) {
                member(unsigned(std::countr_zero(m)), [&]<std::size_t K>() XS2_LAMBDA_INLINE {
                    using U = std::remove_cv_t<typename xsc::member_t<T, K>::type>;
                    if constexpr (is_str<U>() || is_vec<U>()) p = put_container(xsd::at<U, xsc::member_t<T, K>::offset>(r), p);
                });
            }
        }
        return p;
    }
};

// ---- writer ------------------------------------------------------------------

// Builds one message in out: the head and root record, then sections.
class Writer {
public:
    template <class Root>
    Writer(XSyncOut& out, std::uint8_t flags, std::uint64_t stream, std::uint32_t version, const Root& root) : out_(&out) {
        static_assert(Rec<Root>::flat, "xs2: the root record is plain bytes");
        out.clear();
        char* d = out.tail(kHead + 8 + sizeof(Root) + kSlack);
        d[0] = 'X'; d[1] = 'S'; d[2] = char(kFormat); d[3] = char(flags);
        store<std::uint32_t>(d + 4, version);
        std::size_t n = kHead;
        if (flags & kFlagKey) { store<std::uint64_t>(d + n, stream); n += 8; }
        std::memcpy(d + n, &root, sizeof(Root));
        q_ = d + n + sizeof(Root);
        end_ = out.data() + out.capacity();
    }
    Writer(const Writer&) = delete;
    Writer& operator=(const Writer&) = delete;

    // Leaves of n ids (one section, one capacity check).
    template <class Id> XS2_INLINE void leaves(unsigned table, const Id* ids, std::size_t n) {
        // No branch on n: the header is written either way and kept when n != 0.
        char* d = need(4 + 4 * n);
        store<std::uint32_t>(d, std::uint32_t(n) << 8 | code(table, Kind::leave));
        for (std::size_t i = 0; i < n; ++i) store<std::uint32_t>(d + 4 + 4 * i, std::uint32_t(ids[i]));
        q_ = d + (n ? 4 + 4 * n : 0);
    }
    // A section of ops of one table and kind: begin, ops, end (an empty
    // section is dropped).
    XS2_INLINE void begin(unsigned table, Kind kind) {
        char* d = need(4);
        sec_ = d;
        cur_ = code(table, kind);
        n_ = 0;
        q_ = d + 4;
    }
    XS2_INLINE void end() noexcept {
        store<std::uint32_t>(sec_, n_ << 8 | cur_);   // dropped below when empty
        q_ = n_ ? q_ : sec_;
    }
    // An op whose body (id included) is len bytes at body (kSlack readable past it).
    XS2_INLINE void body(const char* body, std::size_t len) {
        char* d = need(len + kSlack);
        copy_body(d, body, len);
        q_ = d + len;
        ++n_;
    }
    // An op whose body is len bytes (0: no op) of at most N at body: N bytes
    // are copied either way (N + kSlack readable at body), no branch.
    template <std::size_t N> XS2_INLINE void body_upto(const char* body, std::size_t len) {
        char* d = need(N + kSlack);
        std::memcpy(d, body, N);
        q_ = d + len;
        n_ += len != 0;
    }
    // Slot A/B: as body / body_upto, with the op's leading u32 (the shared
    // body's id) replaced by this client's slot.
    XS2_INLINE void body_slot(const char* body, std::size_t len, std::uint32_t slot) {
        char* d = need(len + kSlack);
        copy_body(d, body, len);
        store<std::uint32_t>(d, slot);
        q_ = d + len;
        ++n_;
    }
    // Room for n more bytes now (a section's ops then skip the check).
    XS2_INLINE void reserve(std::size_t n) { (void)need(n); }
    // body_slot without the capacity check: the caller reserved len + kSlack.
    XS2_INLINE void body_slot_in_room(const char* body, std::size_t len, std::uint32_t slot) {
        char* d = q_;
        copy_body(d, body, len);
        store<std::uint32_t>(d, slot);
        q_ = d + len;
        ++n_;
    }
    template <std::size_t N> XS2_INLINE void body_upto_slot(const char* body, std::size_t len, std::uint32_t slot) {
        char* d = need(N + kSlack);
        std::memcpy(d, body, N);
        store<std::uint32_t>(d, slot);   // past q_ when len == 0: overwritten later
        q_ = d + len;
        n_ += len != 0;
    }
    // An op of a flat record: the id, then the record's bytes.
    template <class R> XS2_INLINE void flat(std::uint32_t id, const R& r) {
        static_assert(Rec<R>::flat);
        char* d = need(4 + sizeof(R) + kSlack);
        store<std::uint32_t>(d, id);
        std::memcpy(d + 4, &r, sizeof(R));
        q_ = d + 4 + sizeof(R);
        ++n_;
    }
    std::size_t finish() noexcept {
        out_->commit(std::size_t(q_ - out_->data()) - out_->size());
        return out_->size();
    }

private:
    static constexpr unsigned code(unsigned table, Kind kind) noexcept { return table << 2 | unsigned(kind); }
    // The writer's address never leaves it (grow is static), so its cursor
    // can live in registers.
    XS2_INLINE char* need(std::size_t n) {
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
    XS2_NOINLINE static Span grow(XSyncOut& out, std::size_t used, std::size_t n) {
        out.commit(used - out.size());
        out.tail(n);
        return {out.data(), out.data() + out.capacity()};
    }
    XSyncOut* out_;
    char* q_;
    char* end_;
    char* sec_ = nullptr;
    unsigned cur_ = 0;
    std::uint32_t n_ = 0;
};

// ---- receiving (trust tier: admit) --------------------------------------------

struct Cursor {
    std::uint64_t stream = 0;
    std::uint32_t version = 0;
    bool keyed = false;
};

inline constexpr const char* kBadHead = "XS2: bad header";
inline constexpr const char* kGap = "XS2: no key message applied or version gap";
inline constexpr const char* kUnknownTable = "XS2: unknown table";
inline constexpr const char* kBadKind = "XS2: invalid op kind for this table";
inline constexpr const char* kNotHeld = "XS2: an op on an entry the receiver does not hold";
inline constexpr const char* kRefused = "XS2: the receiver could not add an entry";
inline constexpr const char* kTrunc = "XS2: message ends inside a section";

template <class Target, unsigned K> using rec_t = std::tuple_element_t<K, typename Target::tables>;

// Header, stream and version, every section's table and kind, every id held
// (entered ids not held); bodies are the sender's. Returns nullptr or why
// the message was refused (a message refused after its header may be partly
// applied: the cursor is then empty).
template <class Target>
const char* admit(std::string_view msg, Cursor& cur, Target& t) {
    using R0 = rec_t<Target, 0>;
    const char* p = msg.data();
    const char* const end = p + msg.size();
    if (msg.size() < kHead + sizeof(R0) || p[0] != 'X' || p[1] != 'S' || static_cast<unsigned char>(p[2]) != kFormat) return kBadHead;
    const unsigned flags = static_cast<unsigned char>(p[3]);
    const std::uint32_t version = load<std::uint32_t>(p + 4);
    p += kHead;
    std::uint64_t stream = cur.stream;
    if (flags) {
        if (flags != kFlagKey || std::size_t(end - p) < 8 + sizeof(R0)) return kBadHead;
        stream = load<std::uint64_t>(p);
        p += 8;
    } else if (!cur.keyed || version != std::uint32_t(cur.version + 1)) return kGap;
    cur.keyed = false;
    {
        const std::int64_t h = t.template find<0>(0);
        std::memcpy(&t.template record<0>(h), p, sizeof(R0));
        p += sizeof(R0);
        if constexpr (requires { t.template applied<0>(0u, h); }) t.template applied<0>(0u, h);
    }
    constexpr unsigned NT = unsigned(std::tuple_size_v<typename Target::tables>);
    while (p != end) {
        if (end - p < 4) [[unlikely]] return kTrunc;
        const std::uint32_t w = load<std::uint32_t>(p);
        p += 4;
        const unsigned kind = w & 3u, table = (w >> 2) & 0x3fu;
        std::uint32_t n = w >> 8;
        const char* err = xsd::sync_switch<NT>(table, [&]<unsigned K>() XS2_LAMBDA_INLINE -> const char* {
            if constexpr (K == 0) return kBadKind;
            else {
                using R = rec_t<Target, K>;
                constexpr bool keyed = Target::template keyed<K>;
                if (kind == Kind::change) {
                    for (; n; --n) {
                        if (end - p < 4) [[unlikely]] return kTrunc;
                        const std::uint32_t id = load<std::uint32_t>(p);
                        const std::int64_t h = t.template find<K>(id);
                        if (h < 0) [[unlikely]] return kNotHeld;
                        p = Rec<R>::apply_change(p + 4, t.template record<K>(h));
                    }
                    return nullptr;
                }
                if (kind == Kind::enter) {
                    for (; n; --n) {
                        if (end - p < 4) [[unlikely]] return kTrunc;
                        const std::uint32_t id = load<std::uint32_t>(p);
                        std::int64_t h;
                        if constexpr (keyed) {
                            h = t.template enter<K>(id);
                            if (h < 0) [[unlikely]] return kRefused;
                        } else {
                            h = t.template find<K>(id);
                            if (h < 0) [[unlikely]] return kNotHeld;
                        }
                        p = Rec<R>::apply_whole(p + 4, t.template record<K>(h));
                    }
                    return nullptr;
                }
                if constexpr (keyed) {
                    if (kind == Kind::leave) {
                        if (std::size_t(end - p) / 4 < n) [[unlikely]] return kTrunc;
                        for (; n; --n) {
                            const std::uint32_t id = load<std::uint32_t>(p);
                            p += 4;
                            const std::int64_t h = t.template find<K>(id);
                            if (h < 0) [[unlikely]] return kNotHeld;
                            t.template leave<K>(id, h);
                        }
                        return nullptr;
                    }
                }
                return kBadKind;
            }
        });
        if (err) [[unlikely]] return err == xsd::kUnknownTable ? kUnknownTable : err;
    }
    cur.stream = stream;
    cur.version = version;
    cur.keyed = true;
    return nullptr;
}

} // namespace xs2
