#ifndef X_OFFSET_SCHEMA_HPP
#define X_OFFSET_SCHEMA_HPP

// Schema field interface: the only way the library asks about the shape of a
// record. Every walk (validation, relocation, construction, wire signature)
// goes through it, and a backend answers it: xoffset_schema_reflect.hpp (C++26
// reflection, no user code) or xoffset_schema_describe.hpp (XOFFSET_RECORD
// field lists, C++20). It is all compile-time: a walk's machine code is the
// same whichever backend describes the record.
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
//   is_undescribable_v<T>                  a union or non-aggregate class without a field
//                                          list (never under reflection); walks treat it
//                                          as opaque: not admitted, no walk
//   implicit_lifetime_aggregate<T>()       aggregate without a user-provided destructor
//   only_implicit_special_members<T>()     no user-declared special member function

#include <cstddef>

#include "xoffset_config.hpp"

// Reflection answers when the compiler has it, unless
// XOFFSET_SCHEMA_BACKEND_DESCRIBE=1 (CMake XOFFSET_SCHEMA_BACKEND=describe)
// forces the field lists. XOFFSET_RECORD works under both; with reflection
// it is checked against reflection.
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
