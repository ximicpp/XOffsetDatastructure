// The twenty replaceable allocation functions in the order of the tables of
// xoffset_test_host.h, as an X macro: X(index, return type, name, parameters).
// Used by the host (host_new.cpp) and by every hosted program (hosted_new_shim.cpp).
#pragma once

#include <cstddef>
#include <new>

#define XOFFSET_HOST_NEW_FUNCTIONS(X)                                                     \
    X(0, void*, operator new, (std::size_t))                                              \
    X(1, void*, operator new[], (std::size_t))                                            \
    X(2, void*, operator new, (std::size_t, const std::nothrow_t&))                       \
    X(3, void*, operator new[], (std::size_t, const std::nothrow_t&))                     \
    X(4, void*, operator new, (std::size_t, std::align_val_t))                            \
    X(5, void*, operator new[], (std::size_t, std::align_val_t))                          \
    X(6, void*, operator new, (std::size_t, std::align_val_t, const std::nothrow_t&))     \
    X(7, void*, operator new[], (std::size_t, std::align_val_t, const std::nothrow_t&))   \
    X(8, void, operator delete, (void*))                                                  \
    X(9, void, operator delete[], (void*))                                                \
    X(10, void, operator delete, (void*, const std::nothrow_t&))                          \
    X(11, void, operator delete[], (void*, const std::nothrow_t&))                        \
    X(12, void, operator delete, (void*, std::size_t))                                    \
    X(13, void, operator delete[], (void*, std::size_t))                                  \
    X(14, void, operator delete, (void*, std::align_val_t))                               \
    X(15, void, operator delete[], (void*, std::align_val_t))                             \
    X(16, void, operator delete, (void*, std::align_val_t, const std::nothrow_t&))        \
    X(17, void, operator delete[], (void*, std::align_val_t, const std::nothrow_t&))      \
    X(18, void, operator delete, (void*, std::size_t, std::align_val_t))                  \
    X(19, void, operator delete[], (void*, std::size_t, std::align_val_t))

// The address of one form, as this image binds it: its own definition when it
// has one, else the process's (the host's).
#define XOFFSET_HOST_NEW_ADDRESS(index, ret, name, params) \
    reinterpret_cast<const void*>(static_cast<ret(*) params>(&::name)),
