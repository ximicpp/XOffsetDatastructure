#pragma once

// Included after the main definitions. Standalone inclusion is also supported.
#include "xoffsetdatastructure.hpp"
#include <array>
#include <tuple>
#define XOFFSET_VERIFIED_PATHS_API 1

namespace XOffsetDatastructure {
namespace detail {
    inline bool verified_paths_intersect(const void* a, std::size_t an,
                                         const void* b, std::size_t bn) noexcept {
        if (!an || !bn) return false;
        const auto x = reinterpret_cast<std::uintptr_t>(a);
        const auto y = reinterpret_cast<std::uintptr_t>(b);
        return x <= y ? y - x < an : x - y < bn;
    }

    template<class T>
    XWireHeaderV2 VerifiedPathsAccess::header(XBufferCore& source, bool normalize) {
        static_assert(is_v1_wire_admitted_v<T>);
#ifdef XOFFSET_REJECT_IMPLICIT_PADDING
        static_assert(!has_implicit_padding_v<T>,
            "XOFFSET_REJECT_IMPLICIT_PADDING: the wire type has implicit padding (threat model T5/P3)");
#endif
        if (!source.typed_root_access_allowed_)
            throw XException("verified paths: typed source required");
        if (source.schema_bound_ &&
            (source.bound_root_type_id_ != wire_root_type_id<T>() ||
             source.bound_schema_hash_ != wire_schema_hash<T>()))
            throw XException("verified paths: schema mismatch");
        const auto* arena = source.arena();
        if (!arena || std::memcmp(arena->magic, XBufferCore::payload_magic_, 8) != 0 ||
            arena->header_size != sizeof(XBufferCore::arena_header))
            throw XException("verified paths: payload header mismatch");
        if (arena->used_end < XBufferCore::min_payload_size() ||
            arena->used_end > source.segment_size() || arena->used_end > XBufferCore::MAX_PAYLOAD)
            throw XException("verified paths: payload used_end out of range");
        // shrink_to_fit changes only the logical VM length: it does not
        // allocate, shrink host capacity or call T.
        if (normalize) source.shrink_to_fit();
        auto* root = find_root<T>(source);
        if (!root) throw XException("verified paths: root not found");
        // V2: the free table is the wire hole table; recompute it exactly if a
        // host allocation failure (or an unverified load) left it incomplete.
        if (!source.free_table_complete()) {
            std::vector<XBufferCore::WireHole> holes;
            verify_structural_wire_graph(*root, source, std::pmr::new_delete_resource(),
                                         nullptr, 0, false, &holes);
            source.replace_free_blocks(holes);
        }
        XWireHeaderV2 result{};
        std::memcpy(result.magic, XWIRE_MAGIC_V2, sizeof(result.magic));
        result.format_major = XWIRE_FORMAT_MAJOR_V2;
        result.format_minor = XWIRE_FORMAT_MINOR_V2;
        result.header_size = sizeof(result);
        result.used_bytes = source.arena()->used_end;
        const auto* base = static_cast<const char*>(source.get_address());
        result.root_offset = reinterpret_cast<std::uintptr_t>(root) - reinterpret_cast<std::uintptr_t>(base);
        result.hole_count = source.free_block_count();
        result.hole_bytes = source.free_block_bytes();
        result.root_type_id = wire_root_type_id<T>();
        result.schema_hash_lo = wire_schema_hash<T>();
        result.flags = XWIRE_FLAGS_V2;
        result.endian_tag = XWIRE_ENDIAN_TAG_V2;
        return result;
    }

    template<class T>
    void VerifiedPathsAccess::save_into(XBufferCore& source, std::string& out, XSaveOptions options) {
        // A shared record is an identity, not a mapping pin. Resolve it after
        // reserve/new_handler; a moved owner may even be a plain XBufferCore.
        const auto token = identity(source);
        auto* current = &owner(token);
        auto check_alias = [&](XBufferCore& candidate) {
            if (out.capacity() == (std::numeric_limits<std::size_t>::max)())
                throw std::length_error("save_verified_into: output capacity overflows alias range");
            if (verified_paths_intersect(&out, sizeof(out), candidate.get_address(), candidate.segment_size()) ||
                verified_paths_intersect(out.data(), out.capacity() + 1, candidate.get_address(), candidate.segment_size()))
                throw XException("save_verified_into: output aliases source storage");
        };
        check_alias(*current);
        // Host allocations (the free-run list, reserve) are the callback
        // boundaries: the owner is re-resolved after them, and the source
        // must be unchanged when the wire is written. A reused output that
        // already holds the dense size (which bounds every emission) needs no
        // reserve, so the source is examined and normalized once.
        const auto emission = options.emission;
        auto size_of = [&](const XWireHeaderV2& h, const WireGaps& g) {
            return emission == XWireEmission::zero_runs ? wire_dense_size(h, "save_verified_into")
                                                        : wire_free_run_size(h, g, "save_verified_into");
        };
        auto gaps_of = [&](const XBufferCore& core, std::uint64_t used) {
            return emission == XWireEmission::free_runs ? wire_free_runs(core, used) : WireGaps{};
        };
        auto same_holes = [](const XWireHeaderV2& a, const XWireHeaderV2& b) {
            return a.used_bytes == b.used_bytes && a.hole_count == b.hole_count && a.hole_bytes == b.hole_bytes;
        };
        XWireHeaderV2 final_header{};
        WireGaps gaps;
        std::size_t final_size = 0;
        const auto* arena = current->arena();
        if (arena && current->free_table_complete() &&
            sizeof(XWireHeaderV2) + std::uint64_t(arena->used_end) +
                std::uint64_t(current->free_block_count()) * sizeof(XBufferCore::WireHole) <= out.capacity()) {
            gaps = gaps_of(*current, arena->used_end);
            current = &owner(token);
            check_alias(*current);
            const auto used = current->arena() ? current->arena()->used_end : 0;
            const auto holes = current->free_block_count();
            const auto hole_bytes = current->free_block_bytes();
            final_header = header<T>(*current, true);
            if (final_header.used_bytes != used || final_header.hole_count != holes ||
                final_header.hole_bytes != hole_bytes)
                throw XException("save_verified_into: source changed while preparing the output");
            final_size = size_of(final_header, gaps);
        }
        if (final_size == 0 || final_size > out.capacity()) {
            const auto initial = header<T>(*current, false);
            gaps = gaps_of(*current, initial.used_bytes);
            const auto prepared_size = size_of(initial, gaps);
            if (prepared_size > out.max_size())
                throw std::length_error("save_verified_into: output size exceeds string limit");
            out.reserve(prepared_size);
            current = &owner(token);
            check_alias(*current);
            final_header = header<T>(*current, true);
            final_size = size_of(final_header, gaps);
            if (final_size > out.capacity() || final_size != prepared_size || !same_holes(initial, final_header))
                throw XException("save_verified_into: source changed beyond prepared output capacity");
        }
        const auto* base = static_cast<const char*>(current->get_address());
        const auto& core = *current;
#if defined(__cpp_lib_string_resize_and_overwrite) && __cpp_lib_string_resize_and_overwrite >= 202110L
        // Supported string implementation must honor its existing reserved
        // capacity. No host allocation or user callback may occur in this copy.
        out.resize_and_overwrite(final_size, [&](char* destination, std::size_t) noexcept {
            return write_wire(destination, final_header, base, core, emission, gaps);
        });
#else
        out.resize(final_size);
        out.resize(write_wire(out.data(), final_header, base, core, emission, gaps));
#endif
    }
}

#if defined(_MSC_VER)
#define XOFFSET_DETAIL_WIRE_INLINE __forceinline
#elif defined(__clang__) || defined(__GNUC__)
#define XOFFSET_DETAIL_WIRE_INLINE inline __attribute__((always_inline))
#else
#define XOFFSET_DETAIL_WIRE_INLINE inline
#endif
namespace detail {
    // ---- save_compact: canonical wire by direct traversal ----------------
    // Canonical layout: arena header, root at offset 48, then every block in
    // validation (pre)order, each starting where the previous one ends:
    // capacity == size, strings of at most 10 bytes inline, empty containers
    // without storage, no holes. This is the layout XCompactor builds, written
    // straight from the source graph into the output string.
    template <class U> consteval bool compact_has_heads();
    template <class U> consteval bool compact_direct_supported();

    template <class U, std::size_t N = 0>
    consteval bool compact_members_have_heads() {
        if constexpr (N >= reflected_member_count<U>()) {
            return false;
        } else {
            constexpr auto member = std::meta::nonstatic_data_members_of(
                ^^U, std::meta::access_context::unchecked())[N];
            using M = [:std::meta::type_of(member):];
            return compact_has_heads<M>() || compact_members_have_heads<U, N + 1>();
        }
    }
    template <class U, std::size_t N = 0>
    consteval bool compact_members_supported() {
        if constexpr (N >= reflected_member_count<U>()) {
            return true;
        } else {
            constexpr auto member = std::meta::nonstatic_data_members_of(
                ^^U, std::meta::access_context::unchecked())[N];
            using M = [:std::meta::type_of(member):];
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

    // The two head writers shared by CompactWriter and DirectWriter, so the
    // canonical head layout is written in one place: a 16-byte container
    // head at payload offset `at` (bound to the payload base), and the
    // inline form of a string of 1..inline_max bytes (store_inline: bytes
    // 4..14 the characters then zeros, byte 15 = 0x80 | length).
    XOFFSET_DETAIL_WIRE_INLINE void write_container_head(char* base, std::size_t at, std::int32_t data_delta,
                                                         std::uint32_t size, std::uint32_t capacity) noexcept {
        const std::int32_t head[4] = {-static_cast<std::int32_t>(at), data_delta,
                                      static_cast<std::int32_t>(size), static_cast<std::int32_t>(capacity)};
        std::memcpy(base + at, head, sizeof(head));
    }
    XOFFSET_DETAIL_WIRE_INLINE void write_inline_string(char* base, std::size_t at, const char* text,
                                                        std::uint32_t n) noexcept {
        char head[16] = {};
        const std::int32_t delta = -static_cast<std::int32_t>(at);
        std::memcpy(head, &delta, 4);
        for (std::uint32_t i = 0; i < n; ++i) head[4 + i] = text[i];
        head[15] = static_cast<char>(0x80u | n);
        std::memcpy(base + at, head, sizeof(head));
    }

    // Capacity given to a vector of n elements (strings stay tight).
    template <class E>
    XOFFSET_DETAIL_WIRE_INLINE std::uint32_t compact_capacity(std::uint32_t n, std::uint32_t slack_div) noexcept {
        if constexpr (std::is_same_v<std::remove_cv_t<E>, char>) return n;
        if (slack_div == 0 || n < 8u) return n;
        const auto cap = std::uint64_t(n) + n / slack_div + 1u;
        return cap > (std::numeric_limits<std::uint32_t>::max)() ? n : static_cast<std::uint32_t>(cap);
    }

    // With base == nullptr the writer only measures: it walks the source and
    // advances the cursor without writing.
    struct CompactWriter {
        char* base;              // destination payload, or nullptr to measure
        std::size_t cursor;      // next block offset
        std::size_t limit;       // destination payload capacity
        std::uint32_t slack_div; // XCompactOptions::slack_div
        bool overflow = false;   // the source graph did not fit (corrupted source)
        // Reserves a block of `bytes` (padded to 8, padding zeroed).
        bool block(std::size_t bytes, std::size_t& at) noexcept {
            const auto padded = (bytes + 7u) & ~std::size_t(7u);
            if (overflow || padded > limit - cursor) { overflow = true; return false; }
            at = cursor;
            cursor += padded;
            if (base && padded != bytes) std::memset(base + at + bytes, 0, padded - bytes);
            return true;
        }
        template <class E>
        std::uint32_t capacity_for(std::uint32_t n) const noexcept { return compact_capacity<E>(n, slack_div); }
    };
    static_assert(offsetof(XFixedString, arena_delta) == 0 && offsetof(XFixedString, data_delta) == 4 &&
                  offsetof(XFixedString, size_) == 8 && offsetof(XFixedString, capacity_) == 12);
    static_assert(offsetof(XFixedVector<int>, arena_delta) == 0 && offsetof(XFixedVector<int>, data_delta) == 4 &&
                  offsetof(XFixedVector<int>, size_) == 8 && offsetof(XFixedVector<int>, capacity_) == 12);

    // `src` is a source value whose bytes were already copied to payload
    // offset `at`; rewrites every head inside it for the canonical layout.
    template <class U>
    void compact_value(const U& src, std::size_t at, CompactWriter& w) noexcept;

    inline void compact_string(const XFixedString& src, std::size_t at, CompactWriter& w) noexcept {
        const auto text = src.view();
        const auto n = static_cast<std::uint32_t>(text.size());
        if (!w.base) {
            std::size_t data = 0;
            if (n > XFixedString::inline_max) w.block(std::size_t(n) + 1u, data);
            return;
        }
        if (n == 0) { write_container_head(w.base, at, 0, 0, 0); return; }
        if (n <= XFixedString::inline_max) { write_inline_string(w.base, at, text.data(), n); return; }
        std::size_t data = 0;
        if (!w.block(std::size_t(n) + 1u, data)) return;
        std::memcpy(w.base + data, text.data(), n);
        w.base[data + n] = '\0';
        write_container_head(w.base, at, static_cast<std::int32_t>(data - (at + 4u)), n, n);
    }

    template <class E>
    void compact_vector(const XFixedVector<E>& src, std::size_t at, CompactWriter& w) noexcept {
        const auto n = src.size_;
        if (n == 0) {
            if (w.base) write_container_head(w.base, at, 0, 0, 0);
            return;
        }
        const auto bytes = std::size_t(n) * sizeof(E);
        const auto cap = w.capacity_for<E>(n);
        std::size_t data = 0;
        if (!w.block(std::size_t(cap) * sizeof(E), data)) return;
        if (w.base) {
            std::memcpy(w.base + data, static_cast<const void*>(src.data()), bytes);
            if (cap != n) std::memset(w.base + data + bytes, 0, std::size_t(cap - n) * sizeof(E));
            write_container_head(w.base, at, static_cast<std::int32_t>(data - (at + 4u)), n, cap);
        }
        if constexpr (compact_has_heads<E>()) {
            const E* elements = src.data();
            for (std::uint32_t i = 0; i < n; ++i) compact_value(elements[i], data + std::size_t(i) * sizeof(E), w);
        }
    }

    template <class U, std::size_t N>
    void compact_member_at(const U& src, std::size_t at, CompactWriter& w) noexcept {
        constexpr auto member = std::meta::nonstatic_data_members_of(
            ^^U, std::meta::access_context::unchecked())[N];
        using M = [:std::meta::type_of(member):];
        if constexpr (compact_has_heads<M>())
            compact_value(src.[:member:], at + std::meta::offset_of(member).bytes, w);
    }

    template <class U>
    void compact_value(const U& src, std::size_t at, CompactWriter& w) noexcept {
        using C = std::remove_cv_t<U>;
        if constexpr (!compact_has_heads<C>()) {
            return;
        } else if constexpr (is_fixed_string_v<C>) {
            compact_string(static_cast<const XFixedString&>(src), at, w);
        } else if constexpr (is_fixed_vector_v<C>) {
            compact_vector(static_cast<const XFixedVector<typename C::value_type>&>(src), at, w);
        } else if constexpr (std::is_array_v<C>) {
            using Elem = std::remove_extent_t<C>;
            for (std::size_t i = 0; i < std::extent_v<C>; ++i) compact_value(src[i], at + i * sizeof(Elem), w);
        } else {
            static_for<reflected_member_count<C>()>([&](auto index) {
                compact_member_at<C, decltype(index)::value>(src, at, w);
            });
        }
    }

    // Writes the canonical wire of `root` (of `source`) into `out`, which
    // holds at least sizeof(XWireHeaderV2) + `limit` bytes; returns the
    // bytes written, or 0 if the graph did not fit. No allocation.
    template <class T>
    constexpr std::size_t compact_root_end() noexcept {
        return sizeof(XBufferCore::arena_header) + ((sizeof(T) + 7u) & ~std::size_t(7u));
    }

    // Payload bytes of the compact wire of `root`, or 0 if it exceeds `limit`.
    template <class T>
    std::size_t measure_compact(const T& root, std::size_t limit, std::uint32_t slack_div) noexcept {
        if (limit < compact_root_end<T>()) return 0;
        CompactWriter w{nullptr, compact_root_end<T>(), limit, slack_div};
        compact_value(root, sizeof(XBufferCore::arena_header), w);
        return w.overflow ? 0 : w.cursor;
    }

    template <class T>
    std::size_t write_compact_wire(char* out, std::size_t limit, XWireHeaderV2 header,
                                   const XBufferCore::arena_header& source_arena, const T& root,
                                   std::uint32_t slack_div) noexcept {
        constexpr std::size_t arena = sizeof(XBufferCore::arena_header);
        constexpr std::size_t root_bytes = compact_root_end<T>() - arena;
        if (limit < arena + root_bytes) return 0;
        char* payload = out + sizeof(XWireHeaderV2);
        CompactWriter w{payload, arena + root_bytes, limit, slack_div};
        std::memcpy(payload + arena, static_cast<const void*>(&root), sizeof(T));
        if (root_bytes != sizeof(T)) std::memset(payload + arena + sizeof(T), 0, root_bytes - sizeof(T));
        compact_value(root, arena, w);
        if (w.overflow) return 0;
        XBufferCore::arena_header h{};
        std::memcpy(h.magic, source_arena.magic, sizeof(h.magic));
        h.header_size = static_cast<std::uint32_t>(arena);
        h.root_offset = arena;
        h.used_end = w.cursor;
        std::memcpy(payload, &h, sizeof(h));
        header.used_bytes = w.cursor;
        header.root_offset = arena;
        header.hole_count = 0;
        header.hole_bytes = 0;
        header.extent_count = 0;
        std::memcpy(out, &header, sizeof(header));
        return sizeof(XWireHeaderV2) + w.cursor;
    }

    template<class T>
    void VerifiedPathsAccess::save_compact_into(XBufferCore& source, std::string& out, XCompactOptions options) {
        static_assert(compact_direct_supported<T>(),
            "save_compact<T>(): every stored type must be at most 8-byte aligned");
        const auto token = identity(source);
        auto* current = &owner(token);
        auto check_alias = [&](XBufferCore& candidate) {
            if (out.capacity() == (std::numeric_limits<std::size_t>::max)())
                throw std::length_error("save_compact_into: output capacity overflows alias range");
            if (verified_paths_intersect(&out, sizeof(out), candidate.get_address(), candidate.segment_size()) ||
                verified_paths_intersect(out.data(), out.capacity() + 1, candidate.get_address(), candidate.segment_size()))
                throw XException("save_compact_into: output aliases source storage");
        };
        check_alias(*current);
        // The tight payload is never larger than the source's used bytes:
        // each live block shrinks to its size, holes vanish. With slack it
        // can be larger, so it is measured first (a walk of the heads).
        auto h = header<T>(*current, false);
        current = &owner(token); // header may have recomputed the free table
        check_alias(*current);
        auto bound = std::size_t(h.used_bytes);
        if (options.slack_div != 0) {
            bound = measure_compact(*find_root<T>(*current), (std::numeric_limits<std::uint32_t>::max)(),
                                    options.slack_div);
            if (bound == 0) throw XException("save_compact_into: compact payload exceeds the wire limit");
        }
        const auto size = sizeof(XWireHeaderV2) + bound;
        if (size > out.max_size())
            throw std::length_error("save_compact_into: output size exceeds string limit");
        if (size > out.capacity()) {
            out.reserve(size); // only output-allocation/callback boundary
            current = &owner(token);
            check_alias(*current);
            const auto used = std::size_t(h.used_bytes);
            h = header<T>(*current, false);
            if (std::size_t(h.used_bytes) != used)
                throw XException("save_compact_into: source changed beyond prepared output capacity");
        }
        const T& root = *find_root<T>(*current);
        const auto& source_arena = *current->arena();
        std::size_t written = 0;
#if defined(__cpp_lib_string_resize_and_overwrite) && __cpp_lib_string_resize_and_overwrite >= 202110L
        out.resize_and_overwrite(size, [&](char* destination, std::size_t) noexcept {
            written = write_compact_wire(destination, bound, h, source_arena, root, options.slack_div);
            return written;
        });
#else
        out.resize(size);
        written = write_compact_wire(out.data(), bound, h, source_arena, root, options.slack_div);
        out.resize(written);
#endif
        if (written == 0)
            throw XException("save_compact_into: source graph exceeds its payload");
    }
}

namespace detail {
    // ---- direct writer: the write pass of save_compact, write-only ------
    // Same layout and bytes as CompactWriter (canonical, see above), with
    // the same head writers, but small blocks are copied in 8-byte words
    // and there is no measuring branch. Plain: capacity == size
    // (slack_div == 0), decided once per save.
    template <bool Plain>
    struct DirectWriter {
        char* base;
        std::size_t cursor;
        std::size_t limit;
        std::uint32_t slack_div;
        bool overflow = false;
        template <class E>
        XOFFSET_DETAIL_WIRE_INLINE std::uint32_t capacity_for(std::uint32_t n) const noexcept {
            if constexpr (Plain) return n;
            else return compact_capacity<E>(n, slack_div);
        }
        // Reserves `padded` bytes (a multiple of 8); false on overflow.
        XOFFSET_DETAIL_WIRE_INLINE bool take(std::size_t padded, std::size_t& at) noexcept {
            if (padded > limit - cursor) [[unlikely]] { overflow = true; return false; }
            at = cursor;
            cursor += padded;
            return true;
        }
    };
    template <class W> inline constexpr bool is_direct_writer_v = false;
    template <bool P> inline constexpr bool is_direct_writer_v<DirectWriter<P>> = true;

    // n is a multiple of 8.
    XOFFSET_DETAIL_WIRE_INLINE void direct_copy_words(char* dst, const char* src, std::size_t n) noexcept {
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
        constexpr auto member = std::meta::nonstatic_data_members_of(
            ^^U, std::meta::access_context::unchecked())[N];
        using M = [:std::meta::type_of(member):];
        if constexpr (compact_has_heads<M>())
            direct_value(src.[:member:], at + std::meta::offset_of(member).bytes, w);
    }

    template <bool P>
    XOFFSET_DETAIL_WIRE_INLINE void direct_string(const XFixedString& src, std::size_t at, DirectWriter<P>& w) noexcept {
        const auto text = src.view();
        const auto n = static_cast<std::uint32_t>(text.size());
        if (n == 0) { write_container_head(w.base, at, 0, 0, 0); return; }
        if (n <= XFixedString::inline_max) { write_inline_string(w.base, at, text.data(), n); return; }
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
        write_container_head(w.base, at, static_cast<std::int32_t>(data - (at + 4u)), n, n);
    }

    template <class E, bool P>
    XOFFSET_DETAIL_WIRE_INLINE void direct_vector(const XFixedVector<E>& src, std::size_t at, DirectWriter<P>& w) noexcept {
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
        write_container_head(w.base, at, static_cast<std::int32_t>(data - (at + 4u)), n, cap);
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
            direct_string(static_cast<const XFixedString&>(src), at, w);
        } else if constexpr (is_fixed_vector_v<C>) {
            direct_vector(static_cast<const XFixedVector<typename C::value_type>&>(src), at, w);
        } else if constexpr (std::is_array_v<C>) {
            using Elem = std::remove_extent_t<C>;
            for (std::size_t i = 0; i < std::extent_v<C>; ++i) direct_value(src[i], at + i * sizeof(Elem), w);
        } else {
            static_for<reflected_member_count<C>()>([&](auto index) {
                direct_member_at<C, decltype(index)::value>(src, at, w);
            });
        }
    }

    // Wire header of a canonical payload of `used` bytes: no holes, no
    // extents, root at offset 48.
    template <class T>
    XWireHeaderV2 compact_wire_header(std::size_t used) noexcept {
        XWireHeaderV2 header{};
        std::memcpy(header.magic, XWIRE_MAGIC_V2, sizeof(header.magic));
        header.format_major = XWIRE_FORMAT_MAJOR_V2;
        header.format_minor = XWIRE_FORMAT_MINOR_V2;
        header.header_size = sizeof(header);
        header.used_bytes = used;
        header.root_offset = sizeof(XBufferCore::arena_header);
        header.root_type_id = wire_root_type_id<T>();
        header.schema_hash_lo = wire_schema_hash<T>();
        header.flags = XWIRE_FLAGS_V2;
        header.endian_tag = XWIRE_ENDIAN_TAG_V2;
        return header;
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
        if constexpr (N >= reflected_member_count<C>()) {
            return false;
        } else {
            constexpr auto member = std::meta::nonstatic_data_members_of(
                ^^C, std::meta::access_context::unchecked())[N];
            using Mem0 = [:std::meta::type_of(member):];
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
        XOFFSET_DETAIL_WIRE_INLINE const C* find(const C* at) const noexcept {
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
        XOFFSET_DETAIL_WIRE_INLINE bool meets(const void* p, std::size_t n) const noexcept {
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
        constexpr auto member = std::meta::nonstatic_data_members_of(
            ^^U, std::meta::access_context::unchecked())[N];
        return compose_count(draft.[:member:], set);
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
                    static_for<reflected_member_count<C>()>([&](auto index) {
                        found += compose_count_member_at<C, decltype(index)::value>(draft, set);
                    });
                }
            }
        }
        return found;
    }

    template <class C>
    XOFFSET_DETAIL_WIRE_INLINE void zero_padding_bytes(char* at) noexcept {
        if constexpr (std::is_class_v<C> || std::is_array_v<C>) {
            if constexpr (padding_ranges<C>.size() != 0)
                for (const auto& r : padding_ranges<C>)
                    for (std::size_t i = 0; i < r.count; ++i) std::memset(at + r.offset + i * r.stride, 0, r.bytes);
        }
    }

    // With a DirectWriter: writes the subobject `draft` (already copied to
    // payload offset `at`) composed. With a measuring CompactWriter: only
    // measures.
    template <class U, class W, class Set>
    void compose_value(const U& draft, std::size_t at, W& w, const Set& set) noexcept;

    template <class U, std::size_t N, class W, class Set>
    void compose_member_at(const U& draft, std::size_t at, W& w, const Set& set) noexcept {
        constexpr auto member = std::meta::nonstatic_data_members_of(
            ^^U, std::meta::access_context::unchecked())[N];
        compose_value(draft.[:member:], at + std::meta::offset_of(member).bytes, w, set);
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
                    compact_value(*source, at, w);
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
                    static_for<reflected_member_count<C>()>([&](auto index) {
                        compose_member_at<C, decltype(index)::value>(draft, at, w, set);
                    });
                }
                return;
            }
        }
        if constexpr (is_direct_writer_v<W>) direct_value(draft, at, w);
        else compact_value(draft, at, w);
    }

    template <class T, class Set>
    std::size_t measure_composed(const T& root, std::size_t limit, std::uint32_t slack_div, const Set& set) noexcept {
        if (limit < compact_root_end<T>()) return 0;
        CompactWriter w{nullptr, compact_root_end<T>(), limit, slack_div};
        compose_value(root, sizeof(XBufferCore::arena_header), w, set);
        return w.overflow ? 0 : w.cursor;
    }

    // Writes the composed wire into `out` (at least sizeof(XWireHeaderV2) +
    // `limit` bytes); returns the bytes written, or 0 if it did not fit.
    template <bool Plain, class T, class Set>
    std::size_t write_composed_wire(char* out, std::size_t limit, const XBufferCore::arena_header& source_arena,
                                    const T& root, std::uint32_t slack_div, const Set& set) noexcept {
        constexpr std::size_t arena = sizeof(XBufferCore::arena_header);
        constexpr std::size_t root_bytes = compact_root_end<T>() - arena;
        if (limit < arena + root_bytes) return 0;
        char* payload = out + sizeof(XWireHeaderV2);
        DirectWriter<Plain> w{payload, arena + root_bytes, limit, slack_div};
        std::memcpy(payload + arena, static_cast<const void*>(&root), sizeof(T));
        if (root_bytes != sizeof(T)) std::memset(payload + arena + sizeof(T), 0, root_bytes - sizeof(T));
        compose_value(root, arena, w, set);
        if (w.overflow) return 0;
        XBufferCore::arena_header h{};
        std::memcpy(h.magic, source_arena.magic, sizeof(h.magic));
        h.header_size = static_cast<std::uint32_t>(arena);
        h.root_offset = arena;
        h.used_end = w.cursor;
        std::memcpy(payload, &h, sizeof(h));
        const auto header = compact_wire_header<T>(w.cursor);
        std::memcpy(out, &header, sizeof(header));
        return sizeof(XWireHeaderV2) + w.cursor;
    }

    template<class T, class... M>
    void VerifiedPathsAccess::save_composed_into(XBufferCore& source, std::string& out, XCompactOptions options,
                                                 const XAssign<M>&... assigns) {
        static_assert(is_v1_wire_admitted_v<T>);
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
                throw std::length_error("save_compact_into: output capacity overflows alias range");
            if (verified_paths_intersect(&out, sizeof(out), candidate.get_address(), candidate.segment_size()) ||
                verified_paths_intersect(out.data(), out.capacity() + 1, candidate.get_address(), candidate.segment_size())) {
                out_aliases = true;
                throw XException("save_compact_into: output aliases source storage");
            }
        };
        try {
            check_alias(source);
            // Entry checks, before any byte is written.
            const auto* arena = source.arena();
            if (!arena || std::memcmp(arena->magic, XBufferCore::payload_magic_, 8) != 0 ||
                arena->header_size != sizeof(XBufferCore::arena_header))
                throw XException("verified paths: payload header mismatch");
            if (arena->used_end < XBufferCore::min_payload_size() ||
                arena->used_end > source.segment_size() || arena->used_end > XBufferCore::MAX_PAYLOAD)
                throw XException("verified paths: payload used_end out of range");
            const T* root = find_root<T>(source);
            {
                const auto* begin = reinterpret_cast<const char*>(root);
                const std::pair<const char*, std::size_t> ranges[] = {
                    {reinterpret_cast<const char*>(assigns.target), sizeof(M)}...};
                for (std::size_t i = 0; i < sizeof...(M); ++i) {
                    const auto [p, n] = ranges[i];
                    if (p < begin || p > begin + sizeof(T) || n > std::size_t(begin + sizeof(T) - p))
                        throw XException("save_compact_into: assignment target is not inside the root");
                    for (std::size_t j = 0; j < i; ++j)
                        if (verified_paths_intersect(p, n, ranges[j].first, ranges[j].second))
                            throw XException("save_compact_into: assignment targets overlap");
                }
            }
            const ComposeSet<M...> set{std::tuple<const XAssign<M>&...>(assigns...)};
            if (compose_count(*root, set) != sizeof...(M))
                throw XException("save_compact_into: assignment target is not a member subobject of the root "
                                 "(an element of a container cannot be a target)");

            constexpr std::size_t kMaxLimit = XBufferCore::MAX_PAYLOAD;
            auto write = [&](const T& from, const XBufferCore::arena_header& source_arena,
                             std::size_t capacity) -> std::size_t {
                if (capacity <= sizeof(XWireHeaderV2)) return 0;
                const auto limit = (std::min)(capacity - sizeof(XWireHeaderV2), kMaxLimit);
                std::size_t written = 0;
                auto emit = [&](char* destination) noexcept {
                    return options.slack_div == 0
                        ? write_composed_wire<true>(destination, limit, source_arena, from, 0, set)
                        : write_composed_wire<false>(destination, limit, source_arena, from, options.slack_div, set);
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
            if (write(*root, *arena, out.capacity()) != 0) return;
            // Measure, reserve (the only allocation/callback boundary), and
            // write again against the re-resolved owner. Targets are
            // addresses in the draft: if the draft moved, they are stale.
            const auto token = identity(source);
            const auto bound = measure_composed(*root, kMaxLimit, options.slack_div, set);
            if (bound == 0) throw XException("save_compact_into: composed payload exceeds the wire limit");
            const auto size = sizeof(XWireHeaderV2) + bound;
            if (size > out.max_size())
                throw std::length_error("save_compact_into: output size exceeds string limit");
            // Headroom, so a reused output takes the fast path again when the
            // next message is a little larger.
            out.reserve(size + (std::min)(size / 8, out.max_size() - size));
            auto* current = &owner(token);
            check_alias(*current);
            if (find_root<T>(*current) != root)
                throw XException("save_compact_into: the draft moved while the output was allocated");
            if (write(*root, *current->arena(), out.capacity()) == 0)
                throw XException("save_compact_into: source changed beyond prepared output capacity");
        } catch (...) {
            if (!out_aliases) out.clear();
            throw;
        }
    }
}

template<class T> class VerifiedSnapshot;
template<class T>
VerifiedSnapshot<T> freeze_verified(TypedXBuffer<T>&& source,
    std::pmr::memory_resource* scratch = std::pmr::new_delete_resource());

// A snapshot owns the VM payload and a stable header, not a borrowed XBuffer.
// All writable aliases to the source must cease before freeze and remain idle.
template<class T>
class VerifiedSnapshot {
    struct Control {
        XWireHeaderV2 header{};
        std::vector<XBufferCore::WireHole> holes;
        XBuffer source;
    };
    std::shared_ptr<Control> owner_;
    explicit VerifiedSnapshot(std::shared_ptr<Control> owner) noexcept : owner_(std::move(owner)) {}
    friend VerifiedSnapshot<T> freeze_verified<T>(TypedXBuffer<T>&&, std::pmr::memory_resource*);
    // V2: {header, payload, hole table}.
    static std::array<std::span<const std::byte>, 3> views(const std::shared_ptr<Control>& owner) noexcept {
        if (!owner) return {};
        return {std::span<const std::byte>(reinterpret_cast<const std::byte*>(&owner->header), sizeof(owner->header)),
                std::span<const std::byte>(reinterpret_cast<const std::byte*>(owner->source.get_address()),
                                          std::size_t(owner->header.used_bytes)),
                std::span<const std::byte>(reinterpret_cast<const std::byte*>(owner->holes.data()),
                                          owner->holes.size() * sizeof(XBufferCore::WireHole))};
    }
public:
    VerifiedSnapshot() noexcept = default;
    std::array<std::span<const std::byte>, 3> segments() const noexcept { return views(owner_); }
    explicit operator bool() const noexcept { return bool(owner_); }
    const T& root() const {
        if (!owner_) throw XException("VerifiedSnapshot: empty snapshot");
        return *owner_->source.template find_root_storage<T>();
    }
    class IoLease {
        std::shared_ptr<Control> owner_;
        bool cancelled_ = false;
        explicit IoLease(std::shared_ptr<Control> owner) noexcept : owner_(std::move(owner)) {}
        friend class VerifiedSnapshot;
    public:
        IoLease(IoLease&&) noexcept = default;
        IoLease& operator=(IoLease&&) noexcept = default;
        IoLease(const IoLease&) = delete;
        IoLease& operator=(const IoLease&) = delete;
        std::array<std::span<const std::byte>, 3> segments() const noexcept { return views(owner_); }
        void cancel() noexcept { cancelled_ = true; } // cancellation never releases
        bool cancellation_requested() const noexcept { return cancelled_; }
        void complete() noexcept { owner_.reset(); }
        // Caller must move this value into its real completion owner, retaining
        // it until I/O completes even after cancellation. Destruction is not a
        // transport completion event. No asynchronous transport is invented here.
    };
    [[nodiscard]] IoLease acquire_io_lease() const noexcept { return IoLease(owner_); }
};

template<class T>
VerifiedSnapshot<T> freeze_verified(TypedXBuffer<T>&& source, std::pmr::memory_resource* scratch) {
    static_assert(is_v1_wire_admitted_v<T>);
    if (!scratch) throw XException("freeze_verified: null scratch resource");
    const auto identity = detail::VerifiedPathsAccess::identity(source);
    // Empty Control/default XBuffer do not initialize a reservation. Both the
    // control object and shared_ptr block exist before consuming source storage.
    auto owner = std::make_shared<typename VerifiedSnapshot<T>::Control>();
    auto& current = detail::VerifiedPathsAccess::owner(identity);
    if (&current != static_cast<XBufferCore*>(&source))
        throw XException("freeze_verified: source moved during preparation");
    const auto header = detail::VerifiedPathsAccess::header<T>(current, false);
    if (current.arena()->flags != 0 || current.arena()->reserved0 != 0 || current.arena()->reserved1 != 0)
        throw XException("freeze_verified: unsupported payload flags");
    owner->holes.reserve(current.free_block_count());
    current.for_each_free_block([&](XBufferCore::WireHole hole) { owner->holes.push_back(hole); });
    auto* root = detail::find_root<T>(current);
    // Root supplies the additive third-argument verifier/scratch patch. Every
    // invocation creates fresh local membership indexes; no validation cache.
    detail::verify_structural_wire_graph(*root, current, scratch);
    if (&detail::VerifiedPathsAccess::owner(identity) != &current)
        throw XException("freeze_verified: source changed during validation");
    // All throwing control/validation work is over. Normalization now changes
    // only logical VM size and trivial host FreeBlock elements, then the move
    // publishes a complete immutable owner. Scratch/control OOM did not shrink.
    current.shrink_to_fit();
    owner->header = header;
    owner->source = std::move(static_cast<XBuffer&>(source)); // noexcept commit
    // The frozen payload is immutable: it has no host owner for allocation.
    detail::VerifiedPathsAccess::detach_host_owner(owner->source);
    return VerifiedSnapshot<T>(std::move(owner));
}
} // namespace XOffsetDatastructure
