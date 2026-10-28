// Linked into every hosted program library (see xoffset_test_host.h). No standard
// header on purpose: exit and _Exit below replace the C library's for this library
// only (hidden, so the program's own calls bind here and nothing else does).
#include "xoffset_test_host.h"

namespace {
const xoffset_test_host_api* host_api = nullptr;
}

extern "C" {

__attribute__((visibility("default"))) void xoffset_hosted_set_api(const xoffset_test_host_api* api) {
    host_api = api;
}

__attribute__((visibility("hidden"), noreturn)) void exit(int code) {
    host_api->program_exit(code, 0);
    __builtin_unreachable();
}

__attribute__((visibility("hidden"), noreturn)) void _Exit(int code) {
    host_api->program_exit(code, 1);
    __builtin_unreachable();
}

}  // extern "C"
