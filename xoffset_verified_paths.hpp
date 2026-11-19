#pragma once

// Included after the main definitions. Standalone inclusion is also supported.
#include "xoffsetdatastructure.hpp"
#include <array>
#include <tuple>


namespace XOffsetDatastructure {
namespace detail {
    inline bool verified_paths_intersect(const void* a, std::size_t an,
                                         const void* b, std::size_t bn) noexcept {
        if (!an || !bn) return false;
        const auto x = reinterpret_cast<std::uintptr_t>(a);
        const auto y = reinterpret_cast<std::uintptr_t>(b);
        // x <= y ? y - x < an : x - y < bn, as one unsigned comparison: the
        // ranges meet iff x - y lies in (-an, bn), i.e. x - y + an - 1 < an + bn - 1
        // (modulo 2^64), while an + bn - 1 does not wrap.
        const std::size_t span = an + (bn - 1);
        if (span < an) [[unlikely]] return x <= y ? y - x < an : x - y < bn;
        return x - y + (an - 1) < span;
    }

    // Out of line: its hole vector and graph walk stay out of header's frame.
    template<class T>
    XOFFSET_DETAIL_NOINLINE void VerifiedPathsAccess::rebuild_free_table(T& root, XBufferCore& source) {
        std::vector<XBufferCore::WireHole> holes;
        XOFFSET_VREQUIRE(verify_structural_wire_graph(root, source, std::pmr::new_delete_resource(),
                                                      nullptr, 0, false, &holes));
        source.replace_free_blocks(holes);
    }

    template<class T>
    XWireHeader VerifiedPathsAccess::header(XBufferCore& source) {
        static_assert(is_wire_admitted_v<T>);
#ifdef XOFFSET_REJECT_IMPLICIT_PADDING
        static_assert(!has_implicit_padding_v<T>,
            "XOFFSET_REJECT_IMPLICIT_PADDING: the wire type has implicit padding (threat model T5/P3)");
#endif
        // A root cache hit is what root<T>() returns: make<T>(), a verified
        // load of T or root<T>()'s checked path admitted it at the current
        // epoch, so typed access and the schema binding are as the checks
        // below find them. A miss runs those checks and the lookup.
        T* root = source.template cached_root<T>();
        if (!root) {
            if (!source.typed_root_access_allowed_)
                XOFFSET_THROW(XException(XErrc::no_root, "verified paths: typed source required"));
            const auto* image = source.arena();
            if (source.schema_bound_ && image &&
                (image->root_type_id != wire_root_type_id<T>() || image->schema_hash != wire_schema_hash<T>()))
                XOFFSET_THROW(XException(XErrc::schema_mismatch, "verified paths: schema mismatch"));
        }
        const auto* arena = source.arena();
        if (!arena || arena->magic != XBufferCore::image_magic)
            XOFFSET_THROW(XException(XErrc::invalid_payload_header, "verified paths: payload header mismatch"));
        if (arena->used_end < XBufferCore::min_payload_size() ||
            arena->used_end > source.get_size() || arena->used_end > XBufferCore::MAX_PAYLOAD)
            XOFFSET_THROW(XException(XErrc::invalid_payload_header, "verified paths: payload used_end out of range"));
        if (!root) {
            root = find_root<T>(source);
            if (!root) XOFFSET_THROW(XException(XErrc::root_mismatch, "verified paths: root not found"));
        }
        // The free state travels in the image: recompute it exactly if a
        // host allocation failure (or an unverified load) left it incomplete.
        if (!source.free_table_complete()) [[unlikely]] rebuild_free_table<T>(*root, source);
        const auto* sm = source.arena();
        return make_wire_header<T>(sm->used_end, sm->free_dir, sm->free_bytes);
    }

    // The output must not alias the source's storage (or hold it).
    XOFFSET_DETAIL_INLINE void save_check_alias(const std::string& out, const XBufferCore& candidate) {
        if (out.capacity() == (std::numeric_limits<std::size_t>::max)())
            XOFFSET_THROW(std::length_error("save_verified_into: output capacity overflows alias range"));
        if (verified_paths_intersect(&out, sizeof(out), candidate.get_address(), candidate.get_size()) ||
            verified_paths_intersect(out.data(), out.capacity() + 1, candidate.get_address(), candidate.get_size()))
            XOFFSET_THROW(XException(XErrc::invalid_state, "save_verified_into: output aliases source storage"));
    }

    // A dense wire into a reused output (each message of a stream) is
    // written by save_small_into, inline in this function's small frame;
    // every other save takes save_into_general, whose locals (gap list,
    // reserve path) a message written here does not touch.
    template<class T>
    void VerifiedPathsAccess::save_into(XBufferCore& source, std::string& out, XSaveOptions options) {
        if (save_small_into<T>(source, out, options.emission)) [[likely]] return;
        save_into_general<T>(source, out, options);
    }

    // Images below this size are copied by save_small_into (the copier
    // prefaults larger outputs).
    inline constexpr std::size_t save_small_max = std::size_t(1) << 20;

    // save_into_general's fast path for an image that goes as it is: no
    // emission skips a byte of it (free_runs: free bytes below one run, read
    // from the header; zero_runs: below wire_zero_scan_min_payload), it is
    // below save_small_max and the reused output holds it. The wire is the
    // image, fingerprint included: one copy, written inline. False (nothing
    // done) for anything else.
    template<class T>
    XOFFSET_DETAIL_INLINE bool VerifiedPathsAccess::save_small_into(XBufferCore& source, std::string& out,
                                                                        XWireEmission emission) {
        static_assert(is_wire_admitted_v<T>);
#ifdef XOFFSET_REJECT_IMPLICIT_PADDING
        static_assert(!has_implicit_padding_v<T>,
            "XOFFSET_REJECT_IMPLICIT_PADDING: the wire type has implicit padding (threat model T5/P3)");
#endif
        // header's checks for the state this path admits: a root cache
        // hit (typed access and schema binding as make<T>() or a verified
        // load of T left them, and T's fingerprint in the image header), a
        // complete free state, and a used end below save_small_max. A miss
        // or anything else takes save_into_general, which runs them all.
        if (!source.template root_cached_as<T>() || !source.free_table_complete()) return false;
        const auto* image = static_cast<const char*>(source.get_address());
        const auto* arena = std::launder(reinterpret_cast<const XBufferCore::arena_header*>(image));
        const std::size_t used = arena->used_end;
        if (used >= save_small_max ||
            (emission == XWireEmission::free_runs ? arena->free_bytes >= wire_free_run_min_gap
                                                  : emission == XWireEmission::zero_runs &&
                                                        used >= wire_zero_scan_min_payload))
            return false;
        // A message no longer than the previous one (a stream of similar
        // messages) is copied over the output's bytes; the output then
        // ends at `used`. Otherwise its storage must already hold it.
        const bool overwrite = used <= out.size();
        if (!overwrite && used > out.capacity()) return false;
        // The sender's own state, checked again per message: the arena
        // header is intact and `out` is not this buffer's storage. Off by
        // default (save_into_general, the path of every larger save, keeps
        // both checks).
        if constexpr (detail::save_self_check) {
            if (arena->magic != XBufferCore::image_magic || used < XBufferCore::root_at || used > source.get_size())
                XOFFSET_THROW(XException(XErrc::invalid_payload_header, "verified paths: payload header mismatch"));
            save_check_alias(out, source);
        }
        // The wire is the image: one copy into the output's storage.
        if (overwrite) {
            // Shortened first, so the copy is the last access (nothing of
            // the string is read again after it).
            out.erase(used);
            std::memcpy(out.data(), image, used);
            return true;
        }
        // The copy reads one frame object, so the operation is a single
        // pointer wherever the string's resize_and_overwrite is compiled.
#if defined(__cpp_lib_string_resize_and_overwrite) && __cpp_lib_string_resize_and_overwrite >= 202110L
        struct SmallWire {
            const char* image;
            std::size_t size;
        } const wire{image, used};
        out.resize_and_overwrite(used, [w = &wire](char* destination, std::size_t) noexcept {
            std::memcpy(destination, w->image, w->size);
            return w->size;
        });
#else
        out.assign(image, used);
#endif
        return true;
    }

    template<class T>
    XOFFSET_DETAIL_NOINLINE void VerifiedPathsAccess::save_into_general(XBufferCore& source, std::string& out,
                                                                            XSaveOptions options) {
        auto check_alias = [&](XBufferCore& candidate) XOFFSET_DETAIL_LAMBDA_INLINE { save_check_alias(out, candidate); };
        const auto emission = options.emission;
        // A shared record is an identity, not a mapping pin. Resolve it after
        // reserve/new_handler; a moved owner may even be a plain XBufferCore.
        const auto token = identity(source);
        auto* current = &owner(token);
        check_alias(*current);
        // Host allocations (the free-run list, reserve) are the callback
        // boundaries: the owner is re-resolved after them, and the source
        // must be unchanged when the wire is written. A reused output that
        // already holds the image (which bounds every emission) needs no
        // reserve, so the source is examined once.
        auto size_of = [&](const XWireHeader& h, const WireGaps& g) {
            return emission == XWireEmission::zero_runs ? std::size_t(h.used_bytes)
                                                        : wire_free_run_size(h.used_bytes, g);
        };
        auto gaps_of = [&](const XBufferCore& core, std::uint64_t used) {
            return emission == XWireEmission::free_runs ? wire_free_runs(core, used) : WireGaps{};
        };
        auto same_holes = [](const XWireHeader& a, const XWireHeader& b) {
            return a.used_bytes == b.used_bytes && a.free_dir == b.free_dir && a.free_bytes == b.free_bytes;
        };
        XWireHeader final_header{};
        WireGaps gaps;
        std::size_t final_size = 0;
        const auto* arena = current->arena();
        if (arena && current->free_table_complete() && arena->used_end >= XWIRE_PAYLOAD_BEGIN &&
            arena->used_end <= out.capacity()) {
            const auto before = arena->used_end;
            const auto before_dir = arena->free_dir, before_bytes = arena->free_bytes;
            gaps = gaps_of(*current, before);
            current = &owner(token);
            check_alias(*current);
            final_header = header<T>(*current);
            if (final_header.used_bytes != before || final_header.free_dir != before_dir ||
                final_header.free_bytes != before_bytes)
                XOFFSET_THROW(XException(XErrc::invalid_state, "save_verified_into: source changed while preparing the output"));
            final_size = size_of(final_header, gaps);
        }
        if (final_size == 0 || final_size > out.capacity()) {
            const auto initial = header<T>(*current);
            gaps = gaps_of(*current, initial.used_bytes);
            const auto prepared_size = size_of(initial, gaps);
            if (prepared_size > out.max_size())
                XOFFSET_THROW(std::length_error("save_verified_into: output size exceeds string limit"));
            out.reserve(prepared_size);
            current = &owner(token);
            check_alias(*current);
            final_header = header<T>(*current);
            final_size = size_of(final_header, gaps);
            if (final_size > out.capacity() || final_size != prepared_size || !same_holes(initial, final_header))
                XOFFSET_THROW(XException(XErrc::invalid_state, "save_verified_into: source changed beyond prepared output capacity"));
        }
        const auto* base = static_cast<const char*>(current->get_address());
#if defined(__cpp_lib_string_resize_and_overwrite) && __cpp_lib_string_resize_and_overwrite >= 202110L
        // Supported string implementation must honor its existing reserved
        // capacity. No host allocation or user callback may occur in this copy.
        out.resize_and_overwrite(final_size, [&](char* destination, std::size_t) noexcept {
            return write_wire(destination, final_header, base, emission, gaps);
        });
#else
        out.resize(final_size);
        out.resize(write_wire(out.data(), final_header, base, emission, gaps));
#endif
    }
}

namespace detail {
    // ---- save_compact: canonical wire by direct traversal ----------------
    // Canonical layout: image header, root at offset 40, then every block in
    // validation (pre)order, each starting where the previous one ends:
    // capacity == size, strings of at most 10 bytes inline, empty containers
    // without storage, no holes. This is the layout XCompactor builds, written
    // straight from the source graph into the output string.
    template <class U> consteval bool compact_has_heads();
    template <class U> consteval bool compact_direct_supported();

    template <class U, std::size_t N = 0>
    consteval bool compact_members_have_heads() {
        if constexpr (N >= schema::member_count_v<U>) {
            return false;
        } else {
            using member = schema::member_t<U, N>;
            using M = typename member::type;
            return compact_has_heads<M>() || compact_members_have_heads<U, N + 1>();
        }
    }
    template <class U, std::size_t N = 0>
    consteval bool compact_members_supported() {
        if constexpr (N >= schema::member_count_v<U>) {
            return true;
        } else {
            using member = schema::member_t<U, N>;
            using M = typename member::type;
            return compact_direct_supported<M>() && compact_members_supported<U, N + 1>();
        }
    }
    template <class U> consteval bool compact_has_heads() {
        using C = std::remove_cv_t<U>;
        if constexpr (is_fixed_string_v<C> || is_fixed_vector_v<C>) return true;
        else if constexpr (std::is_array_v<C>) return compact_has_heads<std::remove_all_extents_t<C>>();
        else if constexpr (std::is_class_v<C>) return compact_members_have_heads<C>(); // admitted: no bases
        else return false;
    }
    // Blocks are 8-aligned and padded to 8, so canonical packing needs every
    // stored type to be at most 8-aligned (wire admission already requires it).
    template <class U> consteval bool compact_direct_supported() {
        using C = std::remove_cv_t<U>;
        if constexpr (alignof(C) > 8) return false;
        else if constexpr (is_fixed_string_v<C>) return true;
        else if constexpr (is_fixed_vector_v<C>) return compact_direct_supported<typename C::value_type>();
        else if constexpr (std::is_array_v<C>) return compact_direct_supported<std::remove_all_extents_t<C>>();
        else if constexpr (std::is_class_v<C>) return compact_members_supported<C>();
        else return true;
    }

    // The head writers of DirectWriter (below), the one place the canonical
    // head layout is written: a 12-byte container head at payload offset
    // `at` (data_delta is relative to the head's own address), and the inline
    // form of a string of 1..inline_max bytes (store_inline: bytes 0..10 the
    // characters then zeros, byte 11 = 0x80 | length).
    XOFFSET_DETAIL_INLINE void write_container_head(char* base, std::size_t at, std::int32_t data_delta,
                                                         std::uint32_t size, std::uint32_t capacity) noexcept {
        const std::int32_t head[3] = {data_delta, static_cast<std::int32_t>(size),
                                      static_cast<std::int32_t>(capacity)};
        std::memcpy(base + at, head, sizeof(head));
    }
    XOFFSET_DETAIL_INLINE void write_inline_string(char* base, std::size_t at, const char* text,
                                                        std::uint32_t n) noexcept {
        char head[12] = {};
        for (std::uint32_t i = 0; i < n; ++i) head[i] = text[i];
        head[11] = static_cast<char>(0x80u | n);
        std::memcpy(base + at, head, sizeof(head));
    }

    // Capacity given to a vector of n elements (strings stay tight).
    template <class E>
    XOFFSET_DETAIL_INLINE std::uint32_t compact_capacity(std::uint32_t n, std::uint32_t slack_div) noexcept {
        if constexpr (std::is_same_v<std::remove_cv_t<E>, char>) return n;
        if (slack_div == 0 || n < 8u) return n;
        const auto cap = std::uint64_t(n) + n / slack_div + 1u;
        return cap > (std::numeric_limits<std::uint32_t>::max)() ? n : static_cast<std::uint32_t>(cap);
    }

    // Measures the canonical payload: walks the source and advances the
    // cursor over every block the writer would place, without writing.
    struct CompactMeasure {
        std::size_t cursor;      // next block offset
        std::size_t limit;       // payload limit
        std::uint32_t slack_div; // XCompactOptions::slack_div
        bool overflow = false;   // the source graph did not fit (corrupted source)
        // Advances over a block of `bytes` (padded to 8).
        bool block(std::size_t bytes) noexcept {
            const auto padded = (bytes + 7u) & ~std::size_t(7u);
            if (overflow || padded > limit - cursor) { overflow = true; return false; }
            cursor += padded;
            return true;
        }
    };
    static_assert(offsetof(XString, data_delta) == 0 && offsetof(XString, size_) == 4 &&
                  offsetof(XString, capacity_) == 8 && sizeof(XString) == 12);
    static_assert(offsetof(XVectorCore<int>, data_delta) == 0 && offsetof(XVectorCore<int>, size_) == 4 &&
                  offsetof(XVectorCore<int>, capacity_) == 8 && sizeof(XVectorCore<int>) == 12);

    // Measures the subtree of the source value `src`.
    template <class U>
    void compact_value(const U& src, CompactMeasure& w) noexcept;

    inline void compact_string(const XString& src, CompactMeasure& w) noexcept {
        const auto n = src.size();
        if (n > XString::inline_max) w.block(std::size_t(n) + 1u);
    }

    template <class E>
    void compact_vector(const XVectorCore<E>& src, CompactMeasure& w) noexcept {
        const auto n = src.size_;
        if (n == 0) return;
        if (!w.block(std::size_t(compact_capacity<E>(n, w.slack_div)) * sizeof(E))) return;
        if constexpr (compact_has_heads<E>()) {
            const E* elements = src.data();
            for (std::uint32_t i = 0; i < n; ++i) compact_value(elements[i], w);
        }
    }

    template <class U, std::size_t N>
    void compact_member_at(const U& src, CompactMeasure& w) noexcept {
        using member = schema::member_t<U, N>;
        using M = typename member::type;
        if constexpr (compact_has_heads<M>())
            compact_value(member::get(src), w);
    }

    template <class U>
    void compact_value(const U& src, CompactMeasure& w) noexcept {
        using C = std::remove_cv_t<U>;
        if constexpr (!compact_has_heads<C>()) {
            return;
        } else if constexpr (is_fixed_string_v<C>) {
            compact_string(static_cast<const XString&>(src), w);
        } else if constexpr (is_fixed_vector_v<C>) {
            compact_vector(static_cast<const XVectorCore<typename C::value_type>&>(src), w);
        } else if constexpr (std::is_array_v<C>) {
            for (std::size_t i = 0; i < std::extent_v<C>; ++i) compact_value(src[i], w);
        } else {
            static_for<schema::member_count_v<C>>([&](auto index) {
                compact_member_at<C, decltype(index)::value>(src, w);
            });
        }
    }

    template <class T>
    constexpr std::size_t compact_root_end() noexcept {
        return sizeof(XBufferCore::arena_header) + ((sizeof(T) + 7u) & ~std::size_t(7u));
    }

    // Payload bytes of the compact wire of `root`, or 0 if it exceeds `limit`.
    template <class T>
    std::size_t measure_compact(const T& root, std::size_t limit, std::uint32_t slack_div) noexcept {
        if (limit < compact_root_end<T>()) return 0;
        CompactMeasure w{compact_root_end<T>(), limit, slack_div};
        compact_value(root, w);
        return w.overflow ? 0 : w.cursor;
    }
}

namespace detail {
    // ---- direct writer: the write pass of save_compact, write-only ------
    // Writes the canonical layout (see above) that CompactMeasure measures:
    // one 12-byte store per head, small blocks copied in 8-byte words.
    // Plain: capacity == size (slack_div == 0), decided once per save.
    // Offsets (cursor, limit, `at`) count from `base`, the first payload byte
    // on the wire after the header (offset 40); heads hold differences of offsets, so
    // they come out as in the payload.
    template <bool Plain>
    struct DirectWriter {
        char* base;
        std::size_t cursor;
        std::size_t limit;
        std::uint32_t slack_div;
        bool overflow = false;
        template <class E>
        XOFFSET_DETAIL_INLINE std::uint32_t capacity_for(std::uint32_t n) const noexcept {
            if constexpr (Plain) return n;
            else return compact_capacity<E>(n, slack_div);
        }
        // Reserves `padded` bytes (a multiple of 8); false on overflow.
        XOFFSET_DETAIL_INLINE bool take(std::size_t padded, std::size_t& at) noexcept {
            if (padded > limit - cursor) [[unlikely]] { overflow = true; return false; }
            at = cursor;
            cursor += padded;
            return true;
        }
    };
    template <class W> inline constexpr bool is_direct_writer_v = false;
    template <bool P> inline constexpr bool is_direct_writer_v<DirectWriter<P>> = true;

    // n is a multiple of 8.
    XOFFSET_DETAIL_INLINE void direct_copy_words(char* dst, const char* src, std::size_t n) noexcept {
        if (n <= 64) {
            for (std::size_t i = 0; i < n; i += 8) {
                std::uint64_t v;
                std::memcpy(&v, src + i, 8);
                std::memcpy(dst + i, &v, 8);
            }
        } else {
            std::memcpy(dst, src, n);
        }
    }

    template <class U, bool P>
    void direct_value(const U& src, std::size_t at, DirectWriter<P>& w) noexcept;

    template <class U, std::size_t N, bool P>
    void direct_member_at(const U& src, std::size_t at, DirectWriter<P>& w) noexcept {
        using member = schema::member_t<U, N>;
        using M = typename member::type;
        if constexpr (compact_has_heads<M>())
            direct_value(member::get(src), at + member::offset, w);
    }

    template <bool P>
    XOFFSET_DETAIL_INLINE void direct_string(const XString& src, std::size_t at, DirectWriter<P>& w) noexcept {
        const auto text = src.view();
        const auto n = static_cast<std::uint32_t>(text.size());
        if (n == 0) { write_container_head(w.base, at, 0, 0, 0); return; }
        if (n <= XString::inline_max) { write_inline_string(w.base, at, text.data(), n); return; }
        // Block of n + 1 bytes padded to 8. The source block holds at least
        // that many bytes (capacity + 1 padded to 8, see block_bytes_of), so
        // it is copied in words and the bytes from n on are cleared
        // (little-endian, as store_inline).
        const std::size_t padded = (std::size_t(n) + 8u) & ~std::size_t(7u);
        std::size_t data;
        if (!w.take(padded, data)) return;
        char* dst = w.base + data;
        direct_copy_words(dst, text.data(), padded);
        const auto keep = n - (padded - 8u); // 0..7 bytes of the last word
        std::uint64_t last;
        std::memcpy(&last, dst + padded - 8u, 8);
        last &= keep == 0 ? 0 : (~std::uint64_t(0) >> (64u - 8u * keep));
        std::memcpy(dst + padded - 8u, &last, 8);
        write_container_head(w.base, at, static_cast<std::int32_t>(data - at), n, n);
    }

    template <class E, bool P>
    XOFFSET_DETAIL_INLINE void direct_vector(const XVectorCore<E>& src, std::size_t at, DirectWriter<P>& w) noexcept {
        const auto n = src.size_;
        if (n == 0) { write_container_head(w.base, at, 0, 0, 0); return; }
        const auto cap = w.template capacity_for<E>(n);
        const std::size_t live = std::size_t(n) * sizeof(E);
        const std::size_t padded = (std::size_t(cap) * sizeof(E) + 7u) & ~std::size_t(7u);
        std::size_t data;
        if (!w.take(padded, data)) return;
        char* dst = w.base + data;
        const E* elements = src.data();
        const auto* from = reinterpret_cast<const char*>(elements);
        if (sizeof(E) % 8u == 0 && (P || cap == n)) {
            direct_copy_words(dst, from, live); // live == padded
        } else {
            std::memcpy(dst, from, live);
            std::memset(dst + live, 0, padded - live);
        }
        write_container_head(w.base, at, static_cast<std::int32_t>(data - at), n, cap);
        if constexpr (compact_has_heads<E>()) {
            for (std::uint32_t i = 0; i < n; ++i) direct_value(elements[i], data + std::size_t(i) * sizeof(E), w);
        }
    }

    template <class U, bool P>
    void direct_value(const U& src, std::size_t at, DirectWriter<P>& w) noexcept {
        using C = std::remove_cv_t<U>;
        if constexpr (!compact_has_heads<C>()) {
            return;
        } else if constexpr (is_fixed_string_v<C>) {
            direct_string(static_cast<const XString&>(src), at, w);
        } else if constexpr (is_fixed_vector_v<C>) {
            direct_vector(static_cast<const XVectorCore<typename C::value_type>&>(src), at, w);
        } else if constexpr (std::is_array_v<C>) {
            using Elem = std::remove_extent_t<C>;
            for (std::size_t i = 0; i < std::extent_v<C>; ++i) direct_value(src[i], at + i * sizeof(Elem), w);
        } else {
            static_for<schema::member_count_v<C>>([&](auto index) {
                direct_member_at<C, decltype(index)::value>(src, at, w);
            });
        }
    }

    // Wire header of a canonical image of `used` bytes: no free block, no
    // extents.
    template <class T>
    XWireHeader compact_wire_header(std::size_t used) noexcept {
        return make_wire_header<T>(used);
    }
}

namespace detail {
    // ---- save_compact with assignments: compose a wire from parts ---------
    // The wire of the root after `*target = *source` for every assignment,
    // written as save_compact would write it, without changing the root or
    // any source: the root's own bytes, except that each target takes the
    // source's bytes (implicit padding zeroed) and the source's subtree.
    template <class C, class... M>
    consteval bool compose_targets() {
        return (std::is_same_v<C, std::remove_cv_t<M>> || ...);
    }
    template <class C, class... M> consteval bool compose_descend();
    template <class C, std::size_t N, class... M>
    consteval bool compose_members_descend() {
        if constexpr (N >= schema::member_count_v<C>) {
            return false;
        } else {
            using member = schema::member_t<C, N>;
            using Mem0 = typename member::type;
            using Mem = std::remove_cv_t<Mem0>;
            return compose_targets<Mem, M...>() || compose_descend<Mem, M...>() ||
                   compose_members_descend<C, N + 1, M...>();
        }
    }
    // A subobject is visited when one of its subobjects may be a target.
    // Container elements are never visited: a target lies in the root's
    // inline bytes.
    template <class C, class... M>
    consteval bool compose_descend() {
        if constexpr (is_fixed_string_v<C> || is_fixed_vector_v<C> || std::is_scalar_v<C>) return false;
        else if constexpr (std::is_array_v<C>) {
            using E = std::remove_cv_t<std::remove_extent_t<C>>;
            return compose_targets<E, M...>() || compose_descend<E, M...>();
        } else if constexpr (std::is_class_v<C>) return compose_members_descend<C, 0, M...>();
        else return false;
    }

    template <class... M>
    struct ComposeSet {
        std::tuple<const XAssign<M>&...> items;
        template <class C> static consteval bool targets() { return compose_targets<C, M...>(); }
        template <class C> static consteval bool descend() { return compose_descend<C, M...>(); }
        // The source assigned to the subobject `at` (of type C), or null.
        template <class C>
        XOFFSET_DETAIL_INLINE const C* find(const C* at) const noexcept {
            const C* found = nullptr;
            std::apply([&](const auto&... a) {
                ([&] {
                    using A = std::remove_cvref_t<decltype(a)>;
                    if constexpr (std::is_same_v<std::remove_cv_t<typename A::value_type>, C>)
                        if (a.target == at) found = a.source;
                }(), ...);
            }, items);
            return found;
        }
        // Whether a target meets the n bytes at p (else p's subtree is
        // written as is).
        XOFFSET_DETAIL_INLINE bool meets(const void* p, std::size_t n) const noexcept {
            return std::apply([&](const auto&... a) {
                return (verified_paths_intersect(p, n, a.target, sizeof(*a.target)) || ...);
            }, items);
        }
    };

    // Entry check: the number of targets the walk below will find.
    template <class U, class Set>
    std::size_t compose_count(const U& draft, const Set& set) noexcept;
    template <class U, std::size_t N, class Set>
    std::size_t compose_count_member_at(const U& draft, const Set& set) noexcept {
        using member = schema::member_t<U, N>;
        return compose_count(member::get(draft), set);
    }
    template <class U, class Set>
    std::size_t compose_count(const U& draft, const Set& set) noexcept {
        using C = std::remove_cv_t<U>;
        if constexpr (Set::template targets<C>())
            if (set.find(&draft)) return 1;
        std::size_t found = 0;
        if constexpr (Set::template descend<C>()) {
            if (set.meets(&draft, sizeof(C))) {
                if constexpr (std::is_array_v<C>) {
                    for (std::size_t i = 0; i < std::extent_v<C>; ++i) found += compose_count(draft[i], set);
                } else {
                    static_for<schema::member_count_v<C>>([&](auto index) {
                        found += compose_count_member_at<C, decltype(index)::value>(draft, set);
                    });
                }
            }
        }
        return found;
    }

    template <class C>
    XOFFSET_DETAIL_INLINE void zero_padding_bytes(char* at) noexcept {
        if constexpr (std::is_class_v<C> || std::is_array_v<C>) {
            if constexpr (padding_ranges<C>.size() != 0)
                for (const auto& r : padding_ranges<C>)
                    for (std::size_t i = 0; i < r.count; ++i) std::memset(at + r.offset + i * r.stride, 0, r.bytes);
        }
    }

    // With a DirectWriter: writes the subobject `draft` (already copied to
    // writer offset `at`) composed. With a CompactMeasure: only measures.
    template <class U, class W, class Set>
    void compose_value(const U& draft, std::size_t at, W& w, const Set& set) noexcept;

    template <class U, std::size_t N, class W, class Set>
    void compose_member_at(const U& draft, std::size_t at, W& w, const Set& set) noexcept {
        using member = schema::member_t<U, N>;
        compose_value(member::get(draft), at + member::offset, w, set);
    }

    template <class U, class W, class Set>
    void compose_value(const U& draft, std::size_t at, W& w, const Set& set) noexcept {
        using C = std::remove_cv_t<U>;
        if constexpr (Set::template targets<C>()) {
            if (const C* source = set.find(&draft)) {
                if constexpr (is_direct_writer_v<W>) {
                    std::memcpy(w.base + at, static_cast<const void*>(source), sizeof(C));
                    zero_padding_bytes<C>(w.base + at);
                    direct_value(*source, at, w);
                } else {
                    compact_value(*source, w);
                }
                return;
            }
        }
        if constexpr (Set::template descend<C>()) {
            if (set.meets(&draft, sizeof(C))) {
                if constexpr (std::is_array_v<C>) {
                    using E = std::remove_extent_t<C>;
                    for (std::size_t i = 0; i < std::extent_v<C>; ++i) compose_value(draft[i], at + i * sizeof(E), w, set);
                } else {
                    static_for<schema::member_count_v<C>>([&](auto index) {
                        compose_member_at<C, decltype(index)::value>(draft, at, w, set);
                    });
                }
                return;
            }
        }
        if constexpr (is_direct_writer_v<W>) direct_value(draft, at, w);
        else compact_value(draft, w);
    }

    template <class T, class Set>
    std::size_t measure_composed(const T& root, std::size_t limit, std::uint32_t slack_div, const Set& set) noexcept {
        if (limit < compact_root_end<T>()) return 0;
        CompactMeasure w{compact_root_end<T>(), limit, slack_div};
        compose_value(root, sizeof(XBufferCore::arena_header), w, set);
        return w.overflow ? 0 : w.cursor;
    }

    // Payload end limit of a compact wire written into `capacity` bytes.
    inline std::size_t compact_wire_limit(std::size_t capacity) noexcept {
        if (capacity <= sizeof(XWireHeader)) return 0;
        return (std::min)(capacity - sizeof(XWireHeader) + XWIRE_PAYLOAD_BEGIN,
                          std::size_t(XBufferCore::MAX_PAYLOAD));
    }

    // Writes the canonical wire of `root` (with the assignments of `set`)
    // into `out`, which holds at least `limit` bytes (sizeof(XWireHeader) is 40)
    // bytes; returns the bytes written, or 0 if the payload would end past
    // `limit`. No allocation.
    template <bool Plain, class T, class Set>
    std::size_t write_compact_wire(char* out, std::size_t limit, const T& root, std::uint32_t slack_div,
                                    const Set& set) noexcept {
        constexpr std::size_t root_bytes = compact_root_end<T>() - XWIRE_PAYLOAD_BEGIN;
        if (limit < compact_root_end<T>()) return 0;
        char* payload = out + sizeof(XWireHeader);
        DirectWriter<Plain> w{payload, root_bytes, limit - XWIRE_PAYLOAD_BEGIN, slack_div};
        std::memcpy(payload, static_cast<const void*>(&root), sizeof(T));
        if (root_bytes != sizeof(T)) std::memset(payload + sizeof(T), 0, root_bytes - sizeof(T));
        compose_value(root, 0, w, set);
        if (w.overflow) return 0;
        const auto header = compact_wire_header<T>(XWIRE_PAYLOAD_BEGIN + w.cursor);
        std::memcpy(out, &header, sizeof(header));
        return sizeof(XWireHeader) + w.cursor;
    }


    template <class Root, std::size_t Member, std::size_t... I>
    consteval bool flat_projection_members(std::index_sequence<I...>) {
        return ((!compact_has_heads<typename schema::member_t<Root, I>::type>() || I == Member) && ...);
    }

    template <class Root, class Prefix, std::size_t... I>
    consteval bool flat_prefix_matches(std::index_sequence<I...>) {
        return ((std::is_same_v<schema::member_type_t<Root, I>, schema::member_type_t<Prefix, I>> &&
                 schema::member_t<Root, I>::offset == schema::member_t<Prefix, I>::offset) && ...);
    }

    // Owns the output so a stable-layout wire can retain its framing across
    // messages. The record and generated values are copied into its payload.
    enum class FlatRecordFraming { retained, sized_slots = 2 };

    template <class Root, std::size_t Member, class Element, FlatRecordFraming Framing = FlatRecordFraming::retained>
    class FlatRecordWriter {
        using Field = schema::member_t<Root, Member>;
        static_assert(is_wire_admitted_v<Root> && compact_direct_supported<Root>());
        static_assert(std::is_same_v<typename Field::type, XVector<Element>>);
        static_assert(std::is_trivially_copyable_v<Element> && !compact_has_heads<Element>());
        static_assert(flat_projection_members<Root, Member>(
            std::make_index_sequence<schema::member_count_v<Root>>{}));
#ifdef XOFFSET_REJECT_IMPLICIT_PADDING
        static_assert(!has_implicit_padding_v<Root>);
#endif
        static constexpr auto root_bytes = compact_root_end<Root>() - XWIRE_PAYLOAD_BEGIN;
        static constexpr auto max_bytes = (XBufferCore::MAX_PAYLOAD - compact_root_end<Root>()) & ~std::size_t(7u);
        static constexpr auto after_field = Field::offset + sizeof(typename Field::type);
        static constexpr std::align_val_t wire_alignment{64};
        struct Delete {
            void operator()(char* wire) const noexcept { ::operator delete(wire, wire_alignment); }
        };
        std::unique_ptr<char, Delete> wire_;
        std::size_t wire_size_ = 0;
        std::uint32_t capacity_ = 0;
        std::uint32_t size_ = 0;
        struct NoSlots {};
        // A slot keeps the V2 header and vector head for one troop count.
        // Larger records reuse a dynamically sized compact buffer.
        struct Slots {
            std::array<std::unique_ptr<char, Delete>, 5> wires;
            const char* current = nullptr;
        };
#if defined(_MSC_VER) && !defined(__clang__)
        [[msvc::no_unique_address]]
#else
        [[no_unique_address]]
#endif
        std::conditional_t<Framing == FlatRecordFraming::sized_slots, Slots, NoSlots> slots_;

        static constexpr std::size_t payload_extent(std::size_t count) noexcept {
            return (count * sizeof(Element) + 7u) & ~std::size_t(7u);
        }
        static constexpr std::size_t wire_bytes(std::size_t count) noexcept {
            return sizeof(XWireHeader) + root_bytes + payload_extent(count);
        }
        static constexpr auto nonempty_delta = static_cast<std::int32_t>(root_bytes - Field::offset);

        static std::span<const std::byte> sized_wire(const char* data) noexcept {
            if (!data) return {};
            std::uint32_t used;
            std::memcpy(&used, data + offsetof(XWireHeader, used_bytes), sizeof(used));
            return {reinterpret_cast<const std::byte*>(data), sizeof(XWireHeader) + used - XWIRE_PAYLOAD_BEGIN};
        }

        XOFFSET_DETAIL_NOINLINE static char* open_slot(
                std::unique_ptr<char, Delete>& slot, std::size_t count) {
            const auto used = XWIRE_PAYLOAD_BEGIN + root_bytes + payload_extent(count);
            std::unique_ptr<char, Delete> data(static_cast<char*>(::operator new(wire_bytes(count), wire_alignment)));
            std::memset(data.get(), 0, wire_bytes(count));
            const auto head = compact_wire_header<Root>(used);
            std::memcpy(data.get(), &head, sizeof(head));
            const auto size = static_cast<std::uint32_t>(count);
            write_container_head(data.get() + sizeof(XWireHeader), Field::offset,
                                 size == 0 ? 0 : nonempty_delta, size, size);
            slot = std::move(data);
            return slot.get();
        }

        XOFFSET_DETAIL_NOINLINE void resize_wire(std::size_t count) {
            if (count > max_bytes / sizeof(Element))
                XOFFSET_THROW(XException(XErrc::capacity_exceeded, "flat projection exceeds the wire limit"));
            const auto extent = (count * sizeof(Element) + 7u) & ~std::size_t(7u);
            const auto bytes = sizeof(XWireHeader) + root_bytes + extent;
            std::unique_ptr<char, Delete> next(static_cast<char*>(::operator new(bytes, wire_alignment)));
            std::memset(next.get(), 0, bytes);
            const auto head = compact_wire_header<Root>(XWIRE_PAYLOAD_BEGIN + root_bytes + extent);
            std::memcpy(next.get(), &head, sizeof(head));
            wire_ = std::move(next);
            if constexpr (Framing == FlatRecordFraming::retained) {
                wire_size_ = bytes;
                capacity_ = static_cast<std::uint32_t>(count);
            } else {
                // Adding one reserves zero for the unallocated state.
                capacity_ = static_cast<std::uint32_t>(count + 1);
            }
        }

    public:
        FlatRecordWriter() = default;
        FlatRecordWriter(const FlatRecordWriter&) = delete;
        FlatRecordWriter& operator=(const FlatRecordWriter&) = delete;

        std::span<const std::byte> wire() const noexcept {
            if constexpr (Framing == FlatRecordFraming::sized_slots)
                return sized_wire(slots_.current);
            else
                return {reinterpret_cast<const std::byte*>(wire_.get()), wire_size_};
        }

        template <class Prefix = Root, class Prepare, class Generate>
            requires std::is_invocable_r_v<std::size_t, Prepare&, Prefix&> &&
                     std::is_nothrow_invocable_r_v<Element, Generate&, std::size_t>
        XOFFSET_DETAIL_INLINE std::size_t write(Prepare prepare, Generate generate) {
            // A plain prefix can prepare the scalar fields without constructing
            // the vector whose head is emitted separately below.
            if constexpr (!std::is_same_v<Prefix, Root>) {
                static_assert(std::is_standard_layout_v<Prefix> && std::is_trivially_copyable_v<Prefix>);
                static_assert(Member + 1 == schema::member_count_v<Root> && after_field == sizeof(Root));
                static_assert(schema::member_count_v<Prefix> == Member && sizeof(Prefix) >= Field::offset);
                static_assert(flat_prefix_matches<Root, Prefix>(std::make_index_sequence<Member>{}));
            }
            Prefix root{};
            const auto count = std::size_t(prepare(root));
            char* output;
            if constexpr (Framing == FlatRecordFraming::retained) {
                if (count > capacity_ || !wire_) [[unlikely]] resize_wire(count);
                output = wire_.get();
            } else {
                if (count < slots_.wires.size()) [[likely]] {
                    auto& slot = slots_.wires[count];
                    output = slot ? slot.get() : open_slot(slot, count);
                } else {
                    if (count >= capacity_) [[unlikely]] resize_wire(count);
                    output = wire_.get();
                }
            }
            char* body = output + sizeof(XWireHeader);
            const auto* source = reinterpret_cast<const char*>(&root);
            std::memcpy(body, source, Field::offset);
            if constexpr (after_field < sizeof(Root))
                std::memcpy(body + after_field, source + after_field, sizeof(Root) - after_field);
            zero_padding_bytes<Root>(body);
            if constexpr (root_bytes != sizeof(Root))
                std::memset(body + sizeof(Root), 0, root_bytes - sizeof(Root));
            const auto size = static_cast<std::uint32_t>(count);
            if constexpr (Framing == FlatRecordFraming::retained) {
                const auto delta = capacity_ == 0 ? 0 : nonempty_delta;
                write_container_head(body, Field::offset, delta, size, capacity_);
            } else if (count >= slots_.wires.size()) {
                write_container_head(body, Field::offset, nonempty_delta, size, size);
            }
            char* data = body + root_bytes;
            for (std::size_t i = 0; i < count; ++i) {
                const Element value = generate(i);
                std::memcpy(data + i * sizeof(Element), &value, sizeof(Element));
                zero_padding_bytes<Element>(data + i * sizeof(Element));
            }
            if constexpr (Framing == FlatRecordFraming::retained) {
                if (size_ > size) {
                    char* unused = data + count * sizeof(Element);
                    const auto removed = size_ - size;
                    if (removed == 1) std::memset(unused, 0, sizeof(Element));
                    else if (removed == 2) std::memset(unused, 0, 2 * sizeof(Element));
                    else std::memset(unused, 0, removed * sizeof(Element));
                }
                size_ = size;
                return wire_size_;
            } else {
                if constexpr (sizeof(Element) % 8 != 0)
                    std::memset(data + count * sizeof(Element), 0, payload_extent(count) - count * sizeof(Element));
                const auto used = static_cast<std::uint32_t>(XWIRE_PAYLOAD_BEGIN + root_bytes + payload_extent(count));
                if (count >= slots_.wires.size())
                    std::memcpy(output + offsetof(XWireHeader, used_bytes), &used, sizeof(used));
                slots_.current = output;
                return wire_bytes(count);
            }
        }
    };

    template<class T>
    void VerifiedPathsAccess::save_compact_into(XBufferCore& source, std::string& out, XCompactOptions options) {
        static_assert(compact_direct_supported<T>(),
            "save_compact<T>(): every stored type must be at most 8-byte aligned");
        const auto token = identity(source);
        auto* current = &owner(token);
        auto check_alias = [&](XBufferCore& candidate) {
            if (out.capacity() == (std::numeric_limits<std::size_t>::max)())
                XOFFSET_THROW(std::length_error("save_compact_into: output capacity overflows alias range"));
            if (verified_paths_intersect(&out, sizeof(out), candidate.get_address(), candidate.get_size()) ||
                verified_paths_intersect(out.data(), out.capacity() + 1, candidate.get_address(), candidate.get_size()))
                XOFFSET_THROW(XException(XErrc::invalid_state, "save_compact_into: output aliases source storage"));
        };
        check_alias(*current);
        auto h = header<T>(*current);
        current = &owner(token); // header may have recomputed the free table
        check_alias(*current);
        const ComposeSet<> none{};
        auto write = [&](XBufferCore& core, std::size_t capacity) -> std::size_t {
            const auto limit = compact_wire_limit(capacity);
            if (limit == 0) return 0;
            const T& root = *find_root<T>(core);
            std::size_t written = 0;
            auto emit = [&](char* destination) noexcept {
                return options.slack_div == 0
                    ? write_compact_wire<true>(destination, limit, root, 0, none)
                    : write_compact_wire<false>(destination, limit, root, options.slack_div, none);
            };
#if defined(__cpp_lib_string_resize_and_overwrite) && __cpp_lib_string_resize_and_overwrite >= 202110L
            out.resize_and_overwrite(capacity, [&](char* destination, std::size_t) noexcept {
                written = emit(destination);
                return written;
            });
#else
            out.resize(capacity);
            written = emit(out.data());
            out.resize(written);
#endif
            return written;
        };
        // The tight payload is never larger than the source's used bytes:
        // each live block shrinks to its size, holes vanish. With slack it
        // can be larger: an output with room is written without measuring,
        // otherwise the payload is measured first (a walk of the heads).
        auto bound = std::size_t(h.used_bytes);
        if (options.slack_div != 0 || sizeof(XWireHeader) + bound - XWIRE_PAYLOAD_BEGIN <= out.capacity()) {
            if (write(*current, out.capacity()) != 0) return;
        }
        if (options.slack_div != 0) {
            bound = measure_compact(*find_root<T>(*current), (std::numeric_limits<std::uint32_t>::max)(),
                                    options.slack_div);
            if (bound == 0) XOFFSET_THROW(XException(XErrc::capacity_exceeded, "save_compact_into: compact payload exceeds the wire limit"));
        }
        const auto size = sizeof(XWireHeader) + bound - XWIRE_PAYLOAD_BEGIN;
        if (size > out.max_size())
            XOFFSET_THROW(std::length_error("save_compact_into: output size exceeds string limit"));
        if (size > out.capacity()) {
            // Only output-allocation/callback boundary. With slack the next
            // save tries the existing capacity first: leave headroom.
            out.reserve(options.slack_div == 0 ? size : size + (std::min)(size / 8, out.max_size() - size));
            current = &owner(token);
            check_alias(*current);
            const auto used = std::size_t(h.used_bytes);
            h = header<T>(*current);
            if (std::size_t(h.used_bytes) != used)
                XOFFSET_THROW(XException(XErrc::invalid_state, "save_compact_into: source changed beyond prepared output capacity"));
        }
        if (write(*current, out.capacity()) == 0)
            XOFFSET_THROW(XException(XErrc::capacity_exceeded, "save_compact_into: source graph exceeds its payload"));
    }

    template<class T, class... M>
    void VerifiedPathsAccess::save_composed_into(XBufferCore& source, std::string& out, XCompactOptions options,
                                                 const XAssign<M>&... assigns) {
        static_assert(is_wire_admitted_v<T>);
#ifdef XOFFSET_REJECT_IMPLICIT_PADDING
        static_assert(!has_implicit_padding_v<T>,
            "XOFFSET_REJECT_IMPLICIT_PADDING: the wire type has implicit padding (threat model T5/P3)");
#endif
        static_assert(compact_direct_supported<T>(),
            "save_compact<T>(): every stored type must be at most 8-byte aligned");
        // An output inside the draft's storage is rejected untouched; after
        // any other failure out is empty, so a caller that swallows the
        // exception cannot send a partial wire.
        bool out_aliases = false;
        auto check_alias = [&](XBufferCore& candidate) {
            if (out.capacity() == (std::numeric_limits<std::size_t>::max)())
                XOFFSET_THROW(std::length_error("save_compact_into: output capacity overflows alias range"));
            if (verified_paths_intersect(&out, sizeof(out), candidate.get_address(), candidate.get_size()) ||
                verified_paths_intersect(out.data(), out.capacity() + 1, candidate.get_address(), candidate.get_size())) {
                out_aliases = true;
                XOFFSET_THROW(XException(XErrc::invalid_state, "save_compact_into: output aliases source storage"));
            }
        };
        XOFFSET_TRY {
            check_alias(source);
            // Entry checks, before any byte is written.
            const auto* arena = source.arena();
            if (!arena || arena->magic != XBufferCore::image_magic)
                XOFFSET_THROW(XException(XErrc::invalid_payload_header, "verified paths: payload header mismatch"));
            if (arena->used_end < XBufferCore::min_payload_size() ||
                arena->used_end > source.get_size() || arena->used_end > XBufferCore::MAX_PAYLOAD)
                XOFFSET_THROW(XException(XErrc::invalid_payload_header, "verified paths: payload used_end out of range"));
            const T* root = find_root<T>(source);
            {
                const auto* begin = reinterpret_cast<const char*>(root);
                const std::pair<const char*, std::size_t> ranges[] = {
                    {reinterpret_cast<const char*>(assigns.target), sizeof(M)}...};
                for (std::size_t i = 0; i < sizeof...(M); ++i) {
                    const auto [p, n] = ranges[i];
                    if (p < begin || p > begin + sizeof(T) || n > std::size_t(begin + sizeof(T) - p))
                        XOFFSET_THROW(XException(XErrc::invalid_argument, "save_compact_into: assignment target is not inside the root"));
                    for (std::size_t j = 0; j < i; ++j)
                        if (verified_paths_intersect(p, n, ranges[j].first, ranges[j].second))
                            XOFFSET_THROW(XException(XErrc::invalid_argument, "save_compact_into: assignment targets overlap"));
                }
            }
            const ComposeSet<M...> set{std::tuple<const XAssign<M>&...>(assigns...)};
            if (compose_count(*root, set) != sizeof...(M))
                XOFFSET_THROW(XException(XErrc::invalid_argument, "save_compact_into: assignment target is not a member subobject of the root "
                                 "(an element of a container cannot be a target)"));

            constexpr std::size_t kMaxLimit = XBufferCore::MAX_PAYLOAD;
            auto write = [&](const T& from, std::size_t capacity) -> std::size_t {
                const auto limit = compact_wire_limit(capacity);
                if (limit == 0) return 0;
                std::size_t written = 0;
                auto emit = [&](char* destination) noexcept {
                    return options.slack_div == 0
                        ? write_compact_wire<true>(destination, limit, from, 0, set)
                        : write_compact_wire<false>(destination, limit, from, options.slack_div, set);
                };
#if defined(__cpp_lib_string_resize_and_overwrite) && __cpp_lib_string_resize_and_overwrite >= 202110L
                out.resize_and_overwrite(capacity, [&](char* destination, std::size_t) noexcept {
                    written = emit(destination);
                    return written;
                });
#else
                out.resize(capacity);
                written = emit(out.data());
                out.resize(written);
#endif
                return written;
            };
            // Fast path: a reused output usually has room. Writing allocates
            // nothing and runs no host callback, so nothing can move.
            if (write(*root, out.capacity()) != 0) return;
            // Measure, reserve (the only allocation/callback boundary), and
            // write again against the re-resolved owner. Targets are
            // addresses in the draft: if the draft moved, they are stale.
            const auto token = identity(source);
            const auto bound = measure_composed(*root, kMaxLimit, options.slack_div, set);
            if (bound == 0) XOFFSET_THROW(XException(XErrc::capacity_exceeded, "save_compact_into: composed payload exceeds the wire limit"));
            const auto size = sizeof(XWireHeader) + bound - XWIRE_PAYLOAD_BEGIN;
            if (size > out.max_size())
                XOFFSET_THROW(std::length_error("save_compact_into: output size exceeds string limit"));
            // Headroom, so a reused output takes the fast path again when the
            // next message is a little larger.
            out.reserve(size + (std::min)(size / 8, out.max_size() - size));
            auto* current = &owner(token);
            check_alias(*current);
            if (find_root<T>(*current) != root)
                XOFFSET_THROW(XException(XErrc::invalid_state, "save_compact_into: the draft moved while the output was allocated"));
            if (write(*root, out.capacity()) == 0)
                XOFFSET_THROW(XException(XErrc::invalid_state, "save_compact_into: source changed beyond prepared output capacity"));
        } XOFFSET_CATCH(...) {
            if (!out_aliases) out.clear();
            XOFFSET_RETHROW;
        }
    }
}

} // namespace XOffsetDatastructure
