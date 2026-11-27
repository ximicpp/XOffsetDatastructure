// gs_logic: a logic node of the game server demo (demo/gameserver/README.md).
//
// Threads:
//   - the IO thread runs the poll loop: links from gateways, links to and
//     from the other logic nodes;
//   - N shard threads, each an actor with a mailbox. A shard owns its
//     entities (players with their cities, alliances): one TypedXBuffer per
//     entity, used by that thread only;
//   - one write-behind thread that saves dirty entities to Redis in batches.
//
// Every frame between threads and processes carries ids and bytes, never a
// reference into another thread's buffers.
#include "gs/game.hpp"
#include "gs/metrics.hpp"
#include "gs/net.hpp"
#include "gs/redis.hpp"
#include "xoffset_handshake.hpp"
#include "xoffset_receiver.hpp"

#include <atomic>
#include <condition_variable>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace gs {
namespace {

std::atomic<bool> g_stop{false};
void on_signal(int) { g_stop = true; }

struct Options {
    std::uint32_t node = 0;
    std::vector<std::string> nodes;  // host:port of every logic node, this one included
    std::uint32_t shards = 4;
    std::string redis = "127.0.0.1:6379";
    int flush_ms = 1000;  // how often dirty entities are saved
};

unsigned long long ull(std::uint64_t v) { return static_cast<unsigned long long>(v); }

// ---- Write-behind saves --------------------------------------------------------

// Shards hand finished saves here and go on; this thread writes them to
// Redis in one pipelined batch per round. A load first looks here, so a
// player who logs out and straight back in gets the save not yet written.
class Persister {
public:
    explicit Persister(const std::string& redis_addr) : addr_(redis_addr) {}

    bool start() {
        std::string host;
        std::uint16_t port = 0;
        if (!parse_host_port(addr_, host, port) || !redis_.connect(host, port)) return false;
        worker_ = std::thread([this] { run(); });
        return true;
    }
    void stop() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            stopping_ = true;
        }
        wake_.notify_one();
        if (worker_.joinable()) worker_.join();
    }
    void put(std::string key, std::string bytes) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            pending_[std::move(key)] = std::move(bytes);
        }
        wake_.notify_one();
    }
    // The newest save not yet confirmed by Redis.
    std::optional<std::string> pending_get(const std::string& key) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (auto it = pending_.find(key); it != pending_.end()) return it->second;
        if (auto it = inflight_.find(key); it != inflight_.end()) return it->second;
        return std::nullopt;
    }

    Counter writes, bytes, batches, failures;

private:
    void run() {
        for (;;) {
            {
                std::unique_lock<std::mutex> lock(mutex_);
                wake_.wait(lock, [&] { return stopping_ || !pending_.empty(); });
                if (pending_.empty() && stopping_) return;
                inflight_.swap(pending_);
            }
            std::vector<std::pair<std::string, std::string>> batch(inflight_.begin(), inflight_.end());
            std::uint64_t n = 0;
            for (auto& [k, v] : batch) n += v.size();
            if (redis_.set_many(batch)) {
                writes.add(batch.size());
                bytes.add(n);
                batches.add();
            } else {
                failures.add();
                std::fprintf(stderr, "persister: redis write failed: %s\n", redis_.error().c_str());
            }
            std::lock_guard<std::mutex> lock(mutex_);
            inflight_.clear();
        }
    }

    std::string addr_;
    Redis redis_;
    std::thread worker_;
    std::mutex mutex_;
    std::condition_variable wake_;
    std::map<std::string, std::string> pending_;
    std::map<std::string, std::string> inflight_;
    bool stopping_ = false;
};

class Node;

// ---- Shard: one thread, the only user of its entities ------------------------------

class Shard {
public:
    explicit Shard(Node& node) : node_(node) {}

    bool start();
    void stop() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            stopping_ = true;
        }
        wake_.notify_one();
        if (worker_.joinable()) worker_.join();
    }
    void post(std::function<void(Shard&)> job) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            jobs_.push_back(std::move(job));
        }
        wake_.notify_one();
    }

    // ---- Jobs ----
    void login(std::uint64_t player_id, std::uint32_t gateway);
    void logout(std::uint64_t player_id);
    void resync(std::uint64_t player_id);
    void command(std::uint64_t player_id, std::uint32_t gateway, const std::string& wire);
    void add_member(std::uint64_t alliance_id, std::uint64_t player_id, std::uint32_t reply_node);
    void joined(std::uint64_t player_id, std::uint64_t alliance_id);
    void migrate_out(std::uint64_t player_id, std::uint32_t to);
    void migrate_in(std::uint64_t player_id, std::uint32_t gateway, bool dirty, const std::string& player_wire,
                    const std::string& city_wire);

    std::atomic<std::uint64_t> players{0};

private:
    struct OwnedPlayer {
        TypedXBuffer<game::Player> player;
        TypedXBuffer<game::City> city;
        XDeltaBaseline player_base;  // what the client holds
        XDeltaBaseline city_base;
        std::uint32_t gateway = 0;  // which gateway holds the client's session
        bool dirty = false;
    };

    static OwnedPlayer make_owned() {
        return OwnedPlayer{TypedXBuffer<game::Player>::create(1024, grow_to(kPlayerLimit)),
                           TypedXBuffer<game::City>::create(512, grow_to(kCityLimit)), {}, {}, 0, false};
    }

    void run();
    void boot_alliances();
    std::optional<std::string> load(const std::string& key, bool& failed);
    OwnedPlayer* find(std::uint64_t id) {
        auto it = players_.find(id);
        return it == players_.end() ? nullptr : &it->second;
    }
    void create(OwnedPlayer& e, std::uint64_t player_id);
    void save(OwnedPlayer& e);
    void save_dirty();
    void sync(OwnedPlayer& e);
    template <typename T>
    void push(std::uint64_t player_id, std::uint32_t gateway, TypedXBuffer<T>& buffer, XDeltaBaseline& base, Stream s);
    bool refuse(const char* why);
    bool upgrade(OwnedPlayer& e, std::uint32_t building);
    bool rename(OwnedPlayer& e, std::string_view name);
    std::int32_t building_level(OwnedPlayer& e, game::Building b);

    Node& node_;
    std::thread worker_;
    std::mutex mutex_;
    std::condition_variable wake_;
    std::deque<std::function<void(Shard&)>> jobs_;
    bool stopping_ = false;

    // Used by this shard's thread only.
    Redis redis_;  // loads and directory writes
    std::unordered_map<std::uint64_t, OwnedPlayer> players_;
    std::unordered_map<std::uint64_t, TypedXBuffer<game::Alliance>> alliances_;
    std::unordered_set<std::uint64_t> dirty_alliances_;
    std::unordered_map<std::uint64_t, std::uint32_t> moved_;  // player -> node it migrated to
    TypedXBuffer<game::ConfigTable> config_ = TypedXBuffer<game::ConfigTable>::create(512, grow_to(kConfigLimit));
    XReceiver<XLink::server_peer> peer_rx_{grow_to(kAllianceLimit)};                      // Redis, other nodes
    XReceiver<XLink::from_client> client_rx_{XBuffer::maximum_capacity(kCommandLimit)};  // client commands
    std::string out_;
};

// ---- Node: the IO thread and the links ------------------------------------------

class Node {
public:
    explicit Node(Options o) : opt_(std::move(o)), persister_(opt_.redis) {}

    int run();

    const Options& options() const { return opt_; }
    std::uint32_t id() const { return opt_.node; }
    std::uint32_t nodes() const { return std::uint32_t(opt_.nodes.size()); }
    Persister& persister() { return persister_; }
    const std::string& config_wire() const { return config_wire_; }

    Shard& shard_of(std::uint64_t entity_id) {
        return *shards_[std::size_t((entity_id / nodes()) % shards_.size())];
    }
    bool owns_alliance_shard(std::uint64_t alliance_id, const Shard* s) {
        return alliance_node(alliance_id, nodes()) == id() && &shard_of(alliance_id) == s;
    }

    // Any thread: queue a frame on a link; the IO thread writes it.
    void to_gateway(std::uint32_t gateway, std::uint8_t type, std::string body) {
        loop_.post([this, gateway, type, b = std::move(body)] {
            auto it = gateways_.find(gateway);
            Conn* c = it == gateways_.end() ? nullptr : loop_.find(it->second);
            if (c) loop_.send(*c, type, b);
            else dropped_downlinks.add();
        });
    }
    void to_node(std::uint32_t node, std::uint8_t type, std::string body) {
        loop_.post([this, node, type, b = std::move(body)] {
            Conn* c = peer_out_[node] ? loop_.find(peer_out_[node]) : nullptr;
            if (c) loop_.send(*c, type, b);
            else std::fprintf(stderr, "node %u: no link to node %u, frame %u dropped\n", id(), node, type);
        });
    }

    Counter commands, refused, deltas, delta_bytes, key_deltas, logins, creates, migrations_out, migrations_in,
        dropped_downlinks, forwarded;
    // Shard time per command: check, rules, in-place change, deltas written
    // and handed to the IO thread. Saves and the network are not in it.
    Counter command_ns;

private:
    struct Link {
        enum Kind { unknown, gateway, node } kind = unknown;
        std::uint32_t id = 0;
    };

    void on_frame(Conn& c, std::uint8_t type, std::string_view body);
    void connect_peers();
    void report();

    Options opt_;
    Persister persister_;
    std::string config_wire_;
    std::vector<std::unique_ptr<Shard>> shards_;
    Loop loop_;
    std::unordered_map<std::uint32_t, std::uint64_t> gateways_;  // gateway id -> conn id
    std::vector<std::uint64_t> peer_out_;                        // node -> outgoing conn id (0: none)
    std::vector<bool> peer_verified_;
    std::vector<std::unique_ptr<Link>> links_;
    std::uint64_t last_report_us_ = 0;
    std::uint64_t last_commands_ = 0;
    std::uint64_t last_deltas_ = 0;
};

// ---- Shard implementation ----------------------------------------------------------

bool Shard::start() {
    std::string host;
    std::uint16_t port = 0;
    if (!parse_host_port(node_.options().redis, host, port) || !redis_.connect(host, port)) return false;
    if (!peer_rx_.reload(config_, node_.config_wire())) return false;
    worker_ = std::thread([this] { run(); });
    post([](Shard& s) { s.boot_alliances(); });
    return true;
}

void Shard::run() {
    const auto flush_every = std::chrono::milliseconds(node_.options().flush_ms);
    auto next_flush = Clock::now() + flush_every;
    for (;;) {
        std::function<void(Shard&)> job;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            wake_.wait_until(lock, next_flush, [&] { return stopping_ || !jobs_.empty(); });
            if (!jobs_.empty()) {
                job = std::move(jobs_.front());
                jobs_.pop_front();
            } else if (stopping_) {
                break;
            }
        }
        if (job) job(*this);
        if (Clock::now() >= next_flush) {
            save_dirty();
            next_flush = Clock::now() + flush_every;
        }
    }
    // Shutdown: every loaded entity goes to storage.
    for (auto& [id, e] : players_) save(e);
    save_dirty();
}

std::optional<std::string> Shard::load(const std::string& key, bool& failed) {
    failed = false;
    if (auto p = node_.persister().pending_get(key)) return p;
    auto v = redis_.get(key);
    if (!v && !redis_.ok()) failed = true;
    return v;
}

void Shard::boot_alliances() {
    for (std::uint64_t aid = 1; aid <= kAllianceCount; ++aid) {
        if (!node_.owns_alliance_shard(aid, this)) continue;
        auto a = TypedXBuffer<game::Alliance>::create(1024, grow_to(kAllianceLimit));
        bool failed = false;
        if (auto saved = load(alliance_key(aid), failed); saved && peer_rx_.reload(a, *saved)) {
            alliances_.insert_or_assign(aid, std::move(a));
            continue;
        }
        auto r = a.root();
        r.alliance_id() = aid;
        r.name() = "Alliance " + std::to_string(aid);
        r.notice() = std::string("Welcome!");
        alliances_.insert_or_assign(aid, std::move(a));
        dirty_alliances_.insert(aid);
    }
}

void Shard::create(OwnedPlayer& e, std::uint64_t player_id) {
    {
        auto p = e.player.root();
        p.player_id() = player_id;
        p.city_id() = player_id;
        p.level() = 1;
        p.gold() = 300;
        p.food() = 500;
        p.name() = "p" + std::to_string(player_id);
    }
    e.player.edit(
        [](game::Player& p) {
            for (std::uint32_t hero = 1; hero <= 3; ++hero) {
                game::Hero& h = p.heroes.emplace_back();
                h.id = hero;
                h.level = 1;
                h.star = 1;
            }
            p.items.emplace(100u, std::int64_t{5});
        },
        256);
    {
        auto c = e.city.root();
        c.city_id() = player_id;
        c.owner_id() = player_id;
        c.x() = std::int32_t(player_id % 1000);
        c.y() = std::int32_t((player_id / 1000) % 1000);
        c.level() = 1;
    }
    e.city.edit(
        [](game::City& c) {
            c.buildings.emplace(std::uint32_t(game::Building::town_hall), 1);
            c.buildings.emplace(std::uint32_t(game::Building::farm), 1);
        },
        64);
    e.dirty = true;
    node_.creates.add();
}

void Shard::login(std::uint64_t player_id, std::uint32_t gateway) {
    moved_.erase(player_id);  // the directory sends it here now
    if (OwnedPlayer* e = find(player_id)) {  // already loaded: a new session takes over
        e->gateway = gateway;
        resync(player_id);
        return;
    }
    OwnedPlayer e = make_owned();
    bool failed = false;
    auto pw = load(player_key(player_id), failed);
    if (failed) {
        std::fprintf(stderr, "node %u: load of %llu failed: %s\n", node_.id(), ull(player_id), redis_.error().c_str());
        return;
    }
    if (!pw) {
        create(e, player_id);
    } else {
        auto cw = load(city_key(player_id), failed);
        if (!cw || !peer_rx_.reload(e.player, *pw) || !peer_rx_.reload(e.city, *cw)) {
            std::fprintf(stderr, "node %u: save of %llu refused\n", node_.id(), ull(player_id));
            return;
        }
    }
    e.gateway = gateway;
    OwnedPlayer& slot = players_.insert_or_assign(player_id, std::move(e)).first->second;
    players = players_.size();
    node_.logins.add();
    sync(slot);  // fresh baselines: key deltas, the whole state
}

void Shard::logout(std::uint64_t player_id) {
    moved_.erase(player_id);
    auto it = players_.find(player_id);
    if (it == players_.end()) return;
    save(it->second);
    players_.erase(it);
    players = players_.size();
}

void Shard::resync(std::uint64_t player_id) {
    OwnedPlayer* e = find(player_id);
    if (!e) return;
    e->player_base.reset();
    e->city_base.reset();
    sync(*e);
}

void Shard::save(OwnedPlayer& e) {
    if (e.player.relayout_advised()) (void)e.player.relayout();
    if (e.city.relayout_advised()) (void)e.city.relayout();
    const std::uint64_t id = e.player.root().player_id();
    e.player.save_verified_into(out_);
    node_.persister().put(player_key(id), out_);
    e.city.save_verified_into(out_);
    node_.persister().put(city_key(id), out_);
    e.dirty = false;
}

void Shard::save_dirty() {
    for (auto& [id, e] : players_)
        if (e.dirty) save(e);
    for (std::uint64_t aid : dirty_alliances_) {
        auto& a = alliances_.at(aid);
        if (a.relayout_advised()) (void)a.relayout();
        a.save_verified_into(out_);
        node_.persister().put(alliance_key(aid), out_);
    }
    dirty_alliances_.clear();
}

template <typename T>
void Shard::push(std::uint64_t player_id, std::uint32_t gateway, TypedXBuffer<T>& buffer, XDeltaBaseline& base,
                 Stream s) {
    std::string body;
    put_u64(body, player_id);
    put_u8(body, std::uint8_t(s));
    put_u8(body, 0);
    const XDeltaStats st = buffer.save_delta_into(out_, base);
    if (st.empty) return;
    body[9] = char(st.key ? 1 : 0);
    body += out_;
    node_.deltas.add();
    node_.delta_bytes.add(out_.size());
    if (st.key) node_.key_deltas.add();
    node_.to_gateway(gateway, l_downlink, std::move(body));
}

void Shard::sync(OwnedPlayer& e) {
    const std::uint64_t id = e.player.root().player_id();
    push(id, e.gateway, e.player, e.player_base, Stream::player);
    push(id, e.gateway, e.city, e.city_base, Stream::city);
}

bool Shard::refuse(const char*) {
    node_.refused.add();
    return false;
}

std::int32_t Shard::building_level(OwnedPlayer& e, game::Building b) {
    return e.city.read([&](const game::City& c) {
        auto it = c.buildings.find(std::uint32_t(b));
        return it == c.buildings.end() ? 0 : it->second;
    });
}

bool Shard::upgrade(OwnedPlayer& e, std::uint32_t building) {
    std::int32_t max_level = 0;
    std::int64_t per_level = 0;
    config_.read([&](const game::ConfigTable& t) {
        for (const game::BuildingConfig& b : t.buildings)
            if (b.building == building) {
                max_level = b.max_level;
                per_level = b.gold_per_level;
            }
    });
    if (max_level == 0) return refuse("unknown building");
    const std::int32_t level = building_level(e, game::Building(building));
    if (level >= max_level) return refuse("max level");
    const std::int64_t cost = std::int64_t(level + 1) * per_level;
    if (e.player.read([](const game::Player& p) { return p.gold; }) < cost) return refuse("gold");
    e.player.root().gold() -= cost;
    e.city.edit(
        [&](game::City& c) {
            c.buildings[building] = level + 1;
            if (game::Building(building) == game::Building::town_hall) c.level = level + 1;
        },
        64);
    return true;
}

bool Shard::rename(OwnedPlayer& e, std::string_view name) {
    if (name.empty() || name.size() > 16) return refuse("name length");
    for (char ch : name)
        if (ch < 0x20 || ch > 0x7E) return refuse("name characters");
    e.player.edit([&](game::Player& p) { p.name.assign(name); }, 64);
    return true;
}

void Shard::command(std::uint64_t player_id, std::uint32_t gateway, const std::string& wire) {
    OwnedPlayer* e = find(player_id);
    if (!e) {
        if (auto m = moved_.find(player_id); m != moved_.end()) {  // routed here before the gateway learned of the move
            std::string body;
            put_u64(body, player_id);
            put_u32(body, gateway);
            body += wire;
            node_.forwarded.add();
            node_.to_node(m->second, n_command, std::move(body));
        }
        return;
    }
    node_.commands.add();
    const auto t0 = Clock::now();
    struct Timer {
        Node& n;
        Clock::time_point t0;
        ~Timer() {
            n.command_ns.add(std::uint64_t(std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now() - t0).count()));
        }
    } timer{node_, t0};
    // The client's bytes: bounds tier, const access, values checked below.
    auto v = client_rx_.view<game::Command>(wire);
    if (!v) {
        refuse("bounds");
        return;
    }
    const game::Command& c = v->root();
    if (c.player_id != player_id) {
        refuse("player id does not match the session");
        return;
    }
    bool changed = false;
    switch (game::CommandKind(c.kind)) {
        case game::CommandKind::collect: {
            const std::int32_t farm = building_level(*e, game::Building::farm);
            auto p = e->player.root();
            p.gold() += 100;
            p.food() += 50 * farm;
            changed = true;
            break;
        }
        case game::CommandKind::upgrade_building: changed = upgrade(*e, c.target); break;
        case game::CommandKind::rename: changed = rename(*e, c.text.view()); break;
        case game::CommandKind::join_alliance: {
            const auto aid = std::uint64_t(c.arg);
            if (aid == 0 || aid > kAllianceCount || e->player.root().alliance_id() != 0) {
                refuse("alliance");
                break;
            }
            const std::uint32_t an = alliance_node(aid, node_.nodes());
            if (an == node_.id()) {
                const std::uint32_t self = node_.id();
                node_.shard_of(aid).post([aid, player_id, self](Shard& s) { s.add_member(aid, player_id, self); });
            } else {
                std::string body;
                put_u64(body, aid);
                put_u64(body, player_id);
                put_u32(body, node_.id());
                node_.to_node(an, n_join, std::move(body));
            }
            break;
        }
        default: refuse("unknown kind"); break;
    }
    if (changed) {
        e->dirty = true;
        sync(*e);
    }
}

void Shard::add_member(std::uint64_t alliance_id, std::uint64_t player_id, std::uint32_t reply_node) {
    bool admitted = false;
    if (auto it = alliances_.find(alliance_id); it != alliances_.end()) {
        admitted = it->second.read([&](const game::Alliance& a) {
            if (a.members.size() >= 1000) return false;
            for (std::uint64_t m : a.members)
                if (m == player_id) return false;
            return true;
        });
        if (admitted) {
            it->second.edit([&](game::Alliance& a) { a.members.push_back(player_id); }, 64);
            dirty_alliances_.insert(alliance_id);
        }
    }
    const std::uint64_t result = admitted ? alliance_id : 0;
    if (reply_node == node_.id()) {
        node_.shard_of(player_id).post([player_id, result](Shard& s) { s.joined(player_id, result); });
    } else {
        std::string body;
        put_u64(body, player_id);
        put_u64(body, result);
        node_.to_node(reply_node, n_joined, std::move(body));
    }
}

void Shard::joined(std::uint64_t player_id, std::uint64_t alliance_id) {
    OwnedPlayer* e = find(player_id);
    if (!e) {
        if (auto m = moved_.find(player_id); m != moved_.end()) {
            std::string body;
            put_u64(body, player_id);
            put_u64(body, alliance_id);
            node_.to_node(m->second, n_joined, std::move(body));
        }
        return;
    }
    if (alliance_id == 0) return;
    e->player.root().alliance_id() = alliance_id;
    e->dirty = true;
    sync(*e);
}

void Shard::migrate_out(std::uint64_t player_id, std::uint32_t to) {
    auto it = players_.find(player_id);
    if (it == players_.end() || to == node_.id() || to >= node_.nodes()) return;
    OwnedPlayer& e = it->second;
    std::string player_wire, city_wire;
    e.player.save_verified_into(player_wire);
    e.city.save_verified_into(city_wire);
    std::string body;
    put_u64(body, player_id);
    put_u32(body, e.gateway);
    put_u8(body, e.dirty ? 1 : 0);
    put_u32(body, std::uint32_t(player_wire.size()));
    body += player_wire;
    body += city_wire;
    players_.erase(it);
    players = players_.size();
    moved_[player_id] = to;
    node_.migrations_out.add();
    node_.to_node(to, n_migrate, std::move(body));
}

void Shard::migrate_in(std::uint64_t player_id, std::uint32_t gateway, bool dirty, const std::string& player_wire,
                       const std::string& city_wire) {
    OwnedPlayer e = make_owned();
    if (!peer_rx_.reload(e.player, player_wire) || !peer_rx_.reload(e.city, city_wire)) {
        std::fprintf(stderr, "node %u: migration of %llu refused\n", node_.id(), ull(player_id));
        return;
    }
    moved_.erase(player_id);
    e.gateway = gateway;
    e.dirty = dirty;
    OwnedPlayer& slot = players_.insert_or_assign(player_id, std::move(e)).first->second;
    players = players_.size();
    // The directory first, then the gateway: later logins and frames come here.
    if (!redis_.set(route_key(player_id), std::to_string(node_.id())))
        std::fprintf(stderr, "node %u: directory write failed: %s\n", node_.id(), redis_.error().c_str());
    std::string body;
    put_u64(body, player_id);
    put_u32(body, node_.id());
    node_.to_gateway(gateway, l_moved, std::move(body));
    node_.migrations_in.add();
    sync(slot);  // new baselines: the client gets key deltas from its new owner
}

// ---- Node implementation ----------------------------------------------------------

void Node::on_frame(Conn& c, std::uint8_t type, std::string_view body) {
    Link& link = *static_cast<Link*>(c.user);
    Reader r{body};
    // Handshakes: the first frame of every incoming link.
    if (type == l_hello || type == n_hello) {
        const std::uint32_t peer_id = r.u32();
        const std::string hello(r.rest());
        std::string ack, reply;
        bool accepted;
        if (type == l_hello) {
            accepted = accept_schema_hello<game::Player, game::City, game::Command>(hello, ack, "logic").accepted();
        } else {
            accepted = accept_schema_hello<game::Player, game::City, game::Alliance>(hello, ack, "logic").accepted();
        }
        put_u32(reply, id());
        reply += ack;
        loop_.send(c, type == l_hello ? l_ack : n_ack, reply);
        if (!r.ok || !accepted) {
            std::fprintf(stderr, "node %u: link refused\n", id());
            loop_.close_after_send(c);
            return;
        }
        link.kind = type == l_hello ? Link::gateway : Link::node;
        link.id = peer_id;
        if (type == l_hello) {
            gateways_[peer_id] = c.id;
            std::fprintf(stderr, "node %u: gateway %u connected\n", id(), peer_id);
        }
        return;
    }
    if (type == n_ack) {  // on our outgoing link to another node
        const std::uint32_t peer = r.u32();
        const bool ok = r.ok && peer < nodes() &&
                        bool(read_schema_ack<game::Player, game::City, game::Alliance>(std::string(r.rest())));
        if (!ok) {
            loop_.close(c);
            return;
        }
        peer_verified_[peer] = true;
        std::fprintf(stderr, "node %u: link to node %u verified\n", id(), peer);
        return;
    }
    if (link.kind == Link::unknown) {  // nothing before the handshake
        loop_.close(c);
        return;
    }
    switch (type) {
        case l_login: {
            const auto pid = r.u64();
            const std::uint32_t gw = link.id;
            if (r.ok) shard_of(pid).post([pid, gw](Shard& s) { s.login(pid, gw); });
            break;
        }
        case l_command: {
            const auto pid = r.u64();
            const std::uint32_t gw = link.id;
            if (r.ok)
                shard_of(pid).post([pid, gw, w = std::string(r.rest())](Shard& s) { s.command(pid, gw, w); });
            break;
        }
        case l_logout: {
            const auto pid = r.u64();
            if (r.ok) shard_of(pid).post([pid](Shard& s) { s.logout(pid); });
            break;
        }
        case l_resync: {
            const auto pid = r.u64();
            if (r.ok) shard_of(pid).post([pid](Shard& s) { s.resync(pid); });
            break;
        }
        case l_migrate: {
            const auto pid = r.u64();
            const auto to = r.u32();
            if (r.ok) shard_of(pid).post([pid, to](Shard& s) { s.migrate_out(pid, to); });
            break;
        }
        case n_join: {
            const auto aid = r.u64();
            const auto pid = r.u64();
            const auto reply = r.u32();
            if (r.ok && reply < nodes())
                shard_of(aid).post([aid, pid, reply](Shard& s) { s.add_member(aid, pid, reply); });
            break;
        }
        case n_joined: {
            const auto pid = r.u64();
            const auto aid = r.u64();
            if (r.ok) shard_of(pid).post([pid, aid](Shard& s) { s.joined(pid, aid); });
            break;
        }
        case n_migrate: {
            const auto pid = r.u64();
            const auto gw = r.u32();
            const bool dirty = r.u8() != 0;
            const auto psize = r.u32();
            std::string_view rest = r.rest();
            if (!r.ok || psize > rest.size()) break;
            shard_of(pid).post([pid, gw, dirty, pw = std::string(rest.substr(0, psize)),
                                cw = std::string(rest.substr(psize))](Shard& s) { s.migrate_in(pid, gw, dirty, pw, cw); });
            break;
        }
        case n_command: {
            const auto pid = r.u64();
            const auto gw = r.u32();
            if (r.ok)
                shard_of(pid).post([pid, gw, w = std::string(r.rest())](Shard& s) { s.command(pid, gw, w); });
            break;
        }
        default: loop_.close(c); break;
    }
}

void Node::connect_peers() {
    for (std::uint32_t j = 0; j < nodes(); ++j) {
        if (j == id() || (peer_out_[j] && loop_.find(peer_out_[j]))) continue;
        peer_out_[j] = 0;
        peer_verified_[j] = false;
        std::string host;
        std::uint16_t port = 0;
        if (!parse_host_port(opt_.nodes[j], host, port)) continue;
        const int fd = connect_tcp(host, port);
        if (fd < 0) continue;
        Conn& c = loop_.adopt(fd, kServerFrameLimit);
        links_.push_back(std::make_unique<Link>());
        links_.back()->kind = Link::node;
        links_.back()->id = j;
        c.user = links_.back().get();
        peer_out_[j] = c.id;
        std::string hello;
        put_u32(hello, id());
        hello += make_schema_hello<game::Player, game::City, game::Alliance>("node-" + std::to_string(id()),
                                                                              XSchemaRole::server);
        loop_.send(c, n_hello, hello);
    }
}

void Node::report() {
    const std::uint64_t now = now_us();
    if (now - last_report_us_ < 5'000'000) return;
    const double secs = double(now - last_report_us_) / 1e6;
    std::uint64_t players = 0;
    for (auto& s : shards_) players += s->players.load();
    const std::uint64_t cmds = commands.get(), ds = deltas.get();
    std::fprintf(stderr,
                 "node %u: players %llu | commands %.0f/s (%.2f us in shard) refused %llu | deltas %.0f/s, %.1f B avg, %llu key | "
                 "saves %llu (%.1f MB, %llu batches) | migrations out %llu in %llu fwd %llu\n",
                 id(), ull(players), double(cmds - last_commands_) / secs,
                 cmds ? double(command_ns.get()) / double(cmds) / 1000.0 : 0.0, ull(refused.get()),
                 double(ds - last_deltas_) / secs, ds ? double(delta_bytes.get()) / double(ds) : 0.0,
                 ull(key_deltas.get()), ull(persister_.writes.get()), double(persister_.bytes.get()) / 1e6,
                 ull(persister_.batches.get()), ull(migrations_out.get()), ull(migrations_in.get()),
                 ull(forwarded.get()));
    last_report_us_ = now;
    last_commands_ = cmds;
    last_deltas_ = ds;
}

int Node::run() {
    if (opt_.node >= opt_.nodes.size()) {
        std::fprintf(stderr, "gs_logic: --node must index --nodes\n");
        return 2;
    }
    std::string host;
    std::uint16_t port = 0;
    if (!parse_host_port(opt_.nodes[opt_.node], host, port)) return 2;
    const int lfd = listen_tcp(port);
    if (lfd < 0) {
        std::fprintf(stderr, "gs_logic: cannot listen on %u\n", port);
        return 1;
    }
    if (!persister_.start()) {
        std::fprintf(stderr, "gs_logic: cannot connect to redis %s\n", opt_.redis.c_str());
        return 1;
    }
    config_wire_ = build_config();
    for (std::uint32_t i = 0; i < opt_.shards; ++i) shards_.push_back(std::make_unique<Shard>(*this));
    for (auto& s : shards_)
        if (!s->start()) {
            std::fprintf(stderr, "gs_logic: shard start failed (redis or config)\n");
            return 1;
        }
    peer_out_.assign(nodes(), 0);
    peer_verified_.assign(nodes(), false);

    loop_.listen(lfd, kServerFrameLimit, [this](Conn& c) {
        links_.push_back(std::make_unique<Link>());
        c.user = links_.back().get();
    });
    loop_.on_frame([this](Conn& c, std::uint8_t t, std::string_view b) { on_frame(c, t, b); });
    loop_.on_close([this](Conn& c) {
        Link& l = *static_cast<Link*>(c.user);
        if (l.kind == Link::gateway && gateways_.count(l.id) && gateways_[l.id] == c.id) gateways_.erase(l.id);
    });
    std::fprintf(stderr, "node %u: listening on %u, %u shards, redis %s\n", id(), port, opt_.shards,
                 opt_.redis.c_str());
    last_report_us_ = now_us();
    loop_.run(200, [this] {
        if (g_stop) loop_.stop();
        connect_peers();
        report();
    });
    for (auto& s : shards_) s->stop();
    persister_.stop();
    std::fprintf(stderr, "node %u: stopped; %llu saves written\n", id(), ull(persister_.writes.get()));
    return 0;
}

std::vector<std::string> split(const std::string& s) {
    std::vector<std::string> out;
    std::size_t start = 0;
    while (start <= s.size()) {
        const auto comma = s.find(',', start);
        out.push_back(s.substr(start, comma == std::string::npos ? std::string::npos : comma - start));
        if (comma == std::string::npos) break;
        start = comma + 1;
    }
    return out;
}

}  // namespace
}  // namespace gs

int main(int argc, char** argv) {
    gs::Options o;
    for (int i = 1; i + 1 < argc; i += 2) {
        const std::string k = argv[i], v = argv[i + 1];
        if (k == "--node") o.node = std::uint32_t(std::strtoul(v.c_str(), nullptr, 10));
        else if (k == "--nodes") o.nodes = gs::split(v);
        else if (k == "--shards") o.shards = std::uint32_t(std::strtoul(v.c_str(), nullptr, 10));
        else if (k == "--redis") o.redis = v;
        else if (k == "--flush-ms") o.flush_ms = std::atoi(v.c_str());
        else {
            std::fprintf(stderr, "usage: gs_logic --node I --nodes h:p,h:p [--shards N] [--redis h:p] [--flush-ms MS]\n");
            return 2;
        }
    }
    if (o.nodes.empty() || o.shards == 0) {
        std::fprintf(stderr, "usage: gs_logic --node I --nodes h:p,h:p [--shards N] [--redis h:p] [--flush-ms MS]\n");
        return 2;
    }
    std::signal(SIGINT, gs::on_signal);
    std::signal(SIGTERM, gs::on_signal);
    gs::Node node(o);
    return node.run();
}
