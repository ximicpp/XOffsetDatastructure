#pragma once
#include <cstddef>
#include <cstdlib>
#include <new>
#include <sys/resource.h>
namespace output16 {
inline thread_local bool active=false;
inline thread_local unsigned long long allocations=0, requested=0, allocation_minor=0, allocation_major=0;
inline thread_local unsigned long long payload_copies=0, payload_bytes=0, header_copies=0, header_bytes=0;
inline void reset() { allocations=requested=allocation_minor=allocation_major=0;payload_copies=payload_bytes=header_copies=header_bytes=0; }
inline void* allocate(std::size_t n) {
    rusage before{},after{}; const bool observe=active;
    if(observe) getrusage(RUSAGE_SELF,&before);
    void* p=std::malloc(n?n:1);
    if(!p) throw std::bad_alloc();
    if(observe) {
        getrusage(RUSAGE_SELF,&after);++allocations;requested+=n;
        allocation_minor+=after.ru_minflt-before.ru_minflt;
        allocation_major+=after.ru_majflt-before.ru_majflt;
    }
    return p;
}
}
void* operator new(std::size_t n) { return output16::allocate(n); }
void* operator new[](std::size_t n) { return output16::allocate(n); }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete[](void* p) noexcept { std::free(p); }
void operator delete(void* p,std::size_t) noexcept { std::free(p); }
void operator delete[](void* p,std::size_t) noexcept { std::free(p); }
