// Map state demo: client network, SnapshotHolder, sampling script. OWNER: client thread.
#include "client_core.hpp"

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cinttypes>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iterator>
#include <span>
#include <thread>
#include <utility>

namespace mapdemo::client {

// ---- options -----------------------------------------------------------------

const char* usage_text() {
    return "usage: mapdemo_client [--host 127.0.0.1] [--port 47701] [--headless] [--check | --no-check]\n"
           "                      [--buffers reuse|fresh] [--encoding zstd|compact|dense|cycle]\n"
           "                      [--csv-dir DIR] [--auto-drag] [--screenshot-at F1,F2,...] [--evil]\n"
           "       mapdemo_client --self-check [--csv-dir DIR]\n"
           "       mapdemo_client --replay CORPUS_DIR [--buffers reuse|fresh] [--csv-dir DIR]\n";
}

static Error bad_value(std::string_view flag, const char* text) {
    return Error{std::string(flag) + ": bad value '" + text + "'"};
}

static std::optional<long> parse_long(const char* text, long lo, long hi) {
    char* end = nullptr;
    errno = 0;
    long v = std::strtol(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0' || v < lo || v > hi) return std::nullopt;
    return v;
}

Result<Options> parse_options(int argc, char** argv) {
    static constexpr std::string_view kWithValue[] = {"--host",     "--port",     "--csv-dir",      "--replay",
                                                      "--encoding", "--buffers", "--screenshot-at"};
    Options o;
    for (int i = 1; i < argc; ++i) {
        std::string_view a = argv[i];
        const char* v = nullptr;
        if (std::find(std::begin(kWithValue), std::end(kWithValue), a) != std::end(kWithValue)) {
            if (i + 1 >= argc) return Error{std::string(a) + " needs a value"};
            v = argv[++i];
        }
        if (a == "--host") o.host = v;
        else if (a == "--port") {
            const auto port = parse_long(v, 1, 65535);
            if (!port) return bad_value(a, v);
            o.port = static_cast<std::uint16_t>(*port);
        }
        else if (a == "--headless") o.headless = true;
        else if (a == "--check") o.check = true;
        else if (a == "--no-check") o.check = false;
        else if (a == "--csv-dir") o.csv_dir = v;
        else if (a == "--auto-drag") o.auto_drag = true;
        else if (a == "--evil") o.evil = true;
        else if (a == "--self-check") o.self_check = true;
        else if (a == "--replay") o.replay_dir = v;
        else if (a == "--encoding") {
            if (std::string_view(v) == "cycle") {
                o.cycle_encodings = true;
            } else {
                const auto e = codec::parse_encoding(v);
                if (!e) return bad_value(a, v);
                o.encoding = *e;
                o.cycle_encodings = false;
            }
        }
        else if (a == "--buffers") {
            const auto m = parse_buffer_mode(v);
            if (!m) return bad_value(a, v);
            o.buffers = *m;
        }
        else if (a == "--screenshot-at") {
            std::string list = v;
            std::size_t start = 0;
            while (start <= list.size()) {
                std::size_t comma = list.find(',', start);
                std::string item = list.substr(start, comma == std::string::npos ? std::string::npos : comma - start);
                const auto frame = parse_long(item.c_str(), 0, 10000000);
                if (!frame) return bad_value(a, item.c_str());
                o.screenshot_at.push_back(static_cast<int>(*frame));
                if (comma == std::string::npos) break;
                start = comma + 1;
            }
            std::sort(o.screenshot_at.begin(), o.screenshot_at.end());
        } else if (a == "--help" || a == "-h") {
            o.help = true;
            return o;
        } else {
            return Error{"unknown option " + std::string(a)};
        }
    }
    return o;
}

// ---- SnapshotHolder ------------------------------------------------------------

double SnapshotHolder::replace(XBuffer&& next) {
    double destroy_us = clear();
    buf_.emplace(std::move(next));
    root_ = &buf_->root<ViewSnapshot>();
    return destroy_us;
}

Status SnapshotHolder::reload(const std::string& wire) {
    root_ = nullptr;
    if (buf_) {
        XStatus reloaded = buf_->try_reload_verified<ViewSnapshot>(wire);
        if (!reloaded) return xoffset_error(reloaded.error());
    } else {
        XResult<XBuffer> loaded = XBuffer::try_load_verified<ViewSnapshot>(wire, cap_snapshot());
        if (!loaded) return xoffset_error(loaded.error());
        buf_.emplace(std::move(*loaded));
    }
    root_ = &buf_->root<ViewSnapshot>();
    return {};
}

Result<double> SnapshotHolder::install(const std::string& wire, BufferMode mode, double* destroy_us) {
    metrics::Stopwatch sw;
    if (mode == BufferMode::Reuse) {
        MAPDEMO_TRY(reload(wire));
        return sw.us();
    }
    XResult<XBuffer> next = XBuffer::try_load_verified<ViewSnapshot>(wire, cap_snapshot());
    if (!next) return xoffset_error(next.error());
    const double load_us = sw.us();
    const double d = replace(std::move(*next));
    if (destroy_us != nullptr) *destroy_us = d;
    return load_us;
}

double SnapshotHolder::clear() {
    root_ = nullptr;
    if (!buf_) return 0;
    metrics::Stopwatch sw;
    buf_.reset();
    return sw.us();
}

// ---- presentation ----------------------------------------------------------------

std::uint32_t behavior_rgba(Behavior b) noexcept {
    switch (b) {
    case Behavior::Patrol: return 0x2EB82EFFu;   // green
    case Behavior::Hold: return 0xE6C619FFu;     // yellow
    case Behavior::Alert: return 0xE03131FFu;    // red
    }
    return 0x8C8C8CFFu;                          // out-of-range enum value: grey
}

const char* behavior_name(Behavior b) noexcept {
    switch (b) {
    case Behavior::Patrol: return "Patrol";
    case Behavior::Hold: return "Hold";
    case Behavior::Alert: return "Alert";
    }
    return "?";
}

void build_draw_items(const ViewSnapshot& snapshot, const Rect& camera, std::vector<DrawItem>& out) {
    out.clear();
    for (const VisibleUnit& u : snapshot.units) {
        if (!rules::contains(camera, u.pos)) continue;   // snapshot rect may lag the camera
        out.push_back(DrawItem{u.pos.x - camera.min.x, u.pos.y - camera.min.y, behavior_rgba(u.behavior)});
    }
}

const char* category_name(net::ViewCategory c) noexcept {
    switch (c) {
    case net::ViewCategory::Normal: return "normal";
    case net::ViewCategory::FullScene: return "full";
    case net::ViewCategory::Empty: return "empty";
    }
    return "?";
}

Rect clamp_view(Rect r, const Rect& scene) noexcept {
    float w = std::min(r.max.x - r.min.x, scene.max.x - scene.min.x);
    float h = std::min(r.max.y - r.min.y, scene.max.y - scene.min.y);
    r.min.x = std::clamp(r.min.x, scene.min.x, scene.max.x - w);
    r.min.y = std::clamp(r.min.y, scene.min.y, scene.max.y - h);
    r.max.x = r.min.x + w;
    r.max.y = r.min.y + h;
    return r;
}

// ---- CheckLog ----------------------------------------------------------------------

void CheckLog::pass(const std::string& name, std::uint64_t n) { entries_[name].pass += n; }

void CheckLog::fail(const std::string& name, const std::string& detail) {
    Entry& e = entries_[name];
    if (e.fail++ == 0) e.first_failure = detail;
}

void CheckLog::note(const std::string& name, const std::string& detail) { entries_[name].note = detail; }

bool CheckLog::all_ok() const {
    for (const auto& [name, e] : entries_)
        if (e.fail != 0) return false;
    return true;
}

static std::string csv_field(std::string s) {
    for (char& c : s)
        if (c == ',' || c == '\n' || c == '\r') c = ';';
    return s;
}

static std::string entry_detail(std::uint64_t pass, std::uint64_t fail, const std::string& first, const std::string& note) {
    std::string d = "pass=" + std::to_string(pass) + " fail=" + std::to_string(fail);
    if (!note.empty()) d += " " + note;
    if (fail != 0) d += " first: " + first;
    return d;
}

void CheckLog::print(std::FILE* out) const {
    for (const auto& [name, e] : entries_)
        std::fprintf(out, "check %-22s %s  %s\n", name.c_str(), e.fail ? "FAIL" : "pass",
                     entry_detail(e.pass, e.fail, e.first_failure, e.note).c_str());
}

bool CheckLog::write_csv(const std::string& path) const {
    metrics::CsvFile f;
    if (!f.open(path, "check,result,detail")) return false;
    for (const auto& [name, e] : entries_)
        f.row(name + "," + (e.fail ? "fail" : "pass") + "," +
              csv_field(entry_detail(e.pass, e.fail, e.first_failure, e.note)));
    return true;
}

// ---- Session ---------------------------------------------------------------------

std::string join_path(const std::string& dir, const char* file) {
    if (dir.empty()) return file;
    return dir.back() == '/' ? dir + file : dir + "/" + file;
}

// Connects to A and sends Hello; returns A's answer (HelloOk or Error). A port
// forwarder (docker -p, a VM's or an emulator's) can accept the connection
// before A listens and then close it, so a close before any answer is retried
// like a refused connect, for up to 5 s.
static Result<net::Frame> connect_hello(net::Conn& c, const Options& opt) {
    const std::uint64_t give_up = net::now_ns() + 5000000000ull;
    for (;;) {
        Result<net::Conn> opened = net::Conn::try_connect(opt.host, opt.port);
        if (!opened) return std::move(opened).error();
        c = std::move(*opened);
        MAPDEMO_TRY(c.try_send(net::MsgType::Hello, 0, net::hello_payload()));
        Result<net::Frame> answer = c.try_recv({net::MsgType::HelloOk, net::MsgType::Error});
        if (answer || !answer.error().peer_closed || net::now_ns() >= give_up) return answer;
        c.close();
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

Session::Session(const Options& options) : opt_(options) {}

Session::~Session() = default;

Status Session::connect() {
    Result<net::Frame> ok = connect_hello(conn_, opt_);
    if (!ok) return std::move(ok).error();
    if (ok->type == net::MsgType::Error) return Error{"A refused the handshake: " + ok->payload};
    checks_.pass("handshake");

    Result<net::Frame> cat = conn_.try_recv({net::MsgType::Catalog, net::MsgType::Error});
    if (!cat) return std::move(cat).error();
    if (cat->type == net::MsgType::Error) return Error{"A sent Error instead of Catalog: " + cat->payload};
    metrics::Stopwatch sw;
    XResult<XBuffer> buf = XBuffer::try_load_verified<MapCatalog>(cat->payload, cap_catalog());
    if (!buf) return xoffset_error(buf.error());
    rec_.add("c.catalog_load_us", sw.us());
    catalog_.emplace(std::move(*buf));
    catalog_root_ = &catalog_->root<MapCatalog>();

    // Catalog content (structure is already verified; these are business checks).
    const MapCatalog& c = *catalog_root_;
    std::string why;
    if (c.scenes.size() != 1) why = "scene count " + std::to_string(c.scenes.size());
    else if (c.scenes[0].id != kSceneId) why = "scene id " + std::to_string(c.scenes[0].id);
    else if (!rules::rect_is_legal(c.scenes[0].bounds, scene_bounds())) why = "scene bounds not legal";
    else if (c.sites.size() != kSiteCount) why = "site count " + std::to_string(c.sites.size());
    else {
        for (const SiteData& s : c.sites) {
            if (s.scene_id != kSceneId || !rules::contains(c.scenes[0].bounds, s.pos) || s.name.view().empty()) {
                why = "site " + std::to_string(s.id) + " invalid";
                break;
            }
        }
    }
    if (why.empty()) {
        scene_ = c.scenes[0].bounds;
        checks_.pass("catalog");
        checks_.note("catalog", "scenes=1 sites=" + std::to_string(c.sites.size()) + " wire_bytes=" + std::to_string(cat->payload.size()));
    } else {
        checks_.fail("catalog", why);
    }

    MAPDEMO_TRY(conn_.try_set_nonblocking(true));
    if (!opt_.csv_dir.empty()) {
        snap_csv_open_ = snap_csv_.open(join_path(opt_.csv_dir, "client_snapshots.csv"), kSnapshotCsvHeader);
        if (!snap_csv_open_) return Error{"cannot write client_snapshots.csv in " + opt_.csv_dir};
    }
    return {};
}

Status Session::send_view(const Rect& rect, net::ViewCategory category) {
    const bool reuse = opt_.buffers == BufferMode::Reuse;
    std::optional<XBuffer> fresh;
    std::optional<XBuffer>& req = reuse ? req_buf_ : fresh;
    if (req) req->reset<ViewRequest>();
    else req.emplace(XBuffer::create<ViewRequest>(256, cap_view_request()));
    ViewRequest& r = req->root<ViewRequest>();
    r.request_id = next_request_id_++;
    r.scene_id = kSceneId;
    r.rect = rect;
    std::string wire;
    if (reuse) {
        wire = conn_.take_tx_buffer();
        req->save_verified_into<ViewRequest>(wire);
    } else {
        wire = req->save_verified<ViewRequest>();
    }
    sent_request_id_ = r.request_id;
    sent_category_ = category;
    sent_encoding_ = opt_.cycle_encodings ? static_cast<codec::Encoding>(r.request_id % codec::kEncodingCount)
                                          : opt_.encoding;
    sent_rect_ = rect;
    inflight_ = true;
    sent_ns_ = net::now_ns();
    MAPDEMO_TRY(conn_.try_enqueue(net::MsgType::ViewReq, sent_request_id_, std::move(wire),
                                  codec::view_flags(category, sent_encoding_)));
    MAPDEMO_TRY(conn_.try_flush());
    return {};
}

Status Session::pump(int timeout_ms) {
    net::PollItem p;
    p.fd = conn_.fd();
    p.want_read = true;
    p.want_write = conn_.want_write();
    Result<int> ready = net::try_poll(&p, 1, std::max(timeout_ms, 0));
    if (!ready) return std::move(ready).error();
    if (*ready <= 0) return {};
    if (conn_.want_write()) MAPDEMO_TRY(conn_.try_flush());
    if (!p.readable) return {};
    while (!bye_) {
        Result<std::optional<net::Frame>> got =
            conn_.try_recv_nowait({net::MsgType::ViewSnapshot, net::MsgType::Bye, net::MsgType::Error});
        if (!got) return std::move(got).error();
        std::optional<net::Frame>& f = *got;
        if (!f) break;
        if (f->type == net::MsgType::Bye) {
            bye_ = true;
        } else if (f->type == net::MsgType::Error) {
            return Error{"A rejected a view request: " + f->payload};
        } else {
            double rtt = double(net::now_ns() - sent_ns_) / 1000.0;
            if (!inflight_) return Error{"ViewSnapshot without an outstanding request"};
            if (f->correlation != sent_request_id_ || f->flags != codec::view_flags(sent_category_, sent_encoding_)) {
                checks_.fail("correlation", "frame corr " + std::to_string(f->correlation) + " flags " +
                                                std::to_string(f->flags) + " for request " + std::to_string(sent_request_id_));
            } else {
                checks_.pass("correlation");
            }
            inflight_ = false;
            rec_.add("c.view_rtt_us", rtt);
            last_rtt_us_ = rtt;
            // Only one request is outstanding, so at most one snapshot is pending.
            pending_.emplace(Pending{std::move(*f), rtt, sent_category_, sent_encoding_, sent_rect_});
        }
    }
    return {};
}

Result<bool> Session::install_pending() {
    if (!pending_) return false;
    Pending p = std::move(*pending_);
    pending_.reset();

    const bool reuse = opt_.buffers == BufferMode::Reuse;
    // zstd: the declared size is checked against the snapshot cap before
    // anything is allocated; the result is an unverified XOffset wire.
    double decompress_us = -1;
    const std::string* wire = &p.frame.payload;
    if (p.encoding == codec::Encoding::Zstd) {
        metrics::Stopwatch zw;
        MAPDEMO_TRY(unzstd_.decompress(p.frame.payload, kCapSnapshot, plain_));
        decompress_us = zw.us();
        wire = &plain_;
    }
    double destroy_us = -1;   // reuse: nothing is destroyed per snapshot
    Result<double> loaded = holder_.install(*wire, opt_.buffers, &destroy_us);
    if (!loaded) return std::move(loaded).error();
    const double load_us = *loaded;
    if (cur_.live) retire_current(destroy_us);

    rec_.add("c.load_us", load_us);
    last_load_us_ = load_us;
    const ViewSnapshot& s = holder_.view();
    cur_ = Record{};
    cur_.live = true;
    cur_.request_id = s.request_id;
    cur_.tick = s.tick;
    cur_.category = p.category;
    cur_.rect = p.rect;
    cur_.visible = s.units.size();
    cur_.wire_bytes = wire->size();
    cur_.net_bytes = p.frame.payload.size();
    cur_.encoding = p.encoding;
    cur_.rtt_us = p.rtt_us;
    cur_.load_us = load_us;
    cur_.decompress_us = decompress_us;
    rec_.add("c.snap_bytes", double(cur_.wire_bytes));
    rec_.add("c.snap_net_bytes", double(cur_.net_bytes));
    if (decompress_us >= 0) rec_.add("c.decompress_us", decompress_us);
    rec_.add("c.visible_count", double(cur_.visible));
    ++snapshots_;
    ++by_encoding_[static_cast<int>(p.encoding)];
    // Outside the timed region: digests (compared with A's log) and checks.
    cur_.digest = checks::snapshot_digest(s);
    cur_.wire_digest = checks::wire_digest(*wire);
    if (opt_.check_on()) check_snapshot(p.frame);
    if (reuse) conn_.recycle_rx(std::move(p.frame.payload));
    return true;
}

void Session::check_snapshot(const net::Frame& frame) {
    const ViewSnapshot& s = holder_.view();
    auto fail = [&](const char* name, const std::string& why) {
        checks_.fail(name, "request " + std::to_string(cur_.request_id) + ": " + why);
        cur_.check_ok = false;
    };
    if (s.request_id != frame.correlation) fail("snapshot_header", "request_id " + std::to_string(s.request_id));
    else if (s.scene_id != kSceneId) fail("snapshot_header", "scene_id " + std::to_string(s.scene_id));
    else checks_.pass("snapshot_header");

    checks::Result r = checks::snapshot_sanity(s, frame.correlation, cur_.rect);
    if (r.ok) checks_.pass("snapshot_sanity");
    else fail("snapshot_sanity", r.detail);

    switch (cur_.category) {
    case net::ViewCategory::Empty:
        if (s.units.size() == 0) checks_.pass("empty_view_is_empty");
        else fail("empty_view_is_empty", std::to_string(s.units.size()) + " units");
        break;
    case net::ViewCategory::FullScene:
        if (s.units.size() == 0) fail("full_view_nonempty", "0 units");
        else if (full_scene_visible_ && *full_scene_visible_ != s.units.size())
            fail("full_view_nonempty", "visible " + std::to_string(s.units.size()) + " != " + std::to_string(*full_scene_visible_));
        else {
            full_scene_visible_ = s.units.size();
            checks_.pass("full_view_nonempty");
        }
        break;
    case net::ViewCategory::Normal:
        break;
    }
}

void Session::note_read_present(double us) {
    rec_.add("c.read_present_us", us);
    cur_.read_present_sum += us;
    ++cur_.read_present_n;
}

void Session::verify_direct_read() {
    if (holder_.empty()) return;
    const XBuffer& buf = holder_.buffer();
    const ViewSnapshot& s = holder_.view();
    std::uint64_t units = 0, names = 0;
    if (!checks::in_buffer(buf, &s, sizeof(ViewSnapshot))) {
        checks_.fail("direct_read", "snapshot root outside the live buffer");
        cur_.check_ok = false;
        return;
    }
    for (const VisibleUnit& u : s.units) {
        std::string_view name = u.name.view();
        bool ok_unit = checks::in_buffer(buf, &u, sizeof(VisibleUnit));
        bool ok_name = name.empty() || checks::in_buffer(buf, name.data(), name.size());
        if (!ok_unit || !ok_name) {
            checks_.fail("direct_read", "unit " + std::to_string(u.id) + (ok_unit ? " name" : " record") +
                                            " outside the live buffer");
            cur_.check_ok = false;
            return;
        }
        ++units;
        if (!name.empty()) ++names;
    }
    checks_.pass("direct_read");
    direct_units_ += units;
    direct_names_ += names;
    checks_.note("direct_read", "units_in_buffer=" + std::to_string(direct_units_) +
                                    " names_in_buffer=" + std::to_string(direct_names_));
}

void Session::retire_current(double destroy_us) {
    if (destroy_us >= 0) rec_.add("c.destroy_us", destroy_us);
    double rp = cur_.read_present_n ? cur_.read_present_sum / double(cur_.read_present_n) : 0.0;
    if (snap_csv_open_) {
        char destroy[32] = "";
        if (destroy_us >= 0) std::snprintf(destroy, sizeof destroy, "%.3f", destroy_us);
        char unzstd[32] = "";
        if (cur_.decompress_us >= 0) std::snprintf(unzstd, sizeof unzstd, "%.3f", cur_.decompress_us);
        char line[512];
        std::snprintf(line, sizeof line,
                      "%" PRIu64 ",%" PRIu64 ",%s,%zu,%zu,%016" PRIx64 ",%.3f,%.3f,%.3f,%s,%d,%s,%zu,%016" PRIx64 ",%s",
                      cur_.request_id, cur_.tick, category_name(cur_.category), cur_.visible, cur_.wire_bytes,
                      cur_.digest, cur_.rtt_us, cur_.load_us, rp, destroy, cur_.check_ok ? 1 : 0,
                      codec::encoding_name(cur_.encoding), cur_.net_bytes, cur_.wire_digest, unzstd);
        snap_csv_.row(line);
    }
    cur_.live = false;
}

Result<bool> Session::finish() {
    if (cur_.live) {
        const double destroy_us = holder_.clear();
        retire_current(opt_.buffers == BufferMode::Reuse ? -1.0 : destroy_us);
    }
    if (opt_.check_on()) {
        if (snapshots_ == 0) checks_.fail("snapshots_received", "no snapshot received");
        else {
            checks_.pass("snapshots_received", snapshots_);
            checks_.note("snapshots_received", "count=" + std::to_string(snapshots_) +
                                                   " dense=" + std::to_string(by_encoding_[0]) +
                                                   " compact=" + std::to_string(by_encoding_[1]) +
                                                   " zstd=" + std::to_string(by_encoding_[2]));
        }
    }
    bool ok = checks_.all_ok();
    if (!opt_.csv_dir.empty()) {
        if (!rec_.write_csv(join_path(opt_.csv_dir, "client_metrics.csv")))
            return Error{"cannot write client_metrics.csv in " + opt_.csv_dir};
        if (!checks_.write_csv(join_path(opt_.csv_dir, "client_checks.csv")))
            return Error{"cannot write client_checks.csv in " + opt_.csv_dir};
    }
    return ok;
}

// ---- headless sampler -----------------------------------------------------------

// Normal views sweep the scene in a serpentine of 1024x768 rects.
static Rect route_rect(std::uint64_t k, const Rect& scene) {
    const float step_x = 256.0f, step_y = 384.0f;
    const float span_x = (scene.max.x - scene.min.x) - kViewWidth;
    const float span_y = (scene.max.y - scene.min.y) - kViewHeight;
    const std::uint64_t cols = std::uint64_t(span_x / step_x) + 1;
    const std::uint64_t rows = std::uint64_t(span_y / step_y) + 1;
    const std::uint64_t i = k % (cols * rows);
    const std::uint64_t row = i / cols;
    std::uint64_t col = i % cols;
    if (row % 2 == 1) col = cols - 1 - col;
    Rect r;
    r.min = Point{scene.min.x + float(col) * step_x, scene.min.y + float(row) * step_y};
    r.max = Point{r.min.x + kViewWidth, r.min.y + kViewHeight};
    return clamp_view(r, scene);
}

Result<int> run_headless(const Options& opt) {
    Session s(opt);
    MAPDEMO_TRY(s.connect());
    std::fprintf(stdout, "client: connected to %s:%u, catalog %u sites, sampling at %.0f Hz (8 normal / 1 full / 1 empty), "
                         "encoding %s\n",
                 opt.host.c_str(), unsigned(opt.port), unsigned(s.catalog().sites.size()), kViewRequestHz,
                 opt.cycle_encodings ? "cycle" : codec::encoding_name(opt.encoding));
    std::fflush(stdout);

    const std::uint64_t period_ns = std::uint64_t(1e9 / kViewRequestHz);
    std::uint64_t next_due = net::now_ns();
    std::uint64_t next_print = next_due + 1000000000ull;
    std::uint64_t seq = 0, normal_seq = 0;
    std::uint64_t per_cat[3] = {0, 0, 0};
    std::vector<DrawItem> items;
    std::uint64_t sink = 0;

    while (!s.bye()) {
        std::uint64_t now = net::now_ns();
        int timeout_ms = 100;
        if (s.can_request()) timeout_ms = now >= next_due ? 0 : int((next_due - now) / 1000000) + 1;
        MAPDEMO_TRY(s.pump(timeout_ms));

        Result<bool> installed = s.install_pending();
        if (!installed) return std::move(installed).error();
        if (*installed) {
            const Rect cam = s.holder_request_rect();
            metrics::Stopwatch sw;
            build_draw_items(s.holder().view(), cam, items);
            // Labels read names straight from the buffer (window client: id % 100 == 0 shown).
            for (const VisibleUnit& u : s.holder().view().units)
                if (u.id % kEventIdModulo == 0) sink += u.name.view().size() + std::uint64_t(u.hp);
            s.note_read_present(sw.us());
            if (opt.check_on()) s.verify_direct_read();
        }
        if (s.bye()) break;

        now = net::now_ns();
        if (s.can_request() && now >= next_due) {
            std::uint64_t slot = seq++ % 10;
            net::ViewCategory cat = slot < 8 ? net::ViewCategory::Normal
                                  : slot == 8 ? net::ViewCategory::FullScene
                                              : net::ViewCategory::Empty;
            Rect r = cat == net::ViewCategory::Normal ? route_rect(normal_seq++, s.scene())
                   : cat == net::ViewCategory::FullScene ? s.scene()
                                                          : empty_probe_rect();
            ++per_cat[static_cast<int>(cat)];
            MAPDEMO_TRY(s.send_view(r, cat));
            next_due += period_ns;
            if (next_due < now) next_due = now + period_ns;   // behind: do not burst
        }
        if (now >= next_print) {
            next_print += 1000000000ull;
            const ViewSnapshot* v = s.holder().empty() ? nullptr : &s.holder().view();
            std::fprintf(stdout, "client snaps=%" PRIu64 " tick=%" PRIu64 " visible=%zu snap_bytes=%zu net_bytes=%zu (%s) "
                                 "view_rtt_us=%.0f load_us=%.1f\n",
                         s.snapshots(), v ? std::uint64_t(v->tick) : 0, v ? std::size_t(v->units.size()) : 0,
                         s.last_wire_bytes(), s.last_net_bytes(), codec::encoding_name(s.last_encoding()),
                         s.last_rtt_us(), s.last_load_us());
            std::fflush(stdout);
        }
    }
    if (opt.check_on()) {
        // Every category must really have gone through the path (requirements section 5).
        bool all = per_cat[0] && per_cat[1] && per_cat[2];
        std::string detail = "normal=" + std::to_string(per_cat[0]) + " full=" + std::to_string(per_cat[1]) +
                             " empty=" + std::to_string(per_cat[2]);
        if (all) {
            s.checks().pass("categories_sampled");
            s.checks().note("categories_sampled", detail);
        } else {
            s.checks().fail("categories_sampled", detail);
        }
    }
    Result<bool> ok = s.finish();
    if (!ok) return std::move(ok).error();
    s.recorder().print_line(stdout, {"c.view_rtt_us", "c.load_us", "c.decompress_us", "c.read_present_us",
                                     "c.destroy_us", "c.snap_bytes", "c.snap_net_bytes", "c.visible_count"});
    s.checks().print(stdout);
    std::fprintf(stdout, "client: Bye received after %" PRIu64 " snapshots (%" PRIu64 ")\n", s.snapshots(), sink % 2);
    return *ok ? 0 : 1;
}

// ---- --evil probe -------------------------------------------------------------

namespace {

enum class Outcome { Frame, Closed, Timeout };

struct Awaited {
    Outcome outcome = Outcome::Timeout;
    net::Frame frame;
    std::string detail;
};

// A frame, or Closed with the error that ended the connection, or Timeout.
Awaited await_frame(net::Conn& c, int timeout_ms) {
    Awaited a;
    std::uint64_t deadline = net::now_ns() + std::uint64_t(timeout_ms) * 1000000ull;
    Status read = [&]() -> Status {
        MAPDEMO_TRY(c.try_set_nonblocking(true));
        for (;;) {
            if (c.want_write()) MAPDEMO_TRY(c.try_flush());
            Result<std::optional<net::Frame>> f = c.try_recv_nowait(
                {net::MsgType::ViewSnapshot, net::MsgType::Error, net::MsgType::Bye, net::MsgType::Catalog, net::MsgType::HelloOk});
            if (!f) return std::move(f).error();
            if (*f) {
                a.outcome = Outcome::Frame;
                a.frame = std::move(**f);
                return {};
            }
            std::uint64_t now = net::now_ns();
            if (now >= deadline) return {};
            net::PollItem p;
            p.fd = c.fd();
            p.want_read = true;
            MAPDEMO_TRY(net::try_poll(&p, 1, int((deadline - now) / 1000000) + 1));
        }
    }();
    if (!read) {
        a.outcome = Outcome::Closed;
        a.detail = read.error().message;
    }
    return a;
}

Status handshake(net::Conn& c, const Options& opt) {
    Result<net::Frame> ok = connect_hello(c, opt);
    if (!ok) return std::move(ok).error();
    if (ok->type != net::MsgType::HelloOk) return Error{"handshake refused: " + ok->payload};
    MAPDEMO_TRY(c.try_recv({net::MsgType::Catalog}));
    return {};
}

std::string view_request_wire(std::uint64_t id, const Rect& r) {
    XBuffer req = XBuffer::create<ViewRequest>(256, cap_view_request());
    ViewRequest& v = req.root<ViewRequest>();
    v.request_id = id;
    v.scene_id = kSceneId;
    v.rect = r;
    return req.save_verified<ViewRequest>();
}

// Expect A to answer Error or to close the connection; anything else fails.
bool expect_rejected(CheckLog& log, const char* name, const char* what, net::Conn& c) {
    Awaited a = await_frame(c, 5000);
    std::string result;
    bool ok = false;
    if (a.outcome == Outcome::Closed) {
        ok = true;
        result = "connection closed (" + a.detail + ")";
    } else if (a.outcome == Outcome::Timeout) {
        result = "no answer within 5 s";
    } else if (a.frame.type == net::MsgType::Error) {
        ok = true;
        result = "Error: " + a.frame.payload;
    } else {
        result = std::string("unexpected ") + net::type_name(a.frame.type);
    }
    std::fprintf(stdout, "evil: %-40s -> %s %s\n", what, ok ? "rejected," : "NOT rejected,", result.c_str());
    if (ok) {
        log.pass(name);
        log.note(name, result);
    } else {
        log.fail(name, result);
    }
    return ok;
}

} // namespace

Result<int> run_evil(const Options& opt) {
    CheckLog log;
    metrics::Recorder rec("c");
    std::string snap_row;
    const Rect normal{{0.0f, 0.0f}, {kViewWidth, kViewHeight}};

    {   // (1) truncated ViewRequest wire
        net::Conn c;
        MAPDEMO_TRY(handshake(c, opt));
        std::string wire = view_request_wire(1, normal);
        wire.pop_back();
        MAPDEMO_TRY(c.try_send(net::MsgType::ViewReq, 1, wire, static_cast<std::uint16_t>(net::ViewCategory::Normal)));
        expect_rejected(log, "evil_truncated", "truncated ViewRequest wire (-1 byte)", c);
    }
    {   // (2) wrong schema: a ViewSnapshot wire sent as ViewReq
        net::Conn c;
        MAPDEMO_TRY(handshake(c, opt));
        XBuffer snap = XBuffer::create<ViewSnapshot>(512, cap_snapshot());
        ViewSnapshot& v = snap.root<ViewSnapshot>();
        v.request_id = 2;
        v.scene_id = kSceneId;
        v.rect = normal;
        std::string wire = snap.save_verified<ViewSnapshot>();
        MAPDEMO_TRY(c.try_send(net::MsgType::ViewReq, 2, wire, static_cast<std::uint16_t>(net::ViewCategory::Normal)));
        expect_rejected(log, "evil_wrong_schema", "ViewSnapshot wire sent as ViewReq", c);
    }
    {   // (3) header declaring a 1 GiB ViewReq payload, no payload follows
        net::Conn c;
        MAPDEMO_TRY(handshake(c, opt));
        net::FrameHeader h{};
        h.magic = net::kMagic;
        h.type = static_cast<std::uint16_t>(net::MsgType::ViewReq);
        h.flags = 0;
        h.length = std::uint32_t(1) << 30;
        h.reserved = 0;
        h.correlation = 3;
        MAPDEMO_TRY(c.try_send_raw(std::string_view(reinterpret_cast<const char*>(&h), sizeof h)));
        expect_rejected(log, "evil_oversize_header", "header declaring a 1 GiB ViewReq", c);
    }
    {   // (5) an encoding A does not know (3 is reserved)
        net::Conn c;
        MAPDEMO_TRY(handshake(c, opt));
        MAPDEMO_TRY(c.try_send(net::MsgType::ViewReq, 8, view_request_wire(8, normal),
                               static_cast<std::uint16_t>(static_cast<unsigned>(net::ViewCategory::Normal) | (3u << 8))));
        expect_rejected(log, "evil_unknown_encoding", "ViewReq asking for encoding 3", c);
    }
    {   // (6) a reserved flag bit
        net::Conn c;
        MAPDEMO_TRY(handshake(c, opt));
        MAPDEMO_TRY(c.try_send(net::MsgType::ViewReq, 9, view_request_wire(9, normal),
                               static_cast<std::uint16_t>(codec::view_flags(net::ViewCategory::Normal, codec::Encoding::Compact) | 0x1000u)));
        expect_rejected(log, "evil_reserved_flag", "ViewReq with reserved flag bit 12", c);
    }
    {   // (4) one connection: valid request, truncated wire, valid request.
        // With --buffers reuse, A's per-client request buffer is left empty
        // by the rejected reload and must be refilled by the next one.
        net::Conn c;
        MAPDEMO_TRY(handshake(c, opt));
        // "" when A answered with a sound snapshot, else what went wrong.
        auto served = [&](std::uint64_t id) -> Result<std::string> {
            MAPDEMO_TRY(c.try_send(net::MsgType::ViewReq, id, view_request_wire(id, normal),
                                   static_cast<std::uint16_t>(net::ViewCategory::Normal)));
            Awaited a = await_frame(c, 5000);
            if (a.outcome != Outcome::Frame)
                return std::string(a.outcome == Outcome::Closed ? "closed: " + a.detail : "no answer within 5 s");
            if (a.frame.type != net::MsgType::ViewSnapshot) return std::string("got ") + net::type_name(a.frame.type) + " " + a.frame.payload;
            if (a.frame.correlation != id) return "correlation " + std::to_string(a.frame.correlation);
            XResult<XBuffer> b = XBuffer::try_load_verified<ViewSnapshot>(a.frame.payload, cap_snapshot());
            if (!b) return "load_verified: " + b.error().message;
            checks::Result r = checks::snapshot_sanity(b->root<ViewSnapshot>(), id, normal);
            return r.ok ? std::string() : r.detail;
        };
        Result<std::string> why = served(5);
        if (!why) return std::move(why).error();
        if (why->empty()) {
            std::string bad = view_request_wire(6, normal);
            bad.pop_back();
            MAPDEMO_TRY(c.try_send(net::MsgType::ViewReq, 6, bad, static_cast<std::uint16_t>(net::ViewCategory::Normal)));
            if (!expect_rejected(log, "evil_same_conn_rejected", "same connection: truncated wire", c)) *why = "not rejected";
        }
        if (why->empty()) {
            why = served(7);
            if (!why) return std::move(why).error();
        }
        std::fprintf(stdout, "evil: %-40s -> %s\n", "same connection: valid request after it",
                     why->empty() ? "answered" : why->c_str());
        if (why->empty()) {
            log.pass("evil_same_conn_recovers");
            log.note("evil_same_conn_recovers", "requests 5 and 7 answered around rejected 6");
        } else {
            log.fail("evil_same_conn_recovers", *why);
        }
    }
    {   // A still serves a normal client. Its snapshot is logged like any other
        // (client_snapshots.csv) so it can be matched with A's a_snapshots.csv.
        net::Conn c;
        MAPDEMO_TRY(handshake(c, opt));
        const std::uint64_t sent = net::now_ns();
        MAPDEMO_TRY(c.try_send(net::MsgType::ViewReq, 4, view_request_wire(4, normal),
                               static_cast<std::uint16_t>(net::ViewCategory::Normal)));
        Awaited a = await_frame(c, 5000);
        const double rtt_us = double(net::now_ns() - sent) / 1000.0;
        std::string why;
        if (a.outcome != Outcome::Frame) why = a.outcome == Outcome::Closed ? "closed: " + a.detail : "no answer within 5 s";
        else if (a.frame.type != net::MsgType::ViewSnapshot) why = std::string("got ") + net::type_name(a.frame.type) + " " + a.frame.payload;
        else if (a.frame.correlation != 4) why = "correlation " + std::to_string(a.frame.correlation);
        else {
            metrics::Stopwatch sw;
            XResult<XBuffer> b = XBuffer::try_load_verified<ViewSnapshot>(a.frame.payload, cap_snapshot());
            const double load_us = sw.us();
            if (!b) {
                why = "load_verified: " + b.error().message;
            } else {
                const ViewSnapshot& v = b->root<ViewSnapshot>();
                checks::Result r = checks::snapshot_sanity(v, 4, normal);
                if (!r.ok) why = r.detail;
                rec.add("c.view_rtt_us", rtt_us);
                rec.add("c.load_us", load_us);
                rec.add("c.snap_bytes", double(a.frame.payload.size()));
                rec.add("c.visible_count", double(v.units.size()));
                char line[512];
                std::snprintf(line, sizeof line,
                              "%" PRIu64 ",%" PRIu64 ",normal,%zu,%zu,%016" PRIx64 ",%.3f,%.3f,0.000,0.000,%d,dense,%zu,%016" PRIx64 ",",
                              std::uint64_t(v.request_id), std::uint64_t(v.tick), std::size_t(v.units.size()),
                              a.frame.payload.size(), checks::snapshot_digest(v), rtt_us, load_us, r.ok ? 1 : 0,
                              a.frame.payload.size(), checks::wire_digest(a.frame.payload));
                snap_row = line;
            }
        }
        std::fprintf(stdout, "evil: %-40s -> %s\n", "normal request afterwards", why.empty() ? "answered" : why.c_str());
        if (why.empty()) {
            log.pass("evil_normal_after");
            log.note("evil_normal_after", "snapshot wire " + std::to_string(a.frame.payload.size()) + " B");
        } else {
            log.fail("evil_normal_after", why);
        }
    }
    if (!opt.csv_dir.empty()) {
        metrics::CsvFile snaps;
        bool ok = snaps.open(join_path(opt.csv_dir, "client_snapshots.csv"), kSnapshotCsvHeader);
        if (ok && !snap_row.empty()) snaps.row(snap_row);
        if (!ok || !log.write_csv(join_path(opt.csv_dir, "client_checks.csv")) ||
            !rec.write_csv(join_path(opt.csv_dir, "client_metrics.csv")))
            return Error{"cannot write client CSV files in " + opt.csv_dir};
    }
    log.print(stdout);
    return log.all_ok() ? 0 : 1;
}

} // namespace mapdemo::client
