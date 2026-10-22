// Map state demo: client entry (window and --headless). OWNER: client thread.
#include <cstdio>
#include <exception>
#include "client_core.hpp"

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
