#pragma once

// Included after the main definitions. Standalone inclusion is also supported.
#include "xoffsetdatastructure.hpp"
#include <array>
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
        // The dense size bounds every emission. A reused output that already
        // holds it needs no reserve (the only output-allocation/callback
        // boundary), so the source is examined and normalized once.
        XWireHeaderV2 final_header{};
        std::size_t final_size = 0;
        const auto* arena = current->arena();
        if (arena && current->free_table_complete() &&
            sizeof(XWireHeaderV2) + std::uint64_t(arena->used_end) +
                std::uint64_t(current->free_block_count()) * sizeof(XBufferCore::WireHole) <= out.capacity()) {
            final_header = header<T>(*current, true);
            final_size = wire_dense_size(final_header, "save_verified_into");
        }
        if (final_size == 0 || final_size > out.capacity()) {
            const auto initial = header<T>(*current, false);
            const auto prepared_size = wire_dense_size(initial, "save_verified_into");
            if (prepared_size > out.max_size())
                throw std::length_error("save_verified_into: output size exceeds string limit");
            out.reserve(prepared_size);
            current = &owner(token);
            check_alias(*current);
            final_header = header<T>(*current, true);
            final_size = wire_dense_size(final_header, "save_verified_into");
            if (final_size > out.capacity() || final_size != prepared_size)
                throw XException("save_verified_into: source changed beyond prepared output capacity");
        }
        const auto* base = static_cast<const char*>(current->get_address());
        const auto& core = *current;
#if defined(__cpp_lib_string_resize_and_overwrite) && __cpp_lib_string_resize_and_overwrite >= 202110L
        // Supported string implementation must honor its existing reserved
        // capacity. No host allocation or user callback may occur in this copy.
        out.resize_and_overwrite(final_size, [&](char* destination, std::size_t) noexcept {
            return write_wire(destination, final_header, base, core, options.elide_zero_runs);
        });
#else
        out.resize(final_size);
        out.resize(write_wire(out.data(), final_header, base, core, options.elide_zero_runs));
#endif
    }
}

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

    // With base == nullptr the writer only measures: it walks the source and
    // advances the cursor without writing.
    struct CompactWriter {
        char* base;              // destination payload, or nullptr to measure
        std::size_t cursor;      // next block offset
        std::size_t limit;       // destination payload capacity
        std::uint32_t slack_div; // XCompactOptions::slack_div
        bool overflow = false;   // the source graph did not fit (corrupted source)
        void put32(std::size_t at, std::int32_t v) noexcept { std::memcpy(base + at, &v, sizeof(v)); }
        void putu32(std::size_t at, std::uint32_t v) noexcept { std::memcpy(base + at, &v, sizeof(v)); }
        // Reserves a block of `bytes` (padded to 8, padding zeroed).
        bool block(std::size_t bytes, std::size_t& at) noexcept {
            const auto padded = (bytes + 7u) & ~std::size_t(7u);
            if (overflow || padded > limit - cursor) { overflow = true; return false; }
            at = cursor;
            cursor += padded;
            if (base && padded != bytes) std::memset(base + at + bytes, 0, padded - bytes);
            return true;
        }
        // Capacity given to a vector of n elements (strings stay tight).
        template <class E>
        std::uint32_t capacity_for(std::uint32_t n) const noexcept {
            if constexpr (std::is_same_v<std::remove_cv_t<E>, char>) return n;
            if (slack_div == 0 || n < 8u) return n;
            const auto cap = std::uint64_t(n) + n / slack_div + 1u;
            return cap > (std::numeric_limits<std::uint32_t>::max)() ? n : static_cast<std::uint32_t>(cap);
        }
        // Head at `at`: bound to the payload base, empty.
        void empty_head(std::size_t at) noexcept {
            put32(at, -static_cast<std::int32_t>(at));
            put32(at + 4u, 0);
            putu32(at + 8u, 0);
            putu32(at + 12u, 0);
        }
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
        w.empty_head(at);
        if (n == 0) return;
        if (n <= XFixedString::inline_max) {
            char bytes[12] = {};
            std::memcpy(bytes, text.data(), n);
            bytes[11] = static_cast<char>(0x80u | n);
            std::memcpy(w.base + at + 4u, bytes, sizeof(bytes));
            return;
        }
        std::size_t data = 0;
        if (!w.block(std::size_t(n) + 1u, data)) return;
        std::memcpy(w.base + data, text.data(), n);
        w.base[data + n] = '\0';
        w.put32(at + 4u, static_cast<std::int32_t>(data - (at + 4u)));
        w.putu32(at + 8u, n);
        w.putu32(at + 12u, n);
    }

    template <class E>
    void compact_vector(const XFixedVector<E>& src, std::size_t at, CompactWriter& w) noexcept {
        const auto n = src.size_;
        if (w.base) w.empty_head(at);
        if (n == 0) return;
        const auto bytes = std::size_t(n) * sizeof(E);
        const auto cap = w.capacity_for<E>(n);
        std::size_t data = 0;
        if (!w.block(std::size_t(cap) * sizeof(E), data)) return;
        if (w.base) {
            std::memcpy(w.base + data, static_cast<const void*>(src.data()), bytes);
            if (cap != n) std::memset(w.base + data + bytes, 0, std::size_t(cap - n) * sizeof(E));
            w.put32(at + 4u, static_cast<std::int32_t>(data - (at + 4u)));
            w.putu32(at + 8u, n);
            w.putu32(at + 12u, cap);
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
