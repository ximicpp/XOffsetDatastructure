## 1. Core: scoped_allocator_adaptor
- [x] 1.1 Add `#include <boost/container/scoped_allocator.hpp>` to header
- [x] 1.2 Define `x_scoped_alloc<T>` alias wrapping `allocator<T, SM>`
- [x] 1.3 Change `x_vector_impl<T>` to use `x_scoped_alloc<T>` as allocator
- [x] 1.4 Verify `sizeof(XVector<int>)` unchanged (static_assert)

## 2. Wrapper: XVector (5 overloads)
- [x] 2.1 Convert XVector from type alias to wrapper class inheriting `x_vector_impl<T>`
- [x] 2.2 Inherit all constructors via `using Base::Base`
- [x] 2.3 Add generic `push_back(Args...)` with requires constraint
- [x] 2.4 Add generic `insert(pos, Args...)` with requires constraint
- [x] 2.5 Add generic `insert(pos, n, Args...)` — construct T then delegate
- [x] 2.6 Add generic `resize(n, Args...)` — construct T then delegate
- [x] 2.7 Add generic `assign(n, Args...)` — construct T then delegate

## 3. Wrapper: XMap (4 overloads)
- [x] 3.1 Extract `x_map_impl<K,V>` detail alias for flat_map base
- [x] 3.2 Convert XMap from type alias to wrapper class inheriting `x_map_impl<K,V>`
- [x] 3.3 Add generic `operator[](KeyArg)` — find + piecewise_construct emplace
- [x] 3.4 Add generic `erase(KeyArg)` — find + erase(iterator)
- [x] 3.5 Add generic `try_emplace(KeyArg, Args...)` — piecewise_construct emplace
- [x] 3.6 Add generic `insert_or_assign(KeyArg, M)` — piecewise_construct emplace

## 4. Wrapper: XSet (3 overloads)
- [x] 4.1 Extract `x_set_impl<T>` detail alias for flat_set base
- [x] 4.2 Convert XSet from type alias to wrapper class inheriting `x_set_impl<T>`
- [x] 4.3 Add generic `insert(Args...)` with requires constraint
- [x] 4.4 Add generic `insert(pos, Args...)` with requires constraint
- [x] 4.5 Add generic `erase(KeyArg)` — find + erase(iterator)

## 5. Custom type support
- [x] 5.1 Define `XAllocator` convenience typedef for user types
- [x] 5.2 Add `using allocator_type = XAllocator;` to InnerObject in test_nested.cpp
- [x] 5.3 Document `allocator_type` convention in README (Rule 5: Automatic Allocator Propagation)

## 6. Migration: update all test files
- [x] 6.1 Remove explicit `get_segment_manager()` from all container emplace/push_back calls
- [x] 6.2 Remove explicit `XString(...)` construction in map emplace/find/erase calls
- [x] 6.3 Simplify examples

## 7. Verification
- [x] 7.1 Full build + test suite pass (Docker): 25/25 passed
- [x] 7.2 Verify sizeof unchanged via static_assert (XVector, XMap, XSet)
- [x] 7.3 Verify is_safe_leaf / migrate_as / TypeLayout still match