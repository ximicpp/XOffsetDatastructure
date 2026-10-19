#include "validation_scratch.hpp"

namespace xoffset_scratch::detail {
Pool& pool() {
    // A context initializes this before its own construction completes, so
    // ordinary static contexts die before the registry. Pool's destructor does
    // not call user upstreams. The shared target must not be unloaded early.
    static Pool instance;
    return instance;
}
} // namespace xoffset_scratch::detail
