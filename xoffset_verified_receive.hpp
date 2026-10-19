#pragma once
#include "receive_raw_fwd.hpp"
#include "xoffset_verified_paths.hpp"
#include "receive_raw.hpp" // separately authored, root-pinned component
#define XOFFSET_VERIFIED_RECEIVE_API 1

namespace XOffsetDatastructure {
namespace detail {
    struct VerifiedReceiveAccess {
        struct PendingOwner {
            XBufferCore::RegistryState* registry;
            XBufferCore::OwnerRecordPtr record;
            XBufferCore::RegistryOwners::node_type node;
        };
        static PendingOwner prepare_owner(XBufferCore& source) {
            if (std::exchange(pending_registry_failure, false)) throw XBadAlloc();
            try {
                // Initialize process registry and allocate both objects before
                // raw validation, but never expose an unactivated Core owner to
                // new_handler through the global lookup namespace.
                auto* registry = &XBufferCore::registry_state();
                auto record = std::make_shared<XBufferCore::OwnerRecord>(source.buffer_.data(), nullptr,
                    source.buffer_.canonical_byte_origin(),source.buffer_.maximum_capacity());
                XBufferCore::RegistryOwners prepared;
                prepared.emplace(record->payload, record);
                auto node = prepared.extract(prepared.begin());
                return {registry, std::move(record), std::move(node)};
            } catch (const std::bad_alloc&) { throw XBadAlloc(); }
        }
        static void publish_node(XBufferCore& source, PendingOwner& pending) {
            bool inserted = false;
            {
                std::lock_guard lock(pending.registry->mutex);
                if (pending.registry->owners.find(pending.record->payload) == pending.registry->owners.end()) {
                    pending.registry->owners.insert(std::move(pending.node));
                    source.owner_record_ = pending.record;
                    XBufferCore::mark_canonical_registration(*pending.record);
                    inserted = true;
                }
            }
            if (!inserted) throw XException("duplicate arena owner registration");
            // The record remains unpublished (owner==nullptr) until every
            // owning head and typed/schema state is fully alive and initialized.
            owner_lookup_count(&OwnerLookupStats::registrations);
            owner_lookup_count(&OwnerLookupStats::record_object_bytes, sizeof(XBufferCore::OwnerRecord));
        }
    };
    inline void VerifiedPathsAccess::adopt_storage(XBufferCore& target,
            VirtualMemoryStorage& source, XGrowPolicy growth) noexcept {
        // Trusted internal target is a completely constructed default XBuffer.
        // No ordinary XBuffer(size), arena initializer, copy or registry work.
        target.buffer_.swap(source);
        target.growth_policy_ = growth;
        target.typed_root_access_allowed_ = false;
        target.schema_bound_ = false;
        target.root_type_identity_ = nullptr;
        target.bound_root_type_id_ = target.bound_schema_hash_ = 0;
    }
    inline void VerifiedPathsAccess::check_received_payload(XBufferCore& source,
                                                           const XWireHeaderV1& header) {
        const auto arena = read_raw_arena_header(source.data(),source.segment_size());
        if (arena.used_end != header.used_bytes)
            throw XException("load_verified<T>(): non-normalized payload extent");
    }
    inline void VerifiedPathsAccess::rollback_storage(XBufferCore& source, VirtualMemoryStorage& storage) noexcept {
        // Detach the registry before returning the reservation to its token.
        source.unregister_registry();
        source.freelist_runtime_cache_.reset();
        source.buffer_.swap(storage);
    }
    template<class T>
    void VerifiedPathsAccess::publish_received(XBufferCore& source) noexcept {
        source.template bind_root_type<T>();
        source.typed_root_access_allowed_ = true;
        source.schema_bound_ = true;
        source.bound_root_type_id_ = wire_root_type_id<T>();
        source.bound_schema_hash_ = wire_schema_hash<T>();
        source.update_registry_owner();
    }

    template<class T>
    void check_received_wire_header(const XWireHeaderV1& h, std::size_t payload_bytes) {
        if (!wire_magic_matches_v1(h.magic)) throw XException("load_verified<T>(): bad magic");
        if (h.format_major != XWIRE_FORMAT_MAJOR_V1 || h.format_minor != XWIRE_FORMAT_MINOR_V1)
            throw XException("load_verified<T>(): unsupported format version");
        if (h.header_size != sizeof(XWireHeaderV1)) throw XException("load_verified<T>(): header size mismatch");
        if (h.endian_tag != XWIRE_ENDIAN_TAG_V1) throw XException("load_verified<T>(): endian tag mismatch");
        if (h.flags != XWIRE_FLAGS_V1) throw XException("load_verified<T>(): unsupported flags");
        if (h.root_type_id != wire_root_type_id<T>()) throw XException("load_verified<T>(): root type id mismatch");
        if (h.schema_hash_lo != wire_schema_hash<T>() || h.schema_hash_hi != 0)
            throw XException("load_verified<T>(): schema hash mismatch");
        if (h.used_bytes != payload_bytes) throw XException("load_verified<T>(): used size mismatch");
        if (h.reserved_bytes < h.used_bytes || h.reserved_bytes > XBufferCore::MAX_PAYLOAD ||
            h.used_bytes > XBufferCore::MAX_PAYLOAD)
            throw XException("load_verified<T>(): invalid payload or reservation size");
        if (h.crc32c != 0 || h.reserved0 != 0) throw XException("load_verified<T>(): unsupported reserved header fields");
        if (h.root_offset >= h.used_bytes) throw XException("load_verified<T>(): root offset out of bounds");
        if (h.allocator_offset >= h.used_bytes) throw XException("load_verified<T>(): allocator offset out of bounds");
    }
}

template<class T> class VerifiedReceiveToken;
template<class T> requires(detail::receive_raw::future_supported_v<T>)
VerifiedReceiveToken<T> begin_verified_receive(std::size_t,
    std::optional<XBufferCore::MaximumCapacity> = std::nullopt);
template<class T> requires(detail::receive_raw::future_supported_v<T>)
TypedXBuffer<T> finish_verified(VerifiedReceiveToken<T>&,
    std::pmr::memory_resource* = std::pmr::new_delete_resource());

template<class T>
class VerifiedReceiveToken {
    struct State {
        enum class Phase : unsigned char { Idle, Operation, Finishing, Consumed };
        std::array<std::byte, sizeof(XWireHeaderV1)> header{};
        detail::VirtualMemoryStorage storage;
        XGrowPolicy growth;
        std::byte* const base;
        const std::size_t maximum;
        std::size_t expected, received_header = 0, received_payload = 0;
        std::atomic<Phase> phase{Phase::Idle};
        std::atomic<bool> cancelled{false}, complete{false};
        State(std::size_t n, XBufferCore::MaximumCapacity cap)
            : storage(detail::VirtualMemoryStorage::for_receive(n, cap.value, cap.pool)),
              growth(cap.growth), base(storage.canonical_byte_origin()), maximum(cap.value), expected(n) {}
    };
    std::shared_ptr<State> owner_;
    explicit VerifiedReceiveToken(std::shared_ptr<State> owner) noexcept : owner_(std::move(owner)) {}
    friend VerifiedReceiveToken<T> begin_verified_receive<T>(std::size_t, std::optional<XBufferCore::MaximumCapacity>);
    friend TypedXBuffer<T> finish_verified<T>(VerifiedReceiveToken<T>&, std::pmr::memory_resource*);
public:
    VerifiedReceiveToken() noexcept = default;
    VerifiedReceiveToken(VerifiedReceiveToken&&) noexcept = default;
    VerifiedReceiveToken& operator=(VerifiedReceiveToken&&) noexcept = default;
    VerifiedReceiveToken(const VerifiedReceiveToken&) = delete;
    VerifiedReceiveToken& operator=(const VerifiedReceiveToken&) = delete;
    explicit operator bool() const noexcept {
        return owner_ && owner_->phase.load(std::memory_order_acquire) != State::Phase::Consumed;
    }
    const void* payload_address() const noexcept { return *this ? owner_->base : nullptr; }
    std::size_t payload_size() const noexcept { return *this ? owner_->expected : 0; }
    std::size_t maximum_capacity() const noexcept { return *this ? owner_->maximum : 0; }
    std::size_t accessible_bytes() const noexcept { return *this ? owner_->storage.accessible_bytes() : 0; }
    std::size_t received_length() const noexcept { return *this ? owner_->received_payload : 0; }
    void cancel() noexcept { if (owner_) owner_->cancelled.store(true, std::memory_order_release); }

    class Operation {
        std::shared_ptr<State> owner_;
        explicit Operation(std::shared_ptr<State> owner) noexcept : owner_(std::move(owner)) {}
        friend class VerifiedReceiveToken;
    public:
        Operation(Operation&&) noexcept = default;
        Operation& operator=(Operation&&) = delete;
        Operation(const Operation&) = delete;
        Operation& operator=(const Operation&) = delete;
        ~Operation() {
            if (owner_) {
                owner_->cancelled.store(true, std::memory_order_release);
                owner_->complete.store(false, std::memory_order_release);
                // Abandoning an operation is not a completion. Keep the token
                // fail-closed in Phase::Operation; only complete() releases it.
            }
        }
        std::span<std::byte> header_bytes() const noexcept {
            return owner_ ? std::span<std::byte>(owner_->header) : std::span<std::byte>{};
        }
        std::span<std::byte> payload_bytes() const noexcept {
            return owner_ ? std::span<std::byte>(reinterpret_cast<std::byte*>(owner_->base), owner_->expected) : std::span<std::byte>{};
        }
        void cancel() noexcept { if (owner_) owner_->cancelled.store(true, std::memory_order_release); }
        void complete(std::size_t header_bytes, std::size_t payload_bytes) noexcept {
            if (!owner_) return;
            owner_->received_header = header_bytes;
            owner_->received_payload = payload_bytes;
            owner_->complete.store(true, std::memory_order_release);
            owner_->phase.store(State::Phase::Idle, std::memory_order_release);
            owner_.reset();
        }
        // Retain this operation in the transport's actual completion owner.
        // cancel() does not end writes. Old span copies cease being usable at
        // completion; abandoning a still-running operation violates the I/O contract.
    };
    [[nodiscard]] Operation acquire_operation() {
        auto state = owner_;
        if (!state) throw XException("verified receive: empty token");
        auto expected = State::Phase::Idle;
        if (!state->phase.compare_exchange_strong(expected, State::Phase::Operation,
                std::memory_order_acq_rel, std::memory_order_acquire))
            throw XException("verified receive: token busy or consumed");
        state->cancelled.store(false, std::memory_order_release);
        state->complete.store(false, std::memory_order_release);
        return Operation(std::move(state));
    }
    bool grow(std::size_t extra) {
        auto state = owner_;
        if (!state) throw XException("verified receive: empty token");
        auto expected = State::Phase::Idle;
        if (!state->phase.compare_exchange_strong(expected, State::Phase::Finishing,
                std::memory_order_acq_rel, std::memory_order_acquire))
            throw XException("verified receive: token busy or consumed");
        struct Reset { State& s; ~Reset(){s.phase.store(State::Phase::Idle,std::memory_order_release);} } reset{*state};
        if (extra > state->maximum - state->expected) return false;
        if (!extra) return true;
        if (!state->storage.grow_to(state->expected + extra)) return false;
        state->expected += extra;
        state->complete.store(false,std::memory_order_release);
        return true;
    }
};

template<class T> requires(detail::receive_raw::future_supported_v<T>)
VerifiedReceiveToken<T> begin_verified_receive(std::size_t bytes,
        std::optional<XBufferCore::MaximumCapacity> capacity) {
    static_assert(is_v1_wire_admitted_v<T>);
    static_assert(detail::receive_raw::future_supported_v<T>, "T requires audited initial and recursive default future construction");
    const auto cap = capacity.value_or(XBufferCore::maximum_capacity(bytes));
    if (bytes > XBufferCore::MAX_PAYLOAD || cap.value > XBufferCore::MAX_PAYLOAD || bytes > cap.value)
        throw XBadAlloc();
    return VerifiedReceiveToken<T>(std::make_shared<typename VerifiedReceiveToken<T>::State>(bytes,cap));
}

template<class T> requires(detail::receive_raw::future_supported_v<T>)
TypedXBuffer<T> finish_verified(VerifiedReceiveToken<T>& token, std::pmr::memory_resource* scratch) {
    static_assert(detail::receive_raw::future_supported_v<T>, "T requires audited initial and recursive default future construction");
    auto state = token.owner_;
    // This is the LAST access to the caller's Token object. Host callbacks below
    // may move, replace or destroy that object; the State pin remains sufficient.
    if (!state || !scratch) throw XException("verified receive: empty token or null scratch");
    using Phase = typename VerifiedReceiveToken<T>::State::Phase;
    auto expected = Phase::Idle;
    if (!state->phase.compare_exchange_strong(expected,Phase::Finishing,
            std::memory_order_acq_rel,std::memory_order_acquire))
        throw XException("verified receive: token busy or consumed");
    struct Reset { typename VerifiedReceiveToken<T>::State& s;
        bool armed=true;
        ~Reset(){if(armed)s.phase.store(Phase::Idle,std::memory_order_release);} } reset{*state};
    if (!state->complete.load(std::memory_order_acquire) || state->cancelled.load(std::memory_order_acquire) ||
        state->received_header != sizeof(XWireHeaderV1) || state->received_payload != state->expected)
        throw XException("verified receive: incomplete or cancelled input");
    XWireHeaderV1 header{};
    std::memcpy(&header,state->header.data(),sizeof(header));
    detail::check_received_wire_header<T>(header,state->expected);
    XBuffer prepared; // default only; no arena/allocator/storage constructor
    detail::VerifiedPathsAccess::adopt_storage(prepared,state->storage,state->growth);
    struct Rollback {
        XBuffer& buffer; detail::VirtualMemoryStorage& storage; bool active=true;
        ~Rollback(){if(active) detail::VerifiedPathsAccess::rollback_storage(buffer,storage);}
    } rollback{prepared,state->storage}; // established before first possible throw
    const auto raw_arena=detail::read_raw_arena_header(reinterpret_cast<char*>(state->base),state->expected);
    if(raw_arena.used_end!=header.used_bytes)
        throw XException("load_verified<T>(): non-normalized payload extent");
    auto pending = detail::VerifiedReceiveAccess::prepare_owner(prepared);
    // Independent component does no VM/registry/typed publication. All its
    // throwing preparation finishes before the exact no-throw activation step.
    auto proof=detail::receive_raw::validate<T>(state->base,state->maximum,state->expected,
        header.root_offset,header.allocator_offset,scratch);
    // Cancellation observed before the no-callback publication point aborts.
    // There is deliberately no post-callback read/write of the Token object.
    if (state->cancelled.load(std::memory_order_acquire))
        throw XException("verified receive: cancelled during preparation");
    detail::VerifiedReceiveAccess::publish_node(prepared,pending);
    (void)std::move(proof).activate();
    detail::VerifiedPathsAccess::publish_received<T>(prepared);
    TypedXBuffer<T> result(std::move(prepared));
    rollback.active=false;
    reset.armed=false;
    state->phase.store(Phase::Consumed,std::memory_order_release);
    return result;
}
} // namespace XOffsetDatastructure
