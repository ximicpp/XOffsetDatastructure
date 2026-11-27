// gs_bot: load generator and end-to-end check for the game server demo
// (demo/gameserver/README.md).
//
// Each bot is a game client over TCP: schema handshake, login, then it keeps
// a replica of its player and city (key delta at login, small deltas after),
// chooses valid commands from that replica and sends them at a set rate, one
// outstanding at a time. The latency of a command is the time from sending
// it to receiving the player delta it caused.
//
// During the run some bots drop their connection and resume their session
// with the token (the gateway asks the logic node for key deltas), and some
// players are migrated to another logic node while their bot keeps playing.
// At the end every bot logs out and the bot reads a sample of the saves back
// from Redis and compares them with the replicas.
#include "gs/game.hpp"
#include "gs/metrics.hpp"
#include "gs/net.hpp"
#include "gs/redis.hpp"
#include "xoffset_handshake.hpp"
#include "xoffset_receiver.hpp"

#include <cerrno>
#include <cstdio>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cstdlib>
#include <memory>
#include <random>
#include <string>
#include <vector>

namespace gs {
namespace {

unsigned long long ull(std::uint64_t v) { return static_cast<unsigned long long>(v); }

struct Options {
    std::string gateway = "127.0.0.1:7000";
    std::string redis = "127.0.0.1:6379";
    std::uint32_t bots = 100;
    std::uint64_t first_player = 100000;
    double seconds = 10;
    double rate = 10;  // commands per second per bot
    std::uint32_t nodes = 2;
    std::uint32_t reconnects = 0;  // session resumes spread over the run
    std::uint32_t migrations = 0;  // players moved to another node during the run
    std::uint32_t verify = 50;     // saves read back from Redis at the end
    std::string report;            // JSON report path
    bool hostile = true;           // run the hostile-client checks first
};

struct Bot {
    std::uint64_t player = 0;
    std::uint64_t conn = 0;
    enum Stage { off, hello, login, live } stage = off;
    std::uint64_t token = 0;
    std::uint32_t node = 0;
    TypedXBuffer<game::Player> player_view =
        TypedXBuffer<game::Player>::create(1024, XBuffer::maximum_capacity(kPlayerLimit));
    TypedXBuffer<game::City> city_view = TypedXBuffer<game::City>::create(512, XBuffer::maximum_capacity(kCityLimit));
    XDeltaCursor player_cursor, city_cursor;
    bool pending = false;
    std::uint64_t sent_us = 0;
    std::uint64_t next_us = 0;
    std::uint64_t next_ping_us = 0;
    std::uint64_t reconnect_start_us = 0;  // nonzero while resuming
    std::uint64_t migrate_start_us = 0;    // nonzero while waiting for the new owner's key delta
    int name_flip = 0;
    bool logged_out = false;

    bool ready() const { return stage == live && !player_cursor.empty() && !city_cursor.empty(); }
};


// ---- Hostile clients: what a cheater or a broken client sends -----------------

// A blocking client on a raw socket, for the checks below.
class RawClient {
public:
    RawClient(const std::string& host, std::uint16_t port) : fd_(connect_tcp(host, port)) {}
    ~RawClient() {
        if (fd_ >= 0) ::close(fd_);
    }
    bool ok() const { return fd_ >= 0; }
    void send_raw(const std::string& bytes) {
        std::size_t pos = 0;
        while (fd_ >= 0 && pos < bytes.size()) {
            const ssize_t n = ::send(fd_, bytes.data() + pos, bytes.size() - pos, 0);
            if (n > 0) pos += std::size_t(n);
            else if (n < 0 && errno == EAGAIN) wait(POLLOUT, 1000);
            else break;
        }
    }
    void send_frame(std::uint8_t type, std::string_view body) {
        std::string f;
        put_u32(f, std::uint32_t(1 + body.size()));
        put_u8(f, type);
        f.append(body);
        send_raw(f);
    }
    // The next frame within `ms`; type 0 on timeout, 255 when the server closed.
    std::uint8_t read_frame(std::string& body, int ms) {
        const std::uint64_t until = now_us() + std::uint64_t(ms) * 1000;
        for (;;) {
            if (in_.size() >= 5) {
                Reader r{std::string_view(in_).substr(0, 4)};
                const std::uint32_t len = r.u32();
                if (in_.size() - 4 >= len) {
                    const std::uint8_t type = std::uint8_t(in_[4]);
                    body.assign(in_, 5, len - 1);
                    in_.erase(0, 4 + std::size_t(len));
                    return type;
                }
            }
            const std::uint64_t now = now_us();
            if (now >= until) return 0;
            if (!wait(POLLIN, int((until - now) / 1000) + 1)) continue;
            char buf[65536];
            const ssize_t n = ::recv(fd_, buf, sizeof buf, 0);
            if (n == 0 || (n < 0 && errno != EAGAIN && errno != EINTR)) return 255;
            if (n > 0) in_.append(buf, std::size_t(n));
        }
    }
    // Skip frames until `type` arrives; false on timeout or close.
    bool expect(std::uint8_t type, std::string& body, int ms = 2000) {
        for (;;) {
            const std::uint8_t t = read_frame(body, ms);
            if (t == type) return true;
            if (t == 0 || t == 255) return false;
        }
    }
    bool closed_by_server(int ms = 2000) {
        std::string body;
        for (;;) {
            const std::uint8_t t = read_frame(body, ms);
            if (t == 255) return true;
            if (t == 0) return false;
        }
    }

private:
    bool wait(short ev, int ms) {
        pollfd p{fd_, ev, 0};
        return ::poll(&p, 1, ms) > 0;
    }
    int fd_;
    std::string in_;
};

std::string command_wire(std::uint64_t player, game::CommandKind kind) {
    auto b = TypedXBuffer<game::Command>::create(256, XBuffer::maximum_capacity(kCommandLimit));
    b.root().player_id() = player;
    b.root().kind() = std::uint32_t(kind);
    return b.save_verified(XSaveOptions{.emission = XWireEmission::dense});
}

// Each check prints its outcome; returns the number that failed.
int hostile_checks(const std::string& host, std::uint16_t port, std::uint64_t player) {
    int failed = 0;
    auto check = [&](bool ok, const char* what) {
        std::fprintf(stderr, "hostile: %-62s %s\n", what, ok ? "ok" : "FAILED");
        failed += !ok;
    };
    std::string body;
    {
        RawClient c(host, port);
        c.send_frame(c_login, std::string(16, '\1'));
        check(c.closed_by_server(), "a login before the schema hello: connection closed");
    }
    {
        RawClient c(host, port);
        std::string f;
        put_u32(f, 1u << 30);  // claims a 1 GiB frame
        put_u8(f, c_command);
        c.send_raw(f);
        check(c.closed_by_server(), "a frame longer than the client limit: closed before buffering");
    }
    {
        RawClient c(host, port);
        c.send_frame(c_hello, "not a schema hello");
        check(c.closed_by_server(), "a malformed schema hello: refused and closed");
    }
    RawClient c(host, port);
    c.send_frame(c_hello, make_schema_hello<game::Player, game::City, game::Command>("hostile"));
    std::string login;
    put_u64(login, player);
    put_u64(login, 0);
    bool live = c.expect(g_ack, body) && (c.send_frame(c_login, login), c.expect(g_login_ok, body));
    // Both key deltas of the login.
    live = live && c.expect(g_downlink, body) && c.expect(g_downlink, body);
    check(live, "the same connection logs in normally");
    if (!live) return failed + 1;
    std::string wire = command_wire(player, game::CommandKind::collect);
    std::string cut = wire.substr(0, wire.size() - 8);
    c.send_frame(c_command, cut);
    check(c.read_frame(body, 500) == 0, "a truncated command: refused by the bounds check, no delta");
    c.send_frame(c_command, command_wire(player + 1, game::CommandKind::collect));
    check(c.read_frame(body, 500) == 0, "a command naming another player: refused, no delta");
    c.send_frame(c_command, command_wire(player, game::CommandKind(99)));
    check(c.read_frame(body, 500) == 0, "an unknown command kind: refused, no delta");
    std::string garbage(200, '\x7f');
    c.send_frame(c_command, garbage);
    check(c.read_frame(body, 500) == 0, "200 bytes of garbage as a command: refused, no delta");
    c.send_frame(c_command, wire);
    check(c.expect(g_downlink, body), "a valid command afterwards: applied, delta received");
    c.send_frame(c_logout, {});
    return failed;
}

class Runner {
public:
    explicit Runner(Options o) : opt_(std::move(o)), rng_(12345) {}
    int run();

private:
    bool connect(Bot& b);
    void on_frame(Conn& c, std::uint8_t type, std::string_view body);
    void send_command(Bot& b, std::uint64_t now);
    void tick();
    void verify();
    void write_report();

    Options opt_;
    Loop loop_;
    std::mt19937_64 rng_;
    std::vector<std::unique_ptr<Bot>> bots_;
    XReceiver<XLink::from_server> rx_{XBuffer::maximum_capacity(kPlayerLimit)};
    TypedXBuffer<game::Command> out_ = TypedXBuffer<game::Command>::create(256, XBuffer::maximum_capacity(kCommandLimit));
    std::string wire_;
    std::string host_;
    std::uint16_t port_ = 0;

    enum Phase { logging_in, measuring, draining, logging_out, done } phase_ = logging_in;
    std::uint64_t start_us_ = 0, measure_start_us_ = 0, measure_end_us_ = 0, phase_until_us_ = 0;
    std::uint64_t next_event_us_ = 0, event_every_us_ = 0;
    std::uint32_t reconnects_done_ = 0, migrations_done_ = 0;
    std::size_t rr_ = 0;

    Histogram latency_, login_, reconnect_, migrate_;
    std::uint64_t sent_ = 0, completed_ = 0, timeouts_ = 0, aborted_ = 0, errors_ = 0, apply_failures_ = 0;
    std::uint64_t downlinks_ = 0, key_deltas_ = 0, small_deltas_ = 0, small_delta_bytes_ = 0, key_delta_bytes_ = 0;
    std::uint64_t up_bytes_ = 0, down_bytes_ = 0, closed_bytes_in_ = 0, closed_bytes_out_ = 0;
    std::uint64_t verified_ = 0, verify_checked_ = 0;
    std::uint64_t kind_counts_[5] = {};
};

bool Runner::connect(Bot& b) {
    const int fd = connect_tcp(host_, port_);
    if (fd < 0) return false;
    Conn& c = loop_.adopt(fd, kServerFrameLimit);
    c.user = &b;
    b.conn = c.id;
    b.stage = Bot::hello;
    loop_.send(c, c_hello, make_schema_hello<game::Player, game::City, game::Command>("bot-2.0"));
    return true;
}

void Runner::on_frame(Conn& c, std::uint8_t type, std::string_view body) {
    Bot& b = *static_cast<Bot*>(c.user);
    Reader r{body};
    const std::uint64_t now = now_us();
    switch (type) {
        case g_ack: {
            if (!read_schema_ack<game::Player, game::City, game::Command>(std::string(body))) {
                ++errors_;
                loop_.close(c);
                return;
            }
            std::string login;
            put_u64(login, b.player);
            put_u64(login, b.token);
            loop_.send(c, c_login, login);
            b.stage = Bot::login;
            return;
        }
        case g_login_ok:
            b.token = r.u64();
            b.node = r.u32();
            b.stage = Bot::live;
            return;
        case g_downlink: {
            const auto stream = Stream(r.u8());
            const bool key = r.u8() != 0;
            const std::string delta(r.rest());
            ++downlinks_;
            const bool is_player = stream == Stream::player;
            const XStatus s = is_player ? rx_.apply_delta(b.player_view, delta, b.player_cursor)
                                        : rx_.apply_delta(b.city_view, delta, b.city_cursor);
            if (!s) {
                // Out of step with the server: resume the session, which
                // brings key deltas.
                ++apply_failures_;
                loop_.close(c);
                return;
            }
            if (key) {
                ++key_deltas_;
                key_delta_bytes_ += delta.size();
            } else {
                ++small_deltas_;
                small_delta_bytes_ += delta.size();
            }
            if (is_player && b.pending) {
                b.pending = false;
                if (phase_ == measuring || phase_ == draining) {
                    latency_.record(now - b.sent_us);
                    ++completed_;
                }
            }
            if (b.ready()) {
                if (b.reconnect_start_us) {
                    reconnect_.record(now - b.reconnect_start_us);
                    b.reconnect_start_us = 0;
                }
                if (b.migrate_start_us && key && !is_player) {  // the new owner's key deltas arrived
                    migrate_.record(now - b.migrate_start_us);
                    b.migrate_start_us = 0;
                }
            }
            return;
        }
        case g_pong: return;
        case g_error: ++errors_; return;
        default: ++errors_; loop_.close(c); return;
    }
}

void Runner::send_command(Bot& b, std::uint64_t now) {
    Conn* c = loop_.find(b.conn);
    if (!c) return;
    // Choose a command the rules accept, from the replica.
    const auto [gold, alliance, name0] = b.player_view.read([](const game::Player& p) {
        return std::tuple<std::int64_t, std::uint64_t, char>(p.gold, p.alliance_id,
                                                             p.name.view().empty() ? ' ' : p.name.view().back());
    });
    const std::int32_t farm = b.city_view.read([](const game::City& c) {
        auto it = c.buildings.find(std::uint32_t(game::Building::farm));
        return it == c.buildings.end() ? 0 : it->second;
    });
    auto cmd = out_.reset();
    cmd.player_id() = b.player;
    const std::uint64_t roll = rng_() % 100;
    int kind = 0;
    if (alliance == 0 && roll < 2) {
        kind = 3;
        cmd.kind() = std::uint32_t(game::CommandKind::join_alliance);
        cmd.arg() = std::int64_t(1 + rng_() % kAllianceCount);
    } else if (roll < 12) {
        kind = 2;
        cmd.kind() = std::uint32_t(game::CommandKind::rename);
        cmd.text() = "bot" + std::to_string(b.player) + (name0 == 'a' ? "b" : "a");
    } else if (roll < 22 && farm < kFarmMaxLevel && gold >= std::int64_t(farm + 1) * kFarmGoldPerLevel) {
        kind = 1;
        cmd.kind() = std::uint32_t(game::CommandKind::upgrade_building);
        cmd.target() = std::uint32_t(game::Building::farm);
    } else {
        cmd.kind() = std::uint32_t(game::CommandKind::collect);
    }
    if (!out_.try_save_verified_into(wire_, XSaveOptions{.emission = XWireEmission::dense})) {
        ++errors_;
        return;
    }
    ++kind_counts_[kind];
    loop_.send(*c, c_command, wire_);
    b.pending = true;
    b.sent_us = now;
    ++sent_;
}

void Runner::tick() {
    const std::uint64_t now = now_us();
    const std::uint64_t interval = opt_.rate > 0 ? std::uint64_t(1e6 / opt_.rate) : 0;
    std::size_t ready = 0;
    for (auto& bp : bots_) {
        Bot& b = *bp;
        if (b.logged_out) continue;
        if (b.stage == Bot::off || !loop_.find(b.conn)) {  // dropped: resume with the token
            if (phase_ == logging_out || phase_ == done) continue;
            if (b.pending) {
                b.pending = false;
                ++aborted_;
            }
            if (!b.reconnect_start_us) b.reconnect_start_us = now;
            connect(b);
            continue;
        }
        if (!b.ready()) continue;
        ++ready;
        if (now >= b.next_ping_us) {
            std::string t;
            put_u64(t, now);
            if (Conn* c = loop_.find(b.conn)) loop_.send(*c, c_ping, t);
            b.next_ping_us = now + 2'000'000;
        }
        if (b.pending && now - b.sent_us > 5'000'000) {
            b.pending = false;
            ++timeouts_;
        }
        if ((phase_ == measuring) && !b.pending && interval && now >= b.next_us) {
            send_command(b, now);
            b.next_us = std::max(b.next_us + interval, now);
        }
    }

    switch (phase_) {
        case logging_in:
            if (ready == bots_.size() || now - start_us_ > 30'000'000) {
                std::fprintf(stderr, "bot: %zu of %zu bots logged in after %.1f s\n", ready, bots_.size(),
                             double(now - start_us_) / 1e6);
                phase_ = measuring;
                measure_start_us_ = now;
                measure_end_us_ = now + std::uint64_t(opt_.seconds * 1e6);
                for (auto& b : bots_) b->next_us = now + rng_() % (interval ? interval : 1);
                const std::uint32_t events = opt_.reconnects + opt_.migrations;
                event_every_us_ = events ? std::uint64_t(opt_.seconds * 1e6 * 0.8) / events : 0;
                next_event_us_ = now + std::uint64_t(opt_.seconds * 1e6 * 0.1);
            }
            break;
        case measuring:
            if (event_every_us_ && now >= next_event_us_) {
                next_event_us_ += event_every_us_;
                Bot& b = *bots_[rr_++ % bots_.size()];
                Conn* c = loop_.find(b.conn);
                if (c && b.ready()) {
                    const bool migrate = migrations_done_ < opt_.migrations &&
                                         (reconnects_done_ >= opt_.reconnects || (migrations_done_ <= reconnects_done_));
                    if (migrate && opt_.nodes > 1) {
                        std::string m;
                        put_u64(m, b.player);
                        b.node = (b.node + 1) % opt_.nodes;
                        put_u32(m, b.node);
                        loop_.send(*c, c_gm_migrate, m);
                        b.migrate_start_us = now;
                        ++migrations_done_;
                    } else if (reconnects_done_ < opt_.reconnects) {
                        b.reconnect_start_us = now;
                        loop_.close(*c);  // the next tick resumes it
                        b.stage = Bot::off;
                        ++reconnects_done_;
                    }
                }
            }
            if (now >= measure_end_us_) {
                phase_ = draining;
                phase_until_us_ = now + 5'000'000;
            }
            break;
        case draining: {
            bool any = false;
            for (auto& b : bots_) any = any || b->pending;
            if (!any || now >= phase_until_us_) {
                measure_end_us_ = now;
                for (auto& bp : bots_) {
                    if (Conn* c = loop_.find(bp->conn)) {
                        loop_.send(*c, c_logout, {});
                        bp->logged_out = true;
                    }
                }
                phase_ = logging_out;
                phase_until_us_ = now + 3'000'000;  // the logic nodes save, the write-behind thread writes
            }
            break;
        }
        case logging_out:
            if (now >= phase_until_us_) {
                phase_ = done;
                loop_.stop();
            }
            break;
        case done: break;
    }
}

void Runner::verify() {
    Redis redis;
    std::string host;
    std::uint16_t port = 0;
    if (!parse_host_port(opt_.redis, host, port) || !redis.connect(host, port)) {
        std::fprintf(stderr, "bot: cannot connect to redis for the check\n");
        return;
    }
    // Saves are the servers' own data: an operator tool reads them at the
    // trusted tier.
    XReceiver<XLink::server_peer> rx(XBuffer::maximum_capacity(kPlayerLimit));
    const std::size_t n = std::min<std::size_t>(opt_.verify, bots_.size());
    for (std::size_t i = 0; i < n; ++i) {
        Bot& b = *bots_[i * bots_.size() / (n ? n : 1)];
        ++verify_checked_;
        auto bytes = redis.get(player_key(b.player));
        if (!bytes) continue;
        auto saved = rx.load_typed<game::Player>(*bytes);
        if (!saved) continue;
        const bool same = saved->read([&](const game::Player& s) {
            return b.player_view.read([&](const game::Player& v) {
                return s.gold == v.gold && s.food == v.food && s.alliance_id == v.alliance_id &&
                       s.name.view() == v.name.view() && s.level == v.level;
            });
        });
        verified_ += same;
        if (!same) std::fprintf(stderr, "bot: save of %llu differs from its replica\n", ull(b.player));
    }
}

void Runner::write_report() {
    const double secs = double(measure_end_us_ - measure_start_us_) / 1e6;
    const double thr = secs > 0 ? double(completed_) / secs : 0;
    const double up_per = completed_ ? double(up_bytes_) / double(sent_) : 0;
    const double down_per = completed_ ? double(down_bytes_) / double(completed_) : 0;
    const double small_avg = small_deltas_ ? double(small_delta_bytes_) / double(small_deltas_) : 0;
    const double key_avg = key_deltas_ ? double(key_delta_bytes_) / double(key_deltas_) : 0;
    std::printf("\n== gs_bot report ==\n");
    std::printf("bots %u, target %.1f cmd/s each, measured %.1f s\n", opt_.bots, opt_.rate, secs);
    std::printf("commands: sent %llu, completed %llu (%.0f/s), timeouts %llu, aborted by reconnect %llu\n", ull(sent_),
                ull(completed_), thr, ull(timeouts_), ull(aborted_));
    std::printf("  mix: collect %llu, upgrade %llu, rename %llu, join alliance %llu\n", ull(kind_counts_[0]),
                ull(kind_counts_[1]), ull(kind_counts_[2]), ull(kind_counts_[3]));
    std::printf("latency command -> delta (us): p50 %llu p90 %llu p99 %llu p99.9 %llu max %llu mean %.0f\n",
                ull(latency_.percentile(0.5)), ull(latency_.percentile(0.9)), ull(latency_.percentile(0.99)),
                ull(latency_.percentile(0.999)), ull(latency_.max()), latency_.mean());
    std::printf("login -> both key deltas (us): p50 %llu p99 %llu max %llu\n", ull(login_.percentile(0.5)),
                ull(login_.percentile(0.99)), ull(login_.max()));
    std::printf("deltas: %llu small (%.1f B avg), %llu key (%.1f B avg)\n", ull(small_deltas_), small_avg,
                ull(key_deltas_), key_avg);
    std::printf("bytes on the client links: up %.1f B/command, down %.1f B/command (frames included)\n", up_per,
                down_per);
    std::printf("session resumes: %u, resume -> key deltas (us) p50 %llu max %llu\n", reconnects_done_,
                ull(reconnect_.percentile(0.5)), ull(reconnect_.max()));
    std::printf("migrations: %u requested, %llu completed, request -> new owner's key deltas (us) p50 %llu max %llu\n",
                migrations_done_, ull(migrate_.count()), ull(migrate_.percentile(0.5)), ull(migrate_.max()));
    std::printf("errors %llu, delta apply failures %llu\n", ull(errors_), ull(apply_failures_));
    std::printf("saves checked against replicas: %llu of %llu match\n", ull(verified_), ull(verify_checked_));

    if (opt_.report.empty()) return;
    FILE* f = std::fopen(opt_.report.c_str(), "w");
    if (!f) return;
    std::fprintf(f,
                 "{\"bots\":%u,\"rate_per_bot\":%.2f,\"seconds\":%.2f,\"sent\":%llu,\"completed\":%llu,"
                 "\"throughput\":%.1f,\"timeouts\":%llu,\"aborted\":%llu,"
                 "\"latency_us\":{\"p50\":%llu,\"p90\":%llu,\"p99\":%llu,\"p999\":%llu,\"max\":%llu,\"mean\":%.1f},"
                 "\"small_deltas\":%llu,\"small_delta_avg_bytes\":%.1f,\"key_deltas\":%llu,\"key_delta_avg_bytes\":%.1f,"
                 "\"up_bytes_per_command\":%.1f,\"down_bytes_per_command\":%.1f,"
                 "\"resumes\":%u,\"resume_p50_us\":%llu,\"migrations\":%u,\"migrations_completed\":%llu,"
                 "\"migrate_p50_us\":%llu,\"errors\":%llu,\"apply_failures\":%llu,\"verified\":%llu,\"verify_checked\":%llu}\n",
                 opt_.bots, opt_.rate, secs, ull(sent_), ull(completed_), thr, ull(timeouts_), ull(aborted_),
                 ull(latency_.percentile(0.5)), ull(latency_.percentile(0.9)), ull(latency_.percentile(0.99)),
                 ull(latency_.percentile(0.999)), ull(latency_.max()), latency_.mean(), ull(small_deltas_), small_avg,
                 ull(key_deltas_), key_avg, up_per, down_per, reconnects_done_, ull(reconnect_.percentile(0.5)),
                 migrations_done_, ull(migrate_.count()), ull(migrate_.percentile(0.5)), ull(errors_),
                 ull(apply_failures_), ull(verified_), ull(verify_checked_));
    std::fclose(f);
}

int Runner::run() {
    if (!parse_host_port(opt_.gateway, host_, port_)) return 2;
    int hostile_failed = 0;
    if (opt_.hostile) hostile_failed = hostile_checks(host_, port_, opt_.first_player + opt_.bots + 1);
    loop_.on_frame([this](Conn& c, std::uint8_t t, std::string_view b) {
        Bot& bot = *static_cast<Bot*>(c.user);
        const bool was_ready = bot.ready();
        on_frame(c, t, b);
        if (!was_ready && bot.ready() && phase_ == logging_in) login_.record(now_us() - start_us_);
    });
    loop_.on_close([this](Conn& c) {
        closed_bytes_in_ += c.bytes_in;
        closed_bytes_out_ += c.bytes_out;
        Bot& b = *static_cast<Bot*>(c.user);
        if (b.conn == c.id) b.stage = Bot::off;
    });
    start_us_ = now_us();
    for (std::uint32_t i = 0; i < opt_.bots; ++i) {
        auto b = std::make_unique<Bot>();
        b->player = opt_.first_player + i;
        b->node = home_node(b->player, opt_.nodes);
        if (!connect(*b)) {
            std::fprintf(stderr, "bot: cannot connect to %s\n", opt_.gateway.c_str());
            return 1;
        }
        bots_.push_back(std::move(b));
    }
    // Byte counts of the measurement window: snapshot at its start and end.
    std::uint64_t in0 = 0, out0 = 0;
    bool snap = false;
    loop_.run(5, [&] {
        tick();
        auto totals = [&](std::uint64_t& in, std::uint64_t& out) {
            in = closed_bytes_in_;
            out = closed_bytes_out_;
            for (auto& b : bots_)
                if (Conn* c = loop_.find(b->conn)) {
                    in += c->bytes_in;
                    out += c->bytes_out;
                }
        };
        if (phase_ == measuring && !snap) {
            totals(in0, out0);
            snap = true;
        }
        if (phase_ == logging_out && snap) {
            std::uint64_t in1 = 0, out1 = 0;
            totals(in1, out1);
            down_bytes_ = in1 - in0;
            up_bytes_ = out1 - out0;
            snap = false;
        }
    });
    verify();
    write_report();
    if (opt_.hostile)
        std::printf("hostile-client checks: %s\n", hostile_failed ? "FAILED (see the bot log)" : "all passed");
    const bool ok = hostile_failed == 0 && errors_ == 0 && timeouts_ == 0 && completed_ > 0 && verified_ == verify_checked_ &&
                    migrate_.count() == migrations_done_;
    std::printf("%s\n", ok ? "OK" : "FAILED");
    return ok ? 0 : 1;
}

}  // namespace
}  // namespace gs

int main(int argc, char** argv) {
    gs::Options o;
    for (int i = 1; i + 1 < argc; i += 2) {
        const std::string k = argv[i], v = argv[i + 1];
        if (k == "--gateway") o.gateway = v;
        else if (k == "--redis") o.redis = v;
        else if (k == "--bots") o.bots = std::uint32_t(std::strtoul(v.c_str(), nullptr, 10));
        else if (k == "--first-player") o.first_player = std::strtoull(v.c_str(), nullptr, 10);
        else if (k == "--seconds") o.seconds = std::atof(v.c_str());
        else if (k == "--rate") o.rate = std::atof(v.c_str());
        else if (k == "--nodes") o.nodes = std::uint32_t(std::strtoul(v.c_str(), nullptr, 10));
        else if (k == "--reconnects") o.reconnects = std::uint32_t(std::strtoul(v.c_str(), nullptr, 10));
        else if (k == "--migrations") o.migrations = std::uint32_t(std::strtoul(v.c_str(), nullptr, 10));
        else if (k == "--verify") o.verify = std::uint32_t(std::strtoul(v.c_str(), nullptr, 10));
        else if (k == "--report") o.report = v;
        else if (k == "--hostile") o.hostile = v != "0";
        else {
            std::fprintf(stderr,
                         "usage: gs_bot [--gateway h:p] [--redis h:p] [--bots N] [--first-player ID] [--seconds S] "
                         "[--rate CMDS] [--nodes N] [--reconnects N] [--migrations N] [--verify N] [--report FILE] [--hostile 0|1]\n");
            return 2;
        }
    }
    gs::Runner r(o);
    return r.run();
}
