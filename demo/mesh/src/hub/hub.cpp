// Device mesh demo: the hub's relay loop (INTERFACES.md sections 4-6).
// OWNER: network thread.
//
// One thread, one poll loop over the frame listener, every device
// connection, the HTTP listener and its connections. A device goes Hello ->
// Register -> live; the hub forwards each Hop frame to the device named in
// its header (from overwritten with the sender's id) without reading the
// payload, writes it to the archive, and scores each job from the devices'
// Reports against its own replay of the job (types.hpp replay).
#include "hub/hub.hpp"

#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <memory>
#include <set>
#include <utility>
#include "peer/control.hpp"
#include "xmesh/net.hpp"

namespace xmesh::hub {

namespace {

using protocol::MsgType;
namespace fs = std::filesystem;

std::uint64_t splitmix64(std::uint64_t& x) noexcept {
    std::uint64_t z = (x += 0x9E3779B97F4A7C15ull);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
    return z ^ (z >> 31);
}

std::string now_text() {
    const std::uint64_t ms = net::wall_ms();
    const std::uint64_t s = ms / 1000;
    char buf[32];
    std::snprintf(buf, sizeof buf, "%02u:%02u:%02u.%03u", unsigned(s / 3600 % 24), unsigned(s / 60 % 60),
                  unsigned(s % 60), unsigned(ms % 1000));
    return buf;
}

void logf(const std::string& line) {
    std::fprintf(stderr, "[hub %s] %s\n", now_text().c_str(), line.c_str());
    std::fflush(stderr);
}

struct Csv {
    std::FILE* f = nullptr;
    ~Csv() {
        if (f != nullptr) std::fclose(f);
    }
    bool open(const fs::path& path, const char* header) {
        f = std::fopen(path.string().c_str(), "w");
        if (f == nullptr) return false;
        std::fprintf(f, "%s\n", header);
        return true;
    }
    void row(const std::string& line) {
        if (f == nullptr) return;
        std::fputs(line.c_str(), f);
        std::fputc('\n', f);
    }
    void flush() {
        if (f != nullptr) std::fflush(f);
    }
};

enum class ClientState : std::uint8_t { Hello, Register, Live, Closing };

struct Client {
    net::Conn conn;
    ClientState state = ClientState::Hello;
    std::uint32_t device_id = 0;
    std::string label;
    std::uint64_t since_ms = 0;
    bool shut = false;              // shutdown_send done (Closing)
};

struct HttpClient {
    net::Socket fd = net::kInvalidSocket;
    std::string in;
    std::string out;
    std::size_t sent = 0;
    std::uint64_t since_ms = 0;
    bool responding = false;
};

class Hub {
public:
    explicit Hub(const Options& opt) {
        s_.opt = opt;
        s_.hub_build = "xo_hub " + control::build_info();
        s_.start_wall_ms = net::wall_ms();
        s_.start_ms = net::now_ms();
        s_.lan_address = net::guess_lan_address();
    }

    int run();

private:
    // ---- setup and output files ----
    bool open_outputs();
    void write_final_files();

    // ---- connections ----
    void accept_devices();
    void read_client(std::size_t i);
    void on_hello(Client& c, net::Frame& f);
    void on_register(Client& c, net::Frame& f);
    void on_live_frame(Client& c, net::Frame& f);
    void refuse(Client& c, const std::string& why);
    void drop_client(std::size_t i, const std::string& why);
    void broadcast_roster();
    Client* client_of(std::uint32_t device_id);

    // ---- jobs ----
    void maybe_plan();
    void plan();
    void start_jobs();
    bool start_job(std::size_t index);
    void on_hop(Client& c, net::Frame& f);
    void on_report(Client& c, net::Frame& f);
    void evaluate(JobRun& j);
    void finish(JobRun& j, bool passed, const std::string& reason);
    void check_timeouts();
    JobRun* running_job(std::uint64_t job);

    // ---- HTTP ----
    void accept_http();
    void serve_http(HttpClient& h);

    // ---- end ----
    void send_bye();

    State s_;
    net::Listener listener_;
    net::Listener http_listener_;
    std::vector<std::unique_ptr<Client>> clients_;
    std::vector<HttpClient> http_;
    Csv jobs_csv_, hops_csv_;
    fs::path out_;
    bool bye_sent_ = false;
    std::uint64_t bye_ms_ = 0;
    std::uint64_t last_progress_ms_ = 0;
    std::string abort_reason_;
};

// ---- setup -------------------------------------------------------------------------

bool Hub::open_outputs() {
    out_ = fs::path(s_.opt.out_dir);
    std::error_code ec;
    fs::create_directories(out_ / "archive", ec);
    if (ec) {
        logf("error: cannot create " + (out_ / "archive").string() + ": " + ec.message());
        return false;
    }
    return jobs_csv_.open(out_ / "jobs.csv", kJobsCsvHeader) && hops_csv_.open(out_ / "hops.csv", kHopsCsvHeader);
}

// ---- connections -------------------------------------------------------------------

Client* Hub::client_of(std::uint32_t device_id) {
    for (auto& c : clients_)
        if (c->state == ClientState::Live && c->device_id == device_id) return c.get();
    return nullptr;
}

void Hub::accept_devices() {
    for (;;) {
        Result<net::Conn> c = listener_.try_accept();
        if (!c) {
            logf("accept: " + c.error().message);
            return;
        }
        if (!c->is_open()) return;
        if (Status s = c->try_set_nonblocking(true); !s) {
            logf("accept: " + s.error().message);
            continue;
        }
        auto cl = std::make_unique<Client>();
        cl->conn = std::move(*c);
        cl->since_ms = net::now_ms();
        clients_.push_back(std::move(cl));
    }
}

void Hub::refuse(Client& c, const std::string& why) {
    std::string text = why.size() > 4000 ? why.substr(0, 4000) : why;
    (void)c.conn.try_enqueue(MsgType::Error, 0, std::move(text));
    c.state = ClientState::Closing;
    c.since_ms = net::now_ms();
    logf("refused " + (c.label.empty() ? std::string("a connection") : c.label) + ": " + why);
}

void Hub::on_hello(Client& c, net::Frame& f) {
    std::string ack;
    const XSchemaHelloResult r = protocol::accept_hello(f.payload, ack, "xo_hub " + s_.opt.run);
    c.label = r.label.empty() ? std::string("unnamed device") : r.label;
    if (!ack.empty()) (void)c.conn.try_enqueue(MsgType::HelloAck, 0, std::move(ack));
    if (!protocol::hello_complete(r)) return refuse(c, "schema handshake: the Hello does not list every root type");
    if (!r.accepted()) return refuse(c, "schema handshake: this device's schema is not the hub's (xmesh v" +
                                            std::to_string(XMESH_SCHEMA_VERSION) + ")");
    c.state = ClientState::Register;
}

void Hub::on_register(Client& c, net::Frame& f) {
    Result<control::Peer> p = control::read_peer_info(f.payload);
    if (!p) return refuse(c, "Register: " + p.error().message);
    if (p->name.empty() || p->name.size() > 64) return refuse(c, "Register: name must be 1..64 bytes");
    for (const auto& [id, d] : s_.devices)
        if (d.present && d.name == p->name) return refuse(c, "Register: the name " + p->name + " is taken");
    DeviceRec d;
    d.id = s_.next_device_id++;
    d.name = p->name;
    d.build = p->build;
    d.flags = p->flags;
    d.joined_ms = net::wall_ms();
    d.present = true;
    c.device_id = d.id;
    c.label = d.name;
    c.state = ClientState::Live;
    s_.devices[d.id] = d;
    (void)c.conn.try_enqueue(MsgType::Welcome, 0, control::welcome_wire({d.id, s_.hub_build, s_.opt.run}));
    logf("device " + std::to_string(d.id) + " joined: " + d.name + " (" + d.build + ")");
    broadcast_roster();
}

void Hub::broadcast_roster() {
    control::RosterInfo info;
    info.version = ++s_.roster_version;
    for (const auto& [id, d] : s_.devices)
        if (d.present) info.devices.push_back({d.id, d.flags, d.name, d.build});
    const std::string wire = control::roster_wire(info);
    for (auto& c : clients_)
        if (c->state == ClientState::Live) (void)c->conn.try_enqueue(MsgType::Roster, info.version, wire);
}

void Hub::drop_client(std::size_t i, const std::string& why) {
    Client& c = *clients_[i];
    if (c.state == ClientState::Live) {
        DeviceRec& d = s_.devices[c.device_id];
        d.present = false;
        d.left_ms = net::wall_ms();
        if (!bye_sent_) logf("device " + std::to_string(d.id) + " (" + d.name + ") left: " + why);
        for (JobRun& j : s_.jobs) {
            if (j.state != JobState::Running) continue;
            for (const auto& r : j.spec.route)
                if (r.device_id == d.id) {
                    finish(j, false, "device left: " + d.name);
                    break;
                }
        }
        clients_.erase(clients_.begin() + std::ptrdiff_t(i));
        if (!bye_sent_) broadcast_roster();
        return;
    }
    clients_.erase(clients_.begin() + std::ptrdiff_t(i));
}

void Hub::read_client(std::size_t i) {
    Client& c = *clients_[i];
    for (int n = 0; n < 64; ++n) {
        Result<std::optional<net::Frame>> f = [&]() -> Result<std::optional<net::Frame>> {
            switch (c.state) {
                case ClientState::Hello: return c.conn.try_recv_nowait({MsgType::Hello});
                case ClientState::Register: return c.conn.try_recv_nowait({MsgType::Register});
                case ClientState::Live:
                    return c.conn.try_recv_nowait({MsgType::Hop, MsgType::Report, MsgType::Error});
                case ClientState::Closing: break;
            }
            return std::optional<net::Frame>();
        }();
        if (!f) {
            const Error& e = f.error();
            if (c.state == ClientState::Live && !e.peer_closed) {
                // A protocol error from a live device: tell it, then close.
                refuse(c, "protocol error: " + e.message);
                return;
            }
            drop_client(i, e.peer_closed ? "closed" : e.message);
            return;
        }
        if (!*f) return;
        net::Frame& frame = **f;
        switch (c.state) {
            case ClientState::Hello: on_hello(c, frame); break;
            case ClientState::Register: on_register(c, frame); break;
            case ClientState::Live: on_live_frame(c, frame); break;
            case ClientState::Closing: return;
        }
        if (c.state == ClientState::Closing) return;
    }
}

void Hub::on_live_frame(Client& c, net::Frame& f) {
    switch (f.type) {
        case MsgType::Hop: return on_hop(c, f);
        case MsgType::Report: return on_report(c, f);
        case MsgType::Error: {
            const std::string who = s_.devices[c.device_id].name;
            if (JobRun* j = running_job(f.correlation)) finish(*j, false, who + " sent Error: " + f.payload);
            else logf(who + " sent Error: " + f.payload);
            return;
        }
        default: return;
    }
}

// ---- jobs --------------------------------------------------------------------------------

JobRun* Hub::running_job(std::uint64_t job) {
    if (job == 0 || job > s_.jobs.size()) return nullptr;
    JobRun& j = s_.jobs[job - 1];
    return j.state == JobState::Running ? &j : nullptr;
}

void Hub::maybe_plan() {
    if (s_.planned) return;
    std::size_t present = 0;
    for (const auto& [id, d] : s_.devices) present += d.present ? 1 : 0;
    const bool enough = s_.opt.expect > 0 && present >= std::size_t(s_.opt.expect);
    const bool waited = net::now_ms() - s_.start_ms >= std::uint64_t(s_.opt.wait_s) * 1000;
    if (!enough && !waited) return;
    if (present < 2) {
        abort_reason_ = "only " + std::to_string(present) + " device(s) registered after --wait-s " +
                        std::to_string(s_.opt.wait_s);
        s_.planned = true;
        return;
    }
    plan();
}

void Hub::plan() {
    s_.planned = true;
    std::vector<RouteEntry> devs;
    for (const auto& [id, d] : s_.devices)
        if (d.present) devs.push_back({d.id, d.name});
    const Options& o = s_.opt;
    auto add = [&](JobKind kind, TypeId type, Scale scale, SaveMode mode, EvilCase evil, std::vector<RouteEntry> route) {
        JobRun j;
        j.kind = kind;
        j.spec.job = s_.jobs.size() + 1;
        std::uint64_t x = o.seed * 0x100000001B3ull + j.spec.job;
        j.spec.seed = splitmix64(x);
        j.spec.type = type;
        j.spec.scale = scale;
        j.spec.save_mode = mode;
        j.spec.evil = evil;
        j.spec.route = std::move(route);
        j.reports.resize(j.spec.route.size());
        s_.pending.push_back(s_.jobs.size());
        s_.jobs.push_back(std::move(j));
    };
    const std::size_t n = devs.size();
    if (o.plan_pairs)
        for (TypeId t : o.types)
            for (Scale sc : o.scales)
                for (SaveMode m : o.save_modes)
                    for (std::size_t a = 0; a < n; ++a)
                        for (std::size_t b = 0; b < n; ++b)
                            if (a != b) add(JobKind::Pairs, t, sc, m, EvilCase::None, {devs[a], devs[b], devs[a]});
    if (o.plan_chain && n + 1 <= kMaxRoute) {
        std::size_t k = 0;
        for (TypeId t : o.types)
            for (Scale sc : o.scales) {
                std::vector<RouteEntry> perm = devs;
                std::uint64_t x = o.seed ^ (std::uint64_t(t) << 32) ^ (std::uint64_t(sc) << 48);
                for (std::size_t i = perm.size(); i > 1; --i) std::swap(perm[i - 1], perm[splitmix64(x) % i]);
                perm.push_back(perm.front());
                add(JobKind::Chain, t, sc, o.save_modes[k++ % o.save_modes.size()], EvilCase::None, std::move(perm));
            }
    } else if (o.plan_chain) {
        logf("chain: " + std::to_string(n) + " devices do not fit in a route of " + std::to_string(kMaxRoute));
    }
    if (o.plan_evil)
        for (std::size_t a = 0; a < n; ++a) {
            if ((s_.devices[devs[a].device_id].flags & protocol::kPeerEvil) == 0) continue;
            for (std::size_t b = 0; b < n; ++b) {
                if (a == b) continue;
                for (EvilCase e : kAllEvilCases) {
                    const TypeId t = kAllTypes[(s_.jobs.size() + 1) % kAllTypes.size()];
                    add(JobKind::Evil, t, o.scales.front(), o.save_modes.front(), e, {devs[a], devs[b]});
                }
            }
        }
    std::size_t counts[kJobKinds] = {};
    for (const JobRun& j : s_.jobs) ++counts[std::size_t(j.kind)];
    logf("planned " + std::to_string(s_.jobs.size()) + " jobs for " + std::to_string(n) + " devices: " +
         std::to_string(counts[0]) + " pairs, " + std::to_string(counts[1]) + " chain, " + std::to_string(counts[2]) +
         " evil");
}

void Hub::start_jobs() {
    while (!s_.pending.empty() && s_.running < std::size_t(s_.opt.max_inflight)) {
        auto it = s_.pending.end();
        for (auto p = s_.pending.begin(); p != s_.pending.end(); ++p) {
            bool free = true;
            for (const auto& r : s_.jobs[*p].spec.route)
                if (s_.devices[r.device_id].inflight >= std::uint64_t(s_.opt.max_per_device)) {
                    free = false;
                    break;
                }
            if (free) {
                it = p;
                break;
            }
        }
        if (it == s_.pending.end()) {
            if (s_.running != 0) return;
            it = s_.pending.begin();   // nothing in flight: start the head anyway
        }
        const std::size_t index = *it;
        s_.pending.erase(it);
        start_job(index);
    }
}

bool Hub::start_job(std::size_t index) {
    JobRun& j = s_.jobs[index];
    j.state = JobState::Running;
    j.start_ms = net::now_ms();
    ++s_.running;
    std::set<std::uint32_t> seen;
    for (const auto& r : j.spec.route)
        if (seen.insert(r.device_id).second) ++s_.devices[r.device_id].inflight;
    for (const auto& r : j.spec.route)
        if (!s_.devices[r.device_id].present) {
            finish(j, false, "device left: " + r.name);
            return false;
        }
    if (j.kind != JobKind::Evil) {
        std::vector<Step> steps;
        for (std::uint32_t k = 0; k < j.spec.route.size(); ++k) steps.push_back({k, j.spec.route[k].name});
        Result<std::vector<HopExpect>> e = replay(j.spec.type, j.spec.seed, j.spec.scale, j.spec.save_mode, steps);
        if (!e) {
            finish(j, false, "hub replay: " + e.error().message);
            return false;
        }
        j.expect = std::move(*e);
        if (j.expect.size() + 1 != j.spec.route.size()) {
            finish(j, false, "hub replay: " + std::to_string(j.expect.size()) + " hops for a route of " +
                                 std::to_string(j.spec.route.size()));
            return false;
        }
    }
    Result<std::string> meta = make_job_meta(j.spec);
    if (!meta) {
        finish(j, false, "make_job_meta: " + meta.error().message);
        return false;
    }
    Client* c = client_of(j.spec.route[0].device_id);
    if (c == nullptr) {
        finish(j, false, "no such device: " + j.spec.route[0].name);
        return false;
    }
    if (Status st = c->conn.try_enqueue(MsgType::Job, j.spec.job, std::move(*meta), protocol::kHubId,
                                        j.spec.route[0].device_id);
        !st) {
        finish(j, false, "Job: " + st.error().message);
        return false;
    }
    return true;
}

void Hub::on_hop(Client& c, net::Frame& f) {
    DeviceRec& sender = s_.devices[c.device_id];
    JobRun* j = running_job(f.correlation);
    if (j == nullptr) {
        logf("dropped a Hop for job " + std::to_string(f.correlation) + " from " + sender.name + " (not running)");
        return;
    }
    const std::uint32_t k = j->hops_forwarded;
    if (j->kind == JobKind::Evil && k >= 1) return finish(*j, false, sender.name + " sent the evil object on");
    if (std::size_t(k) + 1 >= j->spec.route.size()) return finish(*j, false, sender.name + " sent a hop after the last stop");
    Client* to = f.to == protocol::kHubId ? nullptr : client_of(f.to);
    if (to == nullptr) return finish(*j, false, "no such device: " + std::to_string(f.to) + " (hop from " + sender.name + ")");
    if (s_.opt.archive) {
        const fs::path p = out_ / "archive" / (std::to_string(j->spec.job) + "-" + std::to_string(k) + ".xmhop");
        if (std::FILE* a = std::fopen(p.string().c_str(), "wb")) {
            std::fwrite(f.payload.data(), 1, f.payload.size(), a);
            std::fclose(a);
        }
    }
    ++j->hops_forwarded;
    if (std::size_t(k) + 2 == j->spec.route.size() && j->kind != JobKind::Evil) j->last_payload = f.payload;
    const std::uint32_t to_id = f.to;
    if (Status st = to->conn.try_enqueue(MsgType::Hop, f.correlation, std::move(f.payload), c.device_id, to_id); !st)
        finish(*j, false, "relay: " + st.error().message);
}

void Hub::on_report(Client& c, net::Frame& f) {
    DeviceRec& d = s_.devices[c.device_id];
    ++d.reports;
    Result<ReportRow> row = read_report(f.payload);
    JobRun* j = running_job(f.correlation);
    if (!row) {
        if (j != nullptr) finish(*j, false, "unreadable Report from " + d.name + ": " + row.error().message);
        else logf("unreadable Report from " + d.name + ": " + row.error().message);
        return;
    }
    if (row->verdict == protocol::HopVerdict::Ok) {
        ++d.reports_ok;
        d.verify_ns += row->verify_ns;
        d.check_ns += row->check_ns;
        d.mutate_ns += row->mutate_ns;
        d.save_ns += row->save_ns;
    }
    if (j == nullptr) {
        logf("late Report for job " + std::to_string(f.correlation) + " from " + d.name);
        return;
    }
    if (row->job != f.correlation) return finish(*j, false, "Report from " + d.name + " names job " + std::to_string(row->job));
    const auto& route = j->spec.route;
    std::size_t pos = std::size_t(row->hop) + 1;
    if (row->hop == 0 && c.device_id == route[0].device_id && !j->reports[0]) pos = 0;
    if (pos >= route.size() || route[pos].device_id != c.device_id || j->reports[pos])
        return finish(*j, false, "unexpected Report from " + d.name + " (hop " + std::to_string(row->hop) + ")");
    j->reports[pos] = std::move(*row);
    evaluate(*j);
}

void Hub::evaluate(JobRun& j) {
    const Judgement v = judge(j);
    if (v.done) finish(j, v.passed, v.reason);
}

void Hub::finish(JobRun& j, bool passed, const std::string& reason) {
    if (j.state != JobState::Running) return;
    j.state = passed ? JobState::Passed : JobState::Failed;
    j.end_ms = net::now_ms();
    j.reason = reason;
    --s_.running;
    ++(passed ? s_.passed : s_.failed);
    std::set<std::uint32_t> seen;
    for (const auto& r : j.spec.route) {
        if (!seen.insert(r.device_id).second) continue;
        DeviceRec& d = s_.devices[r.device_id];
        --d.inflight;
        ++d.jobs;
        if (passed) ++d.jobs_passed;
    }
    if (j.kind == JobKind::Pairs || j.kind == JobKind::Cross) {
        auto& cell = s_.matrix[{j.spec.route[0].device_id, j.spec.route[1].device_id}];
        ++cell.first;
        if (passed) ++cell.second;
    }
    if (!passed) logf("job " + std::to_string(j.spec.job) + " " + kind_name(j.kind) + " " + type_name(j.spec.type) +
                      " " + route_text(j.spec) + " FAILED: " + reason);

    // jobs.csv and hops.csv
    jobs_csv_.row(job_csv_row(j, std::to_string(j.end_ms - j.start_ms)));
    for (const std::string& row : hop_csv_rows(j)) hops_csv_.row(row);

    // The dashboard's recent list: the object as the last stop received it.
    Recent rec;
    rec.job = j.spec.job;
    rec.passed = passed;
    rec.text = std::string(kind_name(j.kind)) + " " + type_name(j.spec.type) + " " + scale_name(j.spec.scale) + " " +
               save_mode_name(j.spec.save_mode) + (j.kind == JobKind::Evil ? std::string(" ") + evil_name(j.spec.evil) : "") + ": " +
               route_text(j.spec) + " -> " + (passed ? "pass" : "FAIL") + (reason.empty() ? "" : " (" + reason + ")");
    if (passed && !j.last_payload.empty()) {
        if (Result<protocol::HopParts> parts = protocol::unpack_hop(j.last_payload)) {
            if (Result<Object> obj = receive_object(j.spec.type, j.spec.scale, parts->data)) rec.text += " | " + summary(*obj);
        }
    }
    s_.recent.push_front(std::move(rec));
    if (s_.recent.size() > 30) s_.recent.pop_back();
    j.expect.clear();
    j.expect.shrink_to_fit();
    j.last_payload.clear();
    j.last_payload.shrink_to_fit();
}

void Hub::check_timeouts() {
    const std::uint64_t now = net::now_ms();
    const std::uint64_t limit = std::uint64_t(s_.opt.job_timeout_s) * 1000;
    for (JobRun& j : s_.jobs)
        if (j.state == JobState::Running && now - j.start_ms > limit) {
            std::string missing;
            for (std::size_t pos = 0; pos < j.reports.size(); ++pos)
                if (!j.reports[pos]) missing += (missing.empty() ? "" : ", ") + j.spec.route[pos].name;
            finish(j, false, "timeout (no Report from " + missing + ")");
        }
}

// ---- HTTP -----------------------------------------------------------------------------------

void Hub::accept_http() {
    for (;;) {
        Result<net::Conn> c = http_listener_.try_accept();
        if (!c || !c->is_open()) return;
        if (!c->try_set_nonblocking(true)) continue;
        HttpClient h;
        // The raw socket is served without framing: take it over from the Conn.
        h.fd = c->release();
        h.since_ms = net::now_ms();
        http_.push_back(std::move(h));
    }
}

void Hub::serve_http(HttpClient& h) {
    if (!h.responding) {
        char buf[4096];
        for (;;) {
            const long long r = net::raw_recv(h.fd, buf, sizeof buf);
            if (r > 0) {
                h.in.append(buf, std::size_t(r));
                if (h.in.size() > 16384) break;
                continue;
            }
            if (r == -1) break;
            h.out.clear();
            h.responding = true;   // closed or failed: drop
            h.sent = 0;
            return;
        }
        const std::size_t end = h.in.find("\r\n\r\n");
        if (end == std::string::npos && h.in.size() <= 16384) return;
        std::string path = "/";
        std::string host;
        {
            const std::size_t sp1 = h.in.find(' ');
            const std::size_t sp2 = sp1 == std::string::npos ? sp1 : h.in.find(' ', sp1 + 1);
            if (sp2 != std::string::npos) path = h.in.substr(sp1 + 1, sp2 - sp1 - 1);
            std::size_t at = 0;
            while ((at = h.in.find("\r\n", at)) != std::string::npos && at < end) {
                at += 2;
                if (h.in.size() - at > 5 && (h.in.compare(at, 5, "Host:") == 0 || h.in.compare(at, 5, "host:") == 0)) {
                    const std::size_t eol = h.in.find("\r\n", at);
                    host = h.in.substr(at + 5, eol - at - 5);
                    while (!host.empty() && host.front() == ' ') host.erase(host.begin());
                    break;
                }
            }
        }
        const std::string route = path.substr(0, path.find('?'));
        std::string body, type = "text/html; charset=utf-8", status = "200 OK";
        if (route == "/") body = page_dashboard(s_);
        else if (route == "/join") body = page_join(s_, host);
        else if (route == "/status.json") {
            body = status_json(s_);
            type = "application/json";
        } else {
            status = "404 Not Found";
            body = "not found\n";
            type = "text/plain";
        }
        h.out = "HTTP/1.0 " + status + "\r\nContent-Type: " + type + "\r\nContent-Length: " + std::to_string(body.size()) +
                "\r\nCache-Control: no-store\r\nConnection: close\r\n\r\n" + body;
        h.responding = true;
        h.sent = 0;
    }
    while (h.sent < h.out.size()) {
        const long long w = net::raw_send(h.fd, h.out.data() + h.sent, h.out.size() - h.sent);
        if (w > 0) {
            h.sent += std::size_t(w);
            continue;
        }
        if (w == 0) return;
        h.out.clear();   // error: drop
        h.sent = 0;
        break;
    }
}

// ---- end ------------------------------------------------------------------------------------

void Hub::send_bye() {
    bye_sent_ = true;
    bye_ms_ = net::now_ms();
    for (auto& c : clients_) {
        if (c->state == ClientState::Live) (void)c->conn.try_enqueue(MsgType::Bye, 0, std::string());
        c->state = ClientState::Closing;
        c->since_ms = bye_ms_;
    }
    logf("all jobs done: " + std::to_string(s_.passed) + " passed, " + std::to_string(s_.failed) + " failed; Bye sent");
}

void Hub::write_final_files() {
    jobs_csv_.flush();
    hops_csv_.flush();
    Csv dev;
    if (dev.open(out_ / "devices.csv", "id,name,build,flags,joined_ms,left_ms"))
        for (const auto& [id, d] : s_.devices)
            dev.row(std::to_string(d.id) + "," + csv_field(d.name) + "," + csv_field(d.build) + "," +
                    std::to_string(d.flags) + "," + std::to_string(d.joined_ms) + "," + std::to_string(d.left_ms));
    Csv mat;
    if (mat.open(out_ / "matrix.csv", "sender,receiver,jobs,passed"))
        for (const auto& [key, cell] : s_.matrix)
            mat.row(csv_field(s_.devices[key.first].name) + "," + csv_field(s_.devices[key.second].name) + "," +
                    std::to_string(cell.first) + "," + std::to_string(cell.second));
    std::string sum;
    sum += "run " + s_.opt.run + ", " + s_.hub_build + "\n";
    for (const auto& [id, d] : s_.devices) {
        auto avg_us = [&](std::uint64_t ns) {
            char b[32];
            std::snprintf(b, sizeof b, "%.1f", d.reports_ok ? double(ns) / double(d.reports_ok) / 1000.0 : 0.0);
            return std::string(b);
        };
        sum += "device " + std::to_string(d.id) + " " + d.name + ": jobs " + std::to_string(d.jobs_passed) + "/" +
               std::to_string(d.jobs) + " passed, reports " + std::to_string(d.reports_ok) + " ok of " +
               std::to_string(d.reports) + ", mean us verify " + avg_us(d.verify_ns) + " check " + avg_us(d.check_ns) +
               " mutate " + avg_us(d.mutate_ns) + " save " + avg_us(d.save_ns) + " | " + d.build + "\n";
    }
    std::size_t by_kind[kJobKinds][2] = {};
    for (const JobRun& j : s_.jobs) {
        if (j.state == JobState::Passed) ++by_kind[std::size_t(j.kind)][0];
        if (j.state != JobState::Pending && j.state != JobState::Running) ++by_kind[std::size_t(j.kind)][1];
    }
    for (std::size_t k = 0; k < kJobKinds; ++k) {
        if (by_kind[k][1] == 0 && JobKind(k) == JobKind::Cross) continue;
        sum += std::string("kind ") + kind_name(JobKind(k)) + ": " + std::to_string(by_kind[k][0]) + "/" +
               std::to_string(by_kind[k][1]) + " passed\n";
    }
    const std::size_t unfinished = s_.jobs.size() - s_.passed - s_.failed;
    sum += "jobs " + std::to_string(s_.jobs.size()) + ", passed " + std::to_string(s_.passed) + ", failed " +
           std::to_string(s_.failed) + ", not finished " + std::to_string(unfinished) + "\n";
    if (!abort_reason_.empty()) sum += "aborted: " + abort_reason_ + "\n";
    const bool pass = abort_reason_.empty() && !s_.jobs.empty() && s_.failed == 0 && unfinished == 0;
    sum += pass ? "RESULT PASS\n" : "RESULT FAIL\n";
    if (std::FILE* f = std::fopen((out_ / "summary.txt").string().c_str(), "w")) {
        std::fputs(sum.c_str(), f);
        std::fclose(f);
    }
    std::fputs(sum.c_str(), stderr);
}

int Hub::run() {
    if (!open_outputs()) return 1;
    Result<net::Listener> l = net::Listener::try_bind(s_.opt.bind, s_.opt.port);
    if (!l) {
        logf("error: " + l.error().message);
        return 1;
    }
    listener_ = std::move(*l);
    if (s_.opt.http_port != 0) {
        Result<net::Listener> hl = net::Listener::try_bind(s_.opt.bind, s_.opt.http_port);
        if (!hl) {
            logf("error: " + hl.error().message);
            return 1;
        }
        http_listener_ = std::move(*hl);
    }
    logf(s_.hub_build);
    logf("run " + s_.opt.run + ": frames on " + s_.opt.bind + ":" + std::to_string(s_.opt.port) +
         (s_.opt.http_port ? ", dashboard http://" + (s_.lan_address.empty() ? std::string("localhost") : s_.lan_address) +
                                 ":" + std::to_string(s_.opt.http_port) + "/, join link " + join_link(s_, "")
                           : std::string()));
    std::vector<net::PollItem> items;
    for (;;) {
        const std::uint64_t now = net::now_ms();
        if (!bye_sent_) {
            maybe_plan();
            start_jobs();
            check_timeouts();
            if (s_.planned && s_.running == 0 && s_.pending.empty() && !s_.opt.linger) {
                s_.finished = true;
                send_bye();
            }
        }
        if (now - last_progress_ms_ >= 5000 && s_.planned && !bye_sent_) {
            last_progress_ms_ = now;
            logf("jobs " + std::to_string(s_.passed + s_.failed) + "/" + std::to_string(s_.jobs.size()) + " done, " +
                 std::to_string(s_.failed) + " failed, " + std::to_string(s_.running) + " running");
        }
        // Closing connections: flush, shut down, read to EOF (5 s; 10 s after Bye), close.
        for (std::size_t i = 0; i < clients_.size();) {
            Client& c = *clients_[i];
            if (c.state == ClientState::Closing) {
                Result<bool> flushed = c.conn.try_flush();
                if (flushed && *flushed && !c.shut) {
                    c.conn.shutdown_send();
                    c.shut = true;
                }
                const bool expired = now - c.since_ms > (bye_sent_ ? 10000u : 5000u);
                if (!flushed || expired || (c.shut && !c.conn.discard_input())) {
                    if (c.state == ClientState::Closing && c.device_id != 0 && s_.devices[c.device_id].present) {
                        DeviceRec& d = s_.devices[c.device_id];
                        d.present = false;
                        d.left_ms = net::wall_ms();
                        for (JobRun& j : s_.jobs)
                            if (j.state == JobState::Running)
                                for (const auto& r : j.spec.route)
                                    if (r.device_id == d.id) {
                                        finish(j, false, "device left: " + d.name);
                                        break;
                                    }
                        if (!bye_sent_) broadcast_roster();
                    }
                    clients_.erase(clients_.begin() + std::ptrdiff_t(i));
                    continue;
                }
            } else if (c.state != ClientState::Live && now - c.since_ms > 30000) {
                refuse(c, "handshake not finished within 30 s");
            }
            ++i;
        }
        for (std::size_t i = 0; i < http_.size();) {
            HttpClient& h = http_[i];
            if ((h.responding && h.sent >= h.out.size()) || now - h.since_ms > 10000) {
                net::raw_close(h.fd);
                http_.erase(http_.begin() + std::ptrdiff_t(i));
                continue;
            }
            ++i;
        }
        if (bye_sent_ && clients_.empty()) break;
        if (bye_sent_ && now - bye_ms_ > 15000) break;
        if (!abort_reason_.empty() && !bye_sent_) {
            logf("error: " + abort_reason_);
            send_bye();
        }

        items.clear();
        net::PollItem li;
        li.fd = listener_.fd();
        li.want_read = !bye_sent_;
        items.push_back(li);
        net::PollItem hi;
        hi.fd = http_listener_.fd();
        hi.want_read = s_.opt.http_port != 0;
        items.push_back(hi);
        for (auto& c : clients_) {
            net::PollItem p;
            p.fd = c->conn.fd();
            p.want_read = true;
            p.want_write = c->conn.want_write();
            items.push_back(p);
        }
        for (auto& h : http_) {
            net::PollItem p;
            p.fd = h.fd;
            p.want_read = !h.responding;
            p.want_write = h.responding;
            items.push_back(p);
        }
        Result<int> n = net::try_poll(items.data(), items.size(), 100);
        if (!n) {
            logf("error: " + n.error().message);
            break;
        }
        const std::size_t nclients = clients_.size();
        const std::size_t nhttp = http_.size();
        // Writes first, then reads: a read can drop a client and shift the vector.
        for (std::size_t i = 0; i < nclients; ++i) {
            if (!items[2 + i].writable || clients_[i]->state == ClientState::Closing) continue;
            Result<bool> flushed = clients_[i]->conn.try_flush();
            if (!flushed && clients_[i]->state != ClientState::Closing) {
                clients_[i]->state = ClientState::Closing;
                clients_[i]->since_ms = 0;   // expire at once
            }
        }
        for (std::size_t i = 0; i < nhttp; ++i)
            if (items[2 + nclients + i].readable || items[2 + nclients + i].writable) serve_http(http_[i]);
        std::vector<Client*> readable;
        for (std::size_t i = 0; i < nclients; ++i)
            if (items[2 + i].readable && clients_[i]->state != ClientState::Closing) readable.push_back(clients_[i].get());
        for (Client* c : readable) {
            for (std::size_t i = 0; i < clients_.size(); ++i)
                if (clients_[i].get() == c) {
                    read_client(i);
                    break;
                }
        }
        // Push out what the reads queued without waiting for the next poll.
        for (auto& c : clients_)
            if (c->state != ClientState::Closing && c->conn.want_write()) (void)c->conn.try_flush();
        if (items[0].readable) accept_devices();
        if (items[1].readable) accept_http();
    }
    for (JobRun& j : s_.jobs)
        if (j.state == JobState::Running) finish(j, false, "the hub stopped");
    write_final_files();
    for (auto& h : http_) net::raw_close(h.fd);
    const bool pass = abort_reason_.empty() && !s_.jobs.empty() && s_.failed == 0 && s_.passed == s_.jobs.size();
    return pass ? 0 : 1;
}

} // namespace

int run(const Options& opt) {
    Hub hub(opt);
    return hub.run();
}

} // namespace xmesh::hub
