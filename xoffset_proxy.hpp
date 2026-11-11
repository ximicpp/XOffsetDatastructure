#ifndef X_OFFSET_PROXY_HPP
#define X_OFFSET_PROXY_HPP

// Growth policy part 2: fast proxies (XRef) and pin scopes (read / edit).
//
// A Relocating buffer moves when it outgrows its reservation, so a plain
// reference into it can dangle. Two ways to reach the data stay safe:
//
//   XRef<T>   (buf.ref<Root>(), then .field<&T::m>(), [i], find(k) ...)
//             A proxy holds the buffer's control block (XCtl), a cached base
//             and version, and a path (root offset, container + index, map +
//             key). Every access compares one version word; an allocating
//             operation that would outgrow the reservation moves the buffer
//             first (make_room), then runs. Container proxies keep the size
//             in a register and only compare it with memory, so a push_back
//             loop runs at the speed of a plain reference.
//   edit / read (buf.edit<Root>([&](Root& r) { ... }))
//             Plain references at plain speed. While a scope is open the
//             buffer does not move (an operation that would need to returns
//             `borrowed`), and blocks the containers retire wait until the
//             scope closes, so a reference made inside the scope never points
//             into reused storage. The callable may not return a pointer,
//             reference, string_view or span (checked at compile time).
//
// Failure is soft: nothing throws or aborts. An allocating operation that
// cannot be done has no effect, returns false (or an empty proxy) and records
// the first failure in the buffer (XBuffer::error(): capacity_exceeded,
// out_of_storage, borrowed, stale, out_of_range), and the buffer refuses to
// emit a wire until clear_error().
//
// An allocating operation first runs in place. With exceptions, an
// allocation the payload has no room for throws before anything changes; the
// proxy catches it, makes room (the buffer may move) and runs the operation
// again out of line, so the common case pays no room check. Without
// exceptions it checks the room first (and a recorded failure refuses it).
// Reads through an empty or stale proxy return a value-initialized T.
//
// Element proxies hold container + index (map: key + cached index), checked
// against the size on every access: after the container changed they read
// another element or nothing, never a retired block.

// Growth policy part 4: batch operations (append, assign, emplace_n, batch
// insert into maps and sets, XRef::batch scopes).
#define XOFFSET_PROXY_BATCH_API 1

namespace XOffsetDatastructure {

#if defined(_MSC_VER) && !defined(__clang__)
#define XOFFSET_PROXY_INLINE __forceinline
#elif defined(__GNUC__) || defined(__clang__)
#define XOFFSET_PROXY_INLINE inline __attribute__((always_inline))
#else
#define XOFFSET_PROXY_INLINE inline
#endif

#if defined(_MSC_VER) && !defined(__clang__)
#define XOFFSET_PROXY_ASSUME(x) __assume(x)
#elif defined(__clang__)
#define XOFFSET_PROXY_ASSUME(x) __builtin_assume(x)
#elif defined(__GNUC__)
// Not [[assume]]: GCC outlines its expression and passes the address of the
// enclosing object, which keeps a proxy in memory (dead stores every op).
#define XOFFSET_PROXY_ASSUME(x) do { if (!(x)) __builtin_unreachable(); } while (0)
#else
#define XOFFSET_PROXY_ASSUME(x) ((void)0)
#endif

namespace proxy_detail {
    // The block of every empty proxy: odd version, never live.
    inline detail::XCtl* dead_ctl() noexcept {
        static detail::XCtl ctl;
        return &ctl;
    }

    // Byte offset of a data member, folded to a constant by the compiler.
    template <class C, class M>
    XOFFSET_PROXY_INLINE std::uint32_t member_offset(M C::*mp) noexcept {
        constexpr std::uintptr_t probe = 4096u * alignof(C);
        return static_cast<std::uint32_t>(
            reinterpret_cast<std::uintptr_t>(&(reinterpret_cast<C*>(probe)->*mp)) - probe);
    }

    template <class T> struct member_traits;
    template <class C, class M> struct member_traits<M C::*> {
        using class_type = C;
        using type = M;
    };

    // Extra arena bytes an argument can need when it is copied in (an upper
    // bound), and the bytes of one more element.
    using fixed_detail::deep_bytes;
    template <class E>
    XOFFSET_PROXY_INLINE std::size_t grow_bytes(std::uint32_t capacity) noexcept {
        return fixed_detail::grow_bytes<E>(capacity);
    }

    // The payload lineage a proxy belongs to, with the base and version it
    // last saw. A default Anchor is empty (the dead block): always stale.
    struct Anchor {
        detail::XCtl* ctl = dead_ctl();
        std::uint64_t version = 0;

        Anchor() noexcept = default;
        explicit Anchor(detail::XCtl* c) noexcept : ctl(c ? c : dead_ctl()), version(c ? c->version : 0) {}
        XOFFSET_PROXY_INLINE static Anchor make(detail::XCtl* c, std::uint64_t v) noexcept {
            Anchor a;
            a.ctl = c;
            a.version = v;
            return a;
        }
        // A copy built from the fields (see copy() of the paths).
        XOFFSET_PROXY_INLINE Anchor copy() const noexcept { return make(ctl, version); }

        // Two words, passed and returned in registers. The out-of-line
        // helpers take the anchor and the path by value and return values,
        // never `this` or a member's address: a proxy whose address never
        // escapes lives in registers (GCC keeps a whole proxy in memory once
        // a member's address reaches a call). The base is read from
        // the block (same cache line as the version) rather than carried:
        // a loop then carries only the version.

        // The payload base, or null when the lineage is gone or replaced
        // (or the proxy is empty).
        XOFFSET_PROXY_INLINE char* sync() noexcept {
            if (ctl->version != version) [[unlikely]] {
                const std::uint64_t now = resync_of(ctl, version);
                if (now & 1u) return nullptr;
                version = now;
            }
            char* const base = ctl->base;
            // A live lineage always has a payload (resync_of refuses a null
            // base, and a version match means it was seen live).
            XOFFSET_PROXY_ASSUME(base != nullptr);
            return base;
        }
        // The current version, or 1 (odd: stale) when the proxy's lineage
        // is gone or replaced.
        static XOFFSET_DETAIL_NOINLINE std::uint64_t resync_of(const detail::XCtl* ctl, std::uint64_t version) noexcept {
            const auto now = ctl->version;
            if ((now & 1u) || version < ctl->birth || !ctl->base) return 1;
            return now;
        }
        // The object at `path` now, or null (stale, out of range, missing).
        template <class T, class P>
        XOFFSET_PROXY_INLINE T* at(const P& path) noexcept {
            char* const base = sync();
            return base ? reinterpret_cast<T*>(path.resolve(base)) : nullptr;
        }
        static bool live_at(const detail::XCtl* ctl, std::uint64_t version) noexcept {
            return !(ctl->version & 1u) && version >= ctl->birth && ctl->base;
        }
        bool live() const noexcept { return live_at(ctl, version); }
        // Records a failure in the buffer (the first one stays).
        static XOFFSET_DETAIL_NOINLINE void fail_at(detail::XCtl* ctl, std::uint64_t version, XErrc code) noexcept {
            if (live_at(ctl, version) && ctl->sticky == XErrc::ok) ctl->sticky = code;
        }
        // A write through a proxy that names nothing: out_of_range, or
        // stale (nothing recorded) when its buffer is gone.
        static XOFFSET_DETAIL_NOINLINE void miss_at(detail::XCtl* ctl, std::uint64_t version) noexcept {
            if (live_at(ctl, version) && ctl->sticky == XErrc::ok) ctl->sticky = XErrc::out_of_range;
        }
        XOFFSET_PROXY_INLINE void fail(XErrc code) const noexcept { fail_at(ctl, version, code); }
        // An allocation of up to `bytes` can run in place now: no recorded
        // failure, and the committed payload has the room (so the operation
        // can neither fail nor move the payload).
        XOFFSET_PROXY_INLINE bool room(std::size_t bytes) const noexcept {
            auto* core = ctl->core;
            return ctl->sticky == XErrc::ok && core && bytes <= XBufferCore::MAX_PAYLOAD &&
                   core->has_room(static_cast<XBufferCore::size_type>(bytes));
        }
        XOFFSET_PROXY_INLINE void miss() const noexcept { miss_at(ctl, version); }

        // The allocating slow path: room for `bytes` more (the buffer may
        // move), then `op(base)` with the new base. False (and a recorded
        // error) when the room cannot be made or the operation fails.
        template <class Op>
        bool run_allocating(std::size_t bytes, Op&& op) noexcept {
            if (!sync()) return false;
            if (ctl->sticky != XErrc::ok) return false;
            auto* core = ctl->core;
            if (!core) return false;
            if (bytes > XBufferCore::MAX_PAYLOAD) {
                fail(XErrc::capacity_exceeded);
                return false;
            }
            const XErrc room = core->make_room(static_cast<XBufferCore::size_type>(bytes));
            if (room != XErrc::ok) [[unlikely]] {
                fail(room);
                return false;
            }
            version = ctl->version;
            return run_guarded(op);
        }

        template <class Op>
        bool run_guarded(Op& op) noexcept {
#if XOFFSET_NO_EXCEPTIONS
            detail::SoftFailScope soft;
            op(ctl->base);
            if (soft.failed()) [[unlikely]] {
                fail(soft.code());
                return false;
            }
            return true;
#else
            try {
                op(ctl->base);
                return true;
            } catch (const XException& e) {
                fail(e.code() == XErrc::unspecified ? XErrc::out_of_storage : e.code());
            } catch (const std::bad_alloc&) {
                fail(XErrc::host_allocation_failed);
            } catch (...) {
                fail(XErrc::unspecified);
            }
            return false;
#endif
        }
    };

    // ---- paths: where the object is, given the payload base ----------------

    // Path fields are word-sized: a compiler passes two 32-bit fields of a
    // struct in one register and keeps them packed, unpacking on every
    // access.

    // A fixed payload offset: the root and the members of fixed objects.
    struct RootPath {
        std::size_t off = 0;
        XOFFSET_PROXY_INLINE char* resolve(char* base) const noexcept { return base + off; }
        RootPath plus(std::uint32_t bytes) const noexcept { return {off + bytes}; }
        // A copy built from the fields: what the proxies hand to an
        // out-of-line call. (GCC passes a member copied as a whole from the
        // proxy's own storage, and then keeps the whole proxy in memory.)
        XOFFSET_PROXY_INLINE RootPath copy() const noexcept { return {off}; }
    };

    // Element `index` of the vector at `parent` (+ `sub` bytes into it).
    template <class Parent, class Vec>
    struct IndexPath {
        Parent parent;
        std::size_t index = 0;
        std::size_t sub = 0;
        XOFFSET_PROXY_INLINE char* resolve(char* base) const noexcept {
            char* at = parent.resolve(base);
            if (!at) [[unlikely]] return nullptr;
            auto* vec = reinterpret_cast<Vec*>(at);
            if (index >= vec->size_) [[unlikely]] return nullptr;
            using E = typename Vec::value_type;
            return reinterpret_cast<char*>(
                       fixed_detail::resolve_rel32_live<E>(&vec->data_delta, vec->data_delta) + index) + sub;
        }
        IndexPath plus(std::uint32_t bytes) const noexcept { return {parent, index, sub + bytes}; }
        XOFFSET_PROXY_INLINE IndexPath copy() const noexcept { return {parent.copy(), index, sub}; }
    };

    // Where a container proxy's container is now. A container at a fixed
    // payload offset (a member of the root) caches its address with the
    // version it was computed at, so a loop on it compares one word and
    // carries the address in a register; any other path resolves on every
    // access. Null with `absent` false: the proxy is stale; with `absent`
    // true: the path names nothing (out of range, missing key).
    template <class T, class Path>
    struct Head {
        XOFFSET_PROXY_INLINE T* find(Anchor& a, const Path& path, bool& absent) noexcept {
            char* const base = a.sync();
            if (!base) [[unlikely]] {
                absent = false;
                return nullptr;
            }
            T* const at = reinterpret_cast<T*>(path.resolve(base));
            absent = at == nullptr;
            return at;
        }
        XOFFSET_PROXY_INLINE Anchor anchor(const Anchor& a) const noexcept { return a.copy(); }
        XOFFSET_PROXY_INLINE void forget() noexcept {}
    };
    template <class T>
    struct Head<T, RootPath> {
        T* at = nullptr;
        std::uint64_t version = 0;   // never a block's version: the first use resolves

        XOFFSET_PROXY_INLINE T* find(Anchor& a, const RootPath& path, bool& absent) noexcept {
            absent = false;
            if (a.ctl->version == version) [[likely]] {
                T* const p = at;
                XOFFSET_PROXY_ASSUME(p != nullptr);
                return p;
            }
            // Both fields are set on every path (a stale proxy gets version
            // 0, which never matches), so a loop carries them without copies.
            const Found f = refind(a.copy(), path.off);
            at = f.at;
            version = f.version;
            return f.at;
        }
        // The anchor to hand to an out-of-line path: the newer of the two
        // versions seen live. (find does not update the proxy's own anchor,
        // so a loop on this container carries one version, not two.)
        XOFFSET_PROXY_INLINE Anchor anchor(const Anchor& a) const noexcept {
            return Anchor::make(a.ctl, version > a.version ? version : a.version);
        }
        // The out-of-line path may have moved the payload: resolve again.
        XOFFSET_PROXY_INLINE void forget() noexcept {
            at = nullptr;
            version = 0;
        }

    private:
        struct Found {
            T* at;
            std::uint64_t version;
        };
        static XOFFSET_DETAIL_NOINLINE Found refind(Anchor a, std::size_t off) noexcept {
            // A version this cache saw live is as good as the proxy's own.
            char* const base = a.sync();
            if (!base) return {nullptr, 0};
            return {reinterpret_cast<T*>(base + off), a.version};
        }
    };

    template <class K>
    using lookup_key_t = std::conditional_t<std::is_base_of_v<XFixedString, K>, std::string, K>;

    // The value of `key` in the map at `parent` (+ `sub` bytes into the
    // entry). The index is a hint, checked against the key on every access.
    template <class Parent, class Map>
    struct KeyPath {
        using K = typename Map::key_type;
        using Entry = typename Map::value_type;
        Parent parent;
        lookup_key_t<K> key{};
        mutable std::uint32_t hint = 0;
        std::uint32_t sub = 0;

        XOFFSET_PROXY_INLINE char* resolve(char* base) const noexcept {
            char* at = parent.resolve(base);
            if (!at) [[unlikely]] return nullptr;
            auto* map = reinterpret_cast<Map*>(at);
            const std::uint32_t n = map->size_;
            if (n == 0) return nullptr;
            Entry* items = fixed_detail::resolve_rel32_live<Entry>(&map->data_delta, map->data_delta);
            if (hint >= n || !(items[hint].first == key)) [[unlikely]] {
                auto it = map->find(key);
                if (it == map->end()) return nullptr;
                hint = static_cast<std::uint32_t>(std::to_address(it) - items);
            }
            return reinterpret_cast<char*>(items + hint) + sub;
        }
        KeyPath plus(std::uint32_t bytes) const noexcept { return {parent, key, hint, sub + bytes}; }
        KeyPath copy() const noexcept { return {parent.copy(), key, hint, sub}; }
    };

    // The most elements a container of E can hold (XFixedVector::max_size).
    template <class E>
    inline constexpr std::size_t max_elements = (XBufferCore::MAX_PAYLOAD - 8u) / sizeof(E);

    // A batch source that lives in the payload is copied out first (the
    // payload may move or the block be retired by the batch itself): the
    // source to read, or null (host_allocation_failed recorded).
    template <class X>
    const X* outside_payload(const Anchor& a, const X* src, std::size_t count, std::vector<X>& keep) noexcept {
        auto* core = a.ctl->core;
        if (!src || count == 0 || !core) return src;
        const auto lo = reinterpret_cast<std::uintptr_t>(a.ctl->base);
        const auto at = reinterpret_cast<std::uintptr_t>(src);
        if (at < lo || at - lo >= core->get_buffer().size()) return src;
        XOFFSET_TRY {
            keep.assign(src, src + count);
        } XOFFSET_CATCH(...) {
            a.fail(XErrc::host_allocation_failed);
            return nullptr;
        }
        return keep.data();
    }

    // Capacity for `need` elements in the container at `path` (one regrow,
    // to at least twice the old capacity). False (error recorded) when it
    // cannot be had.
    template <class C, class Path>
    bool reserve_at(Anchor& a, const Path& path, std::size_t need) noexcept {
        char* const base = a.sync();
        if (!base) return false;
        const C* c = reinterpret_cast<const C*>(path.resolve(base));
        if (!c) {
            a.miss();
            return false;
        }
        if (need <= c->capacity_) return true;
        if (need > max_elements<typename C::value_type>) {
            a.fail(XErrc::capacity_exceeded);
            return false;
        }
        using E = typename C::value_type;
        const std::size_t new_cap =
            std::max(need, std::min<std::size_t>(max_elements<typename C::value_type>, std::size_t(c->capacity_) * 2u));
        return a.run_allocating(new_cap * sizeof(E) + alignof(E) + 24u, [&](char* base) {
            if (C* w = reinterpret_cast<C*>(path.resolve(base))) w->reserve(new_cap);
        });
    }

    enum class Kind { value, record, string, vector, set, map };

    template <class E> std::true_type is_vec_test(const XFixedVector<E>*);
    std::false_type is_vec_test(...);
    template <class E> std::true_type is_set_test(const XFixedFlatSet<E>*);
    std::false_type is_set_test(...);
    template <class K, class V> std::true_type is_map_test(const XFixedFlatMap<K, V>*);
    std::false_type is_map_test(...);

    template <class T>
    consteval Kind kind_of() {
        if constexpr (std::is_base_of_v<XFixedString, T>) return Kind::string;
        else if constexpr (decltype(is_map_test(static_cast<T*>(nullptr)))::value) return Kind::map;
        else if constexpr (decltype(is_set_test(static_cast<T*>(nullptr)))::value) return Kind::set;
        else if constexpr (decltype(is_vec_test(static_cast<T*>(nullptr)))::value) return Kind::vector;
        else if constexpr (std::is_class_v<T> && schema::is_described_v<T>) return Kind::record;
        else return Kind::value;
    }

    // A type that may not leave a pin scope: it can point into the payload.
    template <class R> struct escapes : std::false_type {};
    template <class R> struct escapes<R*> : std::true_type {};
    template <class R> struct escapes<R&> : std::true_type {};
    template <class R> struct escapes<R&&> : std::true_type {};
    template <class C, class Tr> struct escapes<std::basic_string_view<C, Tr>> : std::true_type {};
    template <class E, std::size_t N> struct escapes<std::span<E, N>> : std::true_type {};
} // namespace proxy_detail

template <class T, class Path = proxy_detail::RootPath,
          proxy_detail::Kind K = proxy_detail::kind_of<std::remove_cv_t<T>>()>
class XRef;

// Hook for named field accessors (growth policy part 3): XOFFSET_RECORD
// specializes it for its record to give XRef<T> member functions.
template <class T, class Self>
struct XRecordAccess {};

namespace proxy_detail {
    // Common state of every proxy: the lineage and the path.
    template <class T, class Path>
    struct RefBase {
        Anchor a;
        Path path;

        RefBase() noexcept = default;
        RefBase(const Anchor& anchor, const Path& p) noexcept : a(anchor), path(p) {}

        // The object, or null (stale, out of range, missing key).
        XOFFSET_PROXY_INLINE T* get_ptr() noexcept {
            return a.template at<T>(path);
        }
        XOFFSET_PROXY_INLINE const T* get_ptr() const noexcept {
            return const_cast<RefBase*>(this)->get_ptr();
        }

        // Proxy present: names an object now.
        bool valid() const noexcept { return get_ptr() != nullptr; }
        explicit operator bool() const noexcept { return valid(); }
        // The buffer's first recorded failure.
        XErrc error() const noexcept { return a.live() ? a.ctl->sticky : XErrc::stale; }

        template <class M>
        XRef<M, Path> member_at(std::uint32_t offset) const noexcept {
            return XRef<M, Path>(a, path.plus(offset));
        }

        // Batch scope over this object (growth policy part 4): f(T&) with a
        // plain reference, the path resolved once. Room for `expected_growth`
        // bytes is made first (the buffer may move then, while no reference
        // exists); while f runs the payload is pinned as in XBuffer::edit
        // (an operation that would move it fails with `borrowed`; retired
        // blocks wait). False when the proxy names nothing, the buffer has a
        // recorded failure, or an operation inside failed (the first failure
        // is error()). f must return void.
        template <class F>
        bool batch(F&& f, std::size_t expected_growth = 0) {
            static_assert(std::is_void_v<std::invoke_result_t<F&, T&>>,
                          "batch(f): f must return void (a value made inside may point into the payload)");
            T* p = batch_enter(expected_growth);
            if (!p) [[unlikely]] return false;
            BatchExit exit{a.ctl};
#if XOFFSET_NO_EXCEPTIONS
            detail::SoftFailScope soft;
            f(*p);
            if (soft.failed()) [[unlikely]] a.fail(soft.code());
#else
            try {
                f(*p);
            } catch (const XException& e) {
                a.fail(e.code() == XErrc::unspecified ? XErrc::out_of_storage : e.code());
            } catch (const std::bad_alloc&) {
                a.fail(XErrc::host_allocation_failed);
            }
#endif
            return a.ctl->sticky == XErrc::ok;
        }

    private:
        struct BatchExit {
            detail::XCtl* ctl;
            ~BatchExit() { XBufferCore::pin_exit(ctl); }
        };
        // Resolves, makes room, pins: the object, or null (nothing pinned).
        XOFFSET_DETAIL_NOINLINE T* batch_enter(std::size_t expected_growth) noexcept {
            char* const base = a.sync();
            if (!base || a.ctl->sticky != XErrc::ok) return nullptr;
            auto* core = a.ctl->core;
            if (!core || !path.resolve(base)) {
                a.miss();
                return nullptr;
            }
            if (expected_growth != 0 && !core->pinned() && !a.room(expected_growth)) {
                const XErrc room = expected_growth > XBufferCore::MAX_PAYLOAD
                    ? XErrc::capacity_exceeded
                    : core->make_room(static_cast<XBufferCore::size_type>(expected_growth));
                if (room != XErrc::ok) {
                    a.fail(room);
                    return nullptr;
                }
                a.version = a.ctl->version;
            }
            T* p = reinterpret_cast<T*>(path.resolve(a.ctl->base));
            if (!p || !core->pin_enter()) return nullptr;
            return p;
        }
    };
} // namespace proxy_detail

// ---- scalars, enums and other trivially copyable values ---------------------
template <class T, class Path>
class XRef<T, Path, proxy_detail::Kind::value> : public proxy_detail::RefBase<T, Path> {
    using Base = proxy_detail::RefBase<T, Path>;
    static_assert(std::is_trivially_copyable_v<T>, "XRef: a value proxy needs a trivially copyable type");

public:
    using value_type = T;
    using Base::Base;
    XRef(const XRef&) noexcept = default;

    // Read: the value, or T{} when the proxy names nothing.
    XOFFSET_PROXY_INLINE T get() const noexcept {
        if (const T* p = Base::get_ptr()) [[likely]] return *p;
        return T{};
    }
    XOFFSET_PROXY_INLINE operator T() const noexcept { return get(); }

    // Write: false (and out_of_range / stale recorded) when the proxy
    // names nothing.
    XOFFSET_PROXY_INLINE bool set(const T& value) noexcept {
        if (T* p = Base::get_ptr()) [[likely]] {
            *p = value;
            return true;
        }
        miss();
        return false;
    }
    XOFFSET_PROXY_INLINE XRef& operator=(const T& value) noexcept {
        set(value);
        return *this;
    }
    // Assignment between proxies copies the value (reference semantics).
    XOFFSET_PROXY_INLINE XRef& operator=(const XRef& other) noexcept {
        set(other.get());
        return *this;
    }
    template <class F>
    XOFFSET_PROXY_INLINE bool update(F&& f) noexcept {
        if (T* p = Base::get_ptr()) [[likely]] {
            f(*p);
            return true;
        }
        miss();
        return false;
    }
#define XOFFSET_PROXY_COMPOUND(op)                                             \
    template <class U>                                                         \
    XOFFSET_PROXY_INLINE XRef& operator op(const U& rhs) noexcept {            \
        update([&](T& v) { v op rhs; });                                       \
        return *this;                                                          \
    }
    XOFFSET_PROXY_COMPOUND(+=)
    XOFFSET_PROXY_COMPOUND(-=)
    XOFFSET_PROXY_COMPOUND(*=)
    XOFFSET_PROXY_COMPOUND(/=)
    XOFFSET_PROXY_COMPOUND(|=)
    XOFFSET_PROXY_COMPOUND(&=)
    XOFFSET_PROXY_COMPOUND(^=)
#undef XOFFSET_PROXY_COMPOUND
    XOFFSET_PROXY_INLINE XRef& operator++() noexcept { update([](T& v) { ++v; }); return *this; }
    XOFFSET_PROXY_INLINE XRef& operator--() noexcept { update([](T& v) { --v; }); return *this; }

private:
    XOFFSET_PROXY_INLINE void miss() const noexcept { Base::a.miss(); }
};

// ---- records: members by member pointer (named accessors: part 3) -----------
template <class T, class Path>
class XRef<T, Path, proxy_detail::Kind::record>
    : public proxy_detail::RefBase<T, Path>, public XRecordAccess<T, XRef<T, Path, proxy_detail::Kind::record>> {
    using Base = proxy_detail::RefBase<T, Path>;

public:
    using value_type = T;
    using Base::Base;

    // The member `MP` of this record: buf.ref<Unit>().field<&Unit::hp>() = 5.
    template <auto MP>
    XOFFSET_PROXY_INLINE auto field() const noexcept {
        using Traits = proxy_detail::member_traits<decltype(MP)>;
        static_assert(std::is_base_of_v<typename Traits::class_type, T>, "field<MP>: not a member of this record");
        using M = typename Traits::type;
        // As a member of T: a member of a base sits at the base's offset.
        return Base::template member_at<M>(proxy_detail::member_offset(static_cast<M T::*>(MP)));
    }
    // Member I of the schema (declaration order).
    template <std::size_t I>
    XOFFSET_PROXY_INLINE auto member() const noexcept {
        using F = schema::member_t<T, I>;
        return Base::template member_at<typename F::type>(static_cast<std::uint32_t>(F::offset));
    }

    // A copy of a record of plain values.
    T get() const noexcept requires std::is_trivially_copyable_v<T> {
        if (const T* p = Base::get_ptr()) return *p;
        return T{};
    }
    bool set(const T& value) noexcept requires std::is_trivially_copyable_v<T> {
        if (T* p = Base::get_ptr()) {
            *p = value;
            return true;
        }
        Base::a.miss();
        return false;
    }
};

// ---- XString -----------------------------------------------------------------
template <class T, class Path>
class XRef<T, Path, proxy_detail::Kind::string> : public proxy_detail::RefBase<T, Path> {
    using Base = proxy_detail::RefBase<T, Path>;

public:
    using value_type = T;
    using Base::Base;
    XRef(const XRef&) noexcept = default;

    std::uint32_t size() const noexcept {
        const T* p = Base::get_ptr();
        return p ? static_cast<std::uint32_t>(p->size()) : 0u;
    }
    bool empty() const noexcept { return size() == 0; }
    // A copy of the characters (a view would dangle when the buffer moves).
    std::string copy() const {
        const T* p = Base::get_ptr();
        return p ? std::string(p->view()) : std::string();
    }
    std::string str() const { return copy(); }
    // Calls f(std::string_view) with the characters; the view is valid only
    // during the call.
    template <class F>
    decltype(auto) with_view(F&& f) const {
        const T* p = Base::get_ptr();
        return f(p ? p->view() : std::string_view());
    }
    bool operator==(std::string_view rhs) const noexcept {
        const T* p = Base::get_ptr();
        return p ? p->view() == rhs : rhs.empty();
    }

    // Assign: in place when it fits, else make room (the buffer may move)
    // and assign. False (no effect, error recorded) when it cannot be done.
    XOFFSET_PROXY_INLINE bool assign(std::string_view value) noexcept {
        if (T* p = Base::get_ptr()) [[likely]] {
#if XOFFSET_NO_EXCEPTIONS
            if (fits(*p, value.size()) || Base::a.room(value.size() + 24u)) [[likely]] {
                p->assign(value);
                return true;
            }
#else
            try {
                p->assign(value);
                return true;
            } catch (...) {
            }
#endif
        }
        const Done done = assign_slow(Base::a.copy(), Base::path.copy(), value);
        Base::a.version = done.version;
        return done.ok;
    }
    XOFFSET_PROXY_INLINE XRef& operator=(std::string_view value) noexcept {
        assign(value);
        return *this;
    }
    XOFFSET_PROXY_INLINE XRef& operator=(const char* value) noexcept {
        assign(std::string_view(value));
        return *this;
    }
    XOFFSET_PROXY_INLINE XRef& operator=(const std::string& value) noexcept {
        assign(std::string_view(value));
        return *this;
    }
    template <class P2>
    XRef& operator=(const XRef<T, P2>& other) noexcept {
        // Copy first: the source may be in this buffer, which may move.
        const std::string value = other.copy();
        assign(value);
        return *this;
    }
    XRef& operator=(const XRef& other) noexcept {
        const std::string value = other.copy();
        assign(value);
        return *this;
    }
    bool append(std::string_view value) noexcept {
        if (T* p = Base::get_ptr()) [[likely]] {
#if XOFFSET_NO_EXCEPTIONS
            if (fits(*p, std::size_t(p->size()) + value.size()) ||
                Base::a.room(2u * (std::size_t(p->size()) + value.size()) + 24u)) [[likely]] {
                p->append(value);
                return true;
            }
#else
            try {
                p->append(value);
                return true;
            } catch (...) {
            }
#endif
        }
        const Done done = append_slow(Base::a.copy(), Base::path.copy(), value);
        Base::a.version = done.version;
        return done.ok;
    }
    XRef& operator+=(std::string_view value) noexcept {
        append(value);
        return *this;
    }
    void clear() noexcept {
        if (T* p = Base::get_ptr()) p->clear();
    }

private:
    static XOFFSET_PROXY_INLINE bool fits(const T& s, std::size_t n) noexcept {
        return n <= (s.is_inline() || s.data_delta == 0 ? std::size_t(XFixedString::inline_max)
                                                         : std::size_t(s.capacity()));
    }
    // A view into this payload is rebased when the payload moves.
    struct Source {
        std::string_view view;
        std::uintptr_t offset = 0;
        bool inside = false;
    };
    static Source locate(const proxy_detail::Anchor& a, std::string_view value) noexcept {
        Source s{value};
        auto* core = a.ctl->core;
        if (!core || value.empty()) return s;
        const auto base = reinterpret_cast<std::uintptr_t>(a.ctl->base);
        const auto at = reinterpret_cast<std::uintptr_t>(value.data());
        if (at >= base && at - base < core->get_buffer().size()) {
            s.inside = true;
            s.offset = at - base;
        }
        return s;
    }
    static std::string_view rebase(const Source& s, char* base) noexcept {
        return s.inside ? std::string_view(base + s.offset, s.view.size()) : s.view;
    }
    struct Done {
        std::uint64_t version;
        bool ok;
    };
    static XOFFSET_DETAIL_NOINLINE Done assign_slow(proxy_detail::Anchor a, Path path, std::string_view value) noexcept {
        char* const base0 = a.sync();
        if (!base0) return {a.version, false};
        if (!path.resolve(base0)) {
            a.miss();
            return {a.version, false};
        }
        const Source src = locate(a, value);
        const bool ok = a.run_allocating(value.size() + 24u, [&](char* base) {
            if (T* p = reinterpret_cast<T*>(path.resolve(base))) p->assign(rebase(src, base));
        });
        return {a.version, ok};
    }
    static XOFFSET_DETAIL_NOINLINE Done append_slow(proxy_detail::Anchor a, Path path, std::string_view value) noexcept {
        char* const base0 = a.sync();
        if (!base0) return {a.version, false};
        const T* now = reinterpret_cast<const T*>(path.resolve(base0));
        if (!now) {
            a.miss();
            return {a.version, false};
        }
        const std::size_t total = std::size_t(now->size()) + value.size();
        const Source src = locate(a, value);
        const bool ok = a.run_allocating(2u * total + 24u, [&](char* base) {
            if (T* p = reinterpret_cast<T*>(path.resolve(base))) p->append(rebase(src, base));
        });
        return {a.version, ok};
    }
};

namespace proxy_detail {
    struct IterEnd {};
    // Iterator over a proxied vector or set: element `i` of the container,
    // valid across moves; it ends when `i` reaches the current size.
    template <class C, class Path, class Elem>
    class Iter {
    public:
        using value_type = Elem;
        using difference_type = std::ptrdiff_t;
        Iter() noexcept = default;
        Iter(const XRef<C, Path>& c, std::uint32_t i) noexcept : c_(c), i_(i) {}
        Elem operator*() const noexcept { return c_[i_]; }
        Iter& operator++() noexcept { ++i_; return *this; }
        Iter operator++(int) noexcept { auto t = *this; ++i_; return t; }
        friend bool operator==(const Iter& it, IterEnd) noexcept { return it.i_ >= it.c_.size(); }
        friend bool operator!=(const Iter& it, IterEnd e) noexcept { return !(it == e); }
        std::uint32_t index() const noexcept { return i_; }

    private:
        XRef<C, Path> c_;
        std::uint32_t i_ = 0;
    };
} // namespace proxy_detail

// ---- XVector -------------------------------------------------------------------
template <class T, class Path>
class XRef<T, Path, proxy_detail::Kind::vector> : public proxy_detail::RefBase<T, Path> {
    using Base = proxy_detail::RefBase<T, Path>;
    using E = typename T::value_type;
    using ElemPath = proxy_detail::IndexPath<Path, T>;
    // A plain element: stored with one copy, no arena allocation.
    static constexpr bool plain = std::is_trivially_copyable_v<E>;

public:
    using value_type = T;
    using element_ref = XRef<E, ElemPath>;
    using iterator = proxy_detail::Iter<T, Path, element_ref>;

    XRef() noexcept = default;
    // Makes no access: the cached size starts at 0 and, as always, is
    // compared with memory before it is used.
    XRef(const proxy_detail::Anchor& anchor, const Path& p) noexcept : Base(anchor, p) {}

    std::uint32_t size() const noexcept {
        const T* v = Base::get_ptr();
        return v ? static_cast<std::uint32_t>(v->size_) : 0u;
    }
    std::uint32_t capacity() const noexcept {
        const T* v = Base::get_ptr();
        return v ? static_cast<std::uint32_t>(v->capacity_) : 0u;
    }
    bool empty() const noexcept { return size() == 0; }

    // Element i (an empty proxy past the end).
    XOFFSET_PROXY_INLINE element_ref operator[](std::uint32_t i) const noexcept {
        return element_ref(Base::a, ElemPath{Base::path, i, 0});
    }
    element_ref at(std::uint32_t i) const noexcept { return (*this)[i]; }
    element_ref front() const noexcept { return (*this)[0]; }
    element_ref back() const noexcept {
        const auto n = size();
        return (*this)[n ? n - 1u : ~0u];
    }
    // A copy of element i of a plain vector (E{} past the end).
    XOFFSET_PROXY_INLINE E get(std::uint32_t i) const noexcept requires plain {
        const T* v = Base::get_ptr();
        if (v && i < v->size_) [[likely]]
            return fixed_detail::resolve_rel32_live<E>(&v->data_delta, v->data_delta)[i];
        return E{};
    }
    XOFFSET_PROXY_INLINE bool set(std::uint32_t i, const E& value) noexcept requires plain {
        T* v = Base::get_ptr();
        if (v && i < v->size_) [[likely]] {
            fixed_detail::resolve_rel32_live<E>(&v->data_delta, v->data_delta)[i] = value;
            return true;
        }
        Base::a.miss();
        return false;
    }

    iterator begin() const noexcept { return iterator(*this, 0); }
    proxy_detail::IterEnd end() const noexcept { return {}; }

    // Appends a plain value. The size stays in a register: memory's size is
    // only compared (another proxy may have changed it).
    // The value is taken by value and the full case is one out-of-line call
    // (growth is rare), so the caller's value never has its address taken.
    XOFFSET_PROXY_INLINE bool push_back(E value) noexcept requires plain {
        bool absent;
        T* const v = head_.find(Base::a, Base::path, absent);
        if (!v) [[unlikely]] return absent ? missing() : false;
        if (v->size_ != csize_) [[unlikely]] csize_ = reload_size(v);
        if (csize_ < v->capacity_) [[likely]] {
            fixed_detail::resolve_rel32_live<E>(&v->data_delta, v->data_delta)[csize_] = value;
            v->size_ = ++csize_;
            return true;
        }
#if !XOFFSET_NO_EXCEPTIONS
        // Past the capacity: grow in place first (inside the reservation the
        // payload cannot move; a throw changed nothing), else make room.
        try {
            const auto g = v->push_back_full(value);
            v->data_delta = g.data_delta;
            v->capacity_ = g.capacity;
            v->size_ = ++csize_;
            return true;
        } catch (...) {
        }
#endif
        return take(push_slow(head_.anchor(Base::a), Base::path.copy(), value));
    }
    // Appends a copy of a record (strings and containers copied deeply).
    // `value` must not live in this buffer: pass a proxy for that.
    bool push_back(const E& value) noexcept requires (!plain) {
        return take(append_slow(head_.anchor(Base::a), Base::path.copy(), proxy_detail::deep_bytes(value),
                                [&](T& v) { v.push_back(value); }));
    }
    // Appends a copy of another proxy's element (it may live in this buffer).
    template <class P2>
    bool push_back(const XRef<E, P2>& value) noexcept {
        if constexpr (plain) {
            return push_back(value.get());
        } else {
            const E* src = value.get_ptr();
            if (!src) return false;
            auto source = value;
            return take(append_slow(head_.anchor(Base::a), Base::path.copy(), proxy_detail::deep_bytes(*src), [&](T& v) {
                if (const E* s = source.get_ptr()) v.push_back(*s);
            }));
        }
    }
    // Appends a value-initialized element and returns its proxy. Inside
    // the capacity this allocates nothing (an empty record owns no blocks).
    XOFFSET_PROXY_INLINE element_ref emplace_back() noexcept {
        bool absent;
        T* const v = head_.find(Base::a, Base::path, absent);
        if (!v) [[unlikely]] {
            if (absent) missing();
            return element_ref();
        }
        if (v->size_ != csize_) [[unlikely]] csize_ = reload_size(v);
        if (csize_ < v->capacity_) [[likely]] {
            v->size_ = csize_;
            (void)v->emplace_back();
            ++csize_;
        } else if (!grow_emplace(v)) {
            return element_ref();
        }
        return element_ref(head_.anchor(Base::a), ElemPath{Base::path, csize_ - 1u, 0});
    }
    template <class... Args>
    element_ref emplace_back(Args&&... args) noexcept requires (sizeof...(Args) > 0) {
        const std::size_t extra = (std::size_t(0) + ... + proxy_detail::deep_bytes(args));
        if (!take(append_slow(head_.anchor(Base::a), Base::path.copy(), extra,
                              [&](T& v) { (void)v.emplace_back(std::forward<Args>(args)...); })))
            return element_ref();
        return (*this)[csize_ - 1u];
    }
    void pop_back() noexcept {
        if (T* v = Base::get_ptr(); v && v->size_ != 0) {
            v->pop_back();
            csize_ = v->size_;
        }
    }
    void clear() noexcept {
        if (T* v = Base::get_ptr()) {
            v->clear();
            csize_ = 0;
        }
    }
    bool reserve(std::uint32_t n) noexcept {
        T* v = Base::get_ptr();
        if (!v) return missing();
        if (n <= v->capacity_) return true;
        return Base::a.run_allocating(std::size_t(n) * sizeof(E) + alignof(E) + 24u, [&](char* base) {
            if (T* w = reinterpret_cast<T*>(Base::path.resolve(base))) w->reserve(n);
        });
    }
    bool resize(std::uint32_t n) noexcept {
        T* v = Base::get_ptr();
        if (!v) return missing();
        if (n <= v->size_) {
            v->resize(n);
            csize_ = v->size_;
            return true;
        }
        const bool ok = Base::a.run_allocating(std::size_t(n) * sizeof(E) * 2u + alignof(E) + 24u,
                                               [&](char* base) {
            if (T* w = reinterpret_cast<T*>(Base::path.resolve(base))) w->resize(n);
        });
        csize_ = size();
        return ok;
    }
    // Removes element i (false past the end).
    bool erase(std::uint32_t i) noexcept {
        T* v = Base::get_ptr();
        if (!v || i >= v->size_) return false;
        v->erase(v->begin() + i);
        csize_ = v->size_;
        return true;
    }
    // Inserts before element i (i <= size).
    bool insert(std::uint32_t i, const E& value) noexcept {
        T* v = Base::get_ptr();
        if (!v) return missing();
        if (i > v->size_) {
            Base::a.fail(XErrc::out_of_range);
            return false;
        }
        const E copy = value_copy(value);
        return take(append_slow(head_.anchor(Base::a), Base::path.copy(), proxy_detail::deep_bytes(value), [&](T& w) {
            w.insert(w.begin() + i, copy);
        }));
    }

    // ---- batch operations (growth policy part 4): one resolve, at most one
    // make_room and one regrow per call, then a plain copy.

    // Appends `values` (they may live in this buffer, even in this vector).
    XOFFSET_PROXY_INLINE bool append(std::span<const E> values) noexcept requires plain {
        char* const base = Base::a.sync();
        if (!base) [[unlikely]] return false;
        T* v = reinterpret_cast<T*>(Base::path.resolve(base));
        if (!v) [[unlikely]] return missing();
        const std::size_t n = v->size_;
        const std::size_t count = values.size();
        if (count <= std::size_t(v->capacity_) - n) [[likely]] {
            if (count != 0)
                std::memmove(fixed_detail::resolve_rel32_live<E>(&v->data_delta, v->data_delta) + n,
                             values.data(), count * sizeof(E));
            v->size_ = static_cast<char32_t>(n + count);
            csize_ = v->size_;
            return true;
        }
        return take(fill_slow(Base::a, Path(Base::path), values.data(), count, E{}, false));
    }
    template <std::size_t N>
    bool append(const E (&values)[N]) noexcept requires plain {
        return append(std::span<const E>(values, N));
    }
    bool append(std::initializer_list<E> values) noexcept requires plain {
        return append(std::span<const E>(values.begin(), values.size()));
    }
    // Appends `count` copies of `value`.
    bool append(std::uint32_t count, const E& value) noexcept requires plain {
        char* const base = Base::a.sync();
        if (!base) [[unlikely]] return false;
        T* v = reinterpret_cast<T*>(Base::path.resolve(base));
        if (!v) [[unlikely]] return missing();
        const std::size_t n = v->size_;
        if (count <= std::size_t(v->capacity_) - n) [[likely]] {
            std::fill_n(fixed_detail::resolve_rel32_live<E>(&v->data_delta, v->data_delta) + n, count, value);
            v->size_ = static_cast<char32_t>(n + count);
            csize_ = v->size_;
            return true;
        }
        return take(fill_slow(Base::a, Path(Base::path), nullptr, count, value, false));
    }
    // Appends copies of records (strings and containers copied deeply). The
    // values must not live in this buffer.
    bool append(std::span<const E> values) noexcept requires (!plain) {
        std::size_t extra = std::size_t(values.size()) * sizeof(E);
        for (const E& value : values) extra += proxy_detail::deep_bytes(value);
        return take(append_slow(Base::a, Base::path, extra, [&](T& w) {
            w.reserve(std::size_t(w.size_) + values.size());
            for (const E& value : values) w.push_back(value);
        }));
    }
    // Replaces the elements with `values` (they may live in this buffer).
    bool assign(std::span<const E> values) noexcept requires plain {
        char* const base = Base::a.sync();
        if (!base) [[unlikely]] return false;
        T* v = reinterpret_cast<T*>(Base::path.resolve(base));
        if (!v) [[unlikely]] return missing();
        if (values.size() <= v->capacity_) [[likely]] {
            v->assign_range(values);
            csize_ = v->size_;
            return true;
        }
        return take(fill_slow(Base::a, Path(Base::path), values.data(), values.size(), E{}, true));
    }
    bool assign(std::initializer_list<E> values) noexcept requires plain {
        return assign(std::span<const E>(values.begin(), values.size()));
    }
    // Appends `count` value-initialized elements (records own no blocks
    // then): the index of the first, or npos on failure. Fill them with
    // (*this)[i] or a batch scope.
    static constexpr std::uint32_t npos = ~std::uint32_t(0);
    std::uint32_t emplace_n(std::uint32_t count) noexcept {
        const T* v = Base::get_ptr();
        if (!v) {
            missing();
            return npos;
        }
        const std::uint32_t first = v->size_;
        if (std::size_t(first) + count > proxy_detail::max_elements<E>) {
            Base::a.fail(XErrc::capacity_exceeded);
            return npos;
        }
        return resize(first + count) ? first : npos;
    }

private:
    char32_t csize_ = 0;
    proxy_detail::Head<T, Path> head_;

    static E value_copy(const E& value) noexcept requires plain { return value; }
    static const E& value_copy(const E& value) noexcept requires (!plain) { return value; }

    static XOFFSET_DETAIL_NOINLINE char32_t reload_size(const T* v) noexcept { return v->size_; }

    XOFFSET_PROXY_INLINE bool missing() const noexcept {
        Base::a.miss();
        return false;
    }
    struct Grow {
        std::uint64_t version;
        char32_t csize;
        bool ok;
    };
    // emplace_back() past the capacity: in place inside the current
    // reservation (with exceptions; a throw changed nothing), else out of
    // line.
    XOFFSET_PROXY_INLINE bool grow_emplace(T* v) noexcept {
#if !XOFFSET_NO_EXCEPTIONS
        try {
            (void)v->emplace_back();
            csize_ = v->size_;
            return true;
        } catch (...) {
        }
#else
        (void)v;
#endif
        return take(append_slow(head_.anchor(Base::a), Base::path.copy(), 0, [](T& w) { (void)w.emplace_back(); }));
    }

    XOFFSET_PROXY_INLINE bool take(const Grow& g) noexcept {
        Base::a.version = g.version;
        head_.forget();
        csize_ = g.csize;
        return g.ok;
    }
    static XOFFSET_DETAIL_NOINLINE Grow push_slow(proxy_detail::Anchor a, Path path, E value) noexcept {
        T* v = a.template at<T>(path);
        const std::size_t bytes = proxy_detail::grow_bytes<E>(v ? v->capacity_ : 0);
        const bool ok = a.run_allocating(bytes, [&](char* base) {
            if (T* w = reinterpret_cast<T*>(path.resolve(base))) w->push_back(value);
        });
        const T* w = a.template at<const T>(path);
        return {a.version, w ? w->size_ : char32_t(0), ok};
    }
    // append / assign past the capacity: `count` values from `src` (or
    // copies of `value` when src is null), appended or (replace) replacing.
    // A source inside the payload is copied out first: the payload may move.
    static XOFFSET_DETAIL_NOINLINE Grow fill_slow(proxy_detail::Anchor a, const Path& path, const E* src, std::size_t count,
                                                  E value, bool replace) noexcept requires plain {
        char* const base0 = a.sync();
        if (!base0) return {a.version, 0, false};
        T* v = reinterpret_cast<T*>(path.resolve(base0));
        if (!v) {
            a.miss();
            return {a.version, 0, false};
        }
        const std::size_t n = replace ? 0 : std::size_t(v->size_);
        if (count > proxy_detail::max_elements<E> - n) {
            a.fail(XErrc::capacity_exceeded);
            return {a.version, v->size_, false};
        }
        std::vector<E> copy;
        if (src) {
            src = proxy_detail::outside_payload(a, src, count, copy);
            if (!src) return {a.version, v->size_, false};
        }
        const std::size_t target = n + count;
        const std::size_t doubled = std::min<std::size_t>(proxy_detail::max_elements<E>, std::size_t(v->capacity_) * 2u);
        const std::size_t new_cap = std::max(target, doubled);
        const bool ok = a.run_allocating(new_cap * sizeof(E) + alignof(E) + 24u, [&](char* base) {
            T* w = reinterpret_cast<T*>(path.resolve(base));
            if (!w) return;
            if (replace && src) {
                w->assign_range(std::span<const E>(src, count));
                return;
            }
            if (replace) w->clear();
            w->reserve(new_cap);
            E* at = fixed_detail::resolve_rel32_live<E>(&w->data_delta, w->data_delta) + w->size_;
            if (src) std::memcpy(at, src, count * sizeof(E));
            else std::fill_n(at, count, value);
            w->size_ = static_cast<char32_t>(std::size_t(w->size_) + count);
        });
        const T* w = a.template at<const T>(path);
        return {a.version, w ? w->size_ : char32_t(0), ok};
    }
    template <class Op>
    static XOFFSET_DETAIL_NOINLINE Grow append_slow(proxy_detail::Anchor a, Path path, std::size_t extra, Op&& op) noexcept {
        char* const base0 = a.sync();
        if (!base0) return {a.version, 0, false};
        T* v = reinterpret_cast<T*>(path.resolve(base0));
        if (!v) {
            a.miss();
            return {a.version, 0, false};
        }
        const std::size_t bytes = proxy_detail::grow_bytes<E>(v->capacity_) + extra;
        const bool ok = a.run_allocating(bytes, [&](char* base) {
            if (T* w = reinterpret_cast<T*>(path.resolve(base))) op(*w);
        });
        const T* w = a.template at<const T>(path);
        return {a.version, w ? w->size_ : char32_t(0), ok};
    }
};

// ---- XSet ----------------------------------------------------------------------
template <class T, class Path>
class XRef<T, Path, proxy_detail::Kind::set> : public proxy_detail::RefBase<T, Path> {
    using Base = proxy_detail::RefBase<T, Path>;
    using E = typename T::value_type;
    using ElemPath = proxy_detail::IndexPath<Path, T>;
    static constexpr bool plain = std::is_trivially_copyable_v<E>;

public:
    using value_type = T;
    using Base::Base;

    std::uint32_t size() const noexcept {
        const T* s = Base::get_ptr();
        return s ? static_cast<std::uint32_t>(s->size_) : 0u;
    }
    bool empty() const noexcept { return size() == 0; }
    template <class Key>
    bool contains(const Key& key) const noexcept {
        const T* s = Base::get_ptr();
        return s && s->contains(key);
    }
    // A copy of element i of a plain set, in order (E{} past the end).
    E get(std::uint32_t i) const noexcept requires plain {
        const T* s = Base::get_ptr();
        return s && i < s->size_ ? fixed_detail::resolve_rel32_live<E>(&s->data_delta, s->data_delta)[i] : E{};
    }
    // Inserts a value: true when it was not there. False also on failure
    // (error() tells).
    template <class Arg>
    bool insert(const Arg& value) noexcept {
        T* s = Base::get_ptr();
        if (!s) return missing();
        if constexpr (plain) {
            if (s->size_ < s->capacity_) [[likely]] return s->insert(E(value)).second;
        }
        bool inserted = false;
        const auto copy = value_copy(value);
        const std::size_t bytes = proxy_detail::grow_bytes<E>(s->capacity_) + proxy_detail::deep_bytes(value);
        (void)Base::a.run_allocating(bytes, [&](char* base) {
            if (T* w = reinterpret_cast<T*>(Base::path.resolve(base))) inserted = w->insert(copy).second;
        });
        return inserted;
    }
    // Batch insert (growth policy part 4): every value not yet present, one
    // resolve and at most one regrow. The number inserted; on failure
    // nothing is inserted (error() tells).
    std::uint32_t insert(std::span<const E> values) noexcept requires plain {
        std::vector<E> keep;
        const E* src = proxy_detail::outside_payload(Base::a, values.data(), values.size(), keep);
        if (!src && !values.empty()) return 0;
        const T* now = Base::get_ptr();
        if (!now) {
            missing();
            return 0;
        }
        if (!proxy_detail::reserve_at<T>(Base::a, Base::path, std::size_t(now->size_) + values.size())) return 0;
        T* s = reinterpret_cast<T*>(Base::path.resolve(Base::a.ctl->base));
        if (!s) [[unlikely]] return 0;
        std::uint32_t inserted = 0;
        for (std::size_t i = 0; i < values.size(); ++i) inserted += s->insert(src[i]).second ? 1u : 0u;
        return inserted;
    }
    std::uint32_t insert(std::initializer_list<E> values) noexcept requires plain {
        return insert(std::span<const E>(values.begin(), values.size()));
    }
    template <class Key>
    bool erase(const Key& key) noexcept {
        T* s = Base::get_ptr();
        return s && s->erase(key) != 0;
    }
    void clear() noexcept {
        if (T* s = Base::get_ptr()) s->clear();
    }

private:
    template <class Arg>
    static auto value_copy(const Arg& value) noexcept {
        if constexpr (plain) return E(value);
        else if constexpr (std::is_convertible_v<const Arg&, std::string_view>) return std::string(std::string_view(value));
        else return std::cref(value);
    }
    XOFFSET_PROXY_INLINE bool missing() const noexcept {
        Base::a.miss();
        return false;
    }
};

// ---- XMap ----------------------------------------------------------------------
template <class T, class Path>
class XRef<T, Path, proxy_detail::Kind::map> : public proxy_detail::RefBase<T, Path> {
    using Base = proxy_detail::RefBase<T, Path>;
    using K = typename T::key_type;
    using V = typename T::mapped_type;
    using Entry = typename T::value_type;
    using ValuePath = proxy_detail::KeyPath<Path, T>;
    using LookupKey = proxy_detail::lookup_key_t<K>;
    static constexpr bool plain = std::is_trivially_copyable_v<K> && std::is_trivially_copyable_v<V>;

public:
    using value_type = T;
    using mapped_ref = XRef<V, ValuePath>;

    XRef() noexcept = default;
    // Makes no access (the cached size is compared before use).
    XRef(const proxy_detail::Anchor& anchor, const Path& p) noexcept : Base(anchor, p) {}

    std::uint32_t size() const noexcept {
        const T* m = Base::get_ptr();
        return m ? static_cast<std::uint32_t>(m->size_) : 0u;
    }
    bool empty() const noexcept { return size() == 0; }
    template <class Key>
    bool contains(const Key& key) const noexcept {
        const T* m = Base::get_ptr();
        return m && m->contains(key);
    }
    // The value of `key`, or an empty proxy.
    template <class Key>
    mapped_ref find(const Key& key) const noexcept {
        const T* m = Base::get_ptr();
        if (!m) return mapped_ref();
        auto it = m->find(key);
        if (it == m->end()) return mapped_ref();
        return value_ref(LookupKey(key), static_cast<std::uint32_t>(std::to_address(it) - std::to_address(m->begin())));
    }
    // A copy of the value of `key` in a plain map (`fallback` when absent).
    template <class Key>
    XOFFSET_PROXY_INLINE V get(const Key& key, const V& fallback = V{}) const noexcept requires plain {
        const T* m = Base::get_ptr();
        if (!m) return fallback;
        auto it = m->find(key);
        return it == m->end() ? fallback : it->second;
    }

    // Inserts key -> V(args...) when the key is absent: true when inserted.
    // A plain map inside its capacity inserts directly (size kept in a
    // register, as push_back).
    template <class Key, class... Args>
    XOFFSET_PROXY_INLINE bool try_emplace(const Key& key, Args&&... args) noexcept {
        if constexpr (plain) {
            bool absent;
            T* const m = head_.find(Base::a, Base::path, absent);
            if (!m) [[unlikely]] return absent ? missing() : false;
            if (m->size_ != csize_) [[unlikely]] csize_ = reload_size(m);
            // Key and value by value: the full case is one out-of-line call
            // (growth is rare) and the caller's values keep no address.
            const K k(key);
            const V value(std::forward<Args>(args)...);
            if (csize_ < m->capacity_) [[likely]] {
                // Restated, so the inlined insertion sees the size in a
                // register; the new size is counted, not reloaded (a load
                // right after the store would chain the iterations).
                m->size_ = csize_;
                const bool inserted = m->try_emplace(k, value).second;
                csize_ += inserted ? 1u : 0u;
                return inserted;
            }
#if !XOFFSET_NO_EXCEPTIONS
            if (const int inserted = insert_in_place(m, k, value); inserted >= 0) {
                csize_ = m->size_;
                return inserted != 0;
            }
#endif
            const Inserted r = emplace_slow_plain(head_.anchor(Base::a), Base::path.copy(), k, value);
            Base::a.version = r.version;
            head_.forget();
            csize_ = r.csize;
            return r.inserted;
        }
        const Inserted r = emplace_slow(head_.anchor(Base::a), Base::path.copy(), key, std::forward<Args>(args)...);
        Base::a.version = r.version;
        head_.forget();
        csize_ = r.csize;
        return r.inserted;
    }
    // Inserts or assigns: true when inserted.
    template <class Key, class Value>
    bool insert_or_assign(const Key& key, const Value& value) noexcept {
        if (T* m = Base::get_ptr()) {
            if constexpr (plain) {
                auto it = m->find(key);
                if (it != m->end()) {
                    it->second = value;
                    return false;
                }
            }
        }
        bool inserted = false;
        if constexpr (plain) {
            inserted = try_emplace(key, value);
        } else {
            T* m = Base::get_ptr();
            if (!m) return missing();
            const std::size_t bytes = proxy_detail::grow_bytes<Entry>(m->capacity_) +
                                      proxy_detail::deep_bytes(key) + proxy_detail::deep_bytes(value);
            const LookupKey k(key);
            (void)Base::a.run_allocating(bytes, [&](char* base) {
                if (T* w = reinterpret_cast<T*>(Base::path.resolve(base)))
                    inserted = w->insert_or_assign(k, value).second;
            });
            csize_ = size();
        }
        return inserted;
    }
    // Batch insert (growth policy part 4): every key -> value pair whose key
    // is absent (pairs of any type with .first and .second, e.g. std::pair or
    // this map's entries; any order, ascending is fastest). One resolve and
    // at most one regrow. The number inserted; on failure nothing is
    // inserted (error() tells).
    template <class Pair>
    std::uint32_t insert(std::span<const Pair> entries) noexcept
        requires plain && requires(const Pair& e) { K(e.first); V(e.second); } {
        std::vector<Pair> keep;
        const Pair* src = proxy_detail::outside_payload(Base::a, entries.data(), entries.size(), keep);
        if (!src && !entries.empty()) return 0;
        const T* now = Base::get_ptr();
        if (!now) {
            missing();
            return 0;
        }
        if (!proxy_detail::reserve_at<T>(Base::a, Base::path, std::size_t(now->size_) + entries.size())) return 0;
        T* m = reinterpret_cast<T*>(Base::path.resolve(Base::a.ctl->base));
        if (!m) [[unlikely]] return 0;
        std::uint32_t inserted = 0;
        for (std::size_t i = 0; i < entries.size(); ++i)
            inserted += m->try_emplace(K(src[i].first), V(src[i].second)).second ? 1u : 0u;
        csize_ = m->size_;
        return inserted;
    }
    template <class Pair>
    std::uint32_t insert(const std::vector<Pair>& entries) noexcept
        requires plain && requires(const Pair& e) { K(e.first); V(e.second); } {
        return insert(std::span<const Pair>(entries));
    }
    // Capacity for n entries (one regrow).
    bool reserve(std::uint32_t n) noexcept {
        if (!proxy_detail::reserve_at<T>(Base::a, Base::path, n)) return false;
        return true;
    }

    // The value of `key`, inserted value-initialized when absent.
    template <class Key>
    mapped_ref operator[](const Key& key) noexcept {
        (void)try_emplace(key);
        return find(key);
    }
    template <class Key>
    bool erase(const Key& key) noexcept {
        T* m = Base::get_ptr();
        if (!m) return false;
        const bool erased = m->erase(key) != 0;
        csize_ = m->size_;
        return erased;
    }
    void clear() noexcept {
        if (T* m = Base::get_ptr()) {
            m->clear();
            csize_ = 0;
        }
    }

    // Entry i in key order: key() and value() proxies.
    struct EntryRef {
        XRef<Entry, proxy_detail::IndexPath<Path, T>> entry;
        auto key() const noexcept { return entry.template member_at<K>(offsetof_first()); }
        auto value() const noexcept { return entry.template member_at<V>(offsetof_second()); }
        static std::uint32_t offsetof_first() noexcept { return proxy_detail::member_offset(&Entry::first); }
        static std::uint32_t offsetof_second() noexcept { return proxy_detail::member_offset(&Entry::second); }
    };
    EntryRef entry(std::uint32_t i) const noexcept {
        return EntryRef{XRef<Entry, proxy_detail::IndexPath<Path, T>>(Base::a, {Base::path, i, 0})};
    }

private:
    char32_t csize_ = 0;
    proxy_detail::Head<T, Path> head_;

    mapped_ref value_ref(LookupKey key, std::uint32_t index) const noexcept {
        return mapped_ref(Base::a, ValuePath{Base::path, std::move(key), index,
                                             proxy_detail::member_offset(&Entry::second)});
    }
    static XOFFSET_DETAIL_NOINLINE char32_t reload_size(const T* m) noexcept { return m->size_; }
    XOFFSET_PROXY_INLINE bool missing() const noexcept {
        Base::a.miss();
        return false;
    }
    struct Inserted {
        std::uint64_t version;
        char32_t csize;
        bool inserted;
    };
#if !XOFFSET_NO_EXCEPTIONS
    // Insertion past the capacity inside the current reservation: 1 inserted,
    // 0 present, -1 threw (nothing changed).
    static XOFFSET_DETAIL_NOINLINE int insert_in_place(T* m, K key, V value) noexcept requires plain {
        try {
            return m->try_emplace(key, value).second ? 1 : 0;
        } catch (...) {
            return -1;
        }
    }
#endif
    static XOFFSET_DETAIL_NOINLINE Inserted emplace_slow_plain(proxy_detail::Anchor a, Path path, K key, V value) noexcept {
        return emplace_slow(a, path, key, value);
    }
    template <class Key, class... Args>
    static XOFFSET_DETAIL_NOINLINE Inserted emplace_slow(proxy_detail::Anchor a, Path path, const Key& key,
                                                         Args&&... args) noexcept {
        char* const base0 = a.sync();
        if (!base0) return {a.version, 0, false};
        T* m = reinterpret_cast<T*>(path.resolve(base0));
        if (!m) {
            a.miss();
            return {a.version, 0, false};
        }
        if (m->contains(key)) return {a.version, m->size_, false};
        const std::size_t bytes = proxy_detail::grow_bytes<Entry>(m->capacity_) + proxy_detail::deep_bytes(key) +
                                  (std::size_t(0) + ... + proxy_detail::deep_bytes(args));
        bool inserted = false;
        (void)a.run_allocating(bytes, [&](char* base) {
            if (T* w = reinterpret_cast<T*>(path.resolve(base)))
                inserted = w->try_emplace(key, std::forward<Args>(args)...).second;
        });
        const T* w = a.template at<const T>(path);
        return {a.version, w ? w->size_ : char32_t(0), inserted};
    }
};

} // namespace XOffsetDatastructure

#endif
