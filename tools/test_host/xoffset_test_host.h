/* Interface between the test host (tools/test_host) and one hosted program.
 *
 * A hosted program is an ordinary test, demo or benchmark program built a second
 * time as a shared library (cmake/XOffsetTestHost.cmake): its sources unchanged,
 * plus hosted_shim.cpp. The host loads one such library per process, hands it this
 * table through xoffset_hosted_set_api() and calls the library's own main().
 *
 * hosted_shim.cpp defines exit() and _Exit() inside the library, so a program
 * that ends itself (std::exit in a test, std::_Exit in a terminate handler) goes
 * through program_exit and the host records the status before the process ends.
 *
 * Replaceable allocation functions: the host defines all twenty forms of operator
 * new and delete (host_new.cpp), so the C++ runtime's own allocations reach them
 * as they reach a program's replacements in an ordinary executable. A program
 * that replaces some of them (allocation counting, failure injection) has them
 * registered by hosted_new_shim.cpp; the host's forms then forward to them.
 * Plain C, no includes: the shim must not see the C library's declarations. */
#ifndef XOFFSET_TEST_HOST_H
#define XOFFSET_TEST_HOST_H

#ifdef __cplusplus
extern "C" {
#endif

#define XOFFSET_TEST_HOST_API_VERSION 2

/* Order of the allocation functions in the tables below: hosted_new_shim.cpp. */
#define XOFFSET_HOST_NEW_COUNT 20

struct xoffset_test_host_api {
    int version;
    /* The program called exit (quick == 0) or _Exit (quick != 0). Never returns. */
    void (*program_exit)(int code, int quick);
    /* The host's own allocation functions (XOFFSET_HOST_NEW_COUNT entries) ... */
    const void* const* host_new_functions;
    /* ... and the program's: an entry that is null or the host's own is not replaced. */
    void (*set_new_functions)(const void* const* program_functions);
};

typedef void (*xoffset_hosted_set_api_fn)(const struct xoffset_test_host_api* api);
#define XOFFSET_HOSTED_SET_API "xoffset_hosted_set_api"

#ifdef __cplusplus
}
#endif

#endif
