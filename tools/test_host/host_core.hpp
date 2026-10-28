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
//   xoserve             stay up and run requests one after another in this process: the
//                       driver writes inbox/<id>.tar, then inbox/<id>.ready; each run ends as
//                       with xorun (outbox/<id>.tar) and the next one starts. A crash or a
//                       program thread other than main calling exit ends the process; the
//                       driver starts the app again. <root>/ledger.txt keeps the totals the
//                       app shows (dashboard()) across those restarts.
// The request names either a hosted program (lib xoh_<name> in the modules directory)
// or a builtin: @info (device facts), @net HOST PORT SECONDS (wait for a TCP connect,
// e.g. until the local network permission is granted), @sleep SECONDS, and in xoserve
// @reset (zero the ledger) and @quit (end the process after the run).
#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace xoffset_test_host {

struct Options {
    std::string root;          // data directory (created when missing)
    std::string modules_dir;   // where lib xoh_<name> live
    std::string module_suffix; // ".dylib" or ".so"
    std::string platform_info; // extra lines for @info from the shell (UIKit facts)
};

// What the app shows while serving: totals since the last @reset, and the run in progress.
struct Dashboard {
    static constexpr std::size_t kRecent = 8;
    bool serving = false;
    unsigned long long runs = 0, passed = 0, failed = 0;
    unsigned long long bytes_in = 0, bytes_out = 0;     // request and result archives
    unsigned long long demo_runs = 0, demo_snapshots = 0, demo_bytes = 0;  // demo clients' received frames
    unsigned long long restarts = 0;                     // processes that continued the ledger
    std::string current;                                 // program running now ("" between runs)
    bool quiet = false;                                  // a timed run: the screen must not change
    double current_seconds = 0;
    unsigned long long live_snapshots = 0, live_bytes = 0;  // the running demo client so far
    std::vector<std::string> recent;                     // "name\tstatus\tms", newest first
};

// A copy of the current dashboard (any thread; reads the running demo client's output).
Dashboard dashboard();
// A timed or measuring run is in progress: the screen must not change (a lock-free check).
bool timed_run() noexcept;

// Runs the command in argv[0..argc) and ends the process: it does not return.
[[noreturn]] void run_command(int argc, char** argv, const Options& options);

// Starts run_command on a new thread with a large stack (iOS secondary threads get
// 512 KiB by default; test programs expect a process main thread).
void start_command_thread(int argc, char** argv, const Options& options);

// host_new.cpp: the host's operator new and delete, which forward to a hosted
// program's replacements (xoffset_test_host.h).
extern const void* const host_new_functions[];
void set_new_functions(const void* const* program_functions);
// Back to the defaults between two programs run in one process.
void reset_new_functions() noexcept;
// Called on the host's own threads: their allocations keep the default functions.
void keep_host_allocations_on_this_thread() noexcept;

}  // namespace xoffset_test_host
