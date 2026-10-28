// The host's operator new and delete (see xoffset_test_host.h). In an ordinary
// executable a program's replacements also serve the C++ runtime's own allocations
// (std::string growth in libc++, std::pmr's new_delete_resource); a library loaded
// later cannot take them over, so the host defines all twenty forms and forwards
// each to the hosted program's replacement once it is registered. Until then, and
// for the forms the program keeps, they behave like the C++ runtime's defaults:
// malloc and free, the new handler, and the nothrow, array and sized forms calling
// the basic ones.
#include "host_core.hpp"
#include "xoffset_host_new.hpp"
#include "xoffset_test_host.h"

#include <atomic>
#include <cstdlib>

namespace {

std::atomic<const void*> g_program[XOFFSET_HOST_NEW_COUNT];

template <typename F>
F program(int index) noexcept {
    return reinterpret_cast<F>(const_cast<void*>(g_program[index].load(std::memory_order_acquire)));
}

void* default_new(std::size_t n) {
    for (;;) {
        if (void* p = std::malloc(n ? n : 1)) return p;
        std::new_handler handler = std::get_new_handler();
        if (!handler) throw std::bad_alloc();
        handler();
    }
}

void* default_new_aligned(std::size_t n, std::align_val_t a) {
    std::size_t align = static_cast<std::size_t>(a);
    if (align < sizeof(void*)) align = sizeof(void*);
    for (;;) {
        void* p = nullptr;
        if (::posix_memalign(&p, align, n ? n : 1) == 0) return p;
        std::new_handler handler = std::get_new_handler();
        if (!handler) throw std::bad_alloc();
        handler();
    }
}

}  // namespace

#define XO_FORWARD(index, ...)                                                       \
    if (auto f = program<decltype(&forward_type_##index)>(index)) return f(__VA_ARGS__)

// One declaration per form gives the pointer types (forward_type_<index>).
#define XO_TYPE(index, ret, name, params) ret forward_type_##index params;
namespace {
XOFFSET_HOST_NEW_FUNCTIONS(XO_TYPE)
}

void* operator new(std::size_t n) {
    XO_FORWARD(0, n);
    return default_new(n);
}
void* operator new[](std::size_t n) {
    XO_FORWARD(1, n);
    return ::operator new(n);
}
void* operator new(std::size_t n, const std::nothrow_t& t) noexcept {
    XO_FORWARD(2, n, t);
    try {
        return ::operator new(n);
    } catch (...) {
        return nullptr;
    }
}
void* operator new[](std::size_t n, const std::nothrow_t& t) noexcept {
    XO_FORWARD(3, n, t);
    try {
        return ::operator new[](n);
    } catch (...) {
        return nullptr;
    }
}
void* operator new(std::size_t n, std::align_val_t a) {
    XO_FORWARD(4, n, a);
    return default_new_aligned(n, a);
}
void* operator new[](std::size_t n, std::align_val_t a) {
    XO_FORWARD(5, n, a);
    return ::operator new(n, a);
}
void* operator new(std::size_t n, std::align_val_t a, const std::nothrow_t& t) noexcept {
    XO_FORWARD(6, n, a, t);
    try {
        return ::operator new(n, a);
    } catch (...) {
        return nullptr;
    }
}
void* operator new[](std::size_t n, std::align_val_t a, const std::nothrow_t& t) noexcept {
    XO_FORWARD(7, n, a, t);
    try {
        return ::operator new[](n, a);
    } catch (...) {
        return nullptr;
    }
}
void operator delete(void* p) noexcept {
    XO_FORWARD(8, p);
    std::free(p);
}
void operator delete[](void* p) noexcept {
    XO_FORWARD(9, p);
    ::operator delete(p);
}
void operator delete(void* p, const std::nothrow_t& t) noexcept {
    XO_FORWARD(10, p, t);
    ::operator delete(p);
}
void operator delete[](void* p, const std::nothrow_t& t) noexcept {
    XO_FORWARD(11, p, t);
    ::operator delete[](p);
}
void operator delete(void* p, std::size_t n) noexcept {
    XO_FORWARD(12, p, n);
    ::operator delete(p);
}
void operator delete[](void* p, std::size_t n) noexcept {
    XO_FORWARD(13, p, n);
    ::operator delete[](p);
}
void operator delete(void* p, std::align_val_t a) noexcept {
    XO_FORWARD(14, p, a);
    std::free(p);
}
void operator delete[](void* p, std::align_val_t a) noexcept {
    XO_FORWARD(15, p, a);
    ::operator delete(p, a);
}
void operator delete(void* p, std::align_val_t a, const std::nothrow_t& t) noexcept {
    XO_FORWARD(16, p, a, t);
    ::operator delete(p, a);
}
void operator delete[](void* p, std::align_val_t a, const std::nothrow_t& t) noexcept {
    XO_FORWARD(17, p, a, t);
    ::operator delete[](p, a);
}
void operator delete(void* p, std::size_t n, std::align_val_t a) noexcept {
    XO_FORWARD(18, p, n, a);
    ::operator delete(p, a);
}
void operator delete[](void* p, std::size_t n, std::align_val_t a) noexcept {
    XO_FORWARD(19, p, n, a);
    ::operator delete[](p, a);
}

namespace xoffset_test_host {

const void* const host_new_functions[XOFFSET_HOST_NEW_COUNT] = {XOFFSET_HOST_NEW_FUNCTIONS(XOFFSET_HOST_NEW_ADDRESS)};

void set_new_functions(const void* const* program_functions) {
    for (int i = 0; i < XOFFSET_HOST_NEW_COUNT; ++i) {
        const void* f = program_functions[i];
        if (f && f != host_new_functions[i]) g_program[i].store(f, std::memory_order_release);
    }
}

}  // namespace xoffset_test_host
