// Map state demo: client network, SnapshotHolder, sampling script. OWNER: client thread.
#include "client_core.hpp"

#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <cinttypes>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <span>
#include <utility>

namespace mapdemo::client {

// ---- options -----------------------------------------------------------------

const char* usage_text() {
    return "usage: mapdemo_client [--host 127.0.0.1] [--port 47701] [--headless] [--check | --no-check]\n"
           "                      [--buffers reuse|fresh] [--csv-dir DIR] [--auto-drag]\n"
           "                      [--screenshot-at F1,F2,...] [--evil]\n";
}

static long parse_long(std::string_view flag, const char* text, long lo, long hi) {
    char* end = nullptr;
    errno = 0;
    long v = std::strtol(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0' || v < lo || v > hi)
        throw UsageError(std::string(flag) + ": bad value '" + text + "'");
    return v;
}

Options parse_options(int argc, char** argv) {
    Options o;
    for (int i = 1; i < argc; ++i) {
        std::string_view a = argv[i];
        auto value = [&]() -> const char* {
            if (i + 1 >= argc) throw UsageError(std::string(a) + " needs a value");
            return argv[++i];
        };
        if (a == "--host") o.host = value();
        else if (a == "--port") o.port = static_cast<std::uint16_t>(parse_long(a, value(), 1, 65535));
        else if (a == "--headless") o.headless = true;
        else if (a == "--check") o.check = true;
        else if (a == "--no-check") o.check = false;
        else if (a == "--csv-dir") o.csv_dir = value();
        else if (a == "--auto-drag") o.auto_drag = true;
        else if (a == "--evil") o.evil = true;
        else if (a == "--buffers") {
            const char* t = value();
            const auto m = parse_buffer_mode(t);
            if (!m) throw UsageError(std::string(a) + ": bad value '" + t + "'");
            o.buffers = *m;
        }
        else if (a == "--screenshot-at") {
            std::string list = value();
            std::size_t start = 0;
            while (start <= list.size()) {
                std::size_t comma = list.find(',', start);
                std::string item = list.substr(start, comma == std::string::npos ? std::string::npos : comma - start);
                o.screenshot_at.push_back(static_cast<int>(parse_long(a, item.c_str(), 0, 10000000)));
                if (comma == std::string::npos) break;
                start = comma + 1;
            }
            std::sort(o.screenshot_at.begin(), o.screenshot_at.end());
        } else if (a == "--help" || a == "-h") {
            throw UsageError("help");
        } else {
            throw UsageError("unknown option " + std::string(a));
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

void SnapshotHolder::reload(const std::string& wire) {
    root_ = nullptr;
    if (buf_) buf_->reload_verified<ViewSnapshot>(wire);
    else buf_.emplace(XBuffer::load_verified<ViewSnapshot>(wire, cap_snapshot()));
    root_ = &buf_->root<ViewSnapshot>();
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

static std::string join_path(const std::string& dir, const char* file) {
    if (dir.empty()) return file;
    return dir.back() == '/' ? dir + file : dir + "/" + file;
}

Session::Session(const Options& options) : opt_(options) {}

Session::~Session() = default;

void Session::connect() {
    conn_ = net::Conn::connect(opt_.host, opt_.port);
    conn_.send(net::MsgType::Hello, 0, net::hello_payload());
    net::Frame ok = conn_.recv({net::MsgType::HelloOk, net::MsgType::Error});
    if (ok.type == net::MsgType::Error)
        throw net::ProtocolError("A refused the handshake: " + ok.payload);
    checks_.pass("handshake");

    net::Frame cat = conn_.recv({net::MsgType::Catalog, net::MsgType::Error});
    if (cat.type == net::MsgType::Error) throw net::ProtocolError("A sent Error instead of Catalog: " + cat.payload);
    metrics::Stopwatch sw;
    XBuffer buf = XBuffer::load_verified<MapCatalog>(cat.payload, cap_catalog());
    rec_.add("c.catalog_load_us", sw.us());
    catalog_.emplace(std::move(buf));
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
        checks_.note("catalog", "scenes=1 sites=" + std::to_string(c.sites.size()) + " wire_bytes=" + std::to_string(cat.payload.size()));
    } else {
        checks_.fail("catalog", why);
    }

    conn_.set_nonblocking(true);
    if (!opt_.csv_dir.empty()) {
        snap_csv_open_ = snap_csv_.open(join_path(opt_.csv_dir, "client_snapshots.csv"),
            "request_id,tick,category,visible,wire_bytes,digest,rtt_us,load_us,read_present_us,destroy_us,check_ok");
        if (!snap_csv_open_) throw std::runtime_error("cannot write client_snapshots.csv in " + opt_.csv_dir);
    }
}

void Session::send_view(const Rect& rect, net::ViewCategory category) {
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
    sent_rect_ = rect;
    inflight_ = true;
    sent_ns_ = net::now_ns();
    conn_.enqueue(net::MsgType::ViewReq, sent_request_id_, std::move(wire), static_cast<std::uint16_t>(category));
    conn_.flush();
}

void Session::pump(int timeout_ms) {
    pollfd p{conn_.fd(), static_cast<short>(POLLIN | (conn_.want_write() ? POLLOUT : 0)), 0};
    int n = ::poll(&p, 1, std::max(timeout_ms, 0));
    if (n < 0 && errno != EINTR) throw std::runtime_error(std::string("poll: ") + std::strerror(errno));
    if (n <= 0) return;
    if (conn_.want_write()) conn_.flush();
    if (!(p.revents & (POLLIN | POLLHUP | POLLERR))) return;
    while (!bye_) {
        std::optional<net::Frame> f =
            conn_.try_recv({net::MsgType::ViewSnapshot, net::MsgType::Bye, net::MsgType::Error});
        if (!f) break;
        if (f->type == net::MsgType::Bye) {
            bye_ = true;
        } else if (f->type == net::MsgType::Error) {
            throw net::ProtocolError("A rejected a view request: " + f->payload);
        } else {
            double rtt = double(net::now_ns() - sent_ns_) / 1000.0;
            if (!inflight_) throw net::ProtocolError("ViewSnapshot without an outstanding request");
            if (f->correlation != sent_request_id_ || f->flags != static_cast<std::uint16_t>(sent_category_)) {
                checks_.fail("correlation", "frame corr " + std::to_string(f->correlation) + " flags " +
                                                std::to_string(f->flags) + " for request " + std::to_string(sent_request_id_));
            } else {
                checks_.pass("correlation");
            }
            inflight_ = false;
            rec_.add("c.view_rtt_us", rtt);
            last_rtt_us_ = rtt;
            // Only one request is outstanding, so at most one snapshot is pending.
            pending_.emplace(Pending{std::move(*f), rtt, sent_category_, sent_rect_});
        }
    }
}

bool Session::install_pending() {
    if (!pending_) return false;
    Pending p = std::move(*pending_);
    pending_.reset();

    const bool reuse = opt_.buffers == BufferMode::Reuse;
    metrics::Stopwatch sw;
    double load_us = 0;
    double destroy_us = -1;   // reuse: nothing is destroyed per snapshot
    if (reuse) {
        holder_.reload(p.frame.payload);
        load_us = sw.us();
    } else {
        XBuffer next = XBuffer::load_verified<ViewSnapshot>(p.frame.payload, cap_snapshot());
        load_us = sw.us();
        destroy_us = holder_.replace(std::move(next));
    }
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
    cur_.wire_bytes = p.frame.payload.size();
    cur_.rtt_us = p.rtt_us;
    cur_.load_us = load_us;
    rec_.add("c.snap_bytes", double(cur_.wire_bytes));
    rec_.add("c.visible_count", double(cur_.visible));
    ++snapshots_;
    // Outside the timed region: digest (compared with A's log) and checks.
    cur_.digest = checks::snapshot_digest(s);
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
        char line[512];
        std::snprintf(line, sizeof line, "%" PRIu64 ",%" PRIu64 ",%s,%zu,%zu,%016" PRIx64 ",%.3f,%.3f,%.3f,%s,%d",
                      cur_.request_id, cur_.tick, category_name(cur_.category), cur_.visible, cur_.wire_bytes,
                      cur_.digest, cur_.rtt_us, cur_.load_us, rp, destroy, cur_.check_ok ? 1 : 0);
        snap_csv_.row(line);
    }
    cur_.live = false;
}

bool Session::finish() {
    if (cur_.live) {
        const double destroy_us = holder_.clear();
        retire_current(opt_.buffers == BufferMode::Reuse ? -1.0 : destroy_us);
    }
    if (opt_.check_on()) {
        if (snapshots_ == 0) checks_.fail("snapshots_received", "no snapshot received");
        else {
            checks_.pass("snapshots_received", snapshots_);
            checks_.note("snapshots_received", "count=" + std::to_string(snapshots_));
        }
    }
    bool ok = checks_.all_ok();
    if (!opt_.csv_dir.empty()) {
        if (!rec_.write_csv(join_path(opt_.csv_dir, "client_metrics.csv")))
            throw std::runtime_error("cannot write client_metrics.csv in " + opt_.csv_dir);
        if (!checks_.write_csv(join_path(opt_.csv_dir, "client_checks.csv")))
            throw std::runtime_error("cannot write client_checks.csv in " + opt_.csv_dir);
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

int run_headless(const Options& opt) {
    Session s(opt);
    s.connect();
    std::fprintf(stdout, "client: connected to %s:%u, catalog %u sites, sampling at %.0f Hz (8 normal / 1 full / 1 empty)\n",
                 opt.host.c_str(), unsigned(opt.port), unsigned(s.catalog().sites.size()), kViewRequestHz);
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
        s.pump(timeout_ms);

        if (s.install_pending()) {
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
            s.send_view(r, cat);
            next_due += period_ns;
            if (next_due < now) next_due = now + period_ns;   // behind: do not burst
        }
        if (now >= next_print) {
            next_print += 1000000000ull;
            const ViewSnapshot* v = s.holder().empty() ? nullptr : &s.holder().view();
            std::fprintf(stdout, "client snaps=%" PRIu64 " tick=%" PRIu64 " visible=%zu snap_bytes=%zu view_rtt_us=%.0f load_us=%.1f\n",
                         s.snapshots(), v ? std::uint64_t(v->tick) : 0, v ? std::size_t(v->units.size()) : 0,
                         s.last_wire_bytes(), s.last_rtt_us(), s.last_load_us());
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
    bool ok = s.finish();
    s.recorder().print_line(stdout, {"c.view_rtt_us", "c.load_us", "c.read_present_us", "c.destroy_us",
                                     "c.snap_bytes", "c.visible_count"});
    s.checks().print(stdout);
    std::fprintf(stdout, "client: Bye received after %" PRIu64 " snapshots (%" PRIu64 ")\n", s.snapshots(), sink % 2);
    return ok ? 0 : 1;
}

// ---- --evil probe -------------------------------------------------------------

namespace {

enum class Outcome { Frame, Closed, Timeout };

struct Awaited {
    Outcome outcome = Outcome::Timeout;
    net::Frame frame;
    std::string detail;
};

Awaited await_frame(net::Conn& c, int timeout_ms) {
    Awaited a;
    std::uint64_t deadline = net::now_ns() + std::uint64_t(timeout_ms) * 1000000ull;
    try {
        c.set_nonblocking(true);
        for (;;) {
            if (c.want_write()) c.flush();
            std::optional<net::Frame> f = c.try_recv(
                {net::MsgType::ViewSnapshot, net::MsgType::Error, net::MsgType::Bye, net::MsgType::Catalog, net::MsgType::HelloOk});
            if (f) {
                a.outcome = Outcome::Frame;
                a.frame = std::move(*f);
                return a;
            }
            std::uint64_t now = net::now_ns();
            if (now >= deadline) return a;
            pollfd p{c.fd(), POLLIN, 0};
            ::poll(&p, 1, int((deadline - now) / 1000000) + 1);
        }
    } catch (const std::exception& e) {
        a.outcome = Outcome::Closed;
        a.detail = e.what();
    }
    return a;
}

void handshake(net::Conn& c) {
    c.send(net::MsgType::Hello, 0, net::hello_payload());
    net::Frame ok = c.recv({net::MsgType::HelloOk, net::MsgType::Error});
    if (ok.type != net::MsgType::HelloOk) throw net::ProtocolError("handshake refused: " + ok.payload);
    (void)c.recv({net::MsgType::Catalog});
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

int run_evil(const Options& opt) {
    CheckLog log;
    metrics::Recorder rec("c");
    std::string snap_row;
    const Rect normal{{0.0f, 0.0f}, {kViewWidth, kViewHeight}};

    {   // (1) truncated ViewRequest wire
        net::Conn c = net::Conn::connect(opt.host, opt.port);
        handshake(c);
        std::string wire = view_request_wire(1, normal);
        wire.pop_back();
        c.send(net::MsgType::ViewReq, 1, wire, static_cast<std::uint16_t>(net::ViewCategory::Normal));
        expect_rejected(log, "evil_truncated", "truncated ViewRequest wire (-1 byte)", c);
    }
    {   // (2) wrong schema: a ViewSnapshot wire sent as ViewReq
        net::Conn c = net::Conn::connect(opt.host, opt.port);
        handshake(c);
        XBuffer snap = XBuffer::create<ViewSnapshot>(512, cap_snapshot());
        ViewSnapshot& v = snap.root<ViewSnapshot>();
        v.request_id = 2;
        v.scene_id = kSceneId;
        v.rect = normal;
        std::string wire = snap.save_verified<ViewSnapshot>();
        c.send(net::MsgType::ViewReq, 2, wire, static_cast<std::uint16_t>(net::ViewCategory::Normal));
        expect_rejected(log, "evil_wrong_schema", "ViewSnapshot wire sent as ViewReq", c);
    }
    {   // (3) header declaring a 1 GiB ViewReq payload, no payload follows
        net::Conn c = net::Conn::connect(opt.host, opt.port);
        handshake(c);
        net::FrameHeader h{};
        h.magic = net::kMagic;
        h.type = static_cast<std::uint16_t>(net::MsgType::ViewReq);
        h.flags = 0;
        h.length = std::uint32_t(1) << 30;
        h.reserved = 0;
        h.correlation = 3;
        const char* p = reinterpret_cast<const char*>(&h);
        std::size_t left = sizeof h;
        while (left > 0) {
            ssize_t n = ::send(c.fd(), p, left, 0);
            if (n <= 0) throw std::runtime_error(std::string("send: ") + std::strerror(errno));
            p += n;
            left -= std::size_t(n);
        }
        expect_rejected(log, "evil_oversize_header", "header declaring a 1 GiB ViewReq", c);
    }
    {   // (4) one connection: valid request, truncated wire, valid request.
        // With --buffers reuse, A's per-client request buffer is left empty
        // by the rejected reload and must be refilled by the next one.
        net::Conn c = net::Conn::connect(opt.host, opt.port);
        handshake(c);
        auto served = [&](std::uint64_t id) -> std::string {
            c.send(net::MsgType::ViewReq, id, view_request_wire(id, normal), static_cast<std::uint16_t>(net::ViewCategory::Normal));
            Awaited a = await_frame(c, 5000);
            if (a.outcome != Outcome::Frame) return a.outcome == Outcome::Closed ? "closed: " + a.detail : "no answer within 5 s";
            if (a.frame.type != net::MsgType::ViewSnapshot) return std::string("got ") + net::type_name(a.frame.type) + " " + a.frame.payload;
            if (a.frame.correlation != id) return "correlation " + std::to_string(a.frame.correlation);
            try {
                XBuffer b = XBuffer::load_verified<ViewSnapshot>(a.frame.payload, cap_snapshot());
                checks::Result r = checks::snapshot_sanity(b.root<ViewSnapshot>(), id, normal);
                return r.ok ? std::string() : r.detail;
            } catch (const std::exception& e) {
                return std::string("load_verified: ") + e.what();
            }
        };
        std::string why = served(5);
        if (why.empty()) {
            std::string bad = view_request_wire(6, normal);
            bad.pop_back();
            c.send(net::MsgType::ViewReq, 6, bad, static_cast<std::uint16_t>(net::ViewCategory::Normal));
            if (!expect_rejected(log, "evil_same_conn_rejected", "same connection: truncated wire", c)) why = "not rejected";
        }
        if (why.empty()) why = served(7);
        std::fprintf(stdout, "evil: %-40s -> %s\n", "same connection: valid request after it",
                     why.empty() ? "answered" : why.c_str());
        if (why.empty()) {
            log.pass("evil_same_conn_recovers");
            log.note("evil_same_conn_recovers", "requests 5 and 7 answered around rejected 6");
        } else {
            log.fail("evil_same_conn_recovers", why);
        }
    }
    {   // A still serves a normal client. Its snapshot is logged like any other
        // (client_snapshots.csv) so it can be matched with A's a_snapshots.csv.
        net::Conn c = net::Conn::connect(opt.host, opt.port);
        handshake(c);
        const std::uint64_t sent = net::now_ns();
        c.send(net::MsgType::ViewReq, 4, view_request_wire(4, normal), static_cast<std::uint16_t>(net::ViewCategory::Normal));
        Awaited a = await_frame(c, 5000);
        const double rtt_us = double(net::now_ns() - sent) / 1000.0;
        std::string why;
        if (a.outcome != Outcome::Frame) why = a.outcome == Outcome::Closed ? "closed: " + a.detail : "no answer within 5 s";
        else if (a.frame.type != net::MsgType::ViewSnapshot) why = std::string("got ") + net::type_name(a.frame.type) + " " + a.frame.payload;
        else if (a.frame.correlation != 4) why = "correlation " + std::to_string(a.frame.correlation);
        else {
            try {
                metrics::Stopwatch sw;
                XBuffer b = XBuffer::load_verified<ViewSnapshot>(a.frame.payload, cap_snapshot());
                const double load_us = sw.us();
                const ViewSnapshot& v = b.root<ViewSnapshot>();
                checks::Result r = checks::snapshot_sanity(v, 4, normal);
                if (!r.ok) why = r.detail;
                rec.add("c.view_rtt_us", rtt_us);
                rec.add("c.load_us", load_us);
                rec.add("c.snap_bytes", double(a.frame.payload.size()));
                rec.add("c.visible_count", double(v.units.size()));
                char line[512];
                std::snprintf(line, sizeof line, "%" PRIu64 ",%" PRIu64 ",normal,%zu,%zu,%016" PRIx64 ",%.3f,%.3f,0.000,0.000,%d",
                              std::uint64_t(v.request_id), std::uint64_t(v.tick), std::size_t(v.units.size()),
                              a.frame.payload.size(), checks::snapshot_digest(v), rtt_us, load_us, r.ok ? 1 : 0);
                snap_row = line;
            } catch (const std::exception& e) {
                why = std::string("load_verified: ") + e.what();
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
        bool ok = snaps.open(join_path(opt.csv_dir, "client_snapshots.csv"),
            "request_id,tick,category,visible,wire_bytes,digest,rtt_us,load_us,read_present_us,destroy_us,check_ok");
        if (ok && !snap_row.empty()) snaps.row(snap_row);
        if (!ok || !log.write_csv(join_path(opt.csv_dir, "client_checks.csv")) ||
            !rec.write_csv(join_path(opt.csv_dir, "client_metrics.csv")))
            throw std::runtime_error("cannot write client CSV files in " + opt.csv_dir);
    }
    log.print(stdout);
    return log.all_ok() ? 0 : 1;
}

} // namespace mapdemo::client
