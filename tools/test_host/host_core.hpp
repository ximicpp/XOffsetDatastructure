// Test host core: runs one hosted program per process and reports it through files
// in a data directory (on iOS the app's Documents/xo). Shared by the iOS app
// (host_ios.mm) and the command-line host (host_cli.cpp) that checks the same flow
// on Linux and macOS. The driver on the computer side is xo_host_run.py.
//
//   <root>/inbox/<id>.tar    request from the driver: file "request" and inputs under fs/
//                            (request lines "KEY HEX": prog, arg, env, out, outdir, clear)
//   <root>/outbox/<id>.tar   result: "status", "out.txt" (stdout and stderr), outputs under fs/
//   <root>/runs/<id>/        the run's own files; <root>/fs/ inputs and outputs; <root>/work/ cwd
//
// Commands (argv after the program name):
//   xorun <id>          unpack inbox/<id>.tar, run the program it names, pack outbox/<id>.tar
//   xopack <id> <why>   pack outbox/<id>.tar for a run that ended without one (killed, hung)
// The request names either a hosted program (lib xoh_<name> in the modules directory)
// or a builtin: @info (device facts), @net HOST PORT SECONDS (wait for a TCP connect,
// e.g. until the local network permission is granted), @sleep SECONDS.
#pragma once

#include <string>

namespace xoffset_test_host {

struct Options {
    std::string root;          // data directory (created when missing)
    std::string modules_dir;   // where lib xoh_<name> live
    std::string module_suffix; // ".dylib" or ".so"
    std::string platform_info; // extra lines for @info from the shell (UIKit facts)
};

// Runs the command in argv[0..argc) and ends the process: it does not return.
[[noreturn]] void run_command(int argc, char** argv, const Options& options);

// Starts run_command on a new thread with a large stack (iOS secondary threads get
// 512 KiB by default; test programs expect a process main thread).
void start_command_thread(int argc, char** argv, const Options& options);

// host_new.cpp: the host's operator new and delete, which forward to a hosted
// program's replacements (xoffset_test_host.h).
extern const void* const host_new_functions[];
void set_new_functions(const void* const* program_functions);

}  // namespace xoffset_test_host
