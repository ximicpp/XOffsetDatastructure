// Map state demo: framing bodies. OWNER: common-library thread.
//
// POSIX sockets (Linux, macOS, Android, iOS) and Winsock 2 (Windows). The
// platform differences stay in the first block of this file: socket type,
// error codes, close, non-blocking mode, gathered send and poll. Every
// receive path validates the header (magic, reserved, allowed type, per-type
// length limit) before the payload buffer is sized. An Error frame that the
// caller did not list in `allowed` is turned into an Error carrying the
// peer's text. The bodies never throw; the throwing calls at the end of the
// file (servers) wrap the try_* ones.
#include "mapdemo/net.hpp"

#include <algorithm>
#include <bit>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <thread>
#include <utility>
#include <vector>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>
#endif

#include "mapdemo/schema.hpp"

static_assert(std::endian::native == std::endian::little, "mapdemo frames are little endian");

namespace mapdemo::net {
namespace {

// ---- platform layer -------------------------------------------------------------

#if defined(_WIN32)
using NativeSocket = SOCKET;
using IoSize = int;
constexpr int kSendFlags = 0;

int last_error() noexcept { return ::WSAGetLastError(); }
bool would_block(int e) noexcept { return e == WSAEWOULDBLOCK; }
bool interrupted(int e) noexcept { return e == WSAEINTR; }
bool conn_reset(int e) noexcept { return e == WSAECONNRESET || e == WSAECONNABORTED; }
bool accept_retry(int e) noexcept { return e == WSAEINTR || e == WSAECONNRESET; }

std::string error_text(const char* what, int e) {
    char buf[256] = {};
    const DWORD n = ::FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr,
                                     DWORD(e), 0, buf, DWORD(sizeof buf), nullptr);
    std::string text(buf, n);
    while (!text.empty() && (text.back() == '\n' || text.back() == '\r' || text.back() == ' ')) text.pop_back();
    return std::string(what) + ": " + (text.empty() ? "error " + std::to_string(e) : text) +
           " (WSA " + std::to_string(e) + ")";
}

// Winsock needs WSAStartup before the first call; it stays up for the process.
Status ensure_sockets() {
    static const int rc = [] {
        WSADATA data;
        return ::WSAStartup(MAKEWORD(2, 2), &data);
    }();
    if (rc != 0) return Error{"WSAStartup failed: " + std::to_string(rc)};
    return {};
}

void close_socket(Socket s) noexcept { ::closesocket(NativeSocket(s)); }

bool set_socket_nonblocking(Socket s, bool on) noexcept {
    u_long mode = on ? 1 : 0;
    return ::ioctlsocket(NativeSocket(s), FIONBIO, &mode) == 0;
}
#else
using NativeSocket = int;
using IoSize = std::size_t;
#ifdef MSG_NOSIGNAL
constexpr int kSendFlags = MSG_NOSIGNAL;
#else
constexpr int kSendFlags = 0;
#endif

int last_error() noexcept { return errno; }
bool would_block(int e) noexcept { return e == EAGAIN || e == EWOULDBLOCK; }
bool interrupted(int e) noexcept { return e == EINTR; }
bool conn_reset(int e) noexcept { return e == ECONNRESET; }
bool accept_retry(int e) noexcept { return e == EINTR || e == ECONNABORTED; }

std::string error_text(const char* what, int e) { return std::string(what) + ": " + std::strerror(e); }

Status ensure_sockets() { return {}; }

void close_socket(Socket s) noexcept { ::close(NativeSocket(s)); }

bool set_socket_nonblocking(Socket s, bool on) noexcept {
    const int fl = ::fcntl(NativeSocket(s), F_GETFL, 0);
    if (fl < 0) return false;
    const int want = on ? (fl | O_NONBLOCK) : (fl & ~O_NONBLOCK);
    return want == fl || ::fcntl(NativeSocket(s), F_SETFL, want) == 0;
}
#endif

std::string errno_text(const char* what) { return error_text(what, last_error()); }

// socket() result as a Socket; kInvalidSocket on failure.
Socket open_socket(int family, int type, int protocol) noexcept {
    const NativeSocket s = ::socket(family, type, protocol);
#if defined(_WIN32)
    return s == INVALID_SOCKET ? kInvalidSocket : Socket(s);
#else
    return s < 0 ? kInvalidSocket : Socket(s);
#endif
}

void set_int_option(Socket s, int level, int name, int value) noexcept {
    ::setsockopt(NativeSocket(s), level, name, reinterpret_cast<const char*>(&value), sizeof value);
}

void tune_socket(Socket fd) {
    set_int_option(fd, IPPROTO_TCP, TCP_NODELAY, 1);
#ifdef SO_NOSIGPIPE
    set_int_option(fd, SOL_SOCKET, SO_NOSIGPIPE, 1);
#endif
}

// recv() of up to n bytes: > 0 bytes read, 0 peer closed, < 0 error (last_error()).
long long recv_some(Socket fd, char* dst, std::size_t n) noexcept {
    return ::recv(NativeSocket(fd), dst, IoSize(n), 0);
}

Result<FrameHeader> make_header(MsgType type, std::uint64_t correlation, std::size_t length, std::uint16_t flags) {
    if (length > max_payload(type))
        return Error{std::string("send: ") + type_name(type) + " payload of " + std::to_string(length) +
                     " bytes exceeds the limit of " + std::to_string(max_payload(type))};
    FrameHeader h{};
    h.magic = kMagic;
    h.type = static_cast<std::uint16_t>(type);
    h.flags = flags;
    h.length = static_cast<std::uint32_t>(length);
    h.reserved = 0;
    h.correlation = correlation;
    return h;
}

bool is_known(std::uint16_t raw) noexcept {
    return raw >= static_cast<std::uint16_t>(MsgType::Hello) && raw <= static_cast<std::uint16_t>(MsgType::Bye);
}

Error peer_closed(std::string message) { return Error{std::move(message), true}; }

// Header checks shared by recv() and recv_nowait(). Runs before any payload allocation.
Status validate_header(const FrameHeader& h, std::initializer_list<MsgType> allowed) {
    if (h.magic != kMagic) return Error{"bad frame magic"};
    if (h.reserved != 0) return Error{"bad frame header: reserved field is not zero"};
    if (!is_known(h.type)) return Error{"unknown frame type " + std::to_string(h.type)};
    const auto type = static_cast<MsgType>(h.type);
    const bool listed = std::find(allowed.begin(), allowed.end(), type) != allowed.end();
    if (!listed && type != MsgType::Error) return Error{std::string("unexpected frame type ") + type_name(type)};
    if (h.length > max_payload(type))
        return Error{std::string(type_name(type)) + " frame declares " + std::to_string(h.length) +
                     " payload bytes, limit " + std::to_string(max_payload(type))};
    return {};
}

// Completed frame -> Frame, or an Error for an Error frame the caller did not expect.
Result<Frame> finish_frame(const FrameHeader& h, std::string payload, std::initializer_list<MsgType> allowed) {
    const auto type = static_cast<MsgType>(h.type);
    if (type == MsgType::Error && std::find(allowed.begin(), allowed.end(), type) == allowed.end())
        return Error{"peer error: " + payload};
    Frame f;
    f.type = type;
    f.flags = h.flags;
    f.correlation = h.correlation;
    f.payload = std::move(payload);
    return f;
}

Status wait_fd(Socket fd, bool for_write) {
    PollItem p;
    p.fd = fd;
    p.want_read = !for_write;
    p.want_write = for_write;
    for (;;) {
        Result<int> n = try_poll(&p, 1, -1);
        if (!n) return std::move(n).error();
        if (*n != 0) return {};
    }
}

// Blocking read of exactly n bytes (waits when the socket is non-blocking).
Status read_exact(Socket fd, char* dst, std::size_t n, std::uint64_t& counter) {
    std::size_t got = 0;
    while (got < n) {
        const long long r = recv_some(fd, dst + got, n - got);
        if (r > 0) {
            got += static_cast<std::size_t>(r);
            counter += static_cast<std::uint64_t>(r);
            continue;
        }
        if (r == 0) {
            if (got == 0) return peer_closed("connection closed by peer");
            return Error{"connection closed mid-frame"};
        }
        const int e = last_error();
        if (interrupted(e)) continue;
        if (would_block(e)) MAPDEMO_TRY(wait_fd(fd, false));
        else if (conn_reset(e) && got == 0) return peer_closed(error_text("recv", e));
        else return Error{error_text("recv", e)};
    }
    return {};
}

// One gathered send over the unsent tail of header + payload. Returns bytes
// written, 0 when the socket would block; an error on a broken connection.
Result<std::size_t> send_some(Socket fd, const FrameHeader& h, std::string_view payload, std::size_t written) {
    const char* part[2];
    std::size_t len[2];
    int n = 0;
    if (written < sizeof h) {
        part[n] = reinterpret_cast<const char*>(&h) + written;
        len[n] = sizeof h - written;
        ++n;
        if (!payload.empty()) {
            part[n] = payload.data();
            len[n] = payload.size();
            ++n;
        }
    } else {
        const std::size_t off = written - sizeof h;
        part[n] = payload.data() + off;
        len[n] = payload.size() - off;
        ++n;
    }
    for (;;) {
#if defined(_WIN32)
        WSABUF buf[2];
        for (int i = 0; i < n; ++i) {
            buf[i].buf = const_cast<char*>(part[i]);
            buf[i].len = ULONG(std::min<std::size_t>(len[i], 0x7fffffff));
        }
        DWORD sent = 0;
        if (::WSASend(NativeSocket(fd), buf, DWORD(n), &sent, 0, nullptr, nullptr) == 0)
            return std::size_t(sent);
#else
        iovec iov[2];
        for (int i = 0; i < n; ++i) {
            iov[i].iov_base = const_cast<char*>(part[i]);
            iov[i].iov_len = len[i];
        }
        msghdr msg{};
        msg.msg_iov = iov;
        msg.msg_iovlen = static_cast<decltype(msg.msg_iovlen)>(n);
        const ssize_t w = ::sendmsg(NativeSocket(fd), &msg, kSendFlags);
        if (w >= 0) return std::size_t(w);
#endif
        const int e = last_error();
        if (interrupted(e)) continue;
        if (would_block(e)) return std::size_t(0);
        return Error{error_text("send", e)};
    }
}

} // namespace

std::uint32_t max_payload(MsgType type) noexcept {
    switch (type) {
    case MsgType::Hello: return 32;
    case MsgType::HelloOk: return 0;
    case MsgType::Catalog: return static_cast<std::uint32_t>(kCapCatalog);
    case MsgType::ViewReq: return static_cast<std::uint32_t>(kCapViewRequest);
    case MsgType::ViewSnapshot: return static_cast<std::uint32_t>(kCapSnapshot);
    case MsgType::StepReq: return static_cast<std::uint32_t>(kCapBatch);
    case MsgType::StepRes: return static_cast<std::uint32_t>(kCapBatch);
    case MsgType::Error: return 4096;
    case MsgType::Bye: return 0;
    }
    return 0;
}

const char* type_name(MsgType type) noexcept {
    switch (type) {
    case MsgType::Hello: return "Hello";
    case MsgType::HelloOk: return "HelloOk";
    case MsgType::Catalog: return "Catalog";
    case MsgType::ViewReq: return "ViewReq";
    case MsgType::ViewSnapshot: return "ViewSnapshot";
    case MsgType::StepReq: return "StepReq";
    case MsgType::StepRes: return "StepRes";
    case MsgType::Error: return "Error";
    case MsgType::Bye: return "Bye";
    }
    return "?";
}

// ---- Conn --------------------------------------------------------------------

Conn::~Conn() { close(); }

Conn::Conn(Conn&& other) noexcept
    : fd_(std::exchange(other.fd_, kInvalidSocket)),
      rx_header_(other.rx_header_),
      rx_header_got_(std::exchange(other.rx_header_got_, 0)),
      rx_have_header_(std::exchange(other.rx_have_header_, false)),
      rx_payload_(std::move(other.rx_payload_)),
      rx_payload_got_(std::exchange(other.rx_payload_got_, 0)),
      rx_spare_(std::move(other.rx_spare_)),
      tx_(std::move(other.tx_)),
      tx_spare_(std::move(other.tx_spare_)),
      bytes_sent_(std::exchange(other.bytes_sent_, 0)),
      bytes_received_(std::exchange(other.bytes_received_, 0)) {
    other.tx_.clear();
    other.rx_payload_.clear();
}

Conn& Conn::operator=(Conn&& other) noexcept {
    if (this != &other) {
        close();
        fd_ = std::exchange(other.fd_, kInvalidSocket);
        rx_header_ = other.rx_header_;
        rx_header_got_ = std::exchange(other.rx_header_got_, 0);
        rx_have_header_ = std::exchange(other.rx_have_header_, false);
        rx_payload_ = std::move(other.rx_payload_);
        rx_payload_got_ = std::exchange(other.rx_payload_got_, 0);
        rx_spare_ = std::move(other.rx_spare_);
        tx_ = std::move(other.tx_);
        tx_spare_ = std::move(other.tx_spare_);
        bytes_sent_ = std::exchange(other.bytes_sent_, 0);
        bytes_received_ = std::exchange(other.bytes_received_, 0);
        other.tx_.clear();
        other.rx_payload_.clear();
    }
    return *this;
}

Result<Conn> Conn::try_connect(const std::string& host, std::uint16_t port, int retry_ms) {
    MAPDEMO_TRY(ensure_sockets());
    addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    addrinfo* res = nullptr;
    const std::string service = std::to_string(port);
    const int gai = ::getaddrinfo(host.c_str(), service.c_str(), &hints, &res);
    if (gai != 0) return Error{"resolve " + host + ": " + ::gai_strerror(gai)};

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(retry_ms);
    std::string last = "no address";
    for (;;) {
        for (addrinfo* ai = res; ai != nullptr; ai = ai->ai_next) {
            const Socket fd = open_socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
            if (fd == kInvalidSocket) {
                last = errno_text("socket");
                continue;
            }
            int rc;
            do rc = ::connect(NativeSocket(fd), ai->ai_addr, static_cast<int>(ai->ai_addrlen));
            while (rc != 0 && interrupted(last_error()));
            if (rc == 0) {
                ::freeaddrinfo(res);
                tune_socket(fd);
                return Conn(fd);
            }
            last = errno_text("connect");
            close_socket(fd);
        }
        if (std::chrono::steady_clock::now() >= deadline) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    ::freeaddrinfo(res);
    return Error{"connect " + host + ":" + service + " failed after " + std::to_string(retry_ms) + " ms (" + last + ")"};
}

void Conn::close() noexcept {
    if (fd_ != kInvalidSocket) close_socket(fd_);
    fd_ = kInvalidSocket;
    rx_header_ = FrameHeader{};
    rx_header_got_ = 0;
    rx_have_header_ = false;
    rx_payload_.clear();
    rx_payload_got_ = 0;
    tx_.clear();
}

// Frames already queued by enqueue() go out first so the stream stays ordered.
Status Conn::drain_queue() {
    for (;;) {
        Result<bool> done = try_flush();
        if (!done) return std::move(done).error();
        if (*done) return {};
        MAPDEMO_TRY(wait_fd(fd_, true));
    }
}

Status Conn::try_send(MsgType type, std::uint64_t correlation, std::string_view payload, std::uint16_t flags) {
    if (fd_ == kInvalidSocket) return Error{"send on a closed connection"};
    MAPDEMO_TRY(drain_queue());
    Result<FrameHeader> header = make_header(type, correlation, payload.size(), flags);
    if (!header) return std::move(header).error();
    const FrameHeader h = *header;
    const std::size_t total = sizeof h + payload.size();
    std::size_t written = 0;
    while (written < total) {
        Result<std::size_t> w = send_some(fd_, h, payload, written);
        if (!w) return std::move(w).error();
        if (*w == 0) {
            MAPDEMO_TRY(wait_fd(fd_, true));
            continue;
        }
        written += *w;
        bytes_sent_ += *w;
    }
    return {};
}

Status Conn::try_send_raw(std::string_view bytes) {
    if (fd_ == kInvalidSocket) return Error{"send on a closed connection"};
    MAPDEMO_TRY(drain_queue());
    std::size_t written = 0;
    while (written < bytes.size()) {
        const auto w = ::send(NativeSocket(fd_), bytes.data() + written, IoSize(bytes.size() - written), kSendFlags);
        if (w > 0) {
            written += static_cast<std::size_t>(w);
            bytes_sent_ += static_cast<std::uint64_t>(w);
            continue;
        }
        const int e = last_error();
        if (interrupted(e)) continue;
        if (would_block(e)) MAPDEMO_TRY(wait_fd(fd_, true));
        else return Error{error_text("send", e)};
    }
    return {};
}

Result<Frame> Conn::try_recv(std::initializer_list<MsgType> allowed) {
    if (fd_ == kInvalidSocket) return Error{"recv on a closed connection"};
    if (rx_header_got_ != 0 || rx_have_header_)
        return Error{"recv: a non-blocking receive is in progress on this connection"};
    FrameHeader h{};
    MAPDEMO_TRY(read_exact(fd_, reinterpret_cast<char*>(&h), sizeof h, bytes_received_));
    MAPDEMO_TRY(validate_header(h, allowed));
    // A recycled buffer keeps its capacity; its old bytes are overwritten.
    std::string payload;
    if (h.length != 0) {
        payload = std::exchange(rx_spare_, std::string());
        payload.resize(h.length);
        MAPDEMO_TRY(read_exact(fd_, payload.data(), h.length, bytes_received_));
    }
    return finish_frame(h, std::move(payload), allowed);
}

Status Conn::try_set_nonblocking(bool on) {
    if (fd_ == kInvalidSocket) return Error{"set_nonblocking on a closed connection"};
    if (!set_socket_nonblocking(fd_, on)) return Error{errno_text("set_nonblocking")};
    return {};
}

Status Conn::try_enqueue(MsgType type, std::uint64_t correlation, std::string payload, std::uint16_t flags) {
    if (fd_ == kInvalidSocket) return Error{"enqueue on a closed connection"};
    Result<FrameHeader> header = make_header(type, correlation, payload.size(), flags);
    if (!header) return std::move(header).error();
    Pending p;
    p.header = *header;
    p.payload = std::move(payload);
    tx_.push_back(std::move(p));
    return {};
}

Result<bool> Conn::try_flush() {
    while (!tx_.empty()) {
        if (fd_ == kInvalidSocket) return Error{"flush on a closed connection"};
        Pending& p = tx_.front();
        const std::size_t total = sizeof p.header + p.payload.size();
        Result<std::size_t> w = send_some(fd_, p.header, p.payload, p.written);
        if (!w) return std::move(w).error();
        if (*w == 0) return false;
        p.written += *w;
        bytes_sent_ += *w;
        if (p.written == total) {
            if (p.payload.capacity() > tx_spare_.capacity()) tx_spare_ = std::move(p.payload);
            tx_.pop_front();
        }
    }
    return true;
}

Result<std::optional<Frame>> Conn::try_recv_nowait(std::initializer_list<MsgType> allowed) {
    if (fd_ == kInvalidSocket) return Error{"recv_nowait on a closed connection"};
    for (;;) {
        char* dst;
        std::size_t want;
        if (!rx_have_header_) {
            dst = reinterpret_cast<char*>(&rx_header_) + rx_header_got_;
            want = sizeof rx_header_ - rx_header_got_;
        } else {
            dst = rx_payload_.data() + rx_payload_got_;
            want = rx_payload_.size() - rx_payload_got_;
        }
        if (want != 0) {
            const long long r = recv_some(fd_, dst, want);
            const bool mid = rx_header_got_ != 0;
            if (r == 0) {
                if (!mid) return peer_closed("connection closed by peer");
                return Error{"connection closed mid-frame"};
            }
            if (r < 0) {
                const int e = last_error();
                if (interrupted(e)) continue;
                if (would_block(e)) return std::optional<Frame>();
                if (conn_reset(e) && !mid) return peer_closed(error_text("recv", e));
                return Error{error_text("recv", e)};
            }
            bytes_received_ += static_cast<std::uint64_t>(r);
            if (!rx_have_header_) rx_header_got_ += static_cast<std::size_t>(r);
            else rx_payload_got_ += static_cast<std::size_t>(r);
        }
        if (!rx_have_header_) {
            if (rx_header_got_ < sizeof rx_header_) continue;
            MAPDEMO_TRY(validate_header(rx_header_, allowed));   // before the payload is sized
            rx_have_header_ = true;
            rx_payload_.clear();
            if (rx_header_.length != 0) rx_payload_ = std::exchange(rx_spare_, std::string());
            rx_payload_.resize(rx_header_.length);
            rx_payload_got_ = 0;
        }
        if (rx_payload_got_ < rx_payload_.size()) continue;
        const FrameHeader h = rx_header_;
        std::string payload = std::move(rx_payload_);
        rx_payload_ = std::string();
        rx_header_ = FrameHeader{};
        rx_header_got_ = 0;
        rx_have_header_ = false;
        rx_payload_got_ = 0;
        Result<Frame> f = finish_frame(h, std::move(payload), allowed);
        if (!f) return std::move(f).error();
        return std::optional<Frame>(std::move(*f));
    }
}

void Conn::recycle_rx(std::string&& buffer) noexcept {
    if (buffer.capacity() > rx_spare_.capacity()) rx_spare_ = std::move(buffer);
}

std::string Conn::take_tx_buffer() noexcept {
    std::string s = std::exchange(tx_spare_, std::string());
    s.clear();
    return s;
}

// ---- Listener ----------------------------------------------------------------

Listener::~Listener() {
    if (fd_ != kInvalidSocket) close_socket(fd_);
}

Listener::Listener(Listener&& other) noexcept : fd_(std::exchange(other.fd_, kInvalidSocket)) {}

Listener& Listener::operator=(Listener&& other) noexcept {
    if (this != &other) {
        if (fd_ != kInvalidSocket) close_socket(fd_);
        fd_ = std::exchange(other.fd_, kInvalidSocket);
    }
    return *this;
}

Result<Listener> Listener::try_bind(const std::string& host, std::uint16_t port) {
    MAPDEMO_TRY(ensure_sockets());
    addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    addrinfo* res = nullptr;
    const std::string service = std::to_string(port);
    const int gai = ::getaddrinfo(host.empty() ? nullptr : host.c_str(), service.c_str(), &hints, &res);
    if (gai != 0) return Error{"resolve " + host + ": " + ::gai_strerror(gai)};
    std::string last = "no address";
    for (addrinfo* ai = res; ai != nullptr; ai = ai->ai_next) {
        const Socket fd = open_socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
        if (fd == kInvalidSocket) {
            last = errno_text("socket");
            continue;
        }
#if !defined(_WIN32)
        // POSIX: rebind over TIME_WAIT. (On Windows SO_REUSEADDR would allow a
        // second live listener on the port instead.)
        set_int_option(fd, SOL_SOCKET, SO_REUSEADDR, 1);
#endif
        if (::bind(NativeSocket(fd), ai->ai_addr, static_cast<int>(ai->ai_addrlen)) == 0 &&
            ::listen(NativeSocket(fd), 16) == 0) {
            ::freeaddrinfo(res);
            Listener l;
            l.fd_ = fd;
            return Result<Listener>(std::move(l));
        }
        last = errno_text("bind/listen");
        close_socket(fd);
    }
    ::freeaddrinfo(res);
    return Error{"listen on " + host + ":" + service + " failed (" + last + ")"};
}

Result<Conn> Listener::try_accept() {
    if (fd_ == kInvalidSocket) return Error{"accept on a closed listener"};
    for (;;) {
        const NativeSocket raw = ::accept(NativeSocket(fd_), nullptr, nullptr);
#if defined(_WIN32)
        const Socket fd = raw == INVALID_SOCKET ? kInvalidSocket : Socket(raw);
#else
        const Socket fd = raw < 0 ? kInvalidSocket : Socket(raw);
#endif
        if (fd != kInvalidSocket) {
            tune_socket(fd);
            return Conn(fd);
        }
        const int e = last_error();
        if (accept_retry(e)) continue;
        if (would_block(e)) return Conn();
        return Error{error_text("accept", e)};
    }
}

// ---- poll ----------------------------------------------------------------------

Result<int> try_poll(PollItem* items, std::size_t count, int timeout_ms) {
#if defined(_WIN32)
    using PollFd = WSAPOLLFD;
#else
    using PollFd = pollfd;
#endif
    thread_local std::vector<PollFd> fds;
    fds.resize(count);
    for (std::size_t i = 0; i < count; ++i) {
        fds[i] = PollFd{};
        fds[i].fd = NativeSocket(items[i].fd);
        fds[i].events = static_cast<short>((items[i].want_read ? POLLIN : 0) | (items[i].want_write ? POLLOUT : 0));
        items[i].readable = items[i].writable = false;
    }
#if defined(_WIN32)
    const int rc = ::WSAPoll(fds.data(), ULONG(count), timeout_ms < 0 ? -1 : timeout_ms);
#else
    const int rc = ::poll(fds.data(), nfds_t(count), timeout_ms < 0 ? -1 : timeout_ms);
#endif
    if (rc < 0) {
        const int e = last_error();
        if (interrupted(e)) return 0;
        return Error{error_text("poll", e)};
    }
    for (std::size_t i = 0; i < count; ++i) {
        const short rev = fds[i].revents;
        items[i].readable = (rev & (POLLIN | POLLHUP | POLLERR)) != 0;
        items[i].writable = (rev & (POLLOUT | POLLERR | POLLHUP)) != 0;
    }
    return rc;
}

// ---- handshake and clock ------------------------------------------------------

namespace {
constexpr std::uint64_t kHelloHashes[4] = {
    wire_schema_hash_v<MapCatalog>(),
    wire_schema_hash_v<UnitStateBatch>(),
    wire_schema_hash_v<ViewRequest>(),
    wire_schema_hash_v<ViewSnapshot>(),
};
static_assert(sizeof kHelloHashes == 32);
} // namespace

std::string hello_payload() {
    return std::string(reinterpret_cast<const char*>(kHelloHashes), sizeof kHelloHashes);
}

bool hello_matches(std::string_view payload) {
    return payload.size() == sizeof kHelloHashes && std::memcmp(payload.data(), kHelloHashes, sizeof kHelloHashes) == 0;
}

std::uint64_t now_ns() noexcept {
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch())
            .count());
}

// ---- throwing calls (servers) -------------------------------------------------

#if MAPDEMO_EXCEPTIONS
namespace {
[[noreturn]] void raise(Error&& e) {
    if (e.peer_closed) throw PeerClosed(e.message);
    throw ProtocolError(e.message);
}
void value_or_throw(Status&& s) {
    if (!s) raise(std::move(s).error());
}
template <class T>
T value_or_throw(Result<T>&& r) {
    if (!r) raise(std::move(r).error());
    return std::move(*r);
}
} // namespace

Conn Conn::connect(const std::string& host, std::uint16_t port, int retry_ms) {
    return value_or_throw(try_connect(host, port, retry_ms));
}
void Conn::send(MsgType type, std::uint64_t correlation, std::string_view payload, std::uint16_t flags) {
    value_or_throw(try_send(type, correlation, payload, flags));
}
Frame Conn::recv(std::initializer_list<MsgType> allowed) { return value_or_throw(try_recv(allowed)); }
void Conn::send_raw(std::string_view bytes) { value_or_throw(try_send_raw(bytes)); }
void Conn::set_nonblocking(bool on) { value_or_throw(try_set_nonblocking(on)); }
void Conn::enqueue(MsgType type, std::uint64_t correlation, std::string payload, std::uint16_t flags) {
    value_or_throw(try_enqueue(type, correlation, std::move(payload), flags));
}
bool Conn::flush() { return value_or_throw(try_flush()); }
std::optional<Frame> Conn::recv_nowait(std::initializer_list<MsgType> allowed) {
    return value_or_throw(try_recv_nowait(allowed));
}
Listener Listener::bind(const std::string& host, std::uint16_t port) { return value_or_throw(try_bind(host, port)); }
Conn Listener::accept() { return value_or_throw(try_accept()); }
int poll(PollItem* items, std::size_t count, int timeout_ms) { return value_or_throw(try_poll(items, count, timeout_ms)); }
#endif

} // namespace mapdemo::net
