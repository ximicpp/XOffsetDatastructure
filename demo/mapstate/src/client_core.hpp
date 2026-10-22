// Map state demo: client network, SnapshotHolder, sampling script. OWNER: client thread.
//
// Shared by the headless sampler, the --evil probe and the raylib window
// client. Every snapshot is its own verified XBuffer held by SnapshotHolder;
// all field reads go to that live buffer (design section 5). The holder is
// replaced only at a frame boundary, so no reference outlives its buffer.
#pragma once
#include <cstdint>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>
#include "mapdemo/checks.hpp"
#include "mapdemo/metrics.hpp"
#include "mapdemo/net.hpp"
#include "mapdemo/rules.hpp"
#include "mapdemo/schema.hpp"

namespace mapdemo::client {

struct UsageError : std::runtime_error {
    using std::runtime_error::runtime_error;
};

struct Options {
    std::string host = "127.0.0.1";
    std::uint16_t port = net::kDefaultPortA;
    bool headless = false;
    std::optional<bool> check;      // default: on for --headless, off for the window
    std::string csv_dir;            // empty: no CSV files
    bool auto_drag = false;
    std::vector<int> screenshot_at; // frame numbers (window client)
    bool evil = false;

    bool check_on() const { return check.value_or(headless); }
};

// Throws UsageError.
Options parse_options(int argc, char** argv);
const char* usage_text();

// Owns the current snapshot buffer. view() is valid until the next replace();
// callers replace only at the start of a frame and never keep references
// across frames.
class SnapshotHolder {
public:
    bool empty() const noexcept { return root_ == nullptr; }
    const ViewSnapshot& view() const noexcept { return *root_; }
    const XBuffer& buffer() const noexcept { return *buf_; }
    // Destroys the previous buffer (returns that destroy time in microseconds,
    // 0 when there was none), then takes ownership of `next`.
    double replace(XBuffer&& next);
    double clear();

private:
    std::optional<XBuffer> buf_;
    const ViewSnapshot* root_ = nullptr;
};

// Presentation data only: screen position and colour. No ids, names or hp.
struct DrawItem {
    float sx;
    float sy;
    std::uint32_t rgba;   // 0xRRGGBBAA
};
std::uint32_t behavior_rgba(Behavior b) noexcept;   // out-of-range values are grey
const char* behavior_name(Behavior b) noexcept;

// Field read + presentation data: walks the live snapshot and emits one item
// per unit inside `camera` (scene coordinates; screen = scene - camera.min).
void build_draw_items(const ViewSnapshot& snapshot, const Rect& camera, std::vector<DrawItem>& out);

// pass/fail counters per named check, written as client_checks.csv.
class CheckLog {
public:
    void pass(const std::string& name, std::uint64_t n = 1);
    void fail(const std::string& name, const std::string& detail);
    void note(const std::string& name, const std::string& detail);   // coverage text for a passing check
    bool all_ok() const;
    void print(std::FILE* out) const;
    bool write_csv(const std::string& path) const;

private:
    struct Entry {
        std::uint64_t pass = 0;
        std::uint64_t fail = 0;
        std::string first_failure;
        std::string note;
    };
    std::map<std::string, Entry> entries_;
};

const char* category_name(net::ViewCategory c) noexcept;
Rect clamp_view(Rect r, const Rect& scene) noexcept;

// One connection to server A: handshake, catalog, at most one outstanding
// ViewReq, snapshot install at frame start, metrics and CSV rows.
class Session {
public:
    explicit Session(const Options& options);
    ~Session();
    Session(const Session&) = delete;
    Session& operator=(const Session&) = delete;

    // Connect, Hello/HelloOk, receive and verify the catalog. Throws.
    void connect();

    const MapCatalog& catalog() const noexcept { return *catalog_root_; }
    const Rect& scene() const noexcept { return scene_; }

    bool can_request() const noexcept { return !inflight_ && !bye_; }
    void send_view(const Rect& rect, net::ViewCategory category);

    // Waits up to timeout_ms for the socket, writes pending bytes, reads every
    // complete frame. A snapshot frame becomes pending; Bye sets bye().
    void pump(int timeout_ms);
    bool bye() const noexcept { return bye_; }
    bool has_pending() const noexcept { return pending_.has_value(); }

    // Frame start: load_verified the pending snapshot and replace the holder.
    // Returns false when nothing was pending.
    bool install_pending();
    const SnapshotHolder& holder() const noexcept { return holder_; }
    net::ViewCategory holder_category() const noexcept { return cur_.category; }
    const Rect& holder_request_rect() const noexcept { return cur_.rect; }

    // Timing of field read + presentation data for the current snapshot.
    void note_read_present(double us);
    // Direct-read evidence (check mode): every unit and name byte read from
    // the current snapshot lies inside the holder's live buffer.
    void verify_direct_read();

    double last_rtt_us() const noexcept { return last_rtt_us_; }
    double last_load_us() const noexcept { return last_load_us_; }
    std::size_t last_wire_bytes() const noexcept { return cur_.wire_bytes; }
    std::uint64_t snapshots() const noexcept { return snapshots_; }

    metrics::Recorder& recorder() noexcept { return rec_; }
    CheckLog& checks() noexcept { return checks_; }

    // Destroys the last snapshot, writes CSV files (when --csv-dir) and
    // returns true when every check passed.
    bool finish();

private:
    struct Record {
        bool live = false;
        std::uint64_t request_id = 0;
        std::uint64_t tick = 0;
        net::ViewCategory category = net::ViewCategory::Normal;
        Rect rect{};
        std::size_t visible = 0;
        std::size_t wire_bytes = 0;
        std::uint64_t digest = 0;
        double rtt_us = 0, load_us = 0, read_present_sum = 0;
        std::uint64_t read_present_n = 0;
        bool check_ok = true;
    };
    void retire_current(double destroy_us);
    void check_snapshot(const net::Frame& frame);

    const Options& opt_;
    net::Conn conn_;
    std::optional<XBuffer> catalog_;
    const MapCatalog* catalog_root_ = nullptr;
    Rect scene_ = scene_bounds();

    bool inflight_ = false;
    bool bye_ = false;
    std::uint64_t next_request_id_ = 1;
    std::uint64_t sent_request_id_ = 0;
    net::ViewCategory sent_category_ = net::ViewCategory::Normal;
    Rect sent_rect_{};
    std::uint64_t sent_ns_ = 0;

    struct Pending {
        net::Frame frame;
        double rtt_us;
        net::ViewCategory category;
        Rect rect;
    };
    std::optional<Pending> pending_;

    SnapshotHolder holder_;
    Record cur_;
    double last_rtt_us_ = 0, last_load_us_ = 0;
    std::uint64_t snapshots_ = 0;
    std::optional<std::size_t> full_scene_visible_;
    std::uint64_t direct_units_ = 0, direct_names_ = 0;

    metrics::Recorder rec_{"c"};
    CheckLog checks_;
    metrics::CsvFile snap_csv_;
    bool snap_csv_open_ = false;
};

// Entry points (client_main.cpp dispatches). Return the process exit code.
int run_headless(const Options& options);
int run_evil(const Options& options);
#if defined(MAPDEMO_WITH_GUI)
int run_gui(const Options& options);   // client_gui.cpp
#endif

} // namespace mapdemo::client
