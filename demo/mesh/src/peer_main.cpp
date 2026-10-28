// Device mesh demo: the device program (xo_peer, xo_peer_noexc).
// OWNER: network thread (demo/mesh/INTERFACES.md section 6).
//
//   xo_peer (--hub HOST:PORT | --join URL) [--name NAME] [--evil] [--out DIR] [--timeout-s 600]
//   xo_peer --mailbox IN_DIR OUT_DIR --id N [--name NAME] [--list] [--out DIR]
//
// One TCP connection to the hub: Hello, Register, then one message at a
// time in arrival order. A Job is answered with start_job, a Hop with
// handle_hop (hop.hpp); the device sends the next hop (when there is one)
// to the next route device through the hub, then its Report. Bye ends the
// run with exit 0. Nothing here throws: the same source builds
// xo_peer_noexc.
//
// Exit: 0 after Bye; 1 on a handshake, protocol or network failure (or
// --timeout-s passing first); 2 on a usage error.
//
// Mailbox mode (cross-site runs without a live link, INTERFACES.md 6):
// the same start_job / handle_hop on files instead of frames. It answers
// every Job and Hop file in IN_DIR addressed to --id that has no report in
// IN_DIR or OUT_DIR yet, writes the next hop and the report into OUT_DIR
// (file names: src/peer/mailbox.hpp), and repeats until nothing is left.
// --list prints those files instead. Exit 0 unless a file could not be
// read or written; the verdicts are the hub's to score (xo_hub --mailbox-score).
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <set>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>
#include "peer/control.hpp"
#include "peer/mailbox.hpp"
#include "xmesh/hop.hpp"
#include "xmesh/net.hpp"
#include "xmesh/protocol.hpp"
#include "xmesh/types.hpp"

namespace {

using namespace xmesh;
using protocol::MsgType;

struct Options {
    std::string hub_host;
    std::uint16_t hub_port = protocol::kDefaultPort;
    std::string join_url;
    std::string name;
    bool evil = false;
    std::string out_dir;
    int timeout_s = 600;
    int connect_s = 30;
    bool mailbox = false;
    std::string mailbox_in, mailbox_out;
    std::uint32_t id = 0;
    bool list = false;
};

void usage() {
    std::fprintf(stderr,
                 "usage: xo_peer (--hub HOST:PORT | --join URL) [--name NAME] [--evil] [--out DIR]\n"
                 "               [--timeout-s 600] [--connect-s 30]\n"
                 "       xo_peer --mailbox IN_DIR OUT_DIR --id N [--name NAME] [--list] [--out DIR]\n"
                 "  --join takes the hub's join link, http://HOST:47802/join?hub=HOST:47801&run=LABEL\n");
}

// Percent-decoding for a query value.
std::string url_decode(std::string_view s) {
    std::string out;
    for (std::size_t i = 0; i < s.size(); ++i) {
        const char c = s[i];
        if (c == '+') {
            out += ' ';
        } else if (c == '%' && i + 2 < s.size()) {
            auto hex = [](char h) -> int {
                if (h >= '0' && h <= '9') return h - '0';
                if (h >= 'a' && h <= 'f') return h - 'a' + 10;
                if (h >= 'A' && h <= 'F') return h - 'A' + 10;
                return -1;
            };
            const int a = hex(s[i + 1]), b = hex(s[i + 2]);
            if (a < 0 || b < 0) {
                out += c;
                continue;
            }
            out += char(a * 16 + b);
            i += 2;
        } else {
            out += c;
        }
    }
    return out;
}

// The hub address from a join link: its `hub` query value, or the link's
// own host with the default frame port.
bool parse_join(std::string_view url, std::string& host, std::uint16_t& port) {
    std::string_view rest = url;
    if (rest.rfind("http://", 0) == 0) rest.remove_prefix(7);
    else if (rest.rfind("https://", 0) == 0) rest.remove_prefix(8);
    else if (rest.rfind("xmesh://", 0) == 0) rest.remove_prefix(8);
    const std::size_t q = rest.find('?');
    const std::string_view authority_path = rest.substr(0, q);
    if (q != std::string_view::npos) {
        std::string_view query = rest.substr(q + 1);
        const std::size_t hash = query.find('#');
        if (hash != std::string_view::npos) query = query.substr(0, hash);
        while (!query.empty()) {
            const std::size_t amp = query.find('&');
            const std::string_view kv = query.substr(0, amp);
            if (kv.rfind("hub=", 0) == 0)
                return net::split_host_port(url_decode(kv.substr(4)), host, port, protocol::kDefaultPort);
            if (amp == std::string_view::npos) break;
            query.remove_prefix(amp + 1);
        }
    }
    const std::string_view authority = authority_path.substr(0, authority_path.find('/'));
    std::uint16_t ignored = 0;
    if (!net::split_host_port(authority, host, ignored, protocol::kDefaultHttpPort)) return false;
    port = protocol::kDefaultPort;
    return true;
}

bool parse_int(const char* s, long long lo, long long hi, long long& out) {
    if (s == nullptr || *s == '\0') return false;
    char* end = nullptr;
    const long long v = std::strtoll(s, &end, 10);
    if (*end != '\0' || v < lo || v > hi) return false;
    out = v;
    return true;
}

// 0 ok, 2 usage error.
int parse_args(int argc, char** argv, Options& o) {
    for (int i = 1; i < argc; ++i) {
        const std::string_view a = argv[i];
        auto value = [&](const char*& v) {
            if (i + 1 >= argc) return false;
            v = argv[++i];
            return true;
        };
        const char* v = nullptr;
        long long n = 0;
        if (a == "--hub") {
            if (!value(v) || !net::split_host_port(v, o.hub_host, o.hub_port, protocol::kDefaultPort)) return 2;
        } else if (a == "--join") {
            if (!value(v)) return 2;
            o.join_url = v;
            if (!parse_join(o.join_url, o.hub_host, o.hub_port)) {
                std::fprintf(stderr, "error: cannot read a hub address from the join link %s\n", v);
                return 2;
            }
        } else if (a == "--name") {
            if (!value(v)) return 2;
            o.name = v;
        } else if (a == "--evil") {
            o.evil = true;
        } else if (a == "--out") {
            if (!value(v)) return 2;
            o.out_dir = v;
        } else if (a == "--timeout-s") {
            if (!value(v) || !parse_int(v, 1, 7 * 24 * 3600, n)) return 2;
            o.timeout_s = int(n);
        } else if (a == "--connect-s") {
            if (!value(v) || !parse_int(v, 0, 3600, n)) return 2;
            o.connect_s = int(n);
        } else if (a == "--mailbox") {
            if (i + 2 >= argc) return 2;
            o.mailbox = true;
            o.mailbox_in = argv[++i];
            o.mailbox_out = argv[++i];
        } else if (a == "--id") {
            if (!value(v) || !parse_int(v, 1, 0xffffffffll, n)) return 2;
            o.id = std::uint32_t(n);
        } else if (a == "--list") {
            o.list = true;
        } else if (a == "-h" || a == "--help") {
            usage();
            std::exit(0);
        } else {
            std::fprintf(stderr, "error: unknown argument %s\n", argv[i]);
            return 2;
        }
    }
    if (o.mailbox && o.id == 0) {
        std::fprintf(stderr, "error: --mailbox needs --id N (the device id the plan gave this device)\n");
        return 2;
    }
    if (!o.mailbox && o.hub_host.empty()) {
        std::fprintf(stderr, "error: give --hub HOST:PORT or --join URL\n");
        return 2;
    }
    if (o.name.empty()) o.name = control::default_name();
    if (o.name.size() > 64) o.name.resize(64);
    for (char c : o.name)
        if (c < 0x21 || c > 0x7e || c == '>' || c == ',' || c == '"') {
            std::fprintf(stderr, "error: --name: printable ASCII without spaces, '>', ',' or '\"'\n");
            return 2;
        }
    return 0;
}

// Prints to stdout and appends to OUT_DIR/peer.log.
class Log {
public:
    explicit Log(const std::string& dir) {
        if (dir.empty()) return;
        file_ = std::fopen((dir + "/peer.log").c_str(), "w");
    }
    ~Log() {
        if (file_ != nullptr) std::fclose(file_);
    }
    Log(const Log&) = delete;
    Log& operator=(const Log&) = delete;
    void line(const std::string& text) {
        std::fprintf(stdout, "%s\n", text.c_str());
        std::fflush(stdout);
        if (file_ != nullptr) {
            std::fprintf(file_, "%s\n", text.c_str());
            std::fflush(file_);
        }
    }
    bool ok(const std::string& dir) const { return dir.empty() || file_ != nullptr; }

private:
    std::FILE* file_ = nullptr;
};

void write_text(const std::string& path, const std::string& text) {
    if (std::FILE* f = std::fopen(path.c_str(), "w")) {
        std::fputs(text.c_str(), f);
        std::fputc('\n', f);
        std::fclose(f);
    }
}

struct Peer {
    Options opt;
    Log& log;
    net::Conn conn;
    std::uint32_t id = 0;
    std::uint64_t handled = 0, ok = 0, refused = 0;

    Status handshake() {
        Result<net::Conn> c = net::Conn::try_connect(opt.hub_host, opt.hub_port, opt.connect_s * 1000);
        if (!c) return std::move(c).error();
        conn = std::move(*c);
        XMESH_TRY(conn.try_send(MsgType::Hello, 0, protocol::hello(opt.name)));
        Result<std::optional<net::Frame>> ack = conn.try_recv({MsgType::HelloAck}, 30000);
        if (!ack) return Error{"handshake: " + ack.error().message};
        if (!*ack) return Error{"handshake: no HelloAck within 30 s"};
        const XSchemaAckResult r = protocol::read_ack((*ack)->payload);
        if (!r) {
            std::string why = r.message;
            Result<std::optional<net::Frame>> err = conn.try_recv({MsgType::Error}, 2000);
            if (err && *err) why += (why.empty() ? "" : "; ") + std::string("hub: ") + (*err)->payload;
            return Error{"the hub refused this device's schema (" + why + ")"};
        }
        control::Peer me{opt.name, control::build_info(), control::own_flags(opt.evil), protocol::kMaxDataBytes};
        if (me.build.size() > 256) me.build.resize(256);
        XMESH_TRY(conn.try_send(MsgType::Register, 0, control::peer_info_wire(me)));
        Result<std::optional<net::Frame>> w = conn.try_recv({MsgType::Welcome}, 30000);
        if (!w) return Error{"register: " + w.error().message};
        if (!*w) return Error{"register: no Welcome within 30 s"};
        Result<control::WelcomeInfo> welcome = control::read_welcome((*w)->payload);
        if (!welcome) return Error{"Welcome: " + welcome.error().message};
        id = welcome->device_id;
        log.line("joined run " + welcome->run_label + " as device " + std::to_string(id) + " (" + opt.name +
                 "); hub " + welcome->hub_build);
        return {};
    }

    // Sends what a Job or Hop produced: the next hop first, then the Report.
    Status answer(const net::Frame& f, const HopOutcome& out, const char* what) {
        ++handled;
        if (!out.next_payload.empty())
            XMESH_TRY(conn.try_send(MsgType::Hop, out.job, out.next_payload, id, out.next_device));
        if (!out.report.empty()) {
            XMESH_TRY(conn.try_send(MsgType::Report, out.job, out.report, id, protocol::kHubId));
        } else {
            // No readable meta, so no Report: tell the hub which job failed.
            std::string text = std::string(what) + " " + std::to_string(f.correlation) + ": " +
                               protocol::verdict_name(out.verdict) + ": " + out.error;
            if (text.size() > 4000) text.resize(4000);
            XMESH_TRY(conn.try_send(MsgType::Error, f.correlation, text, id, protocol::kHubId));
        }
        if (out.verdict == protocol::HopVerdict::Ok) ++ok;
        else ++refused;
        std::string line = "job " + std::to_string(out.job ? out.job : f.correlation) + " " + what + " " +
                           protocol::verdict_name(out.verdict);
        if (!out.next_payload.empty()) line += " -> device " + std::to_string(out.next_device);
        if (!out.summary.empty()) line += " | " + out.summary;
        if (!out.error.empty()) line += " | " + out.error;
        log.line(line);
        return {};
    }

    int run() {
        if (Status s = handshake(); !s) {
            log.line("error: " + s.error().message);
            return 1;
        }
        const std::uint64_t deadline = net::now_ms() + std::uint64_t(opt.timeout_s) * 1000;
        for (;;) {
            const std::uint64_t now = net::now_ms();
            if (now >= deadline) {
                log.line("error: no Bye within --timeout-s " + std::to_string(opt.timeout_s));
                return 1;
            }
            Result<std::optional<net::Frame>> f =
                conn.try_recv({MsgType::Roster, MsgType::Job, MsgType::Hop, MsgType::Bye, MsgType::Error},
                              int(std::min<std::uint64_t>(deadline - now, 1000)));
            if (!f) {
                log.line("error: " + f.error().message);
                return 1;
            }
            if (!*f) continue;
            const net::Frame& frame = **f;
            switch (frame.type) {
                case MsgType::Roster: {
                    Result<control::RosterInfo> r = control::read_roster(frame.payload);
                    if (!r) {
                        log.line("error: Roster: " + r.error().message);
                        return 1;
                    }
                    std::string names;
                    for (const auto& d : r->devices) names += (names.empty() ? "" : ", ") + d.name;
                    log.line("roster v" + std::to_string(r->version) + ": " + std::to_string(r->devices.size()) +
                             " devices (" + names + ")");
                    break;
                }
                case MsgType::Job: {
                    const HopOutcome out = start_job(frame.payload, id);
                    if (Status s = answer(frame, out, "start"); !s) {
                        log.line("error: " + s.error().message);
                        return 1;
                    }
                    break;
                }
                case MsgType::Hop: {
                    const HopOutcome out = handle_hop(frame.payload, id);
                    const std::string what = "hop from " + std::to_string(frame.from);
                    if (Status s = answer(frame, out, what.c_str()); !s) {
                        log.line("error: " + s.error().message);
                        return 1;
                    }
                    break;
                }
                case MsgType::Error:
                    log.line("error: the hub sent: " + frame.payload);
                    return 1;
                case MsgType::Bye:
                    log.line("bye: handled " + std::to_string(handled) + " (" + std::to_string(ok) + " ok, " +
                             std::to_string(refused) + " refused)");
                    conn.close();
                    return 0;
                default:
                    break;
            }
        }
    }
};

// ---- mailbox mode ------------------------------------------------------------------------

namespace fs = std::filesystem;

// Reads a whole file of at most `limit` bytes.
Status read_file(const fs::path& path, std::uint64_t limit, std::string& out) {
    std::FILE* f = std::fopen(path.string().c_str(), "rb");
    if (f == nullptr) return Error{"cannot open " + path.string()};
    out.clear();
    char buf[65536];
    for (;;) {
        const std::size_t n = std::fread(buf, 1, sizeof buf, f);
        if (n == 0) break;
        if (out.size() + n > limit) {
            std::fclose(f);
            return Error{path.filename().string() + " is over " + std::to_string(limit) + " bytes"};
        }
        out.append(buf, n);
    }
    const bool failed = std::ferror(f) != 0;
    std::fclose(f);
    if (failed) return Error{"cannot read " + path.string()};
    return {};
}

// Writes `<dir>/<name>.tmp`, then renames it to `<dir>/<name>`.
Status write_file(const fs::path& dir, const std::string& name, std::string_view data) {
    const fs::path tmp = dir / (name + ".tmp");
    std::FILE* f = std::fopen(tmp.string().c_str(), "wb");
    if (f == nullptr) return Error{"cannot write " + tmp.string()};
    const bool ok = std::fwrite(data.data(), 1, data.size(), f) == data.size();
    const bool closed = std::fclose(f) == 0;
    if (!ok || !closed) return Error{"cannot write " + tmp.string()};
    std::error_code ec;
    fs::rename(tmp, dir / name, ec);
    if (ec) return Error{"cannot rename " + tmp.string() + ": " + ec.message()};
    return {};
}

bool file_exists(const fs::path& p) {
    std::error_code ec;
    return fs::exists(p, ec);
}

struct MailItem {
    mailbox::FileName name;
    std::uint32_t pos = 0;
    fs::path path;
};

// The Job and Hop files in IN addressed to this device without a report
// (or error file) in IN or OUT, by job and position.
Result<std::vector<MailItem>> pending(const Options& o) {
    const fs::path in(o.mailbox_in), out(o.mailbox_out);
    std::vector<MailItem> items;
    std::error_code ec;
    fs::directory_iterator it(in, ec);
    if (ec) return Error{"cannot list " + in.string() + ": " + ec.message()};
    for (const fs::directory_iterator end; it != end; it.increment(ec)) {
        if (ec) return Error{"cannot list " + in.string() + ": " + ec.message()};
        const std::string file = it->path().filename().string();
        const std::optional<mailbox::FileName> n = mailbox::parse(file);
        if (!n || n->device != o.id) continue;
        if (n->kind != mailbox::FileKind::Job && n->kind != mailbox::FileKind::Hop) continue;
        MailItem m{*n, n->kind == mailbox::FileKind::Job ? 0u : n->number + 1, it->path()};
        const std::string rep = mailbox::report_file(n->job, m.pos, o.id);
        const std::string err = mailbox::error_file(n->job, m.pos, o.id);
        if (file_exists(in / rep) || file_exists(in / err) || file_exists(out / rep) || file_exists(out / err)) continue;
        items.push_back(std::move(m));
    }
    std::sort(items.begin(), items.end(), [](const MailItem& a, const MailItem& b) {
        return a.name.job != b.name.job ? a.name.job < b.name.job : a.pos < b.pos;
    });
    return items;
}

int run_mailbox(const Options& o, Log& log) {
    const fs::path out(o.mailbox_out);
    if (o.list) {
        Result<std::vector<MailItem>> items = pending(o);
        if (!items) {
            log.line("error: " + items.error().message);
            return 1;
        }
        for (const MailItem& m : *items) log.line(m.path.filename().string());
        return 0;
    }
    std::error_code ec;
    fs::create_directories(out, ec);
    if (Status s = write_file(out, mailbox::build_file(o.id), control::build_info() + "\n"); !s) {
        log.line("error: " + s.error().message);
        return 1;
    }
    std::set<std::string> done;   // handled in this run (also when writing its answer failed)
    std::uint64_t handled = 0, ok = 0, refused = 0, io_errors = 0;
    for (;;) {
        Result<std::vector<MailItem>> items = pending(o);
        if (!items) {
            log.line("error: " + items.error().message);
            return 1;
        }
        std::size_t this_pass = 0;
        for (const MailItem& m : *items) {
            const std::string file = m.path.filename().string();
            if (!done.insert(file).second) continue;
            ++this_pass;
            ++handled;
            std::string payload;
            HopOutcome res;
            if (Status s = read_file(m.path, protocol::max_payload(protocol::MsgType::Hop), payload); !s) {
                res.verdict = protocol::HopVerdict::BadEnvelope;
                res.error = s.error().message;
            } else if (m.name.kind == mailbox::FileKind::Job) {
                res = start_job(payload, o.id);
            } else {
                res = handle_hop(payload, o.id);
            }
            Status written;
            if (!res.next_payload.empty())
                written = write_file(out, mailbox::hop_file(m.name.job, m.pos, res.next_device), res.next_payload);
            // The report goes last: it marks the file as answered.
            if (written) {
                if (!res.report.empty()) {
                    written = write_file(out, mailbox::report_file(m.name.job, m.pos, o.id), res.report);
                } else {
                    std::string text = file + ": " + protocol::verdict_name(res.verdict) + ": " + res.error;
                    if (text.size() > 4000) text.resize(4000);
                    written = write_file(out, mailbox::error_file(m.name.job, m.pos, o.id), text + "\n");
                }
            }
            if (!written) {
                ++io_errors;
                log.line("error: " + written.error().message);
                continue;
            }
            if (res.verdict == protocol::HopVerdict::Ok) ++ok;
            else ++refused;
            std::string line = "job " + std::to_string(m.name.job) + " position " + std::to_string(m.pos) + " " +
                               protocol::verdict_name(res.verdict);
            if (!res.next_payload.empty()) line += " -> device " + std::to_string(res.next_device);
            if (!res.summary.empty()) line += " | " + res.summary;
            if (!res.error.empty()) line += " | " + res.error;
            log.line(line);
        }
        if (this_pass == 0) break;
    }
    log.line("mailbox: device " + std::to_string(o.id) + " handled " + std::to_string(handled) + " (" +
             std::to_string(ok) + " ok, " + std::to_string(refused) + " refused, " + std::to_string(io_errors) +
             " not written)");
    return io_errors == 0 ? 0 : 1;
}

} // namespace

int main(int argc, char** argv) {
    Options opt;
    if (parse_args(argc, argv, opt) != 0) {
        usage();
        return 2;
    }
    if (!opt.out_dir.empty()) {
        std::error_code ec;
        fs::create_directories(opt.out_dir, ec);
    }
    Log log(opt.out_dir);
    if (!log.ok(opt.out_dir)) {
        std::fprintf(stderr, "error: cannot write %s/peer.log\n", opt.out_dir.c_str());
        return 2;
    }
    if (opt.mailbox) {
        if (!opt.list)
            log.line("xo_peer " + opt.name + " (mailbox, device " + std::to_string(opt.id) + "): " +
                     control::build_info() + "; in " + opt.mailbox_in + ", out " + opt.mailbox_out);
        return run_mailbox(opt, log);
    }
    const std::string build = control::build_info();
    if (!opt.out_dir.empty()) write_text(opt.out_dir + "/build.txt", build);
    log.line("xo_peer " + opt.name + ": " + build + "; hub " + opt.hub_host + ":" + std::to_string(opt.hub_port));
    Peer peer{opt, log, {}};
    return peer.run();
}
