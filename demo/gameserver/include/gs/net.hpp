// Game server demo: TCP framing and a poll() event loop (POSIX: Linux, macOS).
//
// Every frame on every link is
//     u32 length (little-endian, of what follows)  u8 type  body
// The receiver checks the length against the connection's limit before it
// buffers the body, so a peer cannot make it allocate more than the limit.
//
// Linux waits with epoll, so a wakeup costs the ready sockets, not all of
// them; other systems use poll().
//
// One Loop runs on one thread. Other threads hand it work with post(); the
// loop wakes through a pipe and runs the posted calls in order.
#ifndef GS_NET_HPP
#define GS_NET_HPP

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace gs {

using Clock = std::chrono::steady_clock;

inline std::uint64_t now_us() {
    return std::uint64_t(std::chrono::duration_cast<std::chrono::microseconds>(Clock::now().time_since_epoch()).count());
}

// ---- Little-endian helpers -------------------------------------------------------

inline void put_u8(std::string& s, std::uint8_t v) { s.push_back(char(v)); }
inline void put_u32(std::string& s, std::uint32_t v) {
    for (int i = 0; i < 4; ++i) s.push_back(char((v >> (8 * i)) & 0xFF));
}
inline void put_u64(std::string& s, std::uint64_t v) {
    for (int i = 0; i < 8; ++i) s.push_back(char((v >> (8 * i)) & 0xFF));
}

// Reads fixed-width fields from a body; every read is bounds-checked and a
// short body sets ok = false instead of reading past the end.
struct Reader {
    std::string_view s;
    std::size_t pos = 0;
    bool ok = true;

    std::uint64_t uint(int bytes) {
        if (s.size() - pos < std::size_t(bytes)) {
            ok = false;
            pos = s.size();
            return 0;
        }
        std::uint64_t v = 0;
        for (int i = 0; i < bytes; ++i) v |= std::uint64_t(static_cast<unsigned char>(s[pos + std::size_t(i)])) << (8 * i);
        pos += std::size_t(bytes);
        return v;
    }
    std::uint8_t u8() { return std::uint8_t(uint(1)); }
    std::uint32_t u32() { return std::uint32_t(uint(4)); }
    std::uint64_t u64() { return uint(8); }
    std::string_view rest() {
        std::string_view r = s.substr(pos);
        pos = s.size();
        return r;
    }
};

// ---- Sockets -------------------------------------------------------------------

int listen_tcp(std::uint16_t port);                                     // -1 on failure
int connect_tcp(const std::string& host, std::uint16_t port);          // blocking connect, then non-blocking; -1 on failure
bool parse_host_port(const std::string& text, std::string& host, std::uint16_t& port);

// ---- Connections and the loop -----------------------------------------------------

struct Conn {
    std::uint64_t id = 0;
    int fd = -1;
    std::size_t max_frame = 1 << 20;  // the largest frame this peer may send
    std::string in;
    std::string out;
    std::size_t out_pos = 0;
    bool closing = false;  // close once `out` is written
    bool dead = false;
    bool queued = false;      // on the loop's list of connections with output to write
    bool want_write = false;  // registered for writability (epoll)
    std::uint64_t last_recv_us = 0;
    std::uint64_t bytes_in = 0;
    std::uint64_t bytes_out = 0;
    void* user = nullptr;  // owner's per-connection state
};

class Loop {
public:
    using OnAccept = std::function<void(Conn&)>;
    using OnFrame = std::function<void(Conn&, std::uint8_t type, std::string_view body)>;
    using OnClose = std::function<void(Conn&)>;

    Loop();
    ~Loop();
    Loop(const Loop&) = delete;
    Loop& operator=(const Loop&) = delete;

    void listen(int fd, std::size_t max_frame, OnAccept on_accept);
    Conn& adopt(int fd, std::size_t max_frame);  // an outgoing connection
    void on_frame(OnFrame f) { on_frame_ = std::move(f); }
    void on_close(OnClose f) { on_close_ = std::move(f); }

    // Queue a frame; written when the socket is writable.
    void send(Conn& c, std::uint8_t type, std::string_view body);
    void send(Conn& c, std::uint8_t type, std::string_view head, std::string_view body);
    void close(Conn& c);           // now
    void close_after_send(Conn& c);
    Conn* find(std::uint64_t id);

    // Any thread: run f on the loop thread.
    void post(std::function<void()> f);

    // Run until stop(); `tick` runs at least every tick_ms.
    void run(int tick_ms, const std::function<void()>& tick);
    void stop() { stopping_ = true; }

    std::size_t connections() const { return conns_.size(); }

private:
    void read_ready(Conn& c);
    void write_ready(Conn& c);
    void queue(Conn& c);
    void flush_queued();
    void set_write_interest(Conn& c, bool on);
    void accept_ready(std::size_t listener);
    void reap();
    void drain_posted();

    struct Listener {
        int fd;
        std::size_t max_frame;
        OnAccept on_accept;
    };
    std::vector<Listener> listeners_;
    std::unordered_map<std::uint64_t, std::unique_ptr<Conn>> conns_;
    std::uint64_t next_id_ = 1;
    OnFrame on_frame_;
    OnClose on_close_;
    int wake_[2] = {-1, -1};
    std::mutex posted_mutex_;
    std::vector<std::function<void()>> posted_;
    bool stopping_ = false;
    int ep_ = -1;                       // epoll on Linux; poll() elsewhere
    std::vector<std::uint64_t> queued_;  // connections with output (or a pending close)
    std::vector<std::uint64_t> dead_;
};

}  // namespace gs

#endif
