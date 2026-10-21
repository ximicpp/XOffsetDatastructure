#pragma once
// One executable-local, real global-new failpoint. Pair all delete overloads
// with the allocation family, including the compatible sanitizer profile.
#include <cstdlib>
#include <limits>
#include <new>
#if defined(_WIN32)
#include <malloc.h>
#endif
namespace index_host {
inline thread_local long fail_after = -1;
inline thread_local unsigned attempts{}, failures{}, handler_calls{};
inline void* allocate(std::size_t size, std::size_t alignment) {
    for (;;) {
        bool fail = false;
        if (fail_after >= 0) {
            ++attempts;
            if (fail_after-- == 0) { fail = true; ++failures; }
        }
        void* p = nullptr;
        if (!fail) {
            if (alignment == 0) p = std::malloc(size ? size : 1); // ordinary new; aligned new pairs with _aligned_free
            else {
                const auto n = size ? size : 1;
                if (n > (std::numeric_limits<std::size_t>::max)() - alignment + 1) throw std::bad_alloc();
#if defined(_WIN32)
                p = _aligned_malloc(n, alignment);
#else
                p = std::aligned_alloc(alignment, (n + alignment - 1) / alignment * alignment);
#endif
            }
        }
        if (p) return p;
        if (auto handler = std::get_new_handler()) { ++handler_calls; handler(); }
        else throw std::bad_alloc();
    }
}
inline void free(void* p, bool aligned) noexcept {
#if defined(_WIN32)
    if (aligned) _aligned_free(p); else std::free(p);
#else
    (void)aligned; std::free(p);
#endif
}
struct Arm {
    explicit Arm(long n) { attempts = failures = handler_calls = 0; fail_after = n; }
    ~Arm() { fail_after = -1; }
};
}
void* operator new(std::size_t n) { return index_host::allocate(n, 0); }
void* operator new[](std::size_t n) { return ::operator new(n); }
void operator delete(void* p) noexcept { index_host::free(p, false); }
void operator delete[](void* p) noexcept { index_host::free(p, false); }
void operator delete(void* p, std::size_t) noexcept { index_host::free(p, false); }
void operator delete[](void* p, std::size_t) noexcept { index_host::free(p, false); }
void* operator new(std::size_t n, std::align_val_t a) { return index_host::allocate(n, std::size_t(a)); }
void* operator new[](std::size_t n, std::align_val_t a) { return ::operator new(n, a); }
void operator delete(void* p, std::align_val_t) noexcept { index_host::free(p, true); }
void operator delete[](void* p, std::align_val_t) noexcept { index_host::free(p, true); }
void operator delete(void* p, std::size_t, std::align_val_t) noexcept { index_host::free(p, true); }
void operator delete[](void* p, std::size_t, std::align_val_t) noexcept { index_host::free(p, true); }
void* operator new(std::size_t n, const std::nothrow_t&) noexcept { try { return ::operator new(n); } catch (...) { return nullptr; } }
void* operator new[](std::size_t n, const std::nothrow_t&) noexcept { try { return ::operator new[](n); } catch (...) { return nullptr; } }
void operator delete(void* p, const std::nothrow_t&) noexcept { index_host::free(p, false); }
void operator delete[](void* p, const std::nothrow_t&) noexcept { index_host::free(p, false); }
void* operator new(std::size_t n, std::align_val_t a, const std::nothrow_t&) noexcept { try { return ::operator new(n, a); } catch (...) { return nullptr; } }
void* operator new[](std::size_t n, std::align_val_t a, const std::nothrow_t&) noexcept { try { return ::operator new[](n, a); } catch (...) { return nullptr; } }
void operator delete(void* p, std::align_val_t, const std::nothrow_t&) noexcept { index_host::free(p, true); }
void operator delete[](void* p, std::align_val_t, const std::nothrow_t&) noexcept { index_host::free(p, true); }
