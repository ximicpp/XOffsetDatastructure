// Command-line test host: the same flow as the iOS app (host_core.hpp), for checking
// the hosted program libraries and xo_host_run.py on Linux and macOS.
//
//   xoffset_test_host --root DIR xorun ID | xopack ID REASON | xoserve
//
// The libraries are looked up in the hosted/ directory next to this program.
#include "host_core.hpp"

#include <climits>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>

#include <pthread.h>
#include <unistd.h>

#if defined(__APPLE__)
#include <mach-o/dyld.h>
#endif

namespace {
std::string program_dir() {
    char buf[PATH_MAX] = {};
#if defined(__APPLE__)
    uint32_t size = sizeof buf;
    if (_NSGetExecutablePath(buf, &size) != 0) return ".";
    char real[PATH_MAX];
    if (realpath(buf, real)) std::strcpy(buf, real);
#else
    const ssize_t n = readlink("/proc/self/exe", buf, sizeof buf - 1);
    if (n <= 0) return ".";
    buf[n] = 0;
#endif
    std::string p(buf);
    return p.substr(0, p.rfind('/'));
}
}  // namespace

int main(int argc, char** argv) {
    if (argc < 4 || std::strcmp(argv[1], "--root") != 0) {
        std::cerr << "usage: " << argv[0] << " --root DIR xorun ID | xopack ID REASON | xoserve\n";
        return 2;
    }
    xoffset_test_host::Options o;
    o.root = argv[2];
    o.modules_dir = program_dir() + "/hosted";
#if defined(__APPLE__)
    o.module_suffix = ".dylib";
#else
    o.module_suffix = ".so";
#endif
    xoffset_test_host::keep_host_allocations_on_this_thread();
    xoffset_test_host::start_command_thread(argc - 3, argv + 3, o);
    // the command ends the process
    for (;;) pause();
}
