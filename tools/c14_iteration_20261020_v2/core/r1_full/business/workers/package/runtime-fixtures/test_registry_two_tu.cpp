#include "registry_two_tu_probe.hpp"
#include "validation_scratch.hpp"
#include <stdexcept>
#include <type_traits>

using xoffset_scratch::Statistics;
using xoffset_scratch::ValidationScratch;

static_assert(std::is_copy_constructible<Statistics>::value &&
              std::is_copy_assignable<Statistics>::value,
              "Statistics snapshot must contain copyable values, not atomics");
static_assert(!std::is_copy_constructible<ValidationScratch::Lease>::value &&
              !std::is_move_constructible<ValidationScratch::Lease>::value,
              "Lease must remain scope-bound; factory uses C++17 guaranteed elision");

namespace {
void need(bool condition) { if (!condition) throw std::runtime_error("two-TU registry contract"); }
}
int main() {
    need(scratch_registry_address_from_second_tu() == &xoffset_scratch::detail::pool());
    ValidationScratch context;
    {
        // This initialization must compile despite deleted copy/move members.
        auto lease = context.acquire();
        need(lease.is_primary());
        auto* pointer = lease.upstream_resource()->allocate(64, 16);
        lease.upstream_resource()->deallocate(pointer, 64, 16);
    }
    const auto snapshot = ValidationScratch::statistics();
    auto copy = snapshot;
    copy = snapshot;
    need(snapshot.cached_bytes != 0 && copy.cached_bytes == snapshot.cached_bytes);
    need(scratch_cached_bytes_from_second_tu() == snapshot.cached_bytes);
    context.trim();
    need(scratch_cached_bytes_from_second_tu() == 0);
}
