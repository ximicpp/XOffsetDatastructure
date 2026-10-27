// Map state demo: TCP framing shared by A, B and the client.
// FROZEN INTERFACE (demo/mapstate/INTERFACES.md). Bodies live in src/net.cpp,
// owned by the common-library thread.
//
// Frame = 24-byte FrameHeader (little endian) + payload. State payloads are
// XOffset wires (save_verified / save_compact output, a ViewSnapshot
// optionally zstd-compressed, see codec.hpp); control payloads are raw bytes.
// Receivers check magic, direction (allowed types) and the per-type length
// limit BEFORE allocating the payload buffer.
//
// Sockets: POSIX (Linux, macOS, Android, iOS) and Winsock 2 (Windows: MinGW,
// MSVC). A socket handle is a Socket on both.
#pragma once
#include <cstddef>
#include <cstdint>
#include <deque>
#include <initializer_list>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>

namespace mapdemo::net {

enum class MsgType : std::uint16_t {
    Hello = 1,         // A->B, client->A: hello_payload() (4 schema hashes, 32 B)
    HelloOk = 2,       // B->A, A->client: empty
    Catalog = 3,       // A->client: MapCatalog wire
    ViewReq = 4,       // client->A: ViewRequest wire; flags = codec::view_flags()
    ViewSnapshot = 5,  // A->client: ViewSnapshot in the requested encoding; flags echoed
    StepReq = 6,       // A->B: UnitStateBatch wire; correlation = batch tick
    StepRes = 7,       // B->A: UnitStateBatch wire; correlation = new tick
    Error = 8,         // any direction: UTF-8 text
    Bye = 9,           // A->B, A->client: empty; receiver finishes and exits 0
};

// Test categories for view requests (headless sampling); GUI uses Normal.
// Bits 0-7 of the ViewReq / ViewSnapshot flags (codec.hpp has the rest).
enum class ViewCategory : std::uint16_t { Normal = 0, FullScene = 1, Empty = 2 };

// Socket handle: a file descriptor on POSIX, a SOCKET on Windows (both fit).
using Socket = std::intptr_t;
inline constexpr Socket kInvalidSocket = -1;

inline constexpr std::uint32_t kMagic = 0x31444D58u;   // "XMD1"

struct FrameHeader {
    std::uint32_t magic;
    std::uint16_t type;
    std::uint16_t flags;
    std::uint32_t length;        // payload bytes
    std::uint32_t reserved;      // 0
    std::uint64_t correlation;   // tick (A<->B) or request id (client<->A)
};
static_assert(sizeof(FrameHeader) == 24);

// Per-type receive limit: Catalog 1 MiB, ViewReq 4 KiB, ViewSnapshot 16 MiB,
// StepReq/StepRes 64 MiB, Hello 32 B, Error 4 KiB, HelloOk/Bye 0.
std::uint32_t max_payload(MsgType type) noexcept;
const char* type_name(MsgType type) noexcept;

struct Frame {
    MsgType type{};
    std::uint16_t flags{};
    std::uint64_t correlation{};
    std::string payload;
};

// Bad magic, unexpected type, declared length over the limit, short read, EOF.
struct ProtocolError : std::runtime_error {
    using std::runtime_error::runtime_error;
};

// The peer closed (or reset) the connection between frames.
struct PeerClosed : ProtocolError {
    using ProtocolError::ProtocolError;
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
    static Conn connect(const std::string& host, std::uint16_t port, int retry_ms = 5000);

    Socket fd() const noexcept { return fd_; }
    bool is_open() const noexcept { return fd_ != kInvalidSocket; }
    void close() noexcept;

    // ---- blocking mode (B; simple tools) ----
    // Header and payload go out in one gathered write; the payload is not copied.
    void send(MsgType type, std::uint64_t correlation, std::string_view payload, std::uint16_t flags = 0);
    Frame recv(std::initializer_list<MsgType> allowed);
    // Raw bytes, no framing (the --evil probe sends a bare header).
    void send_raw(std::string_view bytes);

    // ---- non-blocking mode (A, client) ----
    void set_nonblocking(bool on);
    // Takes ownership of the payload; written by flush().
    void enqueue(MsgType type, std::uint64_t correlation, std::string payload, std::uint16_t flags = 0);
    // Writes as much as the socket accepts. Returns true when nothing is pending.
    bool flush();
    bool want_write() const noexcept { return !tx_.empty(); }
    // Reads what is available; returns one complete frame when ready. Same
    // checks as recv(). Throws ProtocolError on EOF or a bad header.
    std::optional<Frame> try_recv(std::initializer_list<MsgType> allowed);

    // ---- payload buffer recycling (optional, --buffers reuse) ----
    // Hands a consumed payload back: the next received frame is sized into
    // its capacity instead of a new allocation.
    void recycle_rx(std::string&& buffer) noexcept;
    // The payload of the last frame enqueue() finished writing, cleared with
    // its capacity kept (an empty string when there is none).
    std::string take_tx_buffer() noexcept;

    std::uint64_t bytes_sent() const noexcept { return bytes_sent_; }
    std::uint64_t bytes_received() const noexcept { return bytes_received_; }

private:
    struct Pending {
        FrameHeader header;
        std::string payload;
        std::size_t written = 0;   // over header + payload
    };
    Socket fd_ = kInvalidSocket;
    // receive state
    FrameHeader rx_header_{};
    std::size_t rx_header_got_ = 0;
    bool rx_have_header_ = false;
    std::string rx_payload_;
    std::size_t rx_payload_got_ = 0;
    std::string rx_spare_;
    // send state
    std::deque<Pending> tx_;
    std::string tx_spare_;
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

    static Listener bind(const std::string& host, std::uint16_t port);
    Socket fd() const noexcept { return fd_; }
    Conn accept();   // blocking unless set non-blocking by the caller via fd()

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
// timeout_ms < 0 waits without limit. Throws ProtocolError on failure.
int poll(PollItem* items, std::size_t count, int timeout_ms);

// Handshake: 4 x u64 wire_schema_hash_v<T>() for MapCatalog, UnitStateBatch,
// ViewRequest, ViewSnapshot, in that order.
std::string hello_payload();
bool hello_matches(std::string_view payload);

// steady_clock in nanoseconds (process-local; never compared across processes).
std::uint64_t now_ns() noexcept;

inline constexpr std::uint16_t kDefaultPortA = 47701;   // A listens for the client
inline constexpr std::uint16_t kDefaultPortB = 47702;   // B listens for A

} // namespace mapdemo::net
