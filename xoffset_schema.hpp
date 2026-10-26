#ifndef X_OFFSET_SCHEMA_HPP
#define X_OFFSET_SCHEMA_HPP

// Schema field interface: the only way the library asks about the shape of a
// record. Every walk (validation, relocation, construction, wire signature,
// direct receive) goes through these names, and a backend answers them:
//
//   xoffset_schema_reflect.hpp   C++26 reflection (P2996), no user code needed
//
// Interface (namespace XOffsetDatastructure::schema):
//   base_count_v<T>, base_t<T, I>          direct bases, declaration order
//   base_is_virtual_v<T, I>                base I is a virtual base
//   member_count_v<T>                      non-static data members, declaration order
//   member_t<T, I>                         ::type, ::offset (bytes), ::is_bit_field,
//                                          ::get(obj) -> obj's member I (any access)
//   member_type_t<T, I>                    member_t<T, I>::type
//   is_described_v<T>                      the backend can describe T (class or union)
//   enum_range<E>()                        {min, max}: the smallest range holding 0 and
//                                          every enumerator of E
//   implicit_lifetime_aggregate<T>()       aggregate without a user-provided destructor
//   only_implicit_special_members<T>()     no user-declared special member function
//
// All of it is compile-time: the machine code of a walk is the same whichever
// backend describes the record.

#include <cstddef>

#if defined(_MSC_VER) && !defined(__clang__)
#define XOFFSET_SCHEMA_INLINE __forceinline
#else
#define XOFFSET_SCHEMA_INLINE inline __attribute__((always_inline))
#endif

namespace XOffsetDatastructure::schema {
    template <class U> struct enum_bounds { U min, max; };
} // namespace XOffsetDatastructure::schema

#include "xoffset_schema_reflect.hpp"

#endif // X_OFFSET_SCHEMA_HPP
