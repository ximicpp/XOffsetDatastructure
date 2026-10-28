// Test host self-check (CTest test_host_new_forwarding, with -DXOFFSET_TEST_HOST=ON):
// a hosted program's replacement of operator new also serves the C++ runtime
// library's own allocations, as in an ordinary executable (host_new.cpp). The
// message of std::runtime_error is allocated inside libstdc++ / libc++.
#include <cstdio>
#include <cstdlib>
#include <new>
#include <stdexcept>

namespace {
long g_news = 0;
}

void* operator new(std::size_t n) {
    ++g_news;
    if (void* p = std::malloc(n ? n : 1)) return p;
    throw std::bad_alloc();
}
void operator delete(void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }

int main() {
    const long before = g_news;
    { std::runtime_error e("a message the C++ runtime library copies into its own allocation"); }
    const long runtime = g_news - before;
    std::printf("operator new calls from the runtime library: %ld\n", runtime);
    if (runtime < 1) {
        std::printf("FAIL: the program's operator new does not serve the runtime library\n");
        return 1;
    }
    return 0;
}
