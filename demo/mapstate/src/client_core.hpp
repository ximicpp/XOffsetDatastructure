// Map state demo: client network, SnapshotHolder, sampling script. OWNER: client thread.
//
// Shared by the headless sampler, the --evil probe, the offline --self-check
// and --replay modes and the raylib window client. Every snapshot is its own
// verified XBuffer held by SnapshotHolder; all field reads go to that live
// buffer (design section 5). The holder is replaced only at a frame
// boundary, so no reference outlives its buffer. A compressed snapshot is
// decompressed first (codec.hpp) and then verified like any other wire.
//
// Nothing here throws: failures come back as an Error (result.hpp), and
// snapshots are verified with try_load_verified / try_reload_verified, so
// the same sources build with and without exceptions.
#pragma once
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include "mapdemo/buffer_mode.hpp"
#include "mapdemo/checks.hpp"
#include "mapdemo/codec.hpp"
#include "mapdemo/metrics.hpp"
#include "mapdemo/net.hpp"
#include "mapdemo/result.hpp"
#include "mapdemo/rules.hpp"
#include "mapdemo/schema.hpp"

namespace mapdemo::client {

struct Options {
    bool help = false;              // --help: print usage_text() and exit 0
    std::string host = "127.0.0.1";
    std::uint16_t port = net::kDefaultPortA;
    bool headless = false;
    std::optional<bool> check;      // default: on for --headless, off for the window
    std::string csv_dir;            // empty: no CSV files
    bool auto_drag = false;
    std::vector<int> screenshot_at; // frame numbers (window client)
    bool evil = false;
    bool self_check = false;
    std::string replay_dir;         // --replay: verify a corpus written by mapdemo_a --corpus-dir
    BufferMode buffers = kDefaultBufferMode;
    // Snapshot encoding requested from A; cycle_encodings: dense, compact,
    // zstd in turn (request id order).
    codec::Encoding encoding = codec::zstd_available() ? codec::Encoding::Zstd : codec::Encoding::Compact;
    bool cycle_encodings = false;
    // Request ids start here (1): a second client on the same A takes
    // another range, so A's log keeps one row per request id.
    std::uint64_t first_request_id = 1;
    // --events: subscribe to A's event stream in this mode (codec::EventEncoding
    // or codec::kEventCycle); kEventsOff (default) does not subscribe.
    std::uint16_t events = kEventsOff;
    std::string events_dict;        // --events-dict FILE; empty: the built-in dictionary
    static constexpr std::uint16_t kEventsOff = 0xffff;
    // --edits N: write back every Nth normal snapshot (headless; 0 = off).
    // The client changes the snapshot in its own buffer, sends it to A with
    // save_verified (Edit) and receives the other writers' edits (Forward).
    std::uint32_t edits = 0;

    bool check_on() const { return check.value_or(headless); }
};

// An Error names the bad option.
Result<Options> parse_options(int argc, char** argv);
const char* usage_text();

// The XOffset receive path's error as the client reports it: its message,
// the text load_verified throws in a build with exceptions.
inline Error xoffset_error(const XError& e) { return Error{e.message}; }

// Owns the current snapshot buffer. view() is valid until the next replace();
// callers replace only at the start of a frame and never keep references
// across frames.
class SnapshotHolder {
public:
    bool empty() const noexcept { return root_ == nullptr; }
    const ViewSnapshot& view() const noexcept { return *root_; }
    const XBuffer& buffer() const noexcept { return *buf_; }
    // --buffers fresh: destroys the previous buffer (returns that destroy
    // time in microseconds, 0 when there was none), then takes ownership of
    // `next`.
    double replace(XBuffer&& next);
    // --buffers reuse: verifies `wire` into the held buffer in place
    // (try_reload_verified; try_load_verified the first time). Everything
    // read from the old content dies here. On a rejected wire the holder is
    // empty and the next reload starts from that empty buffer.
    Status reload(const std::string& wire);
    // Either of the two above, by mode; returns the verify time in us
    // (fresh: *destroy_us gets the old buffer's destroy time). A rejected
    // wire leaves a fresh holder as it was.
    Result<double> install(const std::string& wire, BufferMode mode, double* destroy_us);
    double clear();
    // Write-back (--edits): the held snapshot, writable in place. A change
    // may grow the buffer (up to cap_snapshot()); call edited() after it,
    // before view() is read again.
    ViewSnapshot& edit() noexcept { return buf_->root<ViewSnapshot>(); }
    void edited() noexcept { root_ = &buf_->root<ViewSnapshot>(); }
    void save_verified_into(std::string& out) { buf_->save_verified_into<ViewSnapshot>(out); }

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

// Write-back: the changes a client makes to a snapshot it received, in
// place (the rect must have a non-zero area): the middle unit erased, the
// first remaining unit's hp (kept in 50..100) and behavior changed, a unit
// added at the rect centre (id kEditIdBase + seq, a 20-byte name, too long
// to be stored inline) and the first short (inline) name made long. The
// unit count stays the same (or grows by one from an empty view), and the
// result passes checks::edit_sanity_of, the rules A applies to it.
inline constexpr std::uint32_t kEditIdBase = 0x80000000u;
struct EditSummary {
    static constexpr std::uint32_t kNone = 0xffffffffu;   // no such unit in the snapshot
    std::uint32_t erased_id = kNone;
    std::uint32_t changed_id = kNone;   // hp and behavior
    std::uint32_t added_id = 0;
    std::uint32_t renamed_id = kNone;   // short name made long
};
EditSummary apply_client_edit(ViewSnapshot& snapshot, std::uint64_t seq);

const char* category_name(net::ViewCategory c) noexcept;
std::string join_path(const std::string& dir, const char* file);   // dir/file; file alone when dir is empty

// client_snapshots.csv (INTERFACES.md section 6): one row per snapshot.
// wire_bytes / wire_digest describe the XOffset wire that was verified;
// net_bytes is the frame payload (smaller for zstd).
inline constexpr const char* kEventsCsvHeader =
    "tick,part,encoding,count,wire_bytes,net_bytes,digest,decode_us,verify_us,check_ok";

inline constexpr const char* kSnapshotCsvHeader =
    "request_id,tick,category,visible,wire_bytes,digest,rtt_us,load_us,read_present_us,destroy_us,check_ok,"
    "encoding,net_bytes,wire_digest,decompress_us";
// client_edits.csv (--edits): one row per write-back sent; edit_id is the
// snapshot's request id, digest / wire_digest those of the edited snapshot
// and of the Edit payload (A logs both in a_edits.csv), acked 1 when A's
// EditAck came back. client_forwards.csv: one row per Forward received.
inline constexpr const char* kEditsCsvHeader =
    "edit_id,units,wire_bytes,digest,wire_digest,edit_us,save_us,rtt_us,acked,erased_id,changed_id,added_id,renamed_id";
inline constexpr const char* kForwardsCsvHeader =
    "edit_id,units,wire_bytes,digest,a_digest,wire_digest,load_us,check_ok";
Rect clamp_view(Rect r, const Rect& scene) noexcept;

// One connection to server A: handshake, catalog, at most one outstanding
// ViewReq, snapshot install at frame start, metrics and CSV rows.
class Session {
public:
    explicit Session(const Options& options);
    ~Session();
    Session(const Session&) = delete;
    Session& operator=(const Session&) = delete;

    // Connect, Hello/HelloOk, receive and verify the catalog.
    Status connect();

    const MapCatalog& catalog() const noexcept { return *catalog_root_; }
    const Rect& scene() const noexcept { return scene_; }

    bool can_request() const noexcept { return !inflight_ && !bye_; }
    // Encoding: --encoding, or the next one in turn with --encoding cycle.
    Status send_view(const Rect& rect, net::ViewCategory category);

    // Waits up to timeout_ms for the socket, writes pending bytes, reads every
    // complete frame. A snapshot frame becomes pending; Bye sets bye().
    Status pump(int timeout_ms);
    bool bye() const noexcept { return bye_; }
    bool has_pending() const noexcept { return pending_.has_value(); }

    // Frame start: decode (zstd) and verify the pending snapshot into the
    // holder. False when nothing was pending; an error for a snapshot that
    // does not decode or verify.
    Result<bool> install_pending();
    const SnapshotHolder& holder() const noexcept { return holder_; }
    net::ViewCategory holder_category() const noexcept { return cur_.category; }
    const Rect& holder_request_rect() const noexcept { return cur_.rect; }

    // --edits: after the snapshot's checks, every Nth normal snapshot is
    // changed in place (apply_client_edit) and sent to A as an Edit; one
    // edit is outstanding at a time.
    Status maybe_edit();

    // Timing of field read + presentation data for the current snapshot.
    void note_read_present(double us);
    // Direct-read evidence (check mode): every unit and name byte read from
    // the current snapshot lies inside the holder's live buffer.
    void verify_direct_read();

    double last_rtt_us() const noexcept { return last_rtt_us_; }
    double last_load_us() const noexcept { return last_load_us_; }
    std::size_t last_wire_bytes() const noexcept { return cur_.wire_bytes; }
    std::size_t last_net_bytes() const noexcept { return cur_.net_bytes; }
    codec::Encoding last_encoding() const noexcept { return cur_.encoding; }
    std::uint64_t snapshots() const noexcept { return snapshots_; }

    metrics::Recorder& recorder() noexcept { return rec_; }
    CheckLog& checks() noexcept { return checks_; }

    // Destroys the last snapshot, writes CSV files (when --csv-dir); true
    // when every check passed, an error when a CSV file cannot be written.
    Result<bool> finish();

private:
    struct Record {
        bool live = false;
        std::uint64_t request_id = 0;
        std::uint64_t tick = 0;
        net::ViewCategory category = net::ViewCategory::Normal;
        Rect rect{};
        std::size_t visible = 0;
        std::size_t wire_bytes = 0;      // the XOffset wire (after decompression)
        std::size_t net_bytes = 0;       // the frame payload
        codec::Encoding encoding = codec::Encoding::Dense;
        std::uint64_t digest = 0;
        std::uint64_t wire_digest = 0;
        double rtt_us = 0, load_us = 0, decompress_us = -1, read_present_sum = 0;
        std::uint64_t read_present_n = 0;
        bool check_ok = true;
    };
    void retire_current(double destroy_us);
    void check_snapshot(const net::Frame& frame);
    Status subscribe();
    Status on_events(net::Frame& frame);
    void on_edit_ack(const net::Frame& frame);
    Status on_forward(net::Frame& frame);
    void write_edit_row(bool acked, double rtt_us);

    const Options& opt_;
    net::Conn conn_;
    std::optional<XBuffer> catalog_;
    const MapCatalog* catalog_root_ = nullptr;
    Rect scene_ = scene_bounds();
    std::optional<XBuffer> req_buf_;   // reuse: reset<ViewRequest>() for every request

    bool inflight_ = false;
    bool bye_ = false;
    std::uint64_t next_request_id_ = 1;
    std::uint64_t sent_request_id_ = 0;
    net::ViewCategory sent_category_ = net::ViewCategory::Normal;
    codec::Encoding sent_encoding_ = codec::Encoding::Dense;
    Rect sent_rect_{};
    std::uint64_t sent_ns_ = 0;

    struct Pending {
        net::Frame frame;
        double rtt_us;
        net::ViewCategory category;
        codec::Encoding encoding;
        Rect rect;
    };
    std::optional<Pending> pending_;
    codec::Decompressor unzstd_;
    std::string plain_;                // zstd: the decompressed wire, capacity kept

    SnapshotHolder holder_;
    Record cur_;
    double last_rtt_us_ = 0, last_load_us_ = 0;
    std::uint64_t snapshots_ = 0;
    std::uint64_t by_encoding_[codec::kEncodingCount] = {};
    std::optional<std::size_t> full_scene_visible_;
    std::uint64_t direct_units_ = 0, direct_names_ = 0;

    metrics::Recorder rec_{"c"};
    CheckLog checks_;
    metrics::CsvFile snap_csv_;
    bool snap_csv_open_ = false;

    // event stream (--events)
    codec::EventDecoder events_dec_;
    codec::EventEncoder events_raw_;          // re-packs verified wires for the digest
    std::vector<std::string_view> event_wires_;
    std::string event_raw_;
    std::optional<XBuffer> event_buf_;        // reuse: reloaded for every event
    std::uint64_t event_frames_[codec::kEventEncodingCount] = {};
    std::uint64_t events_ = 0, last_event_tick_ = 0, event_part_ = 0;
    metrics::CsvFile events_csv_;
    bool events_csv_open_ = false;

    // write-back (--edits)
    struct EditRecord {
        std::uint64_t edit_id = 0;
        std::size_t units = 0, wire_bytes = 0;
        std::uint64_t digest = 0, wire_digest = 0;
        double edit_us = 0, save_us = 0;
        EditSummary what;
    };
    std::uint64_t edit_counter_ = 0;          // normal snapshots seen with --edits
    bool edit_inflight_ = false;
    std::uint64_t edit_sent_ns_ = 0;
    EditRecord edit_;
    std::uint64_t edits_sent_ = 0, edits_acked_ = 0, forwards_ = 0;
    std::optional<XBuffer> fwd_buf_;          // reuse: reloaded for every Forward
    metrics::CsvFile edits_csv_, forwards_csv_;
    bool edits_csv_open_ = false;
};

// Entry points (client_main.cpp dispatches). Return the process exit code
// (0 every check passed, 1 a check failed), or an Error that stopped the run
// (exit 2).
Result<int> run_headless(const Options& options);
Result<int> run_evil(const Options& options);
Result<int> run_self_check(const Options& options);   // client_offline.cpp
Result<int> run_replay(const Options& options);       // client_offline.cpp
#if defined(MAPDEMO_WITH_GUI)
Result<int> run_gui(const Options& options);   // client_gui.cpp
#endif

} // namespace mapdemo::client
