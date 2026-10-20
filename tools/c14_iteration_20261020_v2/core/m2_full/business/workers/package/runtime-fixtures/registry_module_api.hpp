#pragma once
#include <cstddef>

struct ScratchRegistryProbeApi {
    unsigned version;
    const void* (*registry_address)() noexcept;
    bool (*cache_blocks)(std::size_t count, std::size_t payload, std::size_t alignment) noexcept;
    std::size_t (*cached_bytes)() noexcept;
    void (*clear_context)() noexcept;
};
using ScratchRegistryProbeEntry = const ScratchRegistryProbeApi* (*)() noexcept;
