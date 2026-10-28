// Device mesh demo: what the types share with hop.cpp and the self-test
// beyond types.hpp. OWNER: types thread.
#pragma once
#include <cstdint>
#include "xmesh/types.hpp"

namespace xmesh::internal {

// The arena bytes in use (payload used_end): what a mutation grows.
std::uint64_t used_bytes(const Object& object) noexcept;

// The largest arena growth one mutation of an object of this type and
// scale causes (checked by the self-test over long routes). mutate()
// refuses to start with less room than this left under the object's
// maximum_capacity, so a build without exceptions never reaches the
// fatal arena-exhausted path.
std::uint32_t mutation_headroom(TypeId type, Scale scale) noexcept;

// The scale whose receive_cap the object was built or received with
// (Large when none matches).
Scale scale_of(const Object& object) noexcept;

} // namespace xmesh::internal
