// gs_gateway: the gateway of the game server demo (demo/gameserver/README.md).
//
// Owns the client connections. Per connection: the schema handshake, login
// with a resume token, heartbeats, a frame size cap and a command rate limit.
// Per player: a session that outlives a dropped connection for a grace period
// (the client reconnects with its token and gets key deltas). Routes every
// command to the logic node that owns the player (the directory in Redis,
// else the player's home node) and never parses a command: the logic node
// reads the client's bytes with XReceiver<XLink::from_client>.
#include "gs/game.hpp"
#include "gs/metrics.hpp"
#include "gs/net.hpp"
#include "gs/redis.hpp"
#include "xoffset_handshake.hpp"

#include <atomic>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <random>
#include <string>
#include <unordered_map>
#include <vector>

namespace gs {
namespace {

std::atomic<bool> g_stop{false};
void on_signal(int) { g_stop = true; }

unsigned long long ull(std::uint64_t v) { return static_cast<unsigned long long>(v); }

struct Options {
    std::uint32_t id = 1;
    std::uint16_t port = 7000;
    std::vector<std::string> nodes;
    std::string redis = "127.0.0.1:6379";
    int heartbeat_ms = 10000;  // a connection silent this long is closed
    int grace_ms = 15000;      // a session without a connection this long logs out
    double rate = 50;          // commands per second per session (burst: twice that)
};

struct Peer {
    enum Kind { client, node } kind = client;
    std::uint32_t node_id = 0;
    enum Stage { hello, login, live } stage = hello;
    std::uint64_t player = 0;
    double tokens = 0;
    std::uint64_t refill_us = 0;
};

struct Session {
    std::uint64_t token = 0;
    std::uint32_t node = 0;
    std::uint64_t conn = 0;  // 0: disconnected
    std::uint64_t disconnected_us = 0;
};

class Gateway {
public:
    explicit Gateway(Options o) : opt_(std::move(o)), rng_(std::random_device{}()) {}
    int run();

private:
    std::uint32_t nodes() const { return std::uint32_t(opt_.nodes.size()); }
    Peer& peer(Conn& c) { return *static_cast<Peer*>(c.user); }
    void on_frame(Conn& c, std::uint8_t type, std::string_view body);
    void on_client_frame(Conn& c, Peer& p, std::uint8_t type, std::string_view body);
    void on_node_frame(Conn& c, Peer& p, std::uint8_t type, std::string_view body);
    void on_close(Conn& c);
    bool to_node(std::uint32_t node, std::uint8_t type, std::uint64_t player, std::string_view rest = {});
    std::uint32_t route(std::uint64_t player);
    void connect_nodes();
    void tick();
    void report();

    Options opt_;
    Loop loop_;
    Redis redis_;
    std::mt19937_64 rng_;
    std::unordered_map<std::uint64_t, std::unique_ptr<Peer>> peers_;  // conn id -> peer
    std::unordered_map<std::uint64_t, Session> sessions_;            // player -> session
    std::vector<std::uint64_t> node_conn_;                           // node -> conn id
    std::vector<bool> node_ok_;
    std::uint64_t last_report_us_ = 0;
    std::uint64_t last_up_ = 0, last_down_ = 0;

    Counter logins_, resumes_, kicks_, commands_up_, rate_limited_, downlinks_, dropped_, heartbeat_closes_,
        expired_, refused_hellos_, moved_;
};

bool Gateway::to_node(std::uint32_t node, std::uint8_t type, std::uint64_t player, std::string_view rest) {
    Conn* c = node < nodes() && node_conn_[node] ? loop_.find(node_conn_[node]) : nullptr;
    if (!c || !node_ok_[node]) return false;
    std::string head;
    put_u64(head, player);
    loop_.send(*c, type, head, rest);
    return true;
}

std::uint32_t Gateway::route(std::uint64_t player) {
    if (auto v = redis_.get(route_key(player))) {
        const unsigned long n = std::strtoul(v->c_str(), nullptr, 10);
        if (n < nodes()) return std::uint32_t(n);
    }
    return home_node(player, nodes());
}

void Gateway::on_frame(Conn& c, std::uint8_t type, std::string_view body) {
    Peer& p = peer(c);
    if (p.kind == Peer::node) on_node_frame(c, p, type, body);
    else on_client_frame(c, p, type, body);
}

void Gateway::on_client_frame(Conn& c, Peer& p, std::uint8_t type, std::string_view body) {
    Reader r{body};
    switch (type) {
        case c_hello: {
            if (p.stage != Peer::hello) break;
            std::string ack;
            const XSchemaHelloResult h =
                accept_schema_hello<game::Player, game::City, game::Command>(std::string(body), ack, "gateway");
            loop_.send(c, g_ack, ack);
            if (!h.accepted()) {
                refused_hellos_.add();
                loop_.close_after_send(c);
                return;
            }
            p.stage = Peer::login;
            return;
        }
        case c_login: {
            if (p.stage != Peer::login) break;
            const std::uint64_t player = r.u64();
            const std::uint64_t token = r.u64();
            if (!r.ok || player == 0) break;
            auto it = sessions_.find(player);
            const bool resume = it != sessions_.end() && token != 0 && token == it->second.token;
            if (it != sessions_.end() && it->second.conn && it->second.conn != c.id) {
                // One connection per player: the newer login wins.
                if (Conn* old = loop_.find(it->second.conn)) {
                    peer(*old).stage = Peer::hello;  // its close no longer touches the session
                    loop_.send(*old, g_error, "logged in elsewhere");
                    loop_.close_after_send(*old);
                    kicks_.add();
                }
            }
            Session& s = sessions_[player];
            if (resume) {
                s.conn = c.id;
                resumes_.add();
                to_node(s.node, l_resync, player);  // the client may have missed deltas: key deltas
            } else {
                s.token = rng_() | 1;
                s.node = route(player);
                s.conn = c.id;
                logins_.add();
                if (!to_node(s.node, l_login, player)) {
                    loop_.send(c, g_error, "logic node unavailable");
                    loop_.close_after_send(c);
                    sessions_.erase(player);
                    return;
                }
            }
            p.stage = Peer::live;
            p.player = player;
            p.tokens = opt_.rate * 2;
            p.refill_us = now_us();
            std::string ok;
            put_u64(ok, s.token);
            put_u32(ok, s.node);
            loop_.send(c, g_login_ok, ok);
            return;
        }
        case c_command: {
            if (p.stage != Peer::live) break;
            const std::uint64_t now = now_us();
            p.tokens = std::min(opt_.rate * 2, p.tokens + double(now - p.refill_us) * opt_.rate / 1e6);
            p.refill_us = now;
            if (p.tokens < 1) {
                rate_limited_.add();
                return;
            }
            p.tokens -= 1;
            auto it = sessions_.find(p.player);
            if (it == sessions_.end()) return;
            commands_up_.add();
            to_node(it->second.node, l_command, p.player, body);  // the client's bytes, unchanged
            return;
        }
        case c_ping: loop_.send(c, g_pong, body.substr(0, 8)); return;
        case c_logout: {
            if (p.stage != Peer::live) break;
            if (auto it = sessions_.find(p.player); it != sessions_.end()) {
                to_node(it->second.node, l_logout, p.player);
                sessions_.erase(it);
            }
            p.stage = Peer::hello;
            loop_.close_after_send(c);
            return;
        }
        case c_gm_migrate: {
            if (p.stage != Peer::live) break;
            const std::uint64_t player = r.u64();
            const std::uint32_t to = r.u32();
            auto it = sessions_.find(player);
            if (!r.ok || it == sessions_.end() || to >= nodes()) return;
            std::string rest;
            put_u32(rest, to);
            to_node(it->second.node, l_migrate, player, rest);
            return;
        }
        default: break;
    }
    loop_.close(c);  // a frame out of order or of an unknown type
}

void Gateway::on_node_frame(Conn& c, Peer& p, std::uint8_t type, std::string_view body) {
    Reader r{body};
    switch (type) {
        case l_ack: {
            const std::uint32_t node = r.u32();
            if (!r.ok || node != p.node_id || !read_schema_ack<game::Player, game::City, game::Command>(std::string(r.rest()))) {
                std::fprintf(stderr, "gateway: node %u refused the link\n", p.node_id);
                loop_.close(c);
                return;
            }
            node_ok_[node] = true;
            std::fprintf(stderr, "gateway: link to node %u verified\n", node);
            return;
        }
        case l_downlink: {
            const std::uint64_t player = r.u64();
            auto it = sessions_.find(player);
            Conn* client = it == sessions_.end() || !it->second.conn ? nullptr : loop_.find(it->second.conn);
            if (!r.ok || !client) {
                dropped_.add();
                return;
            }
            downlinks_.add();
            loop_.send(*client, g_downlink, r.rest());  // stream, key flag, delta: as the logic node wrote them
            return;
        }
        case l_moved: {
            const std::uint64_t player = r.u64();
            const std::uint32_t node = r.u32();
            if (auto it = sessions_.find(player); r.ok && it != sessions_.end() && node < nodes()) {
                it->second.node = node;
                moved_.add();
            }
            return;
        }
        default: loop_.close(c); return;
    }
}

void Gateway::on_close(Conn& c) {
    auto it = peers_.find(c.id);
    if (it == peers_.end()) return;
    Peer& p = *it->second;
    if (p.kind == Peer::node) {
        node_ok_[p.node_id] = false;
        node_conn_[p.node_id] = 0;
        std::fprintf(stderr, "gateway: link to node %u lost\n", p.node_id);
    } else if (p.stage == Peer::live) {
        if (auto s = sessions_.find(p.player); s != sessions_.end() && s->second.conn == c.id) {
            s->second.conn = 0;
            s->second.disconnected_us = now_us();
        }
    }
    peers_.erase(it);
}

void Gateway::connect_nodes() {
    for (std::uint32_t n = 0; n < nodes(); ++n) {
        if (node_conn_[n] && loop_.find(node_conn_[n])) continue;
        std::string host;
        std::uint16_t port = 0;
        if (!parse_host_port(opt_.nodes[n], host, port)) continue;
        const int fd = connect_tcp(host, port);
        if (fd < 0) continue;
        Conn& c = loop_.adopt(fd, kServerFrameLimit);
        auto p = std::make_unique<Peer>();
        p->kind = Peer::node;
        p->node_id = n;
        c.user = p.get();
        peers_.emplace(c.id, std::move(p));
        node_conn_[n] = c.id;
        std::string hello;
        put_u32(hello, opt_.id);
        hello += make_schema_hello<game::Player, game::City, game::Command>("gateway-" + std::to_string(opt_.id),
                                                                             XSchemaRole::server);
        loop_.send(c, l_hello, hello);
    }
}

void Gateway::tick() {
    if (g_stop) loop_.stop();
    connect_nodes();
    const std::uint64_t now = now_us();
    std::vector<std::uint64_t> idle;
    for (auto& [id, p] : peers_) {
        if (p->kind != Peer::client) continue;
        Conn* c = loop_.find(id);
        if (c && now - c->last_recv_us > std::uint64_t(opt_.heartbeat_ms) * 1000) idle.push_back(id);
    }
    for (std::uint64_t id : idle)
        if (Conn* c = loop_.find(id)) {
            heartbeat_closes_.add();
            loop_.close(*c);
        }
    for (auto it = sessions_.begin(); it != sessions_.end();) {
        if (!it->second.conn && now - it->second.disconnected_us > std::uint64_t(opt_.grace_ms) * 1000) {
            to_node(it->second.node, l_logout, it->first);
            expired_.add();
            it = sessions_.erase(it);
        } else {
            ++it;
        }
    }
    report();
}

void Gateway::report() {
    const std::uint64_t now = now_us();
    if (now - last_report_us_ < 5'000'000) return;
    const double secs = double(now - last_report_us_) / 1e6;
    std::size_t online = 0;
    for (auto& [pid, s] : sessions_) online += s.conn != 0;
    std::fprintf(stderr,
                 "gateway %u: sessions %zu (%zu connected) | up %.0f cmd/s, rate-limited %llu | down %.0f frames/s, "
                 "dropped %llu | logins %llu resumes %llu kicks %llu expired %llu heartbeat-closed %llu moved %llu\n",
                 opt_.id, sessions_.size(), online, double(commands_up_.get() - last_up_) / secs,
                 ull(rate_limited_.get()), double(downlinks_.get() - last_down_) / secs, ull(dropped_.get()),
                 ull(logins_.get()), ull(resumes_.get()), ull(kicks_.get()), ull(expired_.get()),
                 ull(heartbeat_closes_.get()), ull(moved_.get()));
    last_report_us_ = now;
    last_up_ = commands_up_.get();
    last_down_ = downlinks_.get();
}

int Gateway::run() {
    std::string host;
    std::uint16_t port = 0;
    if (!parse_host_port(opt_.redis, host, port) || !redis_.connect(host, port)) {
        std::fprintf(stderr, "gs_gateway: cannot connect to redis %s\n", opt_.redis.c_str());
        return 1;
    }
    const int lfd = listen_tcp(opt_.port);
    if (lfd < 0) {
        std::fprintf(stderr, "gs_gateway: cannot listen on %u\n", opt_.port);
        return 1;
    }
    node_conn_.assign(nodes(), 0);
    node_ok_.assign(nodes(), false);
    loop_.listen(lfd, kClientFrameLimit, [this](Conn& c) {
        auto p = std::make_unique<Peer>();
        c.user = p.get();
        peers_.emplace(c.id, std::move(p));
    });
    loop_.on_frame([this](Conn& c, std::uint8_t t, std::string_view b) { on_frame(c, t, b); });
    loop_.on_close([this](Conn& c) { on_close(c); });
    std::fprintf(stderr, "gateway %u: listening on %u, %u logic nodes\n", opt_.id, opt_.port, nodes());
    last_report_us_ = now_us();
    loop_.run(100, [this] { tick(); });
    return 0;
}

std::vector<std::string> split(const std::string& s) {
    std::vector<std::string> out;
    std::size_t start = 0;
    for (;;) {
        const auto comma = s.find(',', start);
        out.push_back(s.substr(start, comma == std::string::npos ? std::string::npos : comma - start));
        if (comma == std::string::npos) return out;
        start = comma + 1;
    }
}

}  // namespace
}  // namespace gs

int main(int argc, char** argv) {
    gs::Options o;
    for (int i = 1; i + 1 < argc; i += 2) {
        const std::string k = argv[i], v = argv[i + 1];
        if (k == "--id") o.id = std::uint32_t(std::strtoul(v.c_str(), nullptr, 10));
        else if (k == "--port") o.port = std::uint16_t(std::strtoul(v.c_str(), nullptr, 10));
        else if (k == "--nodes") o.nodes = gs::split(v);
        else if (k == "--redis") o.redis = v;
        else if (k == "--heartbeat-ms") o.heartbeat_ms = std::atoi(v.c_str());
        else if (k == "--grace-ms") o.grace_ms = std::atoi(v.c_str());
        else if (k == "--rate") o.rate = std::atof(v.c_str());
        else {
            std::fprintf(stderr, "usage: gs_gateway --port P --nodes h:p,h:p [--id N] [--redis h:p] [--heartbeat-ms MS] "
                                 "[--grace-ms MS] [--rate CMDS]\n");
            return 2;
        }
    }
    if (o.nodes.empty()) {
        std::fprintf(stderr, "gs_gateway: --nodes is required\n");
        return 2;
    }
    std::signal(SIGINT, gs::on_signal);
    std::signal(SIGTERM, gs::on_signal);
    gs::Gateway g(o);
    return g.run();
}
