// Map state demo: server A (MAP_DEMO_DESIGN section 4.3). OWNER: server thread.
//
// A is the only owner of the unit state. Single thread, one poll() loop over
// the client listener, every client connection and the connection to B; all
// XOffset state is touched only on this thread, so publishing a new batch and
// reading the current one are serialised by construction (no locks).
//
//   mapdemo_a [--units 10000] [--seed 1] [--steps 0] [--duration-s 0]
//             [--host 127.0.0.1] [--port 47701] [--b-host 127.0.0.1] [--b-port 47702]
//             [--check | --no-check] [--buffers reuse|fresh] [--csv-dir DIR]
//             [--corpus-dir DIR [--corpus-max 60]]
//
// --host is the address A listens on for clients (0.0.0.0 inside a
// container whose port the host forwards). Snapshots go out in the encoding
// each ViewReq names (codec.hpp: dense, compact or compact + zstd).
// --corpus-dir keeps the first --corpus-max snapshot payloads exactly as
// sent, with their digests (corpus.csv), for mapdemo_client --replay on
// targets that cannot reach this server.
//
// Each step: save(cur) -> StepReq to B -> StepRes -> verify into spare ->
// (checks) -> spare becomes cur. At most one step is in flight; steps are
// paced at 20 Hz and never bunched up when A falls behind. View requests are
// answered from cur (the last completed tick) also while a step is in flight.
//   --buffers fresh: save_verified, load_verified into a new XBuffer, the old
//                    state, every request and every snapshot are destroyed.
//   --buffers reuse: save_verified_into a recycled string, reload_verified
//                    into the buffer that held the state before last
//                    (cur/spare ping-pong), one request buffer per client,
//                    one snapshot buffer refilled with reset<ViewSnapshot>().
//                    Same validation and byte-identical wires.
#include <csignal>
#include <cerrno>
#include <cinttypes>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include "mapdemo/buffer_mode.hpp"
#include "mapdemo/build_info.hpp"
#include "mapdemo/checks.hpp"
#include "mapdemo/codec.hpp"
#include "mapdemo/metrics.hpp"
#include "mapdemo/net.hpp"
#include "mapdemo/rules.hpp"
#include "mapdemo/schema.hpp"
#include "mapdemo/std_model.hpp"

namespace {

using namespace mapdemo;
using net::MsgType;
using net::ViewCategory;

constexpr std::uint64_t kStepPeriodNs = 50'000'000ull;   // 20 Hz (kStepSeconds)
constexpr std::uint64_t kByeFlushNs = 2'000'000'000ull;

volatile std::sig_atomic_t g_stop_signal = 0;
extern "C" void on_stop_signal(int) { g_stop_signal = 1; }

struct Options {
    std::uint32_t units = kDefaultUnits;
    std::uint64_t seed = kDefaultSeed;
    std::uint64_t steps = 0;        // 0 = unlimited
    double duration_s = 0;          // 0 = unlimited
    std::string host = "127.0.0.1";
    std::uint16_t port = net::kDefaultPortA;
    std::string b_host = "127.0.0.1";
    std::uint16_t b_port = net::kDefaultPortB;
    bool check = true;
    BufferMode buffers = kDefaultBufferMode;
    std::string csv_dir;
    std::string corpus_dir;
    std::uint64_t corpus_max = 60;
};

[[noreturn]] void usage_error(const std::string& what) {
    std::fprintf(stderr,
        "error: %s\n"
        "usage: mapdemo_a [--units 10000] [--seed 1] [--steps 0] [--duration-s 0]\n"
        "                 [--host 127.0.0.1] [--port 47701] [--b-host 127.0.0.1] [--b-port 47702]\n"
        "                 [--check | --no-check] [--buffers reuse|fresh] [--csv-dir DIR]\n"
        "                 [--corpus-dir DIR [--corpus-max 60]]\n", what.c_str());
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
        if (a == "--units") o.units = std::uint32_t(parse_u64("--units", value(), UINT32_MAX));
        else if (a == "--seed") o.seed = parse_u64("--seed", value(), UINT64_MAX);
        else if (a == "--steps") o.steps = parse_u64("--steps", value(), UINT64_MAX);
        else if (a == "--duration-s") {
            const char* t = value();
            char* end = nullptr;
            o.duration_s = std::strtod(t, &end);
            if (end == t || *end != '\0' || !std::isfinite(o.duration_s) || o.duration_s < 0)
                usage_error(std::string("bad value for --duration-s: ") + t);
        }
        else if (a == "--host") o.host = value();
        else if (a == "--port") o.port = std::uint16_t(parse_u64("--port", value(), 65535));
        else if (a == "--b-host") o.b_host = value();
        else if (a == "--b-port") o.b_port = std::uint16_t(parse_u64("--b-port", value(), 65535));
        else if (a == "--check") o.check = true;
        else if (a == "--no-check") o.check = false;
        else if (a == "--buffers") {
            const char* t = value();
            const auto m = parse_buffer_mode(t);
            if (!m) usage_error(std::string("bad value for --buffers: ") + t);
            o.buffers = *m;
        }
        else if (a == "--csv-dir") o.csv_dir = value();
        else if (a == "--corpus-dir") o.corpus_dir = value();
        else if (a == "--corpus-max") o.corpus_max = parse_u64("--corpus-max", value(), 100000);
        else usage_error(std::string("unknown argument ") + argv[i]);
    }
    return o;
}

// An explicit stop with an exit code (1 = check failed, 2 = protocol/IO/capacity).
struct Fatal {
    int code;
    std::string text;
};

const char* category_name(ViewCategory c) {
    switch (c) {
    case ViewCategory::Normal: return "normal";
    case ViewCategory::FullScene: return "full";
    case ViewCategory::Empty: return "empty";
    }
    return "?";
}

std::string hex16(std::uint64_t v) {
    char buf[17];
    std::snprintf(buf, sizeof buf, "%016" PRIx64, v);
    return std::string(buf, 16);
}

std::string fmt_us(double v) {
    char buf[32];
    std::snprintf(buf, sizeof buf, "%.3f", v);
    return buf;
}

std::string fmt_f(float v) {
    char buf[32];
    std::snprintf(buf, sizeof buf, "%.9g", double(v));
    return buf;
}

// One row of a_checks.csv.
struct CheckRow {
    std::string check;
    bool ok = true;
    std::string detail;
};

struct Client {
    std::uint64_t id = 0;
    net::Conn conn;
    bool hello_done = false;
    bool close_after_flush = false;
    XBuffer req_buf;          // reuse: reloaded for every ViewReq
    bool req_ready = false;
};

class ServerA {
public:
    explicit ServerA(Options opt) : opt_(std::move(opt)), rec_("a") {}
    int run();

private:
    // setup
    void open_csv();
    void build_catalog();
    void build_initial_batch();
    void connect_b();
    // step path
    bool may_send_step(std::uint64_t now) const;
    void send_step(std::uint64_t now);
    void on_step_result(net::Frame frame);
    // clients
    void accept_clients();
    void on_client_readable(Client& c);
    void on_client_frame(Client& c, net::Frame frame);
    void serve_view(Client& c, net::Frame frame);
    void reject(Client& c, const std::string& why, bool close_conn);
    void write_corpus(std::uint64_t request_id, std::uint16_t flags, const char* category, codec::Encoding enc,
                      const std::string& payload, std::size_t wire_bytes, std::uint64_t wire_digest,
                      std::uint64_t digest, std::size_t visible);
    // shutdown
    void finish();
    void flush_all_blocking(std::uint64_t budget_ns);
    void write_results();
    void fail_check(const std::string& check, const std::string& detail);

    UnitStateBatch& cur_root() { return cur_.root<UnitStateBatch>(); }
    bool reuse() const noexcept { return opt_.buffers == BufferMode::Reuse; }

    Options opt_;
    metrics::Recorder rec_;

    std::string catalog_wire_;
    XBuffer cur_;                       // last completed tick
    XBuffer spare_;                     // B's result is verified into this, then swapped with cur_
    bool spare_ready_ = false;          // reuse: spare_ holds an arena to reload into
    XBuffer snap_buf_;                  // reuse: refilled for every snapshot
    bool snap_ready_ = false;
    std::string compact_;               // zstd: the compact wire before compression
    codec::Compressor zstd_;
    std::vector<std::uint32_t> hits_;   // filter output, capacity kept
    std::unique_ptr<stdm::UnitStateBatch> ref_;   // reference model (--check)
    checks::Coverage coverage_;

    net::Listener listener_;
    net::Conn b_;
    std::vector<std::unique_ptr<Client>> clients_;
    std::uint64_t next_client_id_ = 1;

    bool inflight_ = false;
    std::uint64_t inflight_tick_ = 0;
    std::uint64_t send_start_ns_ = 0;
    double inflight_save_us_ = 0;
    std::size_t inflight_req_bytes_ = 0;
    std::uint64_t inflight_req_digest_ = 0;
    std::uint64_t steps_sent_ = 0;
    std::uint64_t steps_done_ = 0;
    std::uint64_t next_due_ns_ = 0;
    std::uint64_t first_send_ns_ = 0;
    std::uint64_t last_done_ns_ = 0;
    std::uint64_t deadline_ns_ = 0;     // 0 = none
    bool stopping_ = false;

    // checks and coverage counters
    std::vector<CheckRow> failures_;
    std::uint64_t step_checks_ = 0;
    std::uint64_t snap_checks_ = 0;
    std::uint64_t snaps_by_cat_[3] = {0, 0, 0};
    std::uint64_t snaps_by_enc_[codec::kEncodingCount] = {};
    std::uint64_t snaps_nonempty_ = 0;
    std::uint64_t snaps_zero_ = 0;
    std::uint64_t category_mismatch_ = 0;
    std::uint64_t rejected_ = 0;

    bool csv_ = false;
    metrics::CsvFile steps_csv_;
    metrics::CsvFile snaps_csv_;
    metrics::CsvFile corpus_csv_;
    std::uint64_t corpus_written_ = 0;
};

// ---------------------------------------------------------------- setup

void ServerA::open_csv() {
    if (opt_.csv_dir.empty()) return;
    csv_ = true;
    const std::string steps_path = opt_.csv_dir + "/a_steps.csv";
    const std::string snaps_path = opt_.csv_dir + "/a_snapshots.csv";
    if (!steps_csv_.open(steps_path,
            "tick,req_bytes,res_bytes,ab_rtt_us,save_us,load_us,destroy_us,free_blocks,free_bytes,check_ok,"
            "req_digest,res_digest"))
        throw Fatal{2, "cannot open " + steps_path};
    if (!snaps_csv_.open(snaps_path,
            "request_id,tick,category,min_x,min_y,max_x,max_y,visible,wire_bytes,digest,"
            "req_load_us,filter_us,build_us,save_us,destroy_us,check_ok,encoding,net_bytes,wire_digest,compress_us"))
        throw Fatal{2, "cannot open " + snaps_path};
}

void ServerA::build_catalog() {
    try {
        XBuffer cat = XBuffer::create<MapCatalog>(4096, cap_catalog());
        rules::build_catalog(cat.root<MapCatalog>(), opt_.seed);
        catalog_wire_ = cat.save_verified<MapCatalog>();
    } catch (const XBadAlloc& e) {
        throw Fatal{2, std::string("capacity: MapCatalog cap ") + std::to_string(kCapCatalog >> 20) +
                       " MiB exceeded (" + e.what() + ")"};
    }
}

void ServerA::build_initial_batch() {
    try {
        cur_ = XBuffer::create<UnitStateBatch>(4096 + std::size_t(opt_.units) * 128, cap_batch());
        rules::init_batch(cur_root(), opt_.units, opt_.seed);
        // The initial batch must also fit on the wire under the receive cap.
        (void)cur_.save_verified<UnitStateBatch>();
    } catch (const XBadAlloc& e) {
        throw Fatal{2, "capacity: --units " + std::to_string(opt_.units) + " exceeds the UnitStateBatch cap of " +
                       std::to_string(kCapBatch >> 20) + " MiB (" + e.what() + ")"};
    } catch (const std::length_error& e) {
        throw Fatal{2, "capacity: --units " + std::to_string(opt_.units) + " exceeds the UnitStateBatch cap of " +
                       std::to_string(kCapBatch >> 20) + " MiB (" + e.what() + ")"};
    }
    if (opt_.check) {
        ref_ = std::make_unique<stdm::UnitStateBatch>();
        rules::init_batch(*ref_, opt_.units, opt_.seed);
        metrics::Stopwatch sw;
        checks::Result inv = checks::batch_invariants(cur_root(), opt_.units, 0);
        if (!inv.ok) fail_check("batch_roundtrip", "initial batch: " + inv.detail);
        checks::Result cmp = checks::compare_with_reference(cur_root(), *ref_);
        if (!cmp.ok) fail_check("reference_model", "initial batch: " + cmp.detail);
        rec_.add("a.check_us", sw.us());
    }
}

void ServerA::connect_b() {
    b_ = net::Conn::connect(opt_.b_host, opt_.b_port);
    b_.send(MsgType::Hello, 0, net::hello_payload());
    net::Frame res = b_.recv({MsgType::HelloOk, MsgType::Error});
    if (res.type == MsgType::Error) throw Fatal{2, "hello: B rejected the handshake: " + res.payload};
    b_.set_nonblocking(true);
}

// ---------------------------------------------------------------- step path

bool ServerA::may_send_step(std::uint64_t now) const {
    if (inflight_ || stopping_) return false;
    if (opt_.steps != 0 && steps_sent_ >= opt_.steps) return false;
    return now >= next_due_ns_;
}

void ServerA::send_step(std::uint64_t now) {
    metrics::Stopwatch sw;
    std::string wire;
    if (reuse()) {
        // The previous StepReq was written completely before B answered it.
        wire = b_.take_tx_buffer();
        cur_.save_verified_into<UnitStateBatch>(wire);
    } else {
        wire = cur_.save_verified<UnitStateBatch>();
    }
    inflight_save_us_ = sw.us();
    rec_.add("a.save_us", inflight_save_us_);
    inflight_req_bytes_ = wire.size();
    rec_.add("a.step_req_bytes", double(wire.size()));
    inflight_req_digest_ = opt_.check ? checks::wire_digest(wire) : 0;

    inflight_tick_ = cur_root().tick;
    send_start_ns_ = net::now_ns();
    if (first_send_ns_ == 0) {
        first_send_ns_ = send_start_ns_;
        if (opt_.duration_s > 0) deadline_ns_ = first_send_ns_ + std::uint64_t(opt_.duration_s * 1e9);
    }
    b_.enqueue(MsgType::StepReq, inflight_tick_, std::move(wire));
    b_.flush();
    inflight_ = true;
    ++steps_sent_;
    // 20 Hz pacing; when behind, send the next step as soon as this one returns
    // instead of bunching up (the real ticks/s is reported).
    next_due_ns_ += kStepPeriodNs;
    if (next_due_ns_ < now) next_due_ns_ = now;
}

void ServerA::on_step_result(net::Frame frame) {
    const std::uint64_t done_ns = net::now_ns();
    if (frame.type == MsgType::Error) throw Fatal{2, "B reported: " + frame.payload};
    if (!inflight_) throw Fatal{2, "protocol: StepRes without a StepReq in flight"};
    const double rtt_us = double(done_ns - send_start_ns_) / 1e3;
    rec_.add("a.ab_rtt_us", rtt_us);
    const std::size_t res_bytes = frame.payload.size();
    rec_.add("a.step_res_bytes", double(res_bytes));
    const std::uint64_t expect = inflight_tick_ + 1;
    if (frame.correlation != expect)
        throw Fatal{2, "protocol: StepRes correlation " + std::to_string(frame.correlation) +
                       ", expected " + std::to_string(expect)};

    metrics::Stopwatch sw;
    try {
        if (reuse() && spare_ready_) {
            spare_.reload_verified<UnitStateBatch>(frame.payload);
        } else {
            spare_ = XBuffer::load_verified<UnitStateBatch>(frame.payload, cap_batch());
            spare_ready_ = true;
        }
    } catch (const std::exception& e) {
        throw Fatal{2, std::string("rejected StepRes from B: ") + e.what()};
    }
    const double load_us = sw.us();
    rec_.add("a.load_us", load_us);
    const std::uint64_t res_digest = opt_.check ? checks::wire_digest(frame.payload) : 0;

    bool check_ok = true;
    if (opt_.check) {
        sw.restart();
        auto& nb = spare_.root<UnitStateBatch>();
        rules::step_batch(*ref_, opt_.seed);
        checks::Result inv = checks::batch_invariants(nb, opt_.units, expect);
        if (!inv.ok) { check_ok = false; fail_check("batch_roundtrip", "tick " + std::to_string(expect) + ": " + inv.detail); }
        checks::Result cmp = checks::compare_with_reference(nb, *ref_);
        if (!cmp.ok) { check_ok = false; fail_check("reference_model", "tick " + std::to_string(expect) + ": " + cmp.detail); }
        coverage_.observe(cur_root(), nb);
        ++step_checks_;
        rec_.add("a.check_us", sw.us());
    } else if (spare_.root<UnitStateBatch>().tick != expect) {
        throw Fatal{2, "protocol: StepRes batch tick " + std::to_string(spare_.root<UnitStateBatch>().tick) +
                       ", expected " + std::to_string(expect)};
    }

    // Publish: the verified buffer becomes the current state.
    cur_.swap(spare_);
    double destroy_us = -1;
    if (reuse()) {
        // The old state stays in spare_ and is reloaded with the next result.
        b_.recycle_rx(std::move(frame.payload));
    } else {
        sw.restart();
        spare_ = XBuffer();
        spare_ready_ = false;
        std::string().swap(frame.payload);
        destroy_us = sw.us();
        rec_.add("a.destroy_us", destroy_us);
    }

    inflight_ = false;
    ++steps_done_;
    last_done_ns_ = done_ns;

    if (csv_) {
        char line[320];
        std::snprintf(line, sizeof line, "%" PRIu64 ",%zu,%zu,%s,%s,%s,%s,%zu,%" PRIu64 ",%d,%s,%s",
            expect, inflight_req_bytes_, res_bytes, fmt_us(rtt_us).c_str(), fmt_us(inflight_save_us_).c_str(),
            fmt_us(load_us).c_str(), destroy_us < 0 ? "" : fmt_us(destroy_us).c_str(), cur_.free_block_count(),
            std::uint64_t(cur_.free_block_bytes()), check_ok ? 1 : 0,
            opt_.check ? hex16(inflight_req_digest_).c_str() : "", opt_.check ? hex16(res_digest).c_str() : "");
        steps_csv_.row(line);
    }
    if (!check_ok) throw Fatal{1, "check failed at tick " + std::to_string(expect)};
}

// ---------------------------------------------------------------- clients

void ServerA::accept_clients() {
    for (;;) {
        net::PollItem p;
        p.fd = listener_.fd();
        p.want_read = true;
        if (net::poll(&p, 1, 0) <= 0 || !p.readable) return;
        auto c = std::make_unique<Client>();
        c->id = next_client_id_++;
        c->conn = listener_.accept();
        if (!c->conn.is_open()) return;
        c->conn.set_nonblocking(true);
        std::fprintf(stderr, "a: client %" PRIu64 " connected\n", c->id);
        clients_.push_back(std::move(c));
    }
}

void ServerA::reject(Client& c, const std::string& why, bool close_conn) {
    ++rejected_;
    std::fprintf(stderr, "rejected: client %" PRIu64 ": %s\n", c.id, why.c_str());
    if (close_conn) {
        c.conn.close();
        return;
    }
    c.conn.enqueue(MsgType::Error, 0, why.substr(0, net::max_payload(MsgType::Error)));
    c.conn.flush();
}

// Corpus for mapdemo_client --replay: the payload exactly as sent, plus what
// the client must get from it (OUT/corpus.csv). Not on a timed path.
void ServerA::write_corpus(std::uint64_t request_id, std::uint16_t flags, const char* category, codec::Encoding enc,
                           const std::string& payload, std::size_t wire_bytes, std::uint64_t wire_digest,
                           std::uint64_t digest, std::size_t visible) {
    if (corpus_written_ == 0 &&
        !corpus_csv_.open(opt_.corpus_dir + "/corpus.csv",
                          "request_id,flags,category,encoding,file,net_bytes,wire_bytes,wire_digest,digest,visible"))
        throw Fatal{2, "cannot write " + opt_.corpus_dir + "/corpus.csv"};
    const std::string file = "snap_" + std::to_string(request_id) + "." + codec::encoding_name(enc);
    std::FILE* f = std::fopen((opt_.corpus_dir + "/" + file).c_str(), "wb");
    const bool ok = f != nullptr && std::fwrite(payload.data(), 1, payload.size(), f) == payload.size();
    if (f != nullptr && std::fclose(f) != 0) throw Fatal{2, "cannot write " + opt_.corpus_dir + "/" + file};
    if (!ok) throw Fatal{2, "cannot write " + opt_.corpus_dir + "/" + file};
    corpus_csv_.row(std::to_string(request_id) + "," + std::to_string(flags) + "," + category + "," +
                    codec::encoding_name(enc) + "," + file + "," + std::to_string(payload.size()) + "," +
                    std::to_string(wire_bytes) + "," + hex16(wire_digest) + "," + hex16(digest) + "," +
                    std::to_string(visible));
    ++corpus_written_;
}

void ServerA::on_client_readable(Client& c) {
    for (;;) {
        std::optional<net::Frame> f;
        try {
            f = c.conn.try_recv({MsgType::Hello, MsgType::ViewReq});
        } catch (const net::PeerClosed&) {
            std::fprintf(stderr, "a: client %" PRIu64 " disconnected\n", c.id);
            c.conn.close();
            return;
        } catch (const net::ProtocolError& e) {
            // Bad header, unexpected type, oversize declared length, or a
            // close mid-frame: the payload buffer was never allocated; drop
            // this connection.
            reject(c, e.what(), true);
            return;
        }
        if (!f) return;
        on_client_frame(c, std::move(*f));
        if (!c.conn.is_open() || c.close_after_flush) return;
    }
}

void ServerA::on_client_frame(Client& c, net::Frame frame) {
    if (frame.type == MsgType::Hello) {
        if (c.hello_done) { reject(c, "second Hello", true); return; }
        if (!net::hello_matches(frame.payload)) {
            reject(c, "hello: schema hash mismatch", false);
            c.close_after_flush = true;
            return;
        }
        c.hello_done = true;
        c.conn.enqueue(MsgType::HelloOk, 0, std::string());
        c.conn.enqueue(MsgType::Catalog, 0, catalog_wire_);
        c.conn.flush();
        return;
    }
    if (!c.hello_done) { reject(c, "ViewReq before Hello", true); return; }
    serve_view(c, std::move(frame));
}

void ServerA::serve_view(Client& c, net::Frame frame) {
    const std::optional<codec::ViewFlags> vf = codec::parse_view_flags(frame.flags);
    if (!vf) {
        reject(c, "ViewReq: unknown flags " + std::to_string(frame.flags), false);
        return;
    }
    if (vf->encoding == codec::Encoding::Zstd && !codec::zstd_available()) {
        reject(c, "ViewReq: encoding zstd is not available (built without zstd)", false);
        return;
    }
    const char* cat = category_name(vf->category);
    const codec::Encoding enc = vf->encoding;

    metrics::Stopwatch sw;
    try {
        if (reuse() && c.req_ready) {
            // A rejected wire leaves the buffer empty; the next one reloads it.
            c.req_buf.reload_verified<ViewRequest>(frame.payload);
        } else {
            c.req_buf = XBuffer::load_verified<ViewRequest>(frame.payload, cap_view_request());
            c.req_ready = true;
        }
    } catch (const std::exception& e) {
        reject(c, std::string("ViewReq: bad wire: ") + e.what(), false);
        return;
    }
    const double req_load_us = sw.us();
    const ViewRequest& req = c.req_buf.root<ViewRequest>();
    if (req.request_id != frame.correlation) {
        reject(c, "ViewReq: request_id " + std::to_string(req.request_id) + " does not match correlation " +
                  std::to_string(frame.correlation), false);
        return;
    }
    if (req.scene_id != kSceneId || !rules::rect_is_legal(req.rect, scene_bounds())) {
        reject(c, "ViewReq: illegal rect or scene", false);
        return;
    }
    rec_.add("a.req_load_us", req_load_us);

    // Filter: plain scan of the current batch (no spatial index, design 4.3).
    sw.restart();
    const auto& units = cur_root().units;
    std::vector<std::uint32_t>& hits = hits_;
    hits.clear();
    const std::uint32_t n = std::uint32_t(units.size());
    for (std::uint32_t i = 0; i < n; ++i) {
        const auto& u = units[i];
        if (u.scene_id == req.scene_id && rules::contains(req.rect, u.pos)) hits.push_back(i);
    }
    const double filter_us = sw.us();
    rec_.add("a.filter_us", filter_us);

    // Build the snapshot in its own arena from the public fields only.
    sw.restart();
    XBuffer& snap_buf = snap_buf_;
    try {
        if (reuse() && snap_ready_) {
            snap_buf.reset<ViewSnapshot>();
        } else {
            snap_buf = XBuffer::create<ViewSnapshot>(4096 + hits.size() * 64, cap_snapshot());
            snap_ready_ = true;
        }
        auto& s = snap_buf.root<ViewSnapshot>();
        s.request_id = req.request_id;
        s.tick = cur_root().tick;
        s.scene_id = req.scene_id;
        s.rect = req.rect;
        s.units.reserve(std::uint32_t(hits.size()));
        for (std::uint32_t i : hits) {
            const auto& u = units[i];
            auto& v = s.units.emplace_back();
            v.id = u.id;
            v.hp = u.hp;
            v.pos = u.pos;
            v.behavior = u.behavior;
            v.name = u.name.view();
        }
    } catch (const XBadAlloc& e) {
        // Never drop units to fit: report the cap explicitly.
        reject(c, std::string("capacity: ViewSnapshot cap ") + std::to_string(kCapSnapshot >> 20) +
                  " MiB exceeded for " + std::to_string(hits.size()) + " units (" + e.what() + ")", false);
        std::fprintf(stderr, "error: capacity: ViewSnapshot cap %zu MiB exceeded\n", kCapSnapshot >> 20);
        return;
    }
    const double build_us = sw.us();
    rec_.add("a.snap_build_us", build_us);

    // Save in the requested encoding. wire = the frame payload; xwire = the
    // XOffset wire the client will verify (the payload itself, or for zstd
    // the compact wire before compression).
    sw.restart();
    std::string wire = reuse() ? c.conn.take_tx_buffer() : std::string();
    std::string& xwire = enc == codec::Encoding::Zstd ? compact_ : wire;
    const XCompactOptions canonical{0};   // capacity == size: the smallest wire
    if (enc == codec::Encoding::Dense) {
        if (reuse()) snap_buf.save_verified_into<ViewSnapshot>(xwire);
        else xwire = snap_buf.save_verified<ViewSnapshot>();
    } else {
        if (reuse()) snap_buf.save_compact_into<ViewSnapshot>(xwire, canonical);
        else xwire = snap_buf.save_compact<ViewSnapshot>(canonical);
    }
    const double save_us = sw.us();
    double compress_us = -1;
    if (enc == codec::Encoding::Zstd) {
        sw.restart();
        zstd_.compress(xwire, wire);
        compress_us = sw.us();
        rec_.add("a.snap_compress_us", compress_us);
    }
    rec_.add("a.snap_save_us", save_us);
    const std::size_t wire_bytes = xwire.size();
    const std::size_t net_bytes = wire.size();
    const std::uint64_t xwire_digest = checks::wire_digest(xwire);
    rec_.add("a.snap_bytes", double(wire_bytes));
    rec_.add("a.snap_net_bytes", double(net_bytes));
    rec_.add("a.visible_count", double(hits.size()));
    ++snaps_by_enc_[static_cast<int>(enc)];

    if (!opt_.corpus_dir.empty() && corpus_written_ < opt_.corpus_max)
        write_corpus(req.request_id, frame.flags, cat, enc, wire, wire_bytes, xwire_digest,
                     checks::snapshot_digest(snap_buf.root<ViewSnapshot>()), hits.size());

    c.conn.enqueue(MsgType::ViewSnapshot, req.request_id, std::move(wire), frame.flags);
    c.conn.flush();

    // Checks and digest stay outside the timed regions.
    const ViewSnapshot& snap = snap_buf.root<ViewSnapshot>();
    const std::uint64_t digest = checks::snapshot_digest(snap);
    bool check_ok = true;
    if (opt_.check) {
        metrics::Stopwatch cw;
        checks::Result r = checks::compare_snapshot(snap, req, *ref_);
        if (!r.ok) {
            check_ok = false;
            fail_check("view_snapshot", "request " + std::to_string(req.request_id) + ": " + r.detail);
        }
        ++snap_checks_;
        rec_.add("a.check_us", cw.us());
    }
    ++snaps_by_cat_[static_cast<std::size_t>(vf->category)];
    if (hits.empty()) ++snaps_zero_; else ++snaps_nonempty_;
    const ViewCategory vc = vf->category;
    if ((vc == ViewCategory::Empty && !hits.empty()) ||
        (vc == ViewCategory::FullScene && hits.size() != std::size_t(opt_.units))) {
        ++category_mismatch_;
        if (opt_.check) {
            check_ok = false;
            fail_check("snapshot_paths", "request " + std::to_string(req.request_id) + ": category " + cat +
                       " got " + std::to_string(hits.size()) + " units");
        }
    }

    const std::uint64_t request_id = req.request_id;
    const std::uint64_t tick = snap.tick;
    const Rect rect = req.rect;
    const std::size_t visible = hits.size();

    double destroy_us = -1;
    if (reuse()) {
        c.conn.recycle_rx(std::move(frame.payload));
    } else {
        sw.restart();
        snap_buf = XBuffer();
        snap_ready_ = false;
        c.req_buf = XBuffer();
        c.req_ready = false;
        destroy_us = sw.us();
        rec_.add("a.snap_destroy_us", destroy_us);
    }

    if (csv_) {
        std::string row;
        row.reserve(256);
        row += std::to_string(request_id); row += ',';
        row += std::to_string(tick); row += ',';
        row += cat; row += ',';
        row += fmt_f(rect.min.x); row += ',';
        row += fmt_f(rect.min.y); row += ',';
        row += fmt_f(rect.max.x); row += ',';
        row += fmt_f(rect.max.y); row += ',';
        row += std::to_string(visible); row += ',';
        row += std::to_string(wire_bytes); row += ',';
        row += hex16(digest); row += ',';
        row += fmt_us(req_load_us); row += ',';
        row += fmt_us(filter_us); row += ',';
        row += fmt_us(build_us); row += ',';
        row += fmt_us(save_us); row += ',';
        if (destroy_us >= 0) row += fmt_us(destroy_us);
        row += ',';
        row += check_ok ? '1' : '0';
        row += ',';
        row += codec::encoding_name(enc); row += ',';
        row += std::to_string(net_bytes); row += ',';
        row += hex16(xwire_digest); row += ',';
        if (compress_us >= 0) row += fmt_us(compress_us);
        snaps_csv_.row(row);
    }
    if (!check_ok) throw Fatal{1, "snapshot check failed for request " + std::to_string(request_id)};
}

// ---------------------------------------------------------------- shutdown

void ServerA::fail_check(const std::string& check, const std::string& detail) {
    std::fprintf(stderr, "error: check %s failed: %s\n", check.c_str(), detail.c_str());
    failures_.push_back(CheckRow{check, false, detail});
}

void ServerA::flush_all_blocking(std::uint64_t budget_ns) {
    const std::uint64_t until = net::now_ns() + budget_ns;
    for (;;) {
        std::vector<net::PollItem> fds;
        auto add = [&](net::Conn& c) {
            if (!c.is_open()) return;
            bool done = false;
            try { done = c.flush(); } catch (const std::exception&) { c.close(); return; }
            if (!done) {
                net::PollItem p;
                p.fd = c.fd();
                p.want_write = true;
                fds.push_back(p);
            }
        };
        add(b_);
        for (auto& c : clients_) add(c->conn);
        if (fds.empty()) return;
        const std::uint64_t now = net::now_ns();
        if (now >= until) return;
        net::poll(fds.data(), fds.size(), int((until - now) / 1'000'000ull) + 1);
    }
}

void ServerA::finish() {
    if (b_.is_open()) b_.enqueue(MsgType::Bye, 0, std::string());
    for (auto& c : clients_)
        if (c->conn.is_open() && c->hello_done) c->conn.enqueue(MsgType::Bye, 0, std::string());
    flush_all_blocking(kByeFlushNs);
}

void ServerA::write_results() {
    const std::uint64_t end_ns = last_done_ns_ != 0 ? last_done_ns_ : net::now_ns();
    const double elapsed_s = first_send_ns_ != 0 ? double(end_ns - first_send_ns_) / 1e9 : 0.0;
    const double tps = elapsed_s > 0 ? double(steps_done_) / elapsed_s : 0.0;
    rec_.add("a.units_count", double(opt_.units));
    rec_.add("a.ticks_per_s", tps);
    std::fprintf(stderr, "a: done, ticks=%" PRIu64 " in %.2f s (%.2f ticks/s), snapshots=%" PRIu64
                 " (normal %" PRIu64 ", full %" PRIu64 ", empty %" PRIu64 "), rejected=%" PRIu64 "\n",
                 steps_done_, elapsed_s, tps, snaps_by_cat_[0] + snaps_by_cat_[1] + snaps_by_cat_[2],
                 snaps_by_cat_[0], snaps_by_cat_[1], snaps_by_cat_[2], rejected_);

    std::vector<CheckRow> rows;
    if (opt_.check) {
        auto failed = [&](const char* name) -> const CheckRow* {
            for (const auto& f : failures_) if (f.check == name) return &f;
            return nullptr;
        };
        auto add = [&](const char* name, bool extra_ok, const std::string& detail) {
            const CheckRow* f = failed(name);
            rows.push_back(CheckRow{name, f == nullptr && extra_ok, f ? f->detail : detail});
        };
        const bool steps_ok = opt_.steps == 0 || steps_done_ == opt_.steps;
        add("batch_roundtrip", steps_ok && steps_done_ > 0,
            "units=" + std::to_string(opt_.units) + " ticks=" + std::to_string(steps_done_) +
            " checked=" + std::to_string(step_checks_) +
            (steps_ok ? "" : " (stopped before --steps)"));
        add("reference_model", step_checks_ > 0, "compared=" + std::to_string(step_checks_) + " batches bitwise");
        {
            const auto& cv = coverage_;
            const std::string detail =
                "name_to_long=" + std::to_string(cv.name_to_long) + " name_to_short=" + std::to_string(cv.name_to_short) +
                " path_grow=" + std::to_string(cv.path_grow) + " path_shrink=" + std::to_string(cv.path_shrink) +
                " hp_changes=" + std::to_string(cv.hp_changes) + " behavior_changes=" + std::to_string(cv.behavior_changes) +
                " grow_shrink_grow=" + (cv.grow_shrink_grow ? "1" : "0");
            rows.push_back(CheckRow{"modification_coverage", cv.complete(), detail});
        }
        add("view_snapshot", true,
            "compared=" + std::to_string(snap_checks_) + " normal=" + std::to_string(snaps_by_cat_[0]) +
            " full=" + std::to_string(snaps_by_cat_[1]) + " empty=" + std::to_string(snaps_by_cat_[2]) +
            " nonempty=" + std::to_string(snaps_nonempty_) + " zero_visible=" + std::to_string(snaps_zero_) +
            " dense=" + std::to_string(snaps_by_enc_[0]) + " compact=" + std::to_string(snaps_by_enc_[1]) +
            " zstd=" + std::to_string(snaps_by_enc_[2]));
        // Each served category hit its expected path: Empty -> zero units,
        // FullScene -> every unit. Whether a run covered both the non-empty
        // and the zero-visible path is judged per tier by tools/summarize.py
        // (the --evil run legitimately serves only a few snapshots).
        rows.push_back(CheckRow{"snapshot_paths", category_mismatch_ == 0,
            "nonempty=" + std::to_string(snaps_nonempty_) + " zero_visible=" + std::to_string(snaps_zero_) +
            " full=" + std::to_string(snaps_by_cat_[1]) + " empty=" + std::to_string(snaps_by_cat_[2]) +
            " category_mismatch=" + std::to_string(category_mismatch_)});
    }
    rows.push_back(CheckRow{"rejected_requests", true, "count=" + std::to_string(rejected_)});

    for (const auto& r : rows)
        if (!r.ok) std::fprintf(stderr, "error: check %s: %s\n", r.check.c_str(), r.detail.c_str());

    if (opt_.csv_dir.empty()) return;
    if (!rec_.write_csv(opt_.csv_dir + "/a_metrics.csv"))
        throw Fatal{2, "cannot write " + opt_.csv_dir + "/a_metrics.csv"};
    metrics::CsvFile checks_csv;
    if (!checks_csv.open(opt_.csv_dir + "/a_checks.csv", "check,result,detail"))
        throw Fatal{2, "cannot write " + opt_.csv_dir + "/a_checks.csv"};
    for (const auto& r : rows) {
        std::string detail = r.detail;
        for (char& ch : detail) if (ch == ',' || ch == '\n') ch = ';';
        checks_csv.row(r.check + "," + (r.ok ? "pass" : "fail") + "," + detail);
    }
}

// ---------------------------------------------------------------- main loop

int ServerA::run() {
    open_csv();
    build_catalog();
    build_initial_batch();
    if (!failures_.empty()) throw Fatal{1, "initial batch failed its checks"};

    listener_ = net::Listener::bind(opt_.host, opt_.port);
    std::fprintf(stderr, "a: listening on %s:%u, units=%u seed=%" PRIu64 " check=%s buffers=%s\na: build %s\n",
                 opt_.host.c_str(), unsigned(opt_.port), opt_.units, opt_.seed, opt_.check ? "on" : "off",
                 buffer_mode_name(opt_.buffers), build_info().c_str());
    connect_b();
    std::fprintf(stderr, "a: connected to B at %s:%u\n", opt_.b_host.c_str(), unsigned(opt_.b_port));

    next_due_ns_ = net::now_ns();
    std::uint64_t last_report = next_due_ns_;
    int exit_code = 0;
    try {
        for (;;) {
            const std::uint64_t now = net::now_ns();
            if (g_stop_signal) stopping_ = true;
            if (deadline_ns_ != 0 && now >= deadline_ns_) stopping_ = true;
            if (opt_.steps != 0 && steps_done_ >= opt_.steps) stopping_ = true;
            if (stopping_ && !inflight_) break;

            if (may_send_step(now)) send_step(now);

            // poll set: listener, B, clients
            auto item = [](net::Socket fd, bool write) {
                net::PollItem p;
                p.fd = fd;
                p.want_read = true;
                p.want_write = write;
                return p;
            };
            std::vector<net::PollItem> fds;
            fds.push_back(item(listener_.fd(), false));
            fds.push_back(item(b_.fd(), b_.want_write()));
            for (auto& c : clients_) fds.push_back(item(c->conn.fd(), c->conn.want_write()));

            int timeout_ms = 1000;
            const std::uint64_t now2 = net::now_ns();
            if (!inflight_ && !stopping_ && (opt_.steps == 0 || steps_sent_ < opt_.steps))
                timeout_ms = next_due_ns_ > now2 ? int((next_due_ns_ - now2 + 999'999) / 1'000'000) : 0;
            if (deadline_ns_ != 0) {
                const int to_deadline = deadline_ns_ > now2 ? int((deadline_ns_ - now2 + 999'999) / 1'000'000) : 0;
                if (to_deadline < timeout_ms) timeout_ms = to_deadline;
            }
            net::poll(fds.data(), fds.size(), timeout_ms);

            // B
            if (fds[1].writable) b_.flush();
            if (fds[1].readable) {
                for (;;) {
                    std::optional<net::Frame> f = b_.try_recv({MsgType::StepRes, MsgType::Error});
                    if (!f) break;
                    on_step_result(std::move(*f));
                }
            }

            // clients (indices shift only after the sweep below)
            for (std::size_t i = 0; i < clients_.size(); ++i) {
                Client& c = *clients_[i];
                const net::PollItem& ev = fds[2 + i];
                if (!c.conn.is_open()) continue;
                if (ev.writable) {
                    try { c.conn.flush(); } catch (const std::exception&) { c.conn.close(); continue; }
                }
                if (c.close_after_flush) {
                    if (!c.conn.want_write()) c.conn.close();
                    continue;
                }
                if (ev.readable) on_client_readable(c);
            }
            std::erase_if(clients_, [](const std::unique_ptr<Client>& c) { return !c->conn.is_open(); });

            if (fds[0].readable) accept_clients();

            const std::uint64_t t = net::now_ns();
            if (t - last_report >= 1'000'000'000ull) {
                last_report = t;
                std::fprintf(stderr, "a: tick=%" PRIu64 " clients=%zu ", steps_done_, clients_.size());
                rec_.print_line(stderr, {"a.ab_rtt_us", "a.save_us", "a.load_us", "a.step_res_bytes",
                                         "a.snap_build_us", "a.visible_count"});
            }
        }
    } catch (const Fatal& f) {
        std::fprintf(stderr, "error: %s\n", f.text.c_str());
        exit_code = f.code;
    } catch (const XBadAlloc& e) {
        std::fprintf(stderr, "error: capacity: %s\n", e.what());
        exit_code = 2;
    } catch (const net::ProtocolError& e) {
        std::fprintf(stderr, "error: protocol: %s\n", e.what());
        exit_code = 2;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "error: %s\n", e.what());
        exit_code = 2;
    }

    finish();
    write_results();
    if (exit_code != 0) return exit_code;
    if (!failures_.empty()) return 1;
    if (opt_.check) {
        // Coverage and snapshot-path checks are judged at the end of the run.
        if (!coverage_.complete()) return 1;
        if (opt_.steps != 0 && steps_done_ != opt_.steps) return 1;
    }
    return 0;
}

} // namespace

int main(int argc, char** argv) {
    Options opt = parse(argc, argv);
    std::signal(SIGINT, on_stop_signal);
    std::signal(SIGTERM, on_stop_signal);
#ifdef SIGPIPE
    std::signal(SIGPIPE, SIG_IGN);
#endif
    try {
        ServerA a(std::move(opt));
        return a.run();
    } catch (const Fatal& f) {
        std::fprintf(stderr, "error: %s\n", f.text.c_str());
        return f.code;
    } catch (const XBadAlloc& e) {
        std::fprintf(stderr, "error: capacity: %s\n", e.what());
    } catch (const net::ProtocolError& e) {
        std::fprintf(stderr, "error: protocol: %s\n", e.what());
    } catch (const std::exception& e) {
        std::fprintf(stderr, "error: %s\n", e.what());
    }
    return 2;
}
