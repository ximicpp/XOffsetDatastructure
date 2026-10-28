// See host_core.hpp. POSIX only (iOS, macOS, Linux, Android).
#include "host_core.hpp"
#include "xoffset_test_host.h"

#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <cerrno>
#include <chrono>
#include <csetjmp>
#include <csignal>
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include <dirent.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <netdb.h>
#include <pthread.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/utsname.h>
#include <unistd.h>

#if defined(__APPLE__)
#include <TargetConditionals.h>
#include <sys/sysctl.h>
#if TARGET_OS_IPHONE && __has_include(<os/proc.h>)
#include <os/proc.h>
#define XO_HOST_HAVE_OS_PROC 1
#endif
#endif

// The owner map of every program library (xoffset_slot_storage.hpp, built with
// XOFFSET_OWNER_MAP_SHARED by cmake/XOffsetTestHost.cmake): one table for the
// process instead of 64 MiB of address space in each loaded library.
extern "C" __attribute__((visibility("default"), used)) std::uint8_t* xoffset_owner_map_shared(std::size_t bytes) {
    static const std::size_t size = bytes;
    static std::uint8_t* const table = [] {
        void* p = ::mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
        return p == MAP_FAILED ? nullptr : static_cast<std::uint8_t*>(p);
    }();
    return bytes == size ? table : nullptr;
}

namespace xoffset_test_host {
namespace {

// ---------------------------------------------------------------- small helpers

bool is_dir(const std::string& p) {
    struct stat st;
    return ::stat(p.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
}

bool exists(const std::string& p) {
    struct stat st;
    return ::lstat(p.c_str(), &st) == 0;
}

void mkdirs(const std::string& path) {
    std::string cur;
    std::size_t i = 0;
    while (i <= path.size()) {
        const std::size_t j = path.find('/', i);
        const std::size_t end = j == std::string::npos ? path.size() : j;
        cur = path.substr(0, end);
        if (!cur.empty()) ::mkdir(cur.c_str(), 0755);
        if (j == std::string::npos) break;
        i = j + 1;
    }
}

void remove_tree(const std::string& path) {
    struct stat st;
    if (::lstat(path.c_str(), &st) != 0) return;
    if (S_ISDIR(st.st_mode)) {
        if (DIR* d = ::opendir(path.c_str())) {
            while (dirent* e = ::readdir(d)) {
                if (!std::strcmp(e->d_name, ".") || !std::strcmp(e->d_name, "..")) continue;
                remove_tree(path + "/" + e->d_name);
            }
            ::closedir(d);
        }
        ::rmdir(path.c_str());
    } else {
        ::unlink(path.c_str());
    }
}

std::vector<std::string> list_dir(const std::string& path) {
    std::vector<std::string> names;
    if (DIR* d = ::opendir(path.c_str())) {
        while (dirent* e = ::readdir(d)) {
            if (!std::strcmp(e->d_name, ".") || !std::strcmp(e->d_name, "..")) continue;
            names.emplace_back(e->d_name);
        }
        ::closedir(d);
    }
    return names;
}

bool read_file(const std::string& path, std::string& out) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    std::ostringstream s;
    s << f.rdbuf();
    out = s.str();
    return true;
}

bool write_file(const std::string& path, const std::string& data) {
    std::ofstream f(path, std::ios::binary | std::ios::trunc);
    f.write(data.data(), static_cast<std::streamsize>(data.size()));
    return static_cast<bool>(f);
}

std::string unhex(const std::string& h) {
    std::string out;
    auto v = [](char c) -> int {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return -1;
    };
    for (std::size_t i = 0; i + 1 < h.size(); i += 2) {
        const int a = v(h[i]), b = v(h[i + 1]);
        if (a < 0 || b < 0) break;
        out.push_back(static_cast<char>(a * 16 + b));
    }
    return out;
}

// A request path or tar name must stay inside its directory.
bool safe_relative(const std::string& p) {
    if (p.empty() || p[0] == '/') return false;
    std::size_t i = 0;
    while (i <= p.size()) {
        const std::size_t j = p.find('/', i);
        const std::string part = p.substr(i, j == std::string::npos ? std::string::npos : j - i);
        if (part == "..") return false;
        if (j == std::string::npos) break;
        i = j + 1;
    }
    return true;
}

// ---------------------------------------------------------------- ustar reader

// Extracts the regular files and directories of a ustar/pax archive under dest.
// keep(name) decides which members to extract. Returns false on a damaged archive.
template <class Keep>
bool untar(const std::string& data, const std::string& dest, Keep keep, std::string* request) {
    std::size_t off = 0;
    auto field = [](const char* p, std::size_t n) {
        std::size_t len = 0;
        while (len < n && p[len]) ++len;
        return std::string(p, len);
    };
    auto octal = [](const char* p, std::size_t n) {
        unsigned long long v = 0;
        for (std::size_t i = 0; i < n && p[i]; ++i) {
            if (p[i] >= '0' && p[i] <= '7') v = v * 8 + static_cast<unsigned>(p[i] - '0');
        }
        return v;
    };
    while (off + 512 <= data.size()) {
        const char* h = data.data() + off;
        bool zero = true;
        for (int i = 0; i < 512; ++i) zero = zero && h[i] == 0;
        if (zero) return true;
        std::string name = field(h, 100);
        const std::string prefix = std::memcmp(h + 257, "ustar", 5) == 0 ? field(h + 345, 155) : std::string();
        if (!prefix.empty()) name = prefix + "/" + name;
        const unsigned long long size = octal(h + 124, 12);
        const char type = h[156];
        off += 512;
        if (off + size > data.size()) return false;
        const std::string body = data.substr(off, static_cast<std::size_t>(size));
        off += static_cast<std::size_t>((size + 511) / 512 * 512);
        while (name.size() > 1 && name.compare(0, 2, "./") == 0) name.erase(0, 2);
        while (!name.empty() && name.back() == '/') name.pop_back();
        if (name.empty() || !safe_relative(name)) continue;
        if (type == 'x' || type == 'g') continue;  // pax headers: names here are short ustar names
        if (name == "request" && request) {
            *request = body;
            continue;
        }
        if (!keep(name)) continue;
        const std::string path = dest + "/" + name;
        if (type == '5') {
            mkdirs(path);
        } else if (type == '0' || type == '\0' || type == '7') {
            const std::size_t slash = path.rfind('/');
            mkdirs(path.substr(0, slash));
            if (!write_file(path, body)) return false;
        }
    }
    return true;
}

// ---------------------------------------------------------------- ustar writer
// Plain file descriptors and static buffers only: it also runs in signal handlers.

constexpr std::size_t kPath = 4096;
char g_root[kPath];            // <root>
char g_run_dir[kPath];         // <root>/runs/<id>
char g_outbox_tmp[kPath];      // <root>/outbox/<id>.tar.tmp
char g_outbox[kPath];          // <root>/outbox/<id>.tar
std::vector<std::string>* g_outputs = nullptr;  // absolute paths, prepared before the run
std::atomic<int> g_finalized{0};
std::atomic<int> g_exit_code{-1000};
std::chrono::steady_clock::time_point g_start;
char g_io[1 << 16];

// Serving (xoserve): one process runs request after request, each program on its own thread.
std::atomic<bool> g_serving{false};
pthread_t g_program_thread;                // the running program's main thread
std::mutex g_run_mu;
std::condition_variable g_run_cv;
bool g_run_done = false;                   // under g_run_mu
std::atomic<bool> g_run_active{false};
thread_local sigjmp_buf* t_exit_jump = nullptr;   // set on the program's main thread
pid_t g_host_pid = 0;   // the process that runs the command; a program's fork() child is not it

// A process the program forked: it reports nothing (its parent's run does).
bool forked_child() { return g_host_pid != 0 && ::getpid() != g_host_pid; }

void cat3(char* out, const char* a, const char* b, const char* c) {
    std::snprintf(out, kPath, "%s%s%s", a, b, c);
}

bool write_all(int fd, const char* p, std::size_t n) {
    while (n) {
        const ssize_t w = ::write(fd, p, n);
        if (w < 0) {
            if (errno == EINTR) continue;
            return false;
        }
        p += w;
        n -= static_cast<std::size_t>(w);
    }
    return true;
}

void put_octal(char* p, std::size_t n, unsigned long long v) {
    // n - 1 digits and a NUL, as tar wants
    p[n - 1] = 0;
    for (std::size_t i = n - 1; i-- > 0;) {
        p[i] = static_cast<char>('0' + (v & 7));
        v >>= 3;
    }
}

// name: archive member name; for a directory size is 0.
bool tar_header(int fd, const char* name, unsigned long long size, bool dir) {
    char h[512];
    std::memset(h, 0, sizeof h);
    const std::size_t len = std::strlen(name);
    if (len <= 100) {
        std::memcpy(h, name, len);
    } else {
        // split at a '/' so that prefix <= 155 and name <= 100
        std::size_t cut = len;
        while (cut > 0 && !(name[cut - 1] == '/' && cut - 1 <= 155 && len - cut <= 100)) --cut;
        if (cut == 0) return false;
        std::memcpy(h + 345, name, cut - 1);
        std::memcpy(h, name + cut, len - cut);
    }
    put_octal(h + 100, 8, dir ? 0755 : 0644);
    put_octal(h + 108, 8, 0);
    put_octal(h + 116, 8, 0);
    put_octal(h + 124, 12, size);
    put_octal(h + 136, 12, 0);
    h[156] = dir ? '5' : '0';
    std::memcpy(h + 257, "ustar", 6);
    std::memcpy(h + 263, "00", 2);
    std::memset(h + 148, ' ', 8);
    unsigned sum = 0;
    for (unsigned char c : h) sum += c;
    put_octal(h + 148, 7, sum);
    h[155] = ' ';
    return write_all(fd, h, 512);
}

bool tar_file(int fd, const char* path, const char* name) {
    const int in = ::open(path, O_RDONLY);
    if (in < 0) return false;
    struct stat st;
    if (::fstat(in, &st) != 0) {
        ::close(in);
        return false;
    }
    const unsigned long long size = static_cast<unsigned long long>(st.st_size);
    bool ok = tar_header(fd, name, size, false);
    unsigned long long left = size;
    while (ok && left) {
        const ssize_t r = ::read(in, g_io, left < sizeof g_io ? static_cast<std::size_t>(left) : sizeof g_io);
        if (r <= 0) {
            // the file shrank: pad with zeros to the size in the header
            std::memset(g_io, 0, sizeof g_io);
            while (ok && left) {
                const std::size_t n = left < sizeof g_io ? static_cast<std::size_t>(left) : sizeof g_io;
                ok = write_all(fd, g_io, n);
                left -= n;
            }
            break;
        }
        ok = write_all(fd, g_io, static_cast<std::size_t>(r));
        left -= static_cast<unsigned long long>(r);
    }
    ::close(in);
    const std::size_t pad = static_cast<std::size_t>((512 - size % 512) % 512);
    if (ok && pad) {
        char z[512] = {};
        ok = write_all(fd, z, pad);
    }
    return ok;
}

void tar_tree(int fd, const char* path, const char* name, int depth) {
    struct stat st;
    if (depth > 64 || ::lstat(path, &st) != 0) return;
    if (S_ISREG(st.st_mode)) {
        tar_file(fd, path, name);
        return;
    }
    if (!S_ISDIR(st.st_mode)) return;
    tar_header(fd, name, 0, true);
    DIR* d = ::opendir(path);
    if (!d) return;
    while (dirent* e = ::readdir(d)) {
        if (!std::strcmp(e->d_name, ".") || !std::strcmp(e->d_name, "..")) continue;
        char sub[kPath], subname[kPath];
        std::snprintf(sub, sizeof sub, "%s/%s", path, e->d_name);
        std::snprintf(subname, sizeof subname, "%s/%s", name, e->d_name);
        tar_tree(fd, sub, subname, depth + 1);
    }
    ::closedir(d);
}

// Writes <run>/status and packs outbox/<id>.tar (atomically). Once per process.
void finalize(const char* status, bool flush) {
    if (g_finalized.exchange(1)) return;
    if (flush) {
        std::cout.flush();
        std::cerr.flush();
        std::fflush(nullptr);
    }
    char line[256];
    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::steady_clock::now() - g_start).count();
    const int n = std::snprintf(line, sizeof line, "%s\nelapsed_ms %lld\n", status, static_cast<long long>(ms));
    char path[kPath];
    cat3(path, g_run_dir, "/status", "");
    int fd = ::open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd >= 0) {
        write_all(fd, line, static_cast<std::size_t>(n));
        ::close(fd);
    }
    fd = ::open(g_outbox_tmp, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) return;
    tar_file(fd, path, "status");
    cat3(path, g_run_dir, "/out.txt", "");
    tar_file(fd, path, "out.txt");
    if (g_outputs) {
        for (const auto& out : *g_outputs) {
            char name[kPath];
            cat3(path, g_root, "/fs", out.c_str());
            cat3(name, "fs", out.c_str(), "");
            tar_tree(fd, path, name, 0);
        }
    }
    char z[1024] = {};
    write_all(fd, z, sizeof z);
    ::fsync(fd);
    ::close(fd);
    ::rename(g_outbox_tmp, g_outbox);
}

// ---------------------------------------------------------------- ending the process

void at_exit_final() {
    if (forked_child() || (g_serving && !g_run_active)) return;
    char status[64];
    const int code = g_exit_code.load();
    if (code == -1000) std::snprintf(status, sizeof status, "exit unknown");
    else std::snprintf(status, sizeof status, "exit %d", code);
    finalize(status, true);
}

void on_signal(int sig) {
    if (forked_child()) {
        std::signal(sig, SIG_DFL);
        std::raise(sig);
        return;
    }
    char status[64];
    std::snprintf(status, sizeof status, "signal %d", sig);
    finalize(status, false);
    std::signal(sig, SIG_DFL);
    std::raise(sig);
}

void end_served_run() {
    std::lock_guard<std::mutex> lock(g_run_mu);
    g_run_done = true;
    g_run_cv.notify_all();
}

[[noreturn]] void program_exit(int code, int quick) {
    if (forked_child()) {
        // a serving host's copy cannot run exit handlers (the copied host threads are gone)
        if (!g_serving) std::exit(code);
        if (!quick) {
            std::cout.flush();
            std::cerr.flush();
            std::fflush(nullptr);
        }
        std::_Exit(code);
    }
    g_exit_code = code;
    if (g_serving) {
        char status[64];
        std::snprintf(status, sizeof status, "exit %d", code);
        finalize(status, true);
        // Back to the start of the program's main thread (no unwinding, as in a process
        // exit) and on to the next request. Any other thread of the program that ends it
        // leaves state behind: the process ends and the driver starts a new one.
        if (t_exit_jump) siglongjmp(*t_exit_jump, 1);
        std::_Exit(code);
    }
    if (quick) {
        char status[64];
        std::snprintf(status, sizeof status, "exit %d", code);
        finalize(status, true);
        std::_Exit(code);
    }
    std::exit(code);
}

const xoffset_test_host_api g_api = {XOFFSET_TEST_HOST_API_VERSION, &program_exit, host_new_functions,
                                     &set_new_functions};

void install_signal_handlers() {
    static char alt[1 << 16];
    stack_t ss{};
    ss.ss_sp = alt;
    ss.ss_size = sizeof alt;
    ::sigaltstack(&ss, nullptr);
    struct sigaction sa{};
    sa.sa_handler = on_signal;
    sa.sa_flags = SA_ONSTACK;
    sigemptyset(&sa.sa_mask);
    for (int sig : {SIGABRT, SIGSEGV, SIGBUS, SIGILL, SIGFPE, SIGTRAP, SIGSYS}) ::sigaction(sig, &sa, nullptr);
    std::signal(SIGPIPE, SIG_IGN);
}

// ---------------------------------------------------------------- builtins

#if defined(__APPLE__)
std::string sysctl_string(const char* name) {
    std::size_t len = 0;
    if (::sysctlbyname(name, nullptr, &len, nullptr, 0) != 0 || len == 0) return "?";
    std::string s(len, '\0');
    if (::sysctlbyname(name, s.data(), &len, nullptr, 0) != 0) return "?";
    while (!s.empty() && s.back() == '\0') s.pop_back();
    return s;
}
long long sysctl_int(const char* name) {
    long long v = 0;
    std::size_t len = sizeof v;
    if (::sysctlbyname(name, &v, &len, nullptr, 0) != 0) return -1;
    if (len == sizeof(int)) return *reinterpret_cast<int*>(&v);
    return v;
}
#endif

int builtin_info(const Options& o) {
    struct utsname u{};
    ::uname(&u);
    std::cout << "uname " << u.sysname << ' ' << u.release << ' ' << u.machine << '\n';
#if defined(__APPLE__)
    for (const char* k : {"hw.machine", "hw.model", "kern.osproductversion", "kern.osversion", "machdep.cpu.brand_string"})
        std::cout << k << ' ' << sysctl_string(k) << '\n';
    for (const char* k : {"hw.memsize", "hw.ncpu", "hw.perflevel0.physicalcpu", "hw.perflevel1.physicalcpu",
                          "hw.perflevel0.l2cachesize", "hw.cachelinesize", "hw.pagesize"})
        std::cout << k << ' ' << sysctl_int(k) << '\n';
#endif
#if defined(XO_HOST_HAVE_OS_PROC)
    if (__builtin_available(iOS 13.0, *)) std::cout << "os_proc_available_memory " << os_proc_available_memory() << '\n';
#endif
#if defined(__linux__)
    std::string cpu;
    if (read_file("/proc/cpuinfo", cpu)) {
        std::istringstream s(cpu);
        for (std::string line; std::getline(s, line);)
            if (line.rfind("model name", 0) == 0 || line.rfind("Hardware", 0) == 0) {
                std::cout << line << '\n';
                break;
            }
    }
#endif
    std::cout << "hardware_concurrency " << std::thread::hardware_concurrency() << '\n';
    std::cout << o.platform_info;
    return 0;
}

int builtin_net(const std::vector<std::string>& args) {
    if (args.size() < 3) {
        std::cerr << "@net HOST PORT SECONDS\n";
        return 2;
    }
    const auto until = std::chrono::steady_clock::now() + std::chrono::seconds(std::atoi(args[2].c_str()));
    int attempts = 0;
    std::string last;
    do {
        ++attempts;
        addrinfo hints{};
        hints.ai_socktype = SOCK_STREAM;
        addrinfo* res = nullptr;
        const int g = ::getaddrinfo(args[0].c_str(), args[1].c_str(), &hints, &res);
        if (g != 0) {
            last = gai_strerror(g);
        } else {
            for (addrinfo* a = res; a; a = a->ai_next) {
                const int s = ::socket(a->ai_family, a->ai_socktype, a->ai_protocol);
                if (s < 0) continue;
                const int c = ::connect(s, a->ai_addr, a->ai_addrlen);
                last = c == 0 ? "" : std::strerror(errno);
                ::close(s);
                if (c == 0) {
                    ::freeaddrinfo(res);
                    std::cout << "connected to " << args[0] << ':' << args[1] << " after " << attempts << " attempts\n";
                    return 0;
                }
            }
            ::freeaddrinfo(res);
        }
        std::this_thread::sleep_for(std::chrono::seconds(1));
    } while (std::chrono::steady_clock::now() < until);
    std::cout << "no connection to " << args[0] << ':' << args[1] << " (" << attempts << " attempts, last: " << last << ")\n";
    return 1;
}

// ---------------------------------------------------------------- commands

struct Request {
    std::string program;
    std::vector<std::string> args, env, outputs, output_dirs, clears;
};

Request parse_request(const std::string& text) {
    Request r;
    std::istringstream s(text);
    for (std::string line; std::getline(s, line);) {
        const std::size_t sp = line.find(' ');
        const std::string key = line.substr(0, sp);
        const std::string value = sp == std::string::npos ? std::string() : unhex(line.substr(sp + 1));
        if (key == "prog") r.program = value;
        else if (key == "arg") r.args.push_back(value);
        else if (key == "env") r.env.push_back(value);
        else if (key == "out") r.outputs.push_back(value);
        else if (key == "outdir") {
            r.outputs.push_back(value);
            r.output_dirs.push_back(value);
        }
        else if (key == "clear") r.clears.push_back(value);
    }
    return r;
}

void set_run_paths(const std::string& root, const std::string& id) {
    std::snprintf(g_root, kPath, "%s", root.c_str());
    std::snprintf(g_run_dir, kPath, "%s/runs/%s", root.c_str(), id.c_str());
    std::snprintf(g_outbox_tmp, kPath, "%s/outbox/%s.tar.tmp", root.c_str(), id.c_str());
    std::snprintf(g_outbox, kPath, "%s/outbox/%s.tar", root.c_str(), id.c_str());
}

[[noreturn]] void fail_early(const std::string& message, int code) {
    std::cerr << "xoffset test host: " << message << '\n';
    program_exit(code, 0);
}

[[noreturn]] void cmd_run(const std::string& id, const Options& o) {
    const std::string& root = o.root;
    for (const char* d : {"inbox", "outbox", "runs", "fs", "work"}) mkdirs(root + "/" + d);
    // One run at a time: leftovers of earlier runs go (the driver has collected them).
    for (const auto& n : list_dir(root + "/outbox")) remove_tree(root + "/outbox/" + n);
    for (const auto& n : list_dir(root + "/runs"))
        if (n != id) remove_tree(root + "/runs/" + n);
    for (const auto& n : list_dir(root + "/inbox"))
        if (n != id + ".tar") remove_tree(root + "/inbox/" + n);
    mkdirs(root + "/runs/" + id);
    set_run_paths(root, id);
    static std::vector<std::string> outputs;
    g_outputs = &outputs;
    g_start = std::chrono::steady_clock::now();

    // stdout and stderr of everything from here on go to the run's out.txt
    const std::string out_txt = root + "/runs/" + id + "/out.txt";
    const int fd = ::open(out_txt.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd >= 0) {
        std::fflush(nullptr);
        ::dup2(fd, 1);
        ::dup2(fd, 2);
        ::close(fd);
    }
    g_host_pid = ::getpid();
    install_signal_handlers();
    std::atexit(at_exit_final);

    std::string tar, request_text;
    const std::string inbox = root + "/inbox/" + id + ".tar";
    if (!read_file(inbox, tar)) fail_early("no " + inbox, 125);
    // first pass: the request (to clear stale inputs and outputs before extracting)
    if (!untar(tar, root, [](const std::string&) { return false; }, &request_text) || request_text.empty())
        fail_early("damaged request archive " + inbox, 125);
    const Request req = parse_request(request_text);
    write_file(root + "/runs/" + id + "/request", request_text);
    for (const auto& p : req.clears) remove_tree(root + "/fs" + p);
    for (const auto& p : req.outputs) {
        remove_tree(root + "/fs" + p);
        outputs.push_back(p);
    }
    if (!untar(tar, root, [](const std::string& n) { return n.compare(0, 3, "fs/") == 0; }, nullptr))
        fail_early("damaged input archive " + inbox, 125);
    ::unlink(inbox.c_str());
    for (const auto& p : req.outputs) {
        const std::string full = root + "/fs" + p;
        mkdirs(full.substr(0, full.rfind('/')));
    }
    for (const auto& p : req.output_dirs) mkdirs(root + "/fs" + p);

    const std::string fs_root = root + "/fs";
    auto expand = [&](std::string a) {
        for (std::size_t at; (at = a.find("@XOFS@")) != std::string::npos;) a.replace(at, 6, fs_root);
        return a;
    };
    for (const auto& kv : req.env) {
        const std::size_t eq = kv.find('=');
        if (eq != std::string::npos) ::setenv(kv.substr(0, eq).c_str(), expand(kv.substr(eq + 1)).c_str(), 1);
    }
    if (::chdir((root + "/work").c_str()) != 0) fail_early("chdir work failed", 125);

    std::vector<std::string> args;
    for (const auto& a : req.args) args.push_back(expand(a));
    int rc = 0;
    if (req.program == "@info") {
        rc = builtin_info(o);
    } else if (req.program == "@net") {
        rc = builtin_net(args);
    } else if (req.program == "@sleep") {
        std::this_thread::sleep_for(std::chrono::seconds(args.empty() ? 1 : std::atoi(args[0].c_str())));
    } else {
        if (req.program.empty() || !safe_relative(req.program) || req.program.find('/') != std::string::npos)
            fail_early("bad program name '" + req.program + "'", 125);
        const std::string lib = o.modules_dir + "/libxoh_" + req.program + o.module_suffix;
        void* h = ::dlopen(lib.c_str(), RTLD_NOW | RTLD_LOCAL);
        if (!h) fail_early(std::string("dlopen ") + lib + ": " + ::dlerror(), 127);
        auto set_api = reinterpret_cast<xoffset_hosted_set_api_fn>(::dlsym(h, XOFFSET_HOSTED_SET_API));
        auto main_fn = reinterpret_cast<int (*)(int, char**)>(::dlsym(h, "main"));
        if (!set_api || !main_fn) fail_early(lib + " has no hosted entry (main, " XOFFSET_HOSTED_SET_API ")", 127);
        set_api(&g_api);
        static std::vector<std::string> argv_store;
        argv_store.clear();
        argv_store.push_back(req.program);
        argv_store.insert(argv_store.end(), args.begin(), args.end());
        static std::vector<char*> argv;
        for (auto& a : argv_store) argv.push_back(a.data());
        argv.push_back(nullptr);
        rc = main_fn(static_cast<int>(argv_store.size()), argv.data());
    }
    program_exit(rc, 0);
}

// ---------------------------------------------------------------- serving

// Programs whose run is timed or measures the process: the screen does not change while
// they run (the app's timer skips its update), and the run starts only after the screen
// has shown that it is paused.
bool quiet_program(const std::string& name) {
    return name.rfind("xo_slg_bench", 0) == 0 || name.rfind("xo_mem_probe", 0) == 0;
}
bool demo_client(const std::string& name) { return name.rfind("mapdemo_client", 0) == 0; }

std::mutex g_dash_mu;
std::atomic<bool> g_quiet{false};  // g_dash.quiet, readable without the lock
Dashboard g_dash;                  // under g_dash_mu
std::string g_current_out;         // the running program's out.txt, under g_dash_mu
std::chrono::steady_clock::time_point g_current_start;

// The last "client snaps=N ... net_total=B" progress line (or the final "Bye received after
// N snapshots, B bytes") in the tail of a demo client's output.
bool demo_progress(const std::string& path, unsigned long long& snaps, unsigned long long& bytes) {
    const int fd = ::open(path.c_str(), O_RDONLY);
    if (fd < 0) return false;
    struct stat st;
    std::string tail;
    if (::fstat(fd, &st) == 0) {
        const off_t from = st.st_size > 4096 ? st.st_size - 4096 : 0;
        tail.resize(static_cast<std::size_t>(st.st_size - from));
        const ssize_t n = ::pread(fd, tail.data(), tail.size(), from);
        tail.resize(n > 0 ? static_cast<std::size_t>(n) : 0);
    }
    ::close(fd);
    bool found = false;
    std::istringstream lines(tail);
    for (std::string line; std::getline(lines, line);) {
        unsigned long long a = 0, b = 0;
        const std::size_t total = line.find("net_total=");
        if (line.rfind("client snaps=", 0) == 0 && total != std::string::npos &&
            std::sscanf(line.c_str(), "client snaps=%llu", &a) == 1 &&
            std::sscanf(line.c_str() + total, "net_total=%llu", &b) == 1) {
            snaps = a, bytes = b, found = true;
        } else if (std::sscanf(line.c_str(), "client: Bye received after %llu snapshots, %llu bytes", &a, &b) == 2) {
            snaps = a, bytes = b, found = true;
        }
    }
    return found;
}

std::string ledger_path(const Options& o) { return o.root + "/ledger.txt"; }

void save_ledger(const Options& o) {
    std::ostringstream s;
    {
        std::lock_guard<std::mutex> lock(g_dash_mu);
        s << "runs " << g_dash.runs << "\npassed " << g_dash.passed << "\nfailed " << g_dash.failed
          << "\nbytes_in " << g_dash.bytes_in << "\nbytes_out " << g_dash.bytes_out << "\ndemo_runs " << g_dash.demo_runs
          << "\ndemo_snapshots " << g_dash.demo_snapshots << "\ndemo_bytes " << g_dash.demo_bytes
          << "\nrestarts " << g_dash.restarts << '\n';
        for (const auto& r : g_dash.recent) s << "recent " << r << '\n';
    }
    write_file(ledger_path(o) + ".tmp", s.str());
    ::rename((ledger_path(o) + ".tmp").c_str(), ledger_path(o).c_str());
}

void load_ledger(const Options& o) {
    std::string text;
    Dashboard d;
    if (read_file(ledger_path(o), text)) {
        std::istringstream s(text);
        for (std::string line; std::getline(s, line);) {
            const std::size_t sp = line.find(' ');
            if (sp == std::string::npos) continue;
            const std::string k = line.substr(0, sp), v = line.substr(sp + 1);
            const unsigned long long n = std::strtoull(v.c_str(), nullptr, 10);
            if (k == "runs") d.runs = n;
            else if (k == "passed") d.passed = n;
            else if (k == "failed") d.failed = n;
            else if (k == "bytes_in") d.bytes_in = n;
            else if (k == "bytes_out") d.bytes_out = n;
            else if (k == "demo_runs") d.demo_runs = n;
            else if (k == "demo_snapshots") d.demo_snapshots = n;
            else if (k == "demo_bytes") d.demo_bytes = n;
            else if (k == "restarts") d.restarts = n;
            else if (k == "recent" && d.recent.size() < Dashboard::kRecent) d.recent.push_back(v);
        }
        ++d.restarts;   // this process continues an earlier one's ledger
    }
    std::lock_guard<std::mutex> lock(g_dash_mu);
    g_dash = std::move(d);
}

unsigned long long file_size(const std::string& p) {
    struct stat st;
    return ::stat(p.c_str(), &st) == 0 ? static_cast<unsigned long long>(st.st_size) : 0;
}

// Global state a program may change and the next one must not inherit.
struct ProcessState {
    std::ios cout_fmt{nullptr}, cerr_fmt{nullptr};
    std::terminate_handler terminate = nullptr;
    std::new_handler new_handler = nullptr;
    int out_fd = -1, err_fd = -1;
    void save() {
        cout_fmt.copyfmt(std::cout);
        cerr_fmt.copyfmt(std::cerr);
        terminate = std::get_terminate();
        new_handler = std::get_new_handler();
        out_fd = ::dup(1);
        err_fd = ::dup(2);
    }
    void restore() {
        std::cout.flush();
        std::cerr.flush();
        std::fflush(nullptr);
        if (out_fd >= 0) ::dup2(out_fd, 1);
        if (err_fd >= 0) ::dup2(err_fd, 2);
        std::cout.copyfmt(cout_fmt);
        std::cerr.copyfmt(cerr_fmt);
        std::cout.clear();
        std::cerr.clear();
        std::set_terminate(terminate);
        std::set_new_handler(new_handler);
        install_signal_handlers();
    }
};

void* program_thread_main(void* p) {
    auto* call = static_cast<std::pair<int (*)(int, char**), std::vector<char*>*>*>(p);
    sigjmp_buf jump;
    if (sigsetjmp(jump, 1) == 0) {
        t_exit_jump = &jump;
        const int rc = call->first(static_cast<int>(call->second->size()) - 1, call->second->data());
        program_exit(rc, 0);   // finalizes and jumps back here
    }
    t_exit_jump = nullptr;
    end_served_run();
    return nullptr;
}

bool address_space_headroom() {
    const char* v = std::getenv("XO_HOST_HEADROOM_MIB");
    const std::size_t mib = v && *v ? std::strtoull(v, nullptr, 10) : 2048;
    if (mib == 0) return true;
    void* p = ::mmap(nullptr, mib << 20, PROT_NONE, MAP_PRIVATE | MAP_ANON | MAP_NORESERVE, -1, 0);
    if (p == MAP_FAILED) return false;
    ::munmap(p, mib << 20);
    return true;
}

// One request, as cmd_run, in this process; returns when the program has ended.
void serve_one(const std::string& id, const Options& o, ProcessState& saved) {
    const std::string& root = o.root;
    for (const auto& n : list_dir(root + "/outbox")) remove_tree(root + "/outbox/" + n);
    for (const auto& n : list_dir(root + "/runs"))
        if (n != id) remove_tree(root + "/runs/" + n);
    mkdirs(root + "/runs/" + id);
    set_run_paths(root, id);
    static std::vector<std::string> outputs;
    outputs.clear();
    g_outputs = &outputs;
    g_finalized = 0;
    g_exit_code = -1000;
    {
        std::lock_guard<std::mutex> lock(g_run_mu);
        g_run_done = false;
    }

    std::string tar, request_text;
    const std::string inbox = root + "/inbox/" + id + ".tar";
    const unsigned long long in_bytes = file_size(inbox);
    const bool have = read_file(inbox, tar);
    Request req;
    if (have && untar(tar, root, [](const std::string&) { return false; }, &request_text) && !request_text.empty())
        req = parse_request(request_text);
    const std::string name = req.program.empty() ? std::string("?") : req.program;
    const bool quiet = quiet_program(name);
    const std::string out_txt = root + "/runs/" + id + "/out.txt";
    {
        std::lock_guard<std::mutex> lock(g_dash_mu);
        g_dash.current = name;
        g_dash.quiet = quiet;
        g_quiet = quiet;
        g_dash.live_snapshots = g_dash.live_bytes = 0;
        g_current_out = demo_client(name) ? out_txt : std::string();
        g_current_start = std::chrono::steady_clock::now();
    }
    // a timed run starts once the screen has drawn its paused state (the app's timer: 1 s)
    if (quiet) std::this_thread::sleep_for(std::chrono::milliseconds(1500));

    g_start = std::chrono::steady_clock::now();
    const int fd = ::open(out_txt.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd >= 0) {
        std::fflush(nullptr);
        ::dup2(fd, 1);
        ::dup2(fd, 2);
        ::close(fd);
    }
    g_run_active = true;
    std::vector<std::string> env_keys;
    auto finish = [&](int rc, const char* problem) {
        if (problem) std::cerr << "xoffset test host: " << problem << '\n';
        char status[64];
        std::snprintf(status, sizeof status, "exit %d", rc);
        finalize(status, true);
    };
    if (!have) {
        finish(125, ("no " + inbox).c_str());
    } else if (request_text.empty()) {
        finish(125, ("damaged request archive " + inbox).c_str());
    } else {
        write_file(root + "/runs/" + id + "/request", request_text);
        for (const auto& p : req.clears) remove_tree(root + "/fs" + p);
        for (const auto& p : req.outputs) {
            remove_tree(root + "/fs" + p);
            outputs.push_back(p);
        }
        const bool inputs_ok = untar(tar, root, [](const std::string& n) { return n.compare(0, 3, "fs/") == 0; }, nullptr);
        ::unlink(inbox.c_str());
        for (const auto& p : req.outputs) {
            const std::string full = root + "/fs" + p;
            mkdirs(full.substr(0, full.rfind('/')));
        }
        for (const auto& p : req.output_dirs) mkdirs(root + "/fs" + p);
        const std::string fs_root = root + "/fs";
        auto expand = [&](std::string a) {
            for (std::size_t at; (at = a.find("@XOFS@")) != std::string::npos;) a.replace(at, 6, fs_root);
            return a;
        };
        for (const auto& kv : req.env) {
            const std::size_t eq = kv.find('=');
            if (eq == std::string::npos) continue;
            env_keys.push_back(kv.substr(0, eq));
            ::setenv(env_keys.back().c_str(), expand(kv.substr(eq + 1)).c_str(), 1);
        }
        std::vector<std::string> args;
        for (const auto& a : req.args) args.push_back(expand(a));
        const std::string lib = o.modules_dir + "/libxoh_" + req.program + o.module_suffix;
        void* h = nullptr;
        if (!inputs_ok) {
            finish(125, ("damaged input archive " + inbox).c_str());
        } else if (::chdir((root + "/work").c_str()) != 0) {
            finish(125, "chdir work failed");
        } else if (req.program == "@info") {
            finish(builtin_info(o), nullptr);
        } else if (req.program == "@net") {
            finish(builtin_net(args), nullptr);
        } else if (req.program == "@sleep") {
            std::this_thread::sleep_for(std::chrono::seconds(args.empty() ? 1 : std::atoi(args[0].c_str())));
            finish(0, nullptr);
        } else if (req.program == "@reset") {
            {
                std::lock_guard<std::mutex> lock(g_dash_mu);
                g_dash = Dashboard{};
                g_dash.serving = true;
            }
            std::cout << "ledger reset\n";
            finish(0, nullptr);
        } else if (req.program == "@quit") {
            std::cout << "the host ends after this run\n";
            finish(0, nullptr);
        } else if (req.program.empty() || !safe_relative(req.program) || req.program.find('/') != std::string::npos) {
            finish(125, ("bad program name '" + req.program + "'").c_str());
        } else if (!(h = ::dlopen(lib.c_str(), RTLD_NOW | RTLD_LOCAL))) {
            finish(127, (std::string("dlopen ") + lib + ": " + ::dlerror()).c_str());
        } else {
            auto set_api = reinterpret_cast<xoffset_hosted_set_api_fn>(::dlsym(h, XOFFSET_HOSTED_SET_API));
            auto main_fn = reinterpret_cast<int (*)(int, char**)>(::dlsym(h, "main"));
            if (!set_api || !main_fn) {
                finish(127, (lib + " has no hosted entry (main, " XOFFSET_HOSTED_SET_API ")").c_str());
            } else {
                reset_new_functions();
                set_api(&g_api);
                std::vector<std::string> argv_store{req.program};
                argv_store.insert(argv_store.end(), args.begin(), args.end());
                std::vector<char*> argv;
                for (auto& a : argv_store) argv.push_back(a.data());
                argv.push_back(nullptr);
                std::pair<int (*)(int, char**), std::vector<char*>*> call{main_fn, &argv};
                pthread_attr_t attr;
                pthread_attr_init(&attr);
                pthread_attr_setstacksize(&attr, std::size_t(64) << 20);
                if (pthread_create(&g_program_thread, &attr, program_thread_main, &call) != 0) {
                    finish(125, "cannot start the program's thread");
                } else {
                    std::unique_lock<std::mutex> lock(g_run_mu);
                    g_run_cv.wait(lock, [] { return g_run_done; });
                    lock.unlock();
                    pthread_join(g_program_thread, nullptr);
                }
                pthread_attr_destroy(&attr);
                reset_new_functions();
            }
            // not closed: a library with thread-local variables cannot be unloaded anyway, and
            // the next run of the same program finds its static data as a new process would
            // only if the library is never shared between runs (most programs run once)
        }
    }
    g_run_active = false;
    saved.restore();
    for (const auto& k : env_keys) ::unsetenv(k.c_str());

    std::string status;
    read_file(root + "/runs/" + id + "/status", status);
    long long ms = 0;
    const std::size_t at = status.find("elapsed_ms ");
    if (at != std::string::npos) ms = std::atoll(status.c_str() + at + 11);
    status = status.substr(0, status.find('\n'));
    unsigned long long snaps = 0, bytes = 0;
    const bool demo = demo_client(name) && demo_progress(out_txt, snaps, bytes);
    const bool counted = req.program != "@reset";   // the totals start after it
    {
        std::lock_guard<std::mutex> lock(g_dash_mu);
        if (counted) ++g_dash.runs;
        if (counted) {
            (status == "exit 0" ? g_dash.passed : g_dash.failed) += 1;
            g_dash.bytes_in += in_bytes;
            g_dash.bytes_out += file_size(g_outbox);
        }
        if (demo) {
            ++g_dash.demo_runs;
            g_dash.demo_snapshots += snaps;
            g_dash.demo_bytes += bytes;
        }
        char line[256];
        std::snprintf(line, sizeof line, "%s\t%s\t%lld", name.c_str(), status.c_str(), ms);
        if (counted) g_dash.recent.insert(g_dash.recent.begin(), line);
        if (g_dash.recent.size() > Dashboard::kRecent) g_dash.recent.resize(Dashboard::kRecent);
        g_dash.current.clear();
        g_dash.quiet = false;
        g_quiet = false;
        g_current_out.clear();
    }
    save_ledger(o);
    if (req.program == "@quit") std::_Exit(0);
    // Programs leave reservations behind (buffers alive at exit, storage pools, slot
    // chunks); an iPhone process has ~3.4 GiB of address space. When the next program
    // could not reserve XO_HOST_HEADROOM_MIB (2048: the largest single buffer an iPhone allows, as
    // test_owner_map needs) in one piece, this process ends
    // after its result is packed and the driver starts a new one.
    if (!address_space_headroom()) std::_Exit(0);
}

[[noreturn]] void cmd_serve(const Options& o) {
    keep_host_allocations_on_this_thread();
    for (const char* d : {"inbox", "outbox", "runs", "fs", "work"}) mkdirs(o.root + "/" + d);
    load_ledger(o);
    g_host_pid = ::getpid();
    g_serving = true;
    install_signal_handlers();
    std::atexit(at_exit_final);
    static ProcessState saved;
    saved.save();
    {
        std::lock_guard<std::mutex> lock(g_dash_mu);
        g_dash.serving = true;
    }
    for (;;) {
        std::vector<std::string> ready;
        for (const auto& n : list_dir(o.root + "/inbox"))
            if (n.size() > 6 && n.compare(n.size() - 6, 6, ".ready") == 0) ready.push_back(n.substr(0, n.size() - 6));
        std::sort(ready.begin(), ready.end());
        for (const auto& id : ready) {
            ::unlink((o.root + "/inbox/" + id + ".ready").c_str());
            serve_one(id, o, saved);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
}

[[noreturn]] void cmd_pack(const std::string& id, const std::string& why, const Options& o) {
    const std::string run = o.root + "/runs/" + id;
    mkdirs(run);
    mkdirs(o.root + "/outbox");
    set_run_paths(o.root, id);
    static std::vector<std::string> outputs;
    std::string request_text, status;
    if (read_file(run + "/request", request_text)) outputs = parse_request(request_text).outputs;
    g_outputs = &outputs;
    g_start = std::chrono::steady_clock::now();
    if (!read_file(run + "/status", status) || status.empty()) status = "none " + why;
    while (!status.empty() && status.back() == '\n') status.pop_back();
    if (!exists(run + "/out.txt")) write_file(run + "/out.txt", "");
    // a request that never ran must not run in the next serving process
    ::unlink((o.root + "/inbox/" + id + ".ready").c_str());
    ::unlink((o.root + "/inbox/" + id + ".tar").c_str());
    finalize(status.substr(0, status.find('\n')).c_str(), true);
    std::_Exit(0);
}

}  // namespace

void run_command(int argc, char** argv, const Options& options) {
    const std::string cmd = argc > 0 ? argv[0] : "";
    if (cmd == "xorun" && argc >= 2) cmd_run(argv[1], options);
    if (cmd == "xopack" && argc >= 3) cmd_pack(argv[1], argv[2], options);
    if (cmd == "xoserve") cmd_serve(options);
    std::cerr << "usage: xorun ID | xopack ID REASON | xoserve\n";
    std::_Exit(2);
}

bool timed_run() noexcept { return g_quiet.load(std::memory_order_relaxed); }

Dashboard dashboard() {
    Dashboard d;
    std::string out;
    double seconds = 0;
    {
        std::lock_guard<std::mutex> lock(g_dash_mu);
        d = g_dash;
        out = g_current_out;
        if (!d.current.empty())
            seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - g_current_start).count();
    }
    d.current_seconds = seconds;
    if (!out.empty() && !d.quiet) demo_progress(out, d.live_snapshots, d.live_bytes);
    return d;
}

namespace {
struct ThreadArgs {
    int argc;
    char** argv;
    Options options;
};
void* thread_main(void* p) {
    auto* a = static_cast<ThreadArgs*>(p);
    run_command(a->argc, a->argv, a->options);
}
}  // namespace

void start_command_thread(int argc, char** argv, const Options& options) {
    auto* a = new ThreadArgs{argc, argv, options};
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setstacksize(&attr, std::size_t(64) << 20);
    pthread_t t;
    if (pthread_create(&t, &attr, thread_main, a) != 0) run_command(argc, argv, options);
    pthread_attr_destroy(&attr);
    pthread_detach(t);
}

}  // namespace xoffset_test_host
