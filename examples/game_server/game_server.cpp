// A game server built on XOffset, in one process
// (docs/GAME_SERVER_ARCHITECTURE.md).
//
// The services of an SLG / MMO back end run here as objects in one program,
// so the example needs no network and runs under CTest. Every message
// between them is a std::string of bytes, exactly what a socket would carry:
//
//   client --frame--> gateway --mailbox--> logic shard --delta--> client
//                                             |   ^
//                                   bytes     v   |  bytes
//                                          storage (KV)
//
//   - Gateway: owns the client connections. It answers the schema handshake,
//     caps the frame size and routes each frame to the shard that owns the
//     session's player. It never parses a command.
//   - Logic shards: one thread each. A shard owns its entities (players with
//     their cities, alliances): each entity is its own TypedXBuffer, used by
//     that thread only, and changed in place (proxies and edit() scopes).
//     Client commands are read with XReceiver<XLink::from_client>, which the
//     default XOFFSET_CHECK_UPLINK=1 keeps at the bounds tier or above.
//     Shards talk to each other by posting jobs that carry ids and bytes,
//     never a reference into another shard's buffer.
//   - Storage: a key-value store. Saving writes the arena bytes as they are;
//     loading reloads them. Nothing is encoded per field.
//   - Clients: hold a replica of their player and their city. Login sends a
//     key delta (the whole state); every later change sends a small delta.
//     One client runs the previous release (game_v1::Player) against v2
//     servers.
//
// The flows: login, commands, a cross-shard request (joining an alliance),
// hostile uplink frames, a lost delta and its key-delta resync, a periodic
// save of dirty entities, a migration to another shard (the same as moving a
// player to another server), logout, and a GM inspecting a save.
#include "game_schema.hpp"
#include "../../xoffset_handshake.hpp"
#include "../../xoffset_receiver.hpp"

#include <condition_variable>
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace {

using namespace XOffsetDatastructure;
using game::Building;
using game::CommandKind;

// ---- Limits: every buffer and every receiver has one ------------------------

constexpr std::size_t kPlayerLimit = 256 << 10;
constexpr std::size_t kCityLimit = 64 << 10;
constexpr std::size_t kAllianceLimit = 1 << 20;
constexpr std::size_t kConfigLimit = 64 << 10;
constexpr std::size_t kCommandLimit = 4096;  // a command is small: the gateway drops larger frames
constexpr std::size_t kMaxAllianceMembers = 50;
constexpr int kShards = 4;

// Server-side buffers grow on demand below their limit, so an in-place
// change never runs out of room.
XBuffer::MaximumCapacity grow_to(std::size_t bytes) {
    return XBuffer::maximum_capacity(bytes).with_growth(XGrowPolicy::Automatic);
}

std::mutex g_log_mutex;

void log(const char* fmt, ...) {
    std::lock_guard<std::mutex> lock(g_log_mutex);
    va_list args;
    va_start(args, fmt);
    std::vprintf(fmt, args);
    va_end(args);
    std::fflush(stdout);
}

unsigned long long ull(std::uint64_t v) { return static_cast<unsigned long long>(v); }

std::string key(const char* kind, std::uint64_t id) { return std::string(kind) + ":" + std::to_string(id); }

// ---- Storage service: a KV store of raw saves ---------------------------------

class KvStore {
public:
    void put(std::string k, std::string bytes) {
        std::lock_guard<std::mutex> lock(mutex_);
        data_[std::move(k)] = std::move(bytes);
        ++writes_;
    }
    std::optional<std::string> get(const std::string& k) const {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = data_.find(k);
        if (it == data_.end()) return std::nullopt;
        return it->second;
    }
    std::size_t writes() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return writes_;
    }

private:
    mutable std::mutex mutex_;
    std::unordered_map<std::string, std::string> data_;
    std::size_t writes_ = 0;
};

// ---- Downlink: what a shard sends to one client connection -------------------

enum class Stream : std::uint8_t { player, city };

struct Downlink {
    Stream stream;
    bool key;  // a key delta: the whole state
    std::string delta;
};

// The server side of one client connection. In a real deployment the
// gateway writes these frames to the client's socket; here the client drains
// them directly.
class ClientLink {
public:
    void push(Downlink d) {
        std::lock_guard<std::mutex> lock(mutex_);
        queue_.push_back(std::move(d));
    }
    std::deque<Downlink> drain() {
        std::lock_guard<std::mutex> lock(mutex_);
        return std::exchange(queue_, {});
    }

private:
    std::mutex mutex_;
    std::deque<Downlink> queue_;
};

class Shard;
class Gateway;

// Counts the jobs posted and not yet finished, so the driver can wait until
// every shard is idle (a job that posts another one counts it before it
// finishes, so chains of jobs are covered).
class Quiescence {
public:
    void add() {
        std::lock_guard<std::mutex> lock(mutex_);
        ++pending_;
    }
    void done() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (--pending_ == 0) idle_.notify_all();
    }
    void wait() {
        std::unique_lock<std::mutex> lock(mutex_);
        idle_.wait(lock, [&] { return pending_ == 0; });
    }

private:
    std::mutex mutex_;
    std::condition_variable idle_;
    long pending_ = 0;
};

struct Cluster {
    KvStore storage;
    std::string config_wire;  // the static configuration, saved once; every shard loads its own copy
    Quiescence quiescence;
    std::vector<std::unique_ptr<Shard>> shards;
    Gateway* gateway = nullptr;

    Shard& shard(int i) { return *shards[std::size_t(i)]; }
    // An alliance lives on a fixed shard; players start on one and may move.
    static int alliance_shard(std::uint64_t alliance_id) { return int(alliance_id % kShards); }
    static int home_shard(std::uint64_t player_id) { return int(player_id % kShards); }
};

// ---- Logic shard: one thread, the only user of its entities' buffers ------------

class Shard {
public:
    Shard(int id, Cluster& cluster) : id_(id), cluster_(cluster) {}

    void start() { worker_ = std::thread([this] { run(); }); }
    void stop() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            stopping_ = true;
        }
        wake_.notify_one();
        worker_.join();
    }

    // The mailbox: any thread posts, this shard's thread runs the job. The
    // mutex hands the job's bytes over to this thread.
    void post(std::function<void(Shard&)> job) {
        cluster_.quiescence.add();
        {
            std::lock_guard<std::mutex> lock(mutex_);
            jobs_.push_back(std::move(job));
        }
        wake_.notify_one();
    }

    int id() const { return id_; }

    // ---- Jobs (each runs on this shard's thread) ----

    // The static configuration: this thread's own read-only copy of the
    // shared bytes (a buffer is used by one thread at a time; the bytes are
    // shared freely).
    void boot() {
        if (XStatus s = peer_rx_.reload(config_, cluster_.config_wire); !s) {
            log("shard %d: configuration refused: %s\n", id_, s.error().message.c_str());
            return;
        }
        config_ok_ = true;
    }

    // Account creation: build the entities in place and save them.
    void create_player(std::uint64_t player_id, const char* name, std::uint64_t city_id, std::int32_t x, std::int32_t y) {
        OwnedPlayer e = make_owned();
        {
            auto p = e.player.root();
            p.player_id() = player_id;
            p.city_id() = city_id;
            p.level() = 1;
            p.gold() = 300;
            p.food() = 500;
            p.name() = std::string(name);
        }
        e.player.edit(
            [](game::Player& p) {
                for (std::uint32_t hero = 1; hero <= 3; ++hero) {
                    game::Hero& h = p.heroes.emplace_back();
                    h.id = hero;
                    h.level = 1;
                    h.star = 1;
                }
                p.items.emplace(100u, std::int64_t{5});  // starter items
            },
            256);
        {
            auto c = e.city.root();
            c.city_id() = city_id;
            c.owner_id() = player_id;
            c.x() = x;
            c.y() = y;
            c.level() = 1;
        }
        e.city.edit([](game::City& c) {
            c.buildings.emplace(std::uint32_t(Building::town_hall), 1);
            c.buildings.emplace(std::uint32_t(Building::farm), 1);
        }, 64);
        save(e);
        log("shard %d: created player %llu \"%s\" with city %llu\n", id_, ull(player_id), name, ull(city_id));
    }

    void create_alliance(std::uint64_t alliance_id, std::uint64_t leader_id, const char* name) {
        auto a = TypedXBuffer<game::Alliance>::create(1024, grow_to(kAllianceLimit));
        {
            auto r = a.root();
            r.alliance_id() = alliance_id;
            r.leader_id() = leader_id;
            r.name() = std::string(name);
            r.notice() = std::string("Welcome!");
        }
        a.edit([&](game::Alliance& al) { al.members.push_back(leader_id); }, 64);
        a.save_verified_into(out_);
        cluster_.storage.put(key("alliance", alliance_id), out_);
        alliances_.insert_or_assign(alliance_id, std::move(a));
        log("shard %d: alliance %llu \"%s\" led by %llu\n", id_, ull(alliance_id), name, ull(leader_id));
    }

    // Login: the save is the buffer. Reload it from storage (our own data:
    // the trusted tier), then send the client a key delta of each entity.
    void login(std::uint64_t player_id, ClientLink* link) {
        auto player_wire = cluster_.storage.get(key("player", player_id));
        if (!player_wire) {
            log("shard %d: no save for player %llu\n", id_, ull(player_id));
            return;
        }
        OwnedPlayer e = make_owned();
        if (XStatus s = peer_rx_.reload(e.player, *player_wire); !s) {
            log("shard %d: save of %llu refused: %s\n", id_, ull(player_id), s.error().message.c_str());
            return;
        }
        const std::uint64_t city_id = e.player.root().city_id();
        auto city_wire = cluster_.storage.get(key("city", city_id));
        if (!city_wire || !peer_rx_.reload(e.city, *city_wire)) {
            log("shard %d: city %llu of %llu missing\n", id_, ull(city_id), ull(player_id));
            return;
        }
        e.link = link;
        OwnedPlayer& slot = players_.insert_or_assign(player_id, std::move(e)).first->second;
        log("shard %d: player %llu logged in (save %zu + %zu bytes)\n", id_, ull(player_id), player_wire->size(),
            city_wire->size());
        sync(slot);
    }

    void logout(std::uint64_t player_id) {
        auto it = players_.find(player_id);
        if (it == players_.end()) return;
        save(it->second);
        players_.erase(it);
        log("shard %d: player %llu logged out, saved\n", id_, ull(player_id));
    }

    // A client command, forwarded by the gateway as the client sent it. The
    // bytes are still the client's: read them with the from_client receiver.
    void on_command(std::uint64_t session_player, const std::string& wire);

    // Joining an alliance, step 2, on the alliance's shard.
    void add_member(std::uint64_t alliance_id, std::uint64_t player_id);
    // Step 3, back on the player's current shard.
    void joined(std::uint64_t player_id, std::uint64_t alliance_id) {
        OwnedPlayer* e = find(player_id);
        if (!e) return;
        if (alliance_id == 0) {
            log("shard %d: player %llu was not admitted\n", id_, ull(player_id));
            return;
        }
        e->player.root().alliance_id() = alliance_id;
        e->dirty = true;
        log("shard %d: player %llu joined alliance %llu\n", id_, ull(player_id), ull(alliance_id));
        sync(*e);
    }

    // An operator tool: a field the v1 client does not know about.
    void gm_set_vip(std::uint64_t player_id, std::int32_t vip) {
        OwnedPlayer* e = find(player_id);
        if (!e) return;
        e->player.root().vip_level() = vip;
        e->dirty = true;
        log("shard %d: GM set vip_level %d on %llu\n", id_, vip, ull(player_id));
        sync(*e);
    }

    // The client rejected a delta (one was lost, or it restarted): start its
    // streams over with key deltas.
    void resync(std::uint64_t player_id) {
        OwnedPlayer* e = find(player_id);
        if (!e) return;
        e->player_base.reset();
        e->city_base.reset();
        log("shard %d: resync of %llu requested\n", id_, ull(player_id));
        sync(*e);
    }

    // The periodic save: every dirty entity, as its arena bytes. Between
    // ticks, shed what long editing left behind once it pays.
    void save_dirty() {
        std::size_t saved = 0;
        for (auto& [player_id, e] : players_) {
            if (!e.dirty) continue;
            save(e);
            ++saved;
        }
        for (std::uint64_t alliance_id : dirty_alliances_) {
            TypedXBuffer<game::Alliance>& a = alliances_.at(alliance_id);
            if (a.relayout_advised()) (void)a.relayout();
            a.save_verified_into(out_);
            cluster_.storage.put(key("alliance", alliance_id), out_);
            ++saved;
        }
        dirty_alliances_.clear();
        if (saved) log("shard %d: saved %zu dirty entities\n", id_, saved);
    }

    // Migration out: the entities leave as their bytes; nothing is encoded.
    // The same call moves a player to another shard of this server or to a
    // shard of another server (where the post becomes a socket write on a
    // server link, read with XReceiver<XLink::server_peer>).
    void migrate_out(std::uint64_t player_id, int to);

    void migrate_in(std::uint64_t player_id, const std::string& player_wire, const std::string& city_wire,
                    ClientLink* link, bool dirty) {
        OwnedPlayer e = make_owned();
        if (!peer_rx_.reload(e.player, player_wire) || !peer_rx_.reload(e.city, city_wire)) {
            log("shard %d: migration of %llu refused\n", id_, ull(player_id));
            return;
        }
        e.link = link;
        e.dirty = dirty;
        OwnedPlayer& slot = players_.insert_or_assign(player_id, std::move(e)).first->second;
        log("shard %d: player %llu arrived (%zu + %zu bytes)\n", id_, ull(player_id), player_wire.size(),
            city_wire.size());
        sync(slot);  // new baselines: the client gets key deltas from its new owner
    }

private:
    struct OwnedPlayer {
        TypedXBuffer<game::Player> player;
        TypedXBuffer<game::City> city;
        XDeltaBaseline player_base;  // what the client holds of each entity
        XDeltaBaseline city_base;
        ClientLink* link = nullptr;  // null: offline
        bool dirty = false;
    };

    static OwnedPlayer make_owned() {
        return OwnedPlayer{TypedXBuffer<game::Player>::create(1024, grow_to(kPlayerLimit)),
                           TypedXBuffer<game::City>::create(512, grow_to(kCityLimit)), {}, {}, nullptr, false};
    }

    OwnedPlayer* find(std::uint64_t player_id) {
        auto it = players_.find(player_id);
        return it == players_.end() ? nullptr : &it->second;
    }

    void save(OwnedPlayer& e) {
        if (e.player.relayout_advised()) (void)e.player.relayout();
        const std::uint64_t player_id = e.player.root().player_id();
        e.player.save_verified_into(out_);
        cluster_.storage.put(key("player", player_id), out_);
        e.city.save_verified_into(out_);
        cluster_.storage.put(key("city", e.city.root().city_id()), out_);
        e.dirty = false;
    }

    // Send the client what changed since its baseline: nothing, a small
    // delta, or (for a new baseline) a key delta with the whole state.
    template <typename T>
    void push(TypedXBuffer<T>& buffer, XDeltaBaseline& base, Stream stream, ClientLink& link) {
        const XDeltaStats st = buffer.save_delta_into(out_, base);
        if (st.empty) return;
        link.push(Downlink{stream, st.key, out_});
    }

    void sync(OwnedPlayer& e) {
        if (!e.link) return;
        push(e.player, e.player_base, Stream::player, *e.link);
        push(e.city, e.city_base, Stream::city, *e.link);
    }

    // ---- Game rules: the content of a command is checked here ----

    void collect(OwnedPlayer& e) {
        const std::int32_t farm = building_level(e, Building::farm);
        auto p = e.player.root();
        p.gold() += 100;
        p.food() += 50 * farm;
        e.dirty = true;
    }

    std::int32_t building_level(OwnedPlayer& e, Building b) {
        return e.city.read([&](const game::City& c) {
            auto it = c.buildings.find(std::uint32_t(b));
            return it == c.buildings.end() ? 0 : it->second;
        });
    }

    bool upgrade(OwnedPlayer& e, std::uint32_t building) {
        std::int32_t max_level = 0;
        std::int64_t per_level = 0;
        config_.read([&](const game::ConfigTable& t) {
            for (const game::BuildingConfig& b : t.buildings) {
                if (b.building == building) {
                    max_level = b.max_level;
                    per_level = b.gold_per_level;
                }
            }
        });
        if (max_level == 0) return refuse(e, "unknown building");
        const std::int32_t level = building_level(e, Building(building));
        if (level >= max_level) return refuse(e, "building at max level");
        const std::int64_t cost = std::int64_t(level + 1) * per_level;
        const std::int64_t gold = e.player.read([](const game::Player& p) { return p.gold; });
        if (gold < cost) return refuse(e, "not enough gold");
        e.player.root().gold() -= cost;
        e.city.edit(
            [&](game::City& c) {
                c.buildings[building] = level + 1;
                if (Building(building) == Building::town_hall) c.level = level + 1;
            },
            64);
        e.dirty = true;
        return true;
    }

    bool rename(OwnedPlayer& e, std::string_view name) {
        if (name.empty() || name.size() > 16) return refuse(e, "name length");
        for (char ch : name)
            if (ch < 0x20 || ch > 0x7E) return refuse(e, "name characters");
        e.player.edit([&](game::Player& p) { p.name.assign(name); }, 64);
        e.dirty = true;
        return true;
    }

    bool request_join(OwnedPlayer& e, std::uint64_t alliance_id) {
        if (e.player.root().alliance_id() != 0) return refuse(e, "already in an alliance");
        if (alliance_id == 0) return refuse(e, "no alliance");
        const std::uint64_t player_id = e.player.root().player_id();
        // Another shard owns the alliance: send it ids, never a reference.
        cluster_.shard(Cluster::alliance_shard(alliance_id)).post([alliance_id, player_id](Shard& s) {
            s.add_member(alliance_id, player_id);
        });
        return true;
    }

    bool refuse(OwnedPlayer& e, const char* why) {
        log("shard %d: command of %llu refused by game rules: %s\n", id_, ull(e.player.root().player_id()), why);
        return false;
    }

    void run() {
        for (;;) {
            std::function<void(Shard&)> job;
            {
                std::unique_lock<std::mutex> lock(mutex_);
                wake_.wait(lock, [&] { return stopping_ || !jobs_.empty(); });
                if (jobs_.empty()) return;  // stopping, and nothing left
                job = std::move(jobs_.front());
                jobs_.pop_front();
            }
            job(*this);
            cluster_.quiescence.done();
        }
    }

    int id_;
    Cluster& cluster_;
    std::thread worker_;
    std::mutex mutex_;
    std::condition_variable wake_;
    std::deque<std::function<void(Shard&)>> jobs_;
    bool stopping_ = false;

    // Everything below is used by this shard's thread only.
    std::unordered_map<std::uint64_t, OwnedPlayer> players_;
    std::unordered_map<std::uint64_t, TypedXBuffer<game::Alliance>> alliances_;
    std::unordered_set<std::uint64_t> dirty_alliances_;
    TypedXBuffer<game::ConfigTable> config_ = TypedXBuffer<game::ConfigTable>::create(512, grow_to(kConfigLimit));
    bool config_ok_ = false;
    XReceiver<XLink::server_peer> peer_rx_{grow_to(kAllianceLimit)};           // storage, other shards
    XReceiver<XLink::from_client> client_rx_{XBuffer::maximum_capacity(kCommandLimit)};  // client commands
    std::string out_;  // reused for every save and delta
};

// ---- Gateway: connections, handshake, frame cap, routing ------------------------

class Gateway {
public:
    explicit Gateway(Cluster& cluster) : cluster_(cluster) {}

    // The first frame of every connection is the schema hello. A client of
    // another generation is refused (it shows its update prompt); a client
    // of an earlier version of an add-only family is accepted.
    bool accept(const std::string& hello, std::string& ack) {
        const XSchemaHelloResult peer =
            accept_schema_hello<game::Player, game::City, game::Command>(hello, ack, "gateway-2.0");
        log("gateway: hello from %s: %s\n", peer.label.c_str(), peer.accepted() ? "accepted" : "refused");
        return peer.accepted();
    }

    // The session's player id comes from authentication, not from the
    // client's frames.
    void login(std::uint64_t player_id, ClientLink* link) {
        int shard;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            shard = route_.try_emplace(player_id, Cluster::home_shard(player_id)).first->second;
            online_.insert(player_id);
        }
        cluster_.shard(shard).post([player_id, link](Shard& s) { s.login(player_id, link); });
    }

    void logout(std::uint64_t player_id) {
        const int shard = route(player_id);
        {
            std::lock_guard<std::mutex> lock(mutex_);
            online_.erase(player_id);
        }
        if (shard >= 0) cluster_.shard(shard).post([player_id](Shard& s) { s.logout(player_id); });
    }

    // An uplink frame: cap it, route it, never parse it.
    void uplink(std::uint64_t session_player, std::string frame) {
        if (frame.size() > kCommandLimit) {
            log("gateway: %zu-byte frame from %llu dropped (limit %zu)\n", frame.size(), ull(session_player),
                kCommandLimit);
            return;
        }
        to_player(session_player,
                  [session_player, f = std::move(frame)](Shard& s) { s.on_command(session_player, f); });
    }

    void resync(std::uint64_t player_id) {
        to_player(player_id, [player_id](Shard& s) { s.resync(player_id); });
    }

    // Post a job to the shard that owns the player now; dropped when the
    // player is offline.
    void to_player(std::uint64_t player_id, std::function<void(Shard&)> job) {
        const int shard = route(player_id);
        if (shard >= 0) cluster_.shard(shard).post(std::move(job));
    }

    int route(std::uint64_t player_id) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!online_.count(player_id)) return -1;
        auto it = route_.find(player_id);
        return it == route_.end() ? -1 : it->second;
    }

    void set_route(std::uint64_t player_id, int shard) {
        std::lock_guard<std::mutex> lock(mutex_);
        route_[player_id] = shard;
    }

private:
    Cluster& cluster_;
    std::mutex mutex_;
    std::unordered_map<std::uint64_t, int> route_;  // player -> owning shard
    std::unordered_set<std::uint64_t> online_;
};

void Shard::on_command(std::uint64_t session_player, const std::string& wire) {
    OwnedPlayer* e = find(session_player);
    if (!e) {  // the player moved away after the gateway routed this frame
        cluster_.gateway->uplink(session_player, wire);
        return;
    }
    // Bounds tier: no read leaves the frame; the values can still be
    // anything, so the game rules check them against the server's state.
    auto v = client_rx_.view<game::Command>(wire);
    if (!v) {
        log("shard %d: frame from %llu rejected by the bounds check (%s)\n", id_, ull(session_player),
            to_string(v.code()));
        return;
    }
    const game::Command& c = v->root();  // const: the client's bytes are never changed or kept
    if (c.player_id != session_player) {
        log("shard %d: command names player %llu on the session of %llu: refused\n", id_, ull(c.player_id),
            ull(session_player));
        return;
    }
    bool changed = false;
    switch (CommandKind(c.kind)) {
        case CommandKind::collect:
            collect(*e);
            changed = true;
            break;
        case CommandKind::upgrade_building: changed = upgrade(*e, c.target); break;
        case CommandKind::rename: changed = rename(*e, c.text.view()); break;
        case CommandKind::join_alliance: request_join(*e, std::uint64_t(c.arg)); break;
        default: refuse(*e, "unknown command kind"); break;
    }
    if (changed) sync(*e);
}

void Shard::add_member(std::uint64_t alliance_id, std::uint64_t player_id) {
    auto it = alliances_.find(alliance_id);
    bool admitted = false;
    if (it != alliances_.end()) {
        TypedXBuffer<game::Alliance>& alliance = it->second;
        admitted = alliance.read([&](const game::Alliance& a) {
            if (a.members.size() >= kMaxAllianceMembers) return false;
            for (std::uint64_t m : a.members)
                if (m == player_id) return false;
            return true;
        });
        if (admitted) {
            alliance.edit([&](game::Alliance& a) { a.members.push_back(player_id); }, 64);
            dirty_alliances_.insert(alliance_id);
        }
    }
    log("shard %d: alliance %llu %s player %llu\n", id_, ull(alliance_id), admitted ? "admits" : "refuses",
        ull(player_id));
    const std::uint64_t result = admitted ? alliance_id : 0;
    cluster_.gateway->to_player(player_id, [player_id, result](Shard& s) { s.joined(player_id, result); });
}

void Shard::migrate_out(std::uint64_t player_id, int to) {
    auto it = players_.find(player_id);
    if (it == players_.end()) return;
    OwnedPlayer& e = it->second;
    std::string player_wire, city_wire;
    e.player.save_verified_into(player_wire);
    e.city.save_verified_into(city_wire);
    ClientLink* link = e.link;
    const bool dirty = e.dirty;
    players_.erase(it);
    cluster_.gateway->set_route(player_id, to);  // new frames go to the new owner
    log("shard %d: player %llu leaves for shard %d\n", id_, ull(player_id), to);
    cluster_.shard(to).post([player_id, pw = std::move(player_wire), cw = std::move(city_wire), link, dirty](Shard& s) {
        s.migrate_in(player_id, pw, cw, link, dirty);
    });
}

// ---- Client: a replica of its player and city, and a command buffer ------------

template <typename PlayerT>
class GameClient {
public:
    GameClient(std::string label, std::uint64_t player_id) : label_(std::move(label)), player_id_(player_id) {}

    bool connect(Gateway& gateway) {
        std::string ack;
        gateway.accept(make_schema_hello<PlayerT, game::City, game::Command>(label_), ack);
        if (XSchemaAckResult r = read_schema_ack<PlayerT, game::City, game::Command>(ack); !r) {
            log("%s: update needed: %s\n", label_.c_str(), r.message.c_str());
            return false;
        }
        return true;
    }

    ClientLink& link() { return link_; }

    // One command buffer reused for every frame; a dense wire for the
    // bounds check.
    const std::string& command(CommandKind kind, std::uint32_t target = 0, std::int64_t arg = 0,
                               std::string_view text = {}, std::uint64_t as_player = 0) {
        auto c = out_.reset();
        c.player_id() = as_player ? as_player : player_id_;
        c.kind() = std::uint32_t(kind);
        c.target() = target;
        c.arg() = arg;
        if (!text.empty()) c.text() = std::string(text);
        if (XStatus s = out_.try_save_verified_into(wire_, XSaveOptions{.emission = XWireEmission::dense}); !s)
            wire_.clear();
        return wire_;
    }

    // Apply what the server sent. A delta that does not follow the replica
    // (one was lost) is rejected; ask for key deltas.
    bool pump(Gateway& gateway) {
        for (Downlink& d : link_.drain()) {
            const bool is_player = d.stream == Stream::player;
            const XStatus s = is_player ? rx_.apply_delta(player_, d.delta, player_cursor_)
                                        : rx_.apply_delta(city_, d.delta, city_cursor_);
            if (!s) {
                log("%s: %s delta rejected (%s); asking for a resync\n", label_.c_str(), is_player ? "player" : "city",
                    to_string(s.code()));
                gateway.resync(player_id_);
                return false;
            }
            log("%s: %s %s delta, %zu bytes\n", label_.c_str(), d.key ? "key" : "small", is_player ? "player" : "city",
                d.delta.size());
        }
        return true;
    }

    void show() {
        player_.read([&](const PlayerT& p) {
            int vip = -1;
            if constexpr (requires { p.vip_level; }) vip = p.vip_level;
            std::string vip_text = vip < 0 ? "n/a in v1" : std::to_string(vip);
            log("%s: \"%.*s\" lv %d gold %lld food %lld alliance %llu heroes %zu vip %s\n", label_.c_str(),
                int(p.name.view().size()), p.name.view().data(), p.level, static_cast<long long>(p.gold),
                static_cast<long long>(p.food), ull(p.alliance_id), std::size_t(p.heroes.size()), vip_text.c_str());
        });
        city_.read([&](const game::City& c) {
            std::string b;
            for (const auto& [building, level] : c.buildings)
                b += " " + std::to_string(building) + ":" + std::to_string(level);
            log("%s: city %llu at (%d, %d) level %d, buildings%s\n", label_.c_str(), ull(c.city_id), c.x, c.y, c.level,
                b.c_str());
        });
    }

    template <typename F>
    auto read_player(F f) { return player_.read(f); }
    template <typename F>
    auto read_city(F f) { return city_.read(f); }

private:
    std::string label_;
    std::uint64_t player_id_;
    ClientLink link_;
    XReceiver<XLink::from_server> rx_{XBuffer::maximum_capacity(kPlayerLimit)};
    TypedXBuffer<PlayerT> player_ = TypedXBuffer<PlayerT>::create(1024, XBuffer::maximum_capacity(kPlayerLimit));
    TypedXBuffer<game::City> city_ = TypedXBuffer<game::City>::create(512, XBuffer::maximum_capacity(kCityLimit));
    XDeltaCursor player_cursor_;
    XDeltaCursor city_cursor_;
    TypedXBuffer<game::Command> out_ = TypedXBuffer<game::Command>::create(256, XBuffer::maximum_capacity(kCommandLimit));
    std::string wire_;
};

// ---- Offline tools: the configuration table and a save of the last release -----

std::string build_config() {
    auto t = TypedXBuffer<game::ConfigTable>::create(512, grow_to(kConfigLimit));
    t.root().version() = 7;
    t.edit(
        [](game::ConfigTable& c) {
            auto add = [&](Building b, std::int32_t max_level, std::int64_t gold) {
                game::BuildingConfig& e = c.buildings.emplace_back();
                e.building = std::uint32_t(b);
                e.max_level = max_level;
                e.gold_per_level = gold;
            };
            add(Building::town_hall, 25, 200);
            add(Building::farm, 20, 100);
            add(Building::barracks, 20, 150);
        },
        256);
    return t.save_compact();
}

// What the previous release (game_v1) wrote for a player who has not logged
// in since: v2 servers load it as it is, vip_level reads 0.
void write_v1_save(KvStore& kv, std::uint64_t player_id, std::uint64_t city_id) {
    auto p = TypedXBuffer<game_v1::Player>::create(1024, grow_to(kPlayerLimit));
    {
        auto r = p.root();
        r.player_id() = player_id;
        r.city_id() = city_id;
        r.level() = 9;
        r.gold() = 1200;
        r.food() = 800;
        r.name() = std::string("Carol");
    }
    p.edit([](game_v1::Player& v) { v.heroes.emplace_back().id = 1; }, 64);
    kv.put(key("player", player_id), p.save_verified());

    auto c = TypedXBuffer<game::City>::create(512, grow_to(kCityLimit));
    {
        auto r = c.root();
        r.city_id() = city_id;
        r.owner_id() = player_id;
        r.x() = 40;
        r.y() = 12;
        r.level() = 3;
    }
    c.edit([](game::City& v) {
        v.buildings.emplace(std::uint32_t(Building::town_hall), 3);
        v.buildings.emplace(std::uint32_t(Building::farm), 4);
    }, 64);
    kv.put(key("city", city_id), c.save_verified());
}

// Servers check each other's schema too, on every server link.
bool check_server_links() {
    std::string ack;
    const XSchemaHelloResult peer = accept_schema_hello<game::Player, game::City, game::Alliance>(
        make_schema_hello<game::Player, game::City, game::Alliance>("shard-2.0", XSchemaRole::server), ack, "shard-2.0");
    return peer.accepted() && read_schema_ack<game::Player, game::City, game::Alliance>(ack);
}

}  // namespace

int main() {
    int failures = 0;
    auto expect = [&](bool ok, const char* what) {
        if (!ok) {
            log("CHECK FAILED: %s\n", what);
            ++failures;
        }
    };

    Cluster cluster;
    cluster.config_wire = build_config();
    Gateway gateway(cluster);
    cluster.gateway = &gateway;
    for (int i = 0; i < kShards; ++i) cluster.shards.push_back(std::make_unique<Shard>(i, cluster));
    expect(check_server_links(), "server link handshake");
    for (auto& s : cluster.shards) s->start();
    for (auto& s : cluster.shards) s->post([](Shard& sh) { sh.boot(); });

    constexpr std::uint64_t alice = 1001, bob = 1002, carol = 1003, wolves = 9002;
    log("\n== accounts ==\n");
    cluster.shard(Cluster::home_shard(alice)).post([](Shard& s) { s.create_player(alice, "Alice", 5001, 10, 20); });
    cluster.shard(Cluster::home_shard(bob)).post([](Shard& s) { s.create_player(bob, "Bob", 5002, 30, 5); });
    cluster.shard(Cluster::alliance_shard(wolves)).post([](Shard& s) { s.create_alliance(wolves, bob, "Iron Wolves"); });
    write_v1_save(cluster.storage, carol, 5003);
    cluster.quiescence.wait();

    log("\n== connect and log in ==\n");
    GameClient<game::Player> a("client-2.0 (Alice)", alice);
    GameClient<game_v1::Player> c("client-1.0 (Carol)", carol);  // has not updated yet
    expect(a.connect(gateway), "v2 client handshake");
    expect(c.connect(gateway), "v1 client handshake");
    gateway.login(alice, &a.link());
    cluster.quiescence.wait();
    gateway.login(carol, &c.link());
    cluster.quiescence.wait();
    expect(a.pump(gateway) && c.pump(gateway), "login key deltas");
    a.show();
    c.show();

    log("\n== commands: in-place changes, small deltas ==\n");
    gateway.uplink(alice, a.command(CommandKind::collect));
    gateway.uplink(alice, a.command(CommandKind::upgrade_building, std::uint32_t(Building::farm)));
    gateway.uplink(alice, a.command(CommandKind::rename, 0, 0, "Alice the Bold"));
    cluster.quiescence.wait();
    expect(a.pump(gateway), "command deltas");
    a.show();

    log("\n== cross-shard request: join an alliance by id ==\n");
    gateway.uplink(alice, a.command(CommandKind::join_alliance, 0, std::int64_t(wolves)));
    cluster.quiescence.wait();
    expect(a.pump(gateway), "alliance delta");
    expect(a.read_player([](const game::Player& p) { return p.alliance_id; }) == wolves, "Alice in the alliance");

    log("\n== hostile uplink frames ==\n");
    {
        std::string cut = a.command(CommandKind::collect);
        cut.resize(cut.size() - 8);
        gateway.uplink(alice, cut);  // truncated: the bounds check
        gateway.uplink(alice, a.command(CommandKind::collect, 0, 0, {}, bob));  // another player's id
        gateway.uplink(alice, a.command(CommandKind(77)));                     // a kind that does not exist
        gateway.uplink(alice, a.command(CommandKind::upgrade_building, std::uint32_t(Building::town_hall)));  // too poor
        gateway.uplink(alice, std::string(kCommandLimit + 1, '\0'));            // oversized: the gateway drops it
        cluster.quiescence.wait();
        expect(a.pump(gateway), "nothing sent for refused commands");
    }

    log("\n== an older client against the new server ==\n");
    gateway.uplink(carol, c.command(CommandKind::collect));
    gateway.to_player(carol, [](Shard& s) { s.gm_set_vip(carol, 3); });
    cluster.quiescence.wait();
    expect(c.pump(gateway), "v1 client applies v2 deltas");
    c.show();

    log("\n== a lost delta ==\n");
    gateway.uplink(alice, a.command(CommandKind::collect));
    cluster.quiescence.wait();
    a.link().drain();  // dropped on the way
    log("(the delta for that change was lost)\n");
    gateway.uplink(alice, a.command(CommandKind::collect));
    cluster.quiescence.wait();
    expect(!a.pump(gateway), "out-of-order delta rejected");
    cluster.quiescence.wait();
    expect(a.pump(gateway), "key deltas after the resync");

    log("\n== periodic save ==\n");
    const std::size_t writes_before = cluster.storage.writes();
    for (auto& s : cluster.shards) s->post([](Shard& sh) { sh.save_dirty(); });
    cluster.quiescence.wait();
    log("storage: %zu writes\n", cluster.storage.writes() - writes_before);

    log("\n== migration: Alice moves to another shard ==\n");
    const int from = gateway.route(alice), to = (from + 1) % kShards;
    cluster.shard(from).post([to](Shard& s) { s.migrate_out(alice, to); });
    cluster.quiescence.wait();
    expect(gateway.route(alice) == to, "route follows the player");
    expect(a.pump(gateway), "key deltas from the new owner");
    gateway.uplink(alice, a.command(CommandKind::collect));
    cluster.quiescence.wait();
    expect(a.pump(gateway), "commands reach the new owner");
    a.show();

    log("\n== logout ==\n");
    const std::int64_t alice_gold = a.read_player([](const game::Player& p) { return p.gold; });
    gateway.logout(alice);
    gateway.logout(carol);
    cluster.quiescence.wait();

    log("\n== GM: inspect saves straight from storage ==\n");
    {
        XReceiver<XLink::server_peer> rx(grow_to(kPlayerLimit));
        auto alice_save = rx.load_typed<game::Player>(*cluster.storage.get(key("player", alice)));
        auto carol_save = rx.load_typed<game::Player>(*cluster.storage.get(key("player", carol)));
        // A tool of the previous release still reads the new saves.
        auto carol_old_tool = rx.load_typed<game_v1::Player>(*cluster.storage.get(key("player", carol)));
        auto wolves_save = rx.load_typed<game::Alliance>(*cluster.storage.get(key("alliance", wolves)));
        expect(alice_save && carol_save && carol_old_tool && wolves_save, "saves load");
        if (alice_save && carol_save && carol_old_tool && wolves_save) {
            alice_save->read([&](const game::Player& p) {
                log("gm: player %llu \"%.*s\" gold %lld alliance %llu\n", ull(p.player_id), int(p.name.view().size()),
                    p.name.view().data(), static_cast<long long>(p.gold), ull(p.alliance_id));
                expect(p.gold == alice_gold && p.alliance_id == wolves && p.name.view() == "Alice the Bold",
                       "Alice's save matches her client");
            });
            carol_save->read([&](const game::Player& p) {
                log("gm: player %llu vip_level %d (written by v2, kept in v1's spare bytes)\n", ull(p.player_id),
                    p.vip_level);
                expect(p.vip_level == 3 && p.gold == 1300, "Carol's save");
            });
            carol_old_tool->read([&](const game_v1::Player& p) { expect(p.gold == 1300, "v1 tool reads a v2 save"); });
            wolves_save->read([&](const game::Alliance& al) {
                log("gm: alliance %llu \"%.*s\" has %zu members\n", ull(al.alliance_id), int(al.name.view().size()),
                    al.name.view().data(), std::size_t(al.members.size()));
                expect(al.members.size() == 2, "alliance members saved");
            });
        }
    }

    for (auto& s : cluster.shards) s->stop();
    log("\n%s\n", failures == 0 ? "OK" : "FAILED");
    return failures == 0 ? 0 : 1;
}
