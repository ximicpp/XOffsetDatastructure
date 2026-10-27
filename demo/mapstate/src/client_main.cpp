// Map state demo: client entry (window and --headless). OWNER: client thread.
#include <cstdio>
#include <exception>
#include <stdexcept>
#include <string>
#include "client_core.hpp"
#include "mapdemo/build_info.hpp"

int main(int argc, char** argv) {
    using namespace mapdemo::client;
    Options opt;
    try {
        opt = parse_options(argc, argv);
    } catch (const UsageError& e) {
        if (std::string_view(e.what()) == "help") {
            std::fputs(usage_text(), stdout);
            return 0;
        }
        std::fprintf(stderr, "error: %s\n%s", e.what(), usage_text());
        return 2;
    }
    try {
        const std::string build = mapdemo::build_info();
        std::fprintf(stdout, "client: build %s\n", build.c_str());
        std::fflush(stdout);
        if (!opt.csv_dir.empty()) {
            // Which compiler, platform and schema backend produced these results.
            std::FILE* f = std::fopen(join_path(opt.csv_dir, "client_build.txt").c_str(), "w");
            if (f == nullptr) throw std::runtime_error("cannot write client_build.txt in " + opt.csv_dir);
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
    } catch (const mapdemo::XBadAlloc& e) {
        std::fprintf(stderr, "error: capacity exceeded: %s\n", e.what());
        return 2;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "error: %s\n", e.what());
        return 2;
    }
}
