#ifndef X_OFFSET_SCHEMA_HPP
#define X_OFFSET_SCHEMA_HPP

// Schema field interface: the only way the library asks about the shape of a
// record. Every walk (validation, relocation, construction, wire signature,
// direct receive) goes through these names, and a backend answers them:
//
//   xoffset_schema_reflect.hpp   C++26 reflection (P2996), no user code needed
//   xoffset_schema_describe.hpp  field lists written with XOFFSET_RECORD (C++20)
//
// Reflection answers whenever the compiler has it,
// unless XOFFSET_SCHEMA_BACKEND_DESCRIBE=1 forces the field lists (the CMake
// option XOFFSET_SCHEMA_BACKEND=describe sets it). XOFFSET_RECORD is available
// under both; with reflection it is checked against reflection.
//
// Interface (namespace XOffsetDatastructure::schema):
//   base_count_v<T>, base_t<T, I>          direct bases, declaration order
//   base_is_virtual_v<T, I>                base I is a virtual base
//   member_count_v<T>                      non-static data members, declaration order
//   member_t<T, I>                         ::type, ::offset (bytes), ::is_bit_field,
//                                          ::name (identifier, string_view),
//                                          ::get(obj) -> obj's member I (any access)
//   member_type_t<T, I>                    member_t<T, I>::type
//   is_described_v<T>                      the backend can describe T (class or union)
//   is_undescribable_v<T>                  T is a class or union no field list can describe
//                                          (a union or a non-aggregate class) and it has
//                                          none; never true under reflection. Walks treat
//                                          such a T as opaque: not admitted, no walk.
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

// GCC 16 defines __cpp_impl_reflection; the P2996 Clang fork answers
// __has_feature(reflection). (GCC before 14 has no __has_feature.)
#if defined(__cpp_impl_reflection)
#define XOFFSET_SCHEMA_COMPILER_REFLECTION 1
#elif defined(__has_feature)
#if __has_feature(reflection)
#define XOFFSET_SCHEMA_COMPILER_REFLECTION 1
#endif
#endif

#if defined(XOFFSET_SCHEMA_COMPILER_REFLECTION) && \
    !(defined(XOFFSET_SCHEMA_BACKEND_DESCRIBE) && XOFFSET_SCHEMA_BACKEND_DESCRIBE)
#define XOFFSET_SCHEMA_REFLECTION 1
#else
#define XOFFSET_SCHEMA_REFLECTION 0
#endif

#include "xoffset_schema_describe.hpp"
#if XOFFSET_SCHEMA_REFLECTION
#include "xoffset_schema_reflect.hpp"
#endif

#endif // X_OFFSET_SCHEMA_HPP
