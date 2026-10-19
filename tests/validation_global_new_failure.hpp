#pragma once
// Real, one-shot global C++ new failures in range registration and typed load.
// Fixture encoding, expected values, and logging are outside armed scopes.
// This does not inject malloc, mmap/OS failures or promise multi-operation rollback.
#include <array>
#include <atomic>
#include <cstdlib>
#include <limits>
#include <new>
#if defined(_WIN32)
#include <malloc.h>
#endif
namespace fail_new {
constexpr std::size_t trace_capacity = 4096;
std::atomic<long long> outstanding_blocks{0};
thread_local bool watching = false;
thread_local long fail_after = -1;
thread_local std::size_t attempts = 0, hits = 0;
thread_local bool overflow = false;
thread_local std::array<std::size_t, trace_capacity> requests{};
void check(std::size_t bytes) {
    if (!watching) return;
    if (attempts < requests.size()) requests[attempts] = bytes; else overflow = true;
    ++attempts;
    if (fail_after == 0) { fail_after = -1; ++hits; throw std::bad_alloc(); }
    if (fail_after > 0) --fail_after;
}
void* ordinary(std::size_t bytes) {
    check(bytes);
    if (auto* p = std::malloc(bytes ? bytes : 1)) { outstanding_blocks.fetch_add(1, std::memory_order_relaxed); return p; }
    throw std::bad_alloc();
}
void* aligned(std::size_t bytes, std::align_val_t requested) {
    check(bytes);
    const auto alignment = static_cast<std::size_t>(requested);
    bytes = bytes ? bytes : 1;
    if (!alignment || bytes > (std::numeric_limits<std::size_t>::max)() - (alignment - 1)) throw std::bad_alloc();
    bytes = (bytes + alignment - 1) / alignment * alignment;
#if defined(_WIN32)
    auto* p = _aligned_malloc(bytes, alignment);
#else
    auto* p = std::aligned_alloc(alignment, bytes);
#endif
    if (p) { outstanding_blocks.fetch_add(1, std::memory_order_relaxed); return p; }
    throw std::bad_alloc();
}
void release(void* p) noexcept {
    if (!p) return;
    outstanding_blocks.fetch_sub(1, std::memory_order_relaxed); std::free(p);
}
void release_aligned(void* p) noexcept {
    if (!p) return;
    outstanding_blocks.fetch_sub(1, std::memory_order_relaxed);
#if defined(_WIN32)
    _aligned_free(p);
#else
    std::free(p);
#endif
}
struct Arm {
    explicit Arm(long point) { attempts = hits = 0; overflow = false; fail_after = point; watching = true; }
    ~Arm() { watching = false; fail_after = -1; }
    Arm(const Arm&) = delete; Arm& operator=(const Arm&) = delete;
};
long long live() { return outstanding_blocks.load(std::memory_order_relaxed); }
}
void* operator new(std::size_t n) { return fail_new::ordinary(n); }
void* operator new[](std::size_t n) { return fail_new::ordinary(n); }
void* operator new(std::size_t n, std::align_val_t a) { return fail_new::aligned(n, a); }
void* operator new[](std::size_t n, std::align_val_t a) { return fail_new::aligned(n, a); }
void* operator new(std::size_t n, const std::nothrow_t&) noexcept { try { return ::operator new(n); } catch (...) { return nullptr; } }
void* operator new[](std::size_t n, const std::nothrow_t&) noexcept { try { return ::operator new[](n); } catch (...) { return nullptr; } }
void* operator new(std::size_t n, std::align_val_t a, const std::nothrow_t&) noexcept { try { return ::operator new(n, a); } catch (...) { return nullptr; } }
void* operator new[](std::size_t n, std::align_val_t a, const std::nothrow_t&) noexcept { try { return ::operator new[](n, a); } catch (...) { return nullptr; } }
void operator delete(void* p) noexcept { fail_new::release(p); }
void operator delete[](void* p) noexcept { fail_new::release(p); }
void operator delete(void* p, std::size_t) noexcept { fail_new::release(p); }
void operator delete[](void* p, std::size_t) noexcept { fail_new::release(p); }
void operator delete(void* p, std::align_val_t) noexcept { fail_new::release_aligned(p); }
void operator delete[](void* p, std::align_val_t) noexcept { fail_new::release_aligned(p); }
void operator delete(void* p, std::size_t, std::align_val_t) noexcept { fail_new::release_aligned(p); }
void operator delete[](void* p, std::size_t, std::align_val_t) noexcept { fail_new::release_aligned(p); }
void operator delete(void* p, const std::nothrow_t&) noexcept { fail_new::release(p); }
void operator delete[](void* p, const std::nothrow_t&) noexcept { fail_new::release(p); }
void operator delete(void* p, std::align_val_t, const std::nothrow_t&) noexcept { fail_new::release_aligned(p); }
void operator delete[](void* p, std::align_val_t, const std::nothrow_t&) noexcept { fail_new::release_aligned(p); }

// The complete real global-new/delete hook prefix is added from the pinned
