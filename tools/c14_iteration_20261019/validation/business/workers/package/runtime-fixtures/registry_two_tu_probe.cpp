#include "registry_two_tu_probe.hpp"
#include "validation_scratch.hpp"

const void* scratch_registry_address_from_second_tu() {
    return &xoffset_scratch::detail::pool();
}
std::size_t scratch_cached_bytes_from_second_tu() {
    return xoffset_scratch::ValidationScratch::statistics().cached_bytes;
}
