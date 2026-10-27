#pragma once
// Older tests give their records an allocator constructor. Constructing a
// record through its own constructor is a C++26-only path: without reflection
// a record is described by an XOFFSET_RECORD field list, which needs an
// aggregate. C++20/C++23 builds (and the C++26 field-list backend) drop the
// constructors, so the same tests run on the aggregate, field-list path.
#include "../xoffsetdatastructure.hpp"

#ifndef XOFFSET_TEST_ALLOCATOR_CTORS
#define XOFFSET_TEST_ALLOCATOR_CTORS XOFFSET_SCHEMA_REFLECTION
#endif

// v.emplace_back(alloc) where the element has an allocator constructor, and
// v.emplace_back() where it is an aggregate.
template <class Vector, class Alloc>
decltype(auto) emplace_with_allocator(Vector& v, const Alloc& alloc) {
#if XOFFSET_TEST_ALLOCATOR_CTORS
    return v.emplace_back(alloc);
#else
    (void)alloc;
    return v.emplace_back();
#endif
}
