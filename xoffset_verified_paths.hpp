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

    inline std::size_t verified_wire_size(const XWireHeaderV2& h) {
        const auto holes = std::size_t(h.hole_count) * sizeof(XBufferCore::WireHole);
        if (std::size_t(h.used_bytes) > (std::numeric_limits<std::size_t>::max)() - sizeof(XWireHeaderV2) ||
            holes > (std::numeric_limits<std::size_t>::max)() - sizeof(XWireHeaderV2) - std::size_t(h.used_bytes))
            throw std::length_error("save_verified_into: output size exceeds string limit");
        return sizeof(XWireHeaderV2) + std::size_t(h.used_bytes) + holes;
    }

    template<class T>
    void VerifiedPathsAccess::save_into(XBufferCore& source, std::string& out) {
        // A shared record is an identity, not a mapping pin. Resolve it after
        // reserve/new_handler; a moved owner may even be a plain XBufferCore.
        const auto token = identity(source);
        auto& initial = owner(token);
        auto check_alias = [&](XBufferCore& current) {
            if (out.capacity() == (std::numeric_limits<std::size_t>::max)())
                throw std::length_error("save_verified_into: output capacity overflows alias range");
            if (verified_paths_intersect(&out, sizeof(out), current.get_address(), current.segment_size()) ||
                verified_paths_intersect(out.data(), out.capacity() + 1, current.get_address(), current.segment_size()))
                throw XException("save_verified_into: output aliases source storage");
        };
        check_alias(initial);
        const auto initial_header = header<T>(initial, false);
        const auto prepared_size = verified_wire_size(initial_header);
        if (prepared_size > out.max_size())
            throw std::length_error("save_verified_into: output size exceeds string limit");
        out.reserve(prepared_size); // only output-allocation/callback boundary

        auto& current = owner(token);
        check_alias(current);
        auto final_header = header<T>(current, false);
        const auto final_size = verified_wire_size(final_header);
        if (final_size > out.capacity())
            throw XException("save_verified_into: source changed beyond prepared output capacity");
        final_header = header<T>(current, true);
        const auto* base = static_cast<const char*>(current.get_address());
        const auto used = std::size_t(final_header.used_bytes);
#if defined(__cpp_lib_string_resize_and_overwrite) && __cpp_lib_string_resize_and_overwrite >= 202110L
        // Supported string implementation must honor its existing reserved
        // capacity. No host allocation or user callback may occur in this copy.
        out.resize_and_overwrite(final_size, [&](char* destination, std::size_t) noexcept {
            std::memcpy(destination, &final_header, sizeof(final_header));
            std::memcpy(destination + sizeof(final_header), base, used);
            current.write_free_blocks(destination + sizeof(final_header) + used);
            return final_size;
        });
#else
        out.resize(final_size);
        std::memcpy(out.data(), &final_header, sizeof(final_header));
        std::memcpy(out.data() + sizeof(final_header), base, used);
        current.write_free_blocks(out.data() + sizeof(final_header) + used);
#endif
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
