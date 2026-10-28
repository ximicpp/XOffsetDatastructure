// Linked into every hosted program library next to hosted_shim.cpp: registers the
// program's replacements of operator new and delete with the host (see
// xoffset_test_host.h). The forms the program does not replace bind to the host's
// own here, and the host skips those.
#include "xoffset_host_new.hpp"
#include "xoffset_test_host.h"

extern "C" __attribute__((visibility("hidden"))) void xoffset_hosted_install_new(const xoffset_test_host_api* api) {
    if (api->version < 2 || !api->set_new_functions) return;
    static const void* const functions[XOFFSET_HOST_NEW_COUNT] = {XOFFSET_HOST_NEW_FUNCTIONS(XOFFSET_HOST_NEW_ADDRESS)};
    api->set_new_functions(functions);
}
