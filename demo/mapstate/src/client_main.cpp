// Map state demo: client entry (window and --headless). OWNER: client thread.
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <string>
#include <string_view>
#include "client_core.hpp"
#include "mapdemo/build_info.hpp"

namespace {

using namespace mapdemo;

// A failure the XOffset library cannot return in a build without exceptions
// (say an arena that runs out while a request is built) ends the run like
// an error that stops it: message on stderr, exit code 2. With exceptions
// the same failures arrive as exceptions and main() reports them.
[[noreturn]] void on_fatal(XErrc code, const char* message) noexcept {
    std::fflush(stdout);
    std::fprintf(stderr, "error: %s (%s)\n", message, to_string(code));
    std::fflush(stderr);
    std::_Exit(2);
}

Result<int> run(const client::Options& opt) {
    using namespace mapdemo::client;
    const std::string build = build_info();
    std::fprintf(stdout, "client: build %s\n", build.c_str());
    std::fflush(stdout);
    if (!opt.csv_dir.empty()) {
        // Which compiler, platform and schema backend produced these results.
        std::FILE* f = std::fopen(join_path(opt.csv_dir, "client_build.txt").c_str(), "w");
        if (f == nullptr) return Error{"cannot write client_build.txt in " + opt.csv_dir};
        std::fprintf(f, "%s\n", build.c_str());
        std::fclose(f);
    }
    if (opt.self_check) return run_self_check(opt);
    if (!opt.replay_dir.empty()) return run_replay(opt);
    if (opt.evil) return run_evil(opt);
    if (opt.headless) return run_headless(opt);
#if defined(MAPDEMO_WITH_GUI)
    return run_gui(opt);
#else
    std::fprintf(stderr, "error: built without raylib; only --headless and --evil are available\n");
    return 2;
#endif
}

int client_main(int argc, char** argv) {
    Result<client::Options> opt = client::parse_options(argc, argv);
    if (!opt) {
        std::fprintf(stderr, "error: %s\n%s", opt.error().message.c_str(), client::usage_text());
        return 2;
    }
    if (opt->help) {
        std::fputs(client::usage_text(), stdout);
        return 0;
    }
    Result<int> rc = run(*opt);
    if (!rc) {
        std::fprintf(stderr, "error: %s\n", rc.error().message.c_str());
        return 2;
    }
    return *rc;
}

} // namespace

int main(int argc, char** argv) {
    XOffsetDatastructure::set_fatal_handler(on_fatal);
#if MAPDEMO_EXCEPTIONS
    try {
        return client_main(argc, argv);
    } catch (const std::exception& e) {
        std::fprintf(stderr, "error: %s\n", e.what());
        return 2;
    }
#else
    return client_main(argc, argv);
#endif
}
