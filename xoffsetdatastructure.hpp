#ifndef X_OFFSET_DATA_STRUCTURE_HPP
#define X_OFFSET_DATA_STRUCTURE_HPP

// XOffset: records that live in one relocatable arena and go on the wire
// as they are. This header includes the whole library in dependency order;
// include it rather than one of its parts. Start with docs/GETTING_STARTED.md;
// the API is in docs/USER_GUIDE.md.

#include "xoffset_arena.hpp"
#include "xoffset_containers.hpp"
#include "xoffset_wire.hpp"
#include "xoffset_schema_hash.hpp"
#include "xoffset_buffer.hpp"
#include "xoffset_compactor.hpp"
#include "xoffset_registration.hpp"

#include "xoffset_verified_paths.hpp"
#include "xoffset_core_lifetime.hpp"
#include "xoffset_delta.hpp"

#endif
