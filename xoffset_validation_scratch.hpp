#pragma once
#include "xoffsetdatastructure.hpp"
#include "validation_scratch.hpp"

namespace XOffsetDatastructure {
using ValidationScratch = ::xoffset_scratch::ValidationScratch;

// New explicit production entry. Link the single optional shared scratch
// runtime. The old load API never acquires this context implicitly.
template<class T>
TypedXBuffer<T> load_verified_with_scratch(
        std::span<const std::byte> data, ValidationScratch& scratch,
        std::optional<XBufferCore::MaximumCapacity> capacity = std::nullopt) {
    auto lease = scratch.acquire();
    return TypedXBuffer<T>(XBuffer::load_verified_with_resource<T>(
        data, capacity, lease.upstream_resource()));
}
template<class T>
TypedXBuffer<T> load_verified_with_scratch(
        const std::string& data, ValidationScratch& scratch,
        std::optional<XBufferCore::MaximumCapacity> capacity = std::nullopt) {
    return load_verified_with_scratch<T>(std::span<const std::byte>(
        reinterpret_cast<const std::byte*>(data.data()), data.size()), scratch, capacity);
}
} // namespace XOffsetDatastructure
