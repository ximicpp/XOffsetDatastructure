// Device mesh demo: TCP framing between a device and the hub.
// OWNER: network thread (demo/mesh/INTERFACES.md section 1). Bodies live in
// src/net.cpp, adapted from demo/mapstate's net.hpp / net.cpp.
//
// Frame = 32-byte protocol::FrameHeader (little endian) + payload. The
// receive path checks the magic, the zero fields, the allowed types and
// protocol::max_payload(type) before it sizes the payload buffer.
//
// Sockets: POSIX (Linux, macOS, Android, iOS) and Winsock 2 (Windows: MinGW,
// MSVC, clang-cl). Every call returns a Result or a Status and never throws,
// so the device program builds without exceptions (xo_peer_noexc); the hub
// uses the same calls.
#pragma once
#include <cstddef>
#include <cstdint>
#include <deque>
#include <initializer_list>
#include <optional>
#include <string>
#include <string_view>
#include "xmesh/protocol.hpp"
#include "xmesh/result.hpp"

namespace xmesh::net {

using protocol::MsgType;

// Socket handle: a file descriptor on POSIX, a SOCKET on Windows (both fit).
using Socket = std::intptr_t;
inline constexpr Socket kInvalidSocket = -1;

struct Frame {
    MsgType type{};
    std::uint32_t from = 0;
    std::uint32_t to = 0;
    std::uint64_t correlation = 0;
    std::string payload;
};

class Conn {
public:
    Conn() = default;
    explicit Conn(Socket fd) noexcept : fd_(fd) {}
    ~Conn();
    Conn(Conn&& other) noexcept;
    Conn& operator=(Conn&& other) noexcept;
    Conn(const Conn&) = delete;
    Conn& operator=(const Conn&) = delete;

    // Connects with TCP_NODELAY, retrying until retry_ms elapses.
    static Result<Conn> try_connect(const std::string& host, std::uint16_t port, int retry_ms = 5000);

    Socket fd() const noexcept { return fd_; }
    bool is_open() const noexcept { return fd_ != kInvalidSocket; }
    void close() noexcept;
    // Gives up the socket without closing it (the hub's HTTP server).
    Socket release() noexcept;
    // A Windows peer drops what it has not read yet when the connection is
    // reset, so the hub ends a session with shutdown_send() after its last
    // frame, discard_input() while the socket is readable until it returns
    // false (the peer closed), then close(). Non-blocking mode.
    void shutdown_send() noexcept;
    bool discard_input() noexcept;

    // ---- blocking mode (the device) ----
    // Header and payload go out in one gathered write.
    Status try_send(MsgType type, std::uint64_t correlation, std::string_view payload, std::uint32_t from = 0,
                    std::uint32_t to = 0);
    // Waits at most timeout_ms (< 0: no limit) for the first byte of a
    // frame; nullopt when none came. Then reads the whole frame.
    Result<std::optional<Frame>> try_recv(std::initializer_list<MsgType> allowed, int timeout_ms = -1);

    // ---- non-blocking mode (the hub) ----
    Status try_set_nonblocking(bool on);
    // Takes ownership of the payload; written by try_flush().
    Status try_enqueue(MsgType type, std::uint64_t correlation, std::string payload, std::uint32_t from = 0,
                       std::uint32_t to = 0);
    // Writes as much as the socket accepts. True when nothing is pending.
    Result<bool> try_flush();
    bool want_write() const noexcept { return !tx_.empty(); }
    std::size_t pending_bytes() const noexcept { return tx_bytes_; }
    // Reads what is available: one complete frame when ready, nullopt when
    // the socket has no more bytes yet. An Error on EOF or a bad header.
    Result<std::optional<Frame>> try_recv_nowait(std::initializer_list<MsgType> allowed);

    std::uint64_t bytes_sent() const noexcept { return bytes_sent_; }
    std::uint64_t bytes_received() const noexcept { return bytes_received_; }

private:
    struct Pending {
        protocol::FrameHeader header;
        std::string payload;
        std::size_t written = 0;   // over header + payload
    };
    Status drain_queue();   // blocking: writes every enqueued frame
    Socket fd_ = kInvalidSocket;
    // receive state
    protocol::FrameHeader rx_header_{};
    std::size_t rx_header_got_ = 0;
    bool rx_have_header_ = false;
    std::string rx_payload_;
    std::size_t rx_payload_got_ = 0;
    // send state
    std::deque<Pending> tx_;
    std::size_t tx_bytes_ = 0;
    std::uint64_t bytes_sent_ = 0;
    std::uint64_t bytes_received_ = 0;
};

class Listener {
public:
    Listener() = default;
    ~Listener();
    Listener(Listener&& other) noexcept;
    Listener& operator=(Listener&& other) noexcept;
    Listener(const Listener&) = delete;
    Listener& operator=(const Listener&) = delete;

    // Non-blocking listener.
    static Result<Listener> try_bind(const std::string& host, std::uint16_t port);
    Socket fd() const noexcept { return fd_; }
    // A closed Conn when nothing is pending.
    Result<Conn> try_accept();

private:
    Socket fd_ = kInvalidSocket;
};

// Readiness wait: poll() on POSIX, WSAPoll() on Windows. readable is also set
// when the peer closed or the socket has an error (the next read reports it).
struct PollItem {
    Socket fd = kInvalidSocket;
    bool want_read = false;
    bool want_write = false;
    bool readable = false;   // out
    bool writable = false;   // out
};
// Number of ready items, 0 on timeout (and on an interrupted wait);
// timeout_ms < 0 waits without limit.
Result<int> try_poll(PollItem* items, std::size_t count, int timeout_ms);

// Raw socket helpers for the hub's small HTTP server (no framing).
// recv: > 0 bytes, 0 peer closed, -1 would block, -2 error.
long long raw_recv(Socket fd, char* dst, std::size_t n) noexcept;
// Bytes written, 0 when the socket would block, -1 on an error.
long long raw_send(Socket fd, const char* src, std::size_t n) noexcept;
void raw_close(Socket fd) noexcept;

// The first IPv4 address of this host that is not a loopback one ("" when
// there is none): the default host of the hub's join link.
std::string guess_lan_address();

// steady_clock in nanoseconds and milliseconds (process-local).
std::uint64_t now_ns() noexcept;
std::uint64_t now_ms() noexcept;
// system_clock in milliseconds since 1970 (for CSV timestamps).
std::uint64_t wall_ms() noexcept;

// "host:port" -> host and port (default_port when there is no ":port");
// "[v6]:port" accepted. False when the text is not one.
bool split_host_port(std::string_view text, std::string& host, std::uint16_t& port, std::uint16_t default_port);

} // namespace xmesh::net
