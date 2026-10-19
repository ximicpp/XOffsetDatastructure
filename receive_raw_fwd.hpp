#pragma once
#include <cstddef>
#include <cstdint>
#include <memory_resource>

namespace XOffsetDatastructure {
class XBufferCore;
template<class T> struct wire_record_layout;
template<class T> struct wire_activation;
template<class T> struct wire_future_lifetime;
template<class... T> struct wire_future_children;
namespace detail {
template<class T> consteval bool raw_implicit_lifetime();
namespace custom_future_contract {
template<class T> consteval bool surface_complete();
}
struct alignas(8) RawPayloadHeader {
    char magic[8];
    std::uint32_t header_size, flags;
    std::uint64_t root_offset, allocator_offset, used_end, reserved0;
};
struct RawWireVerification {
    std::size_t root_offset, allocator_offset, used_bytes;
};
struct CanonicalRawBytes;
struct RawActivationStats {
    std::uint64_t owning_heads{};
    std::uint64_t custom_objects{};
    std::uint64_t ordinary_outers{}; // recovered pre-fill implicit objects
    std::uint64_t capacity_arrays{}; // recovered, never restarted after input
    std::uint64_t allocation_headers{};
    std::uint64_t allocator_states{};
    std::uint64_t free_block_headers{};
    std::uint64_t arena_headers{};
};
template<class T> consteval bool receive_raw_supported();
template<class T> consteval bool receive_raw_future_supported();
RawPayloadHeader read_raw_arena_header(const char*, std::size_t);
template<class T>
std::size_t raw_root_offset(const char*, std::size_t, const RawPayloadHeader&);
std::size_t raw_allocator_offset(const char*, std::size_t, const RawPayloadHeader&);
template<class T>
RawWireVerification verify_structural_wire_graph_raw(
    XBufferCore&, std::pmr::memory_resource* = std::pmr::new_delete_resource());
template<class T>
RawActivationStats activate_verified_wire(char*, std::size_t) noexcept;

namespace receive_raw {
template<class T>
inline constexpr bool supported_v = receive_raw_supported<T>();
// Initial raw activation and safe DEFAULT future construction are different
// domains. A custom initial activation policy does not certify later helpers.
template<class T> consteval bool future_supported() {
    return receive_raw_future_supported<T>();
}
template<class T>
inline constexpr bool future_supported_v = future_supported<T>();

// Caller has established the complete array object at this same address, kept
// its representation, and completed every potentially throwing preparation.
// Only INLINE heads are restarted; still-live out-of-line children are untouched.
template<class T> requires(future_supported_v<T>)
void reactivate_inline_prefix(T* data, std::size_t live_count) noexcept;

template<class T> class Proof;
template<class T> requires(supported_v<T>)
Proof<T> validate(char* base, std::size_t used, std::uint64_t wire_root_offset,
                  std::uint64_t wire_allocator_offset,
                  std::pmr::memory_resource* = std::pmr::new_delete_resource());
template<class T> requires(supported_v<T>)
Proof<T> validate(std::byte* origin, std::size_t maximum, std::size_t used,
                  std::uint64_t wire_root_offset,std::uint64_t wire_allocator_offset,
                  std::pmr::memory_resource* = std::pmr::new_delete_resource());

// A proof is neither a storage owner nor a registry/transport lease. Its caller
// keeps the same bytes, addresses and lifetimes quiescent until activation.
// Destruction of an unused proof has no effect on the received bytes.
template<class T> class Proof {
    char* base_{};
    std::size_t root_{};
    std::byte* origin_{};
    std::size_t maximum_{},used_{};
    Proof(char* base, std::size_t root) noexcept : base_(base), root_(root) {}
    Proof(std::byte* origin,std::size_t maximum,std::size_t used,std::size_t root) noexcept
        :base_(reinterpret_cast<char*>(origin)),root_(root),origin_(origin),maximum_(maximum),used_(used){}
    template<class U> requires(supported_v<U>)
    friend Proof<U> validate(char*, std::size_t, std::uint64_t, std::uint64_t,
                              std::pmr::memory_resource*);
    template<class U> requires(supported_v<U>)
    friend Proof<U> validate(std::byte*,std::size_t,std::size_t,std::uint64_t,std::uint64_t,
                              std::pmr::memory_resource*);
public:
    Proof(const Proof&) = delete;
    Proof& operator=(const Proof&) = delete;
    Proof(Proof&& other) noexcept : base_(other.base_), root_(other.root_),origin_(other.origin_),
        maximum_(other.maximum_),used_(other.used_) {
        other.base_ = nullptr;
        other.origin_ = nullptr;
    }
    Proof& operator=(Proof&& other) noexcept {
        if (this != &other) {
            base_ = other.base_; root_ = other.root_;
            origin_=other.origin_;maximum_=other.maximum_;used_=other.used_;
            other.base_ = nullptr;
            other.origin_ = nullptr;
        }
        return *this;
    }
    ~Proof() = default;
    RawActivationStats activate() && noexcept;
};
} // namespace receive_raw
} // namespace detail
} // namespace XOffsetDatastructure
