// Map state demo: server B (MAP_DEMO_DESIGN section 4.2). OWNER: server thread.
//
// Blocking single-connection loop. B owns no state across steps: every
// StepReq is verified into an arena, advanced one step in place inside that
// arena, saved and returned. B never advances anything on its own.
//   --buffers fresh: load_verified into a new XBuffer, save_verified, destroy.
//   --buffers reuse: reload_verified into the one buffer B keeps,
//                    save_verified_into a kept string; payload strings are
//                    handed back to the connection. Same bytes either way.
//
//   mapdemo_b [--port 47702] [--seed 1] [--buffers reuse|fresh] [--csv-dir DIR]
//
// --seed must match A's seed (the rules depend on it); A's reference model
// reports any mismatch as a failed check.
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <string>
#include <string_view>
#include "mapdemo/buffer_mode.hpp"
#include "mapdemo/build_info.hpp"
#include "mapdemo/metrics.hpp"
#include "mapdemo/net.hpp"
#include "mapdemo/rules.hpp"
#include "mapdemo/schema.hpp"

namespace {

using namespace mapdemo;
using net::MsgType;

struct Options {
    std::uint16_t port = net::kDefaultPortB;
    std::uint64_t seed = kDefaultSeed;
    BufferMode buffers = kDefaultBufferMode;
    std::string csv_dir;
};

[[noreturn]] void usage_error(const std::string& what) {
    std::fprintf(stderr, "error: %s\nusage: mapdemo_b [--port 47702] [--seed 1] [--buffers reuse|fresh] [--csv-dir DIR]\n", what.c_str());
    std::exit(2);
}

std::uint64_t parse_u64(const char* flag, const char* text, std::uint64_t max) {
    char* end = nullptr;
    errno = 0;
    const unsigned long long v = std::strtoull(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0' || text[0] == '-' || v > max)
        usage_error(std::string("bad value for ") + flag + ": " + text);
    return v;
}

Options parse(int argc, char** argv) {
    Options o;
    for (int i = 1; i < argc; ++i) {
        const std::string_view a = argv[i];
        auto value = [&]() -> const char* {
            if (i + 1 >= argc) usage_error(std::string("missing value for ") + argv[i]);
            return argv[++i];
        };
        if (a == "--port") o.port = std::uint16_t(parse_u64("--port", value(), 65535));
        else if (a == "--seed") o.seed = parse_u64("--seed", value(), UINT64_MAX);
        else if (a == "--buffers") {
            const char* t = value();
            const auto m = parse_buffer_mode(t);
            if (!m) usage_error(std::string("bad value for --buffers: ") + t);
            o.buffers = *m;
        }
        else if (a == "--csv-dir") o.csv_dir = value();
        else usage_error(std::string("unknown argument ") + argv[i]);
    }
    return o;
}

// Best effort: tell A why B stops before exiting.
void send_error(net::Conn& conn, const std::string& text) {
    if (!conn.is_open()) return;
    try {
        conn.send(MsgType::Error, 0, std::string_view(text).substr(0, net::max_payload(MsgType::Error)));
    } catch (const std::exception&) {
    }
}

int run(const Options& opt) {
    metrics::Recorder rec("b");
    net::Listener listener = net::Listener::bind("127.0.0.1", opt.port);
    std::fprintf(stderr, "b: listening on 127.0.0.1:%u\nb: build %s\n", unsigned(opt.port), build_info().c_str());
    net::Conn conn = listener.accept();

    {
        net::Frame hello = conn.recv({MsgType::Hello});
        if (!net::hello_matches(hello.payload)) {
            send_error(conn, "schema hash mismatch");
            std::fprintf(stderr, "error: hello: schema hash mismatch with A\n");
            return 2;
        }
        conn.send(MsgType::HelloOk, 0, {});
    }
    std::fprintf(stderr, "b: A connected, buffers=%s\n", buffer_mode_name(opt.buffers));

    const bool reuse = opt.buffers == BufferMode::Reuse;
    XBuffer kept;          // reuse: the one arena every step is reloaded into
    bool kept_ready = false;
    std::string out;       // reuse: the output string, capacity kept
    std::uint64_t steps = 0;
    std::uint64_t last_report = net::now_ns();
    int exit_code = 0;
    for (;;) {
        net::Frame req = conn.recv({MsgType::StepReq, MsgType::Bye, MsgType::Error});
        if (req.type == MsgType::Bye) break;
        if (req.type == MsgType::Error) {
            std::fprintf(stderr, "error: A reported: %s\n", req.payload.c_str());
            exit_code = 2;
            break;
        }
        rec.add("b.in_bytes", double(req.payload.size()));

        metrics::Stopwatch sw;
        XBuffer fresh;
        XBuffer& buf = reuse ? kept : fresh;
        try {
            if (reuse && kept_ready) {
                kept.reload_verified<UnitStateBatch>(req.payload);
            } else {
                buf = XBuffer::load_verified<UnitStateBatch>(req.payload, cap_batch());
                kept_ready = reuse;
            }
        } catch (const std::exception& e) {
            const std::string text = std::string("rejected StepReq: ") + e.what();
            send_error(conn, text);
            std::fprintf(stderr, "error: %s\n", text.c_str());
            return 2;
        }
        rec.add("b.load_us", sw.us());

        auto& batch = buf.root<UnitStateBatch>();
        if (batch.tick != req.correlation) {
            const std::string text = "StepReq correlation " + std::to_string(req.correlation) +
                                     " does not match batch tick " + std::to_string(batch.tick);
            send_error(conn, text);
            std::fprintf(stderr, "error: protocol: %s\n", text.c_str());
            return 2;
        }

        sw.restart();
        try {
            rules::step_batch(batch, opt.seed);
        } catch (const XBadAlloc& e) {
            const std::string text = std::string("capacity: UnitStateBatch cap ") +
                                     std::to_string(kCapBatch >> 20) + " MiB exceeded while stepping (" + e.what() + ")";
            send_error(conn, text);
            std::fprintf(stderr, "error: %s\n", text.c_str());
            return 2;
        }
        rec.add("b.modify_us", sw.us());

        sw.restart();
        if (reuse) buf.save_verified_into<UnitStateBatch>(out);
        else out = buf.save_verified<UnitStateBatch>();
        rec.add("b.save_us", sw.us());
        rec.add("b.out_bytes", double(out.size()));

        sw.restart();
        conn.send(MsgType::StepRes, batch.tick, out);
        rec.add("b.send_us", sw.us());

        if (reuse) {
            // Nothing is destroyed: the next StepReq is received into this
            // payload string and reloaded into `kept`.
            conn.recycle_rx(std::move(req.payload));
        } else {
            sw.restart();
            fresh = XBuffer();
            std::string().swap(req.payload);
            std::string().swap(out);
            rec.add("b.destroy_us", sw.us());
        }

        ++steps;
        const std::uint64_t now = net::now_ns();
        if (now - last_report >= 1'000'000'000ull) {
            last_report = now;
            std::fprintf(stderr, "b: steps=%llu ", static_cast<unsigned long long>(steps));
            rec.print_line(stderr, {"b.load_us", "b.modify_us", "b.save_us", "b.send_us", "b.in_bytes"});
        }
    }

    std::fprintf(stderr, "b: done, %llu steps\n", static_cast<unsigned long long>(steps));
    if (!opt.csv_dir.empty() && !rec.write_csv(opt.csv_dir + "/b_metrics.csv")) {
        std::fprintf(stderr, "error: cannot write %s/b_metrics.csv\n", opt.csv_dir.c_str());
        return 2;
    }
    return exit_code;
}

} // namespace

int main(int argc, char** argv) {
    const Options opt = parse(argc, argv);
    try {
        return run(opt);
    } catch (const XBadAlloc& e) {
        std::fprintf(stderr, "error: capacity: %s\n", e.what());
    } catch (const net::ProtocolError& e) {
        std::fprintf(stderr, "error: protocol: %s\n", e.what());
    } catch (const std::exception& e) {
        std::fprintf(stderr, "error: %s\n", e.what());
    }
    return 2;
}
