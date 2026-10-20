#include "registry_module_api.hpp"
#include "validation_scratch.hpp"
#include <memory>
#include <vector>

namespace {
// Each module has its OWN context but imports the SAME exported pool() runtime.
std::unique_ptr<xoffset_scratch::ValidationScratch> context;
const void* registry_address() noexcept { return &xoffset_scratch::detail::pool(); }
std::size_t cached_bytes() noexcept {
    return xoffset_scratch::ValidationScratch::statistics().cached_bytes;
}
void clear_context() noexcept { context.reset(); }
bool cache_blocks(std::size_t count, std::size_t payload, std::size_t alignment) noexcept {
    try {
        if (!context) context = std::make_unique<xoffset_scratch::ValidationScratch>();
        std::vector<void*> pointers;
        pointers.reserve(count); // All vector metadata is prepared before raw allocation.
        auto lease = context->acquire();
        try {
            for (std::size_t i = 0; i < count; ++i)
                pointers.push_back(lease.upstream_resource()->allocate(payload, alignment));
        } catch (...) {
            for (auto* pointer : pointers)
                lease.upstream_resource()->deallocate(pointer, payload, alignment);
            throw;
        }
        for (auto* pointer : pointers)
            lease.upstream_resource()->deallocate(pointer, payload, alignment);
        return true;
    } catch (...) {
        return false;
    }
}
const ScratchRegistryProbeApi api{1, registry_address, cache_blocks, cached_bytes, clear_context};
}

#if defined(_WIN32)
#define SCRATCH_PROBE_EXPORT __declspec(dllexport)
#elif defined(__GNUC__) || defined(__clang__)
#define SCRATCH_PROBE_EXPORT __attribute__((visibility("default")))
#else
#define SCRATCH_PROBE_EXPORT
#endif
extern "C" SCRATCH_PROBE_EXPORT const ScratchRegistryProbeApi* validation_scratch_probe() noexcept {
    return &api;
}
