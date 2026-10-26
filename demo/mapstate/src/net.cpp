// Map state demo: framing bodies. OWNER: common-library thread.
//
// POSIX sockets only (Linux, macOS). Every receive path validates the header
// (magic, reserved, allowed type, per-type length limit) before the payload
// buffer is sized. An Error frame that the caller did not list in `allowed`
// is turned into a ProtocolError carrying the peer's text.
#include "mapdemo/net.hpp"

#include <algorithm>
#include <bit>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <thread>
#include <utility>

#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>

#include "mapdemo/schema.hpp"

static_assert(std::endian::native == std::endian::little, "mapdemo frames are little endian");

namespace mapdemo::net {
namespace {

#ifdef MSG_NOSIGNAL
constexpr int kSendFlags = MSG_NOSIGNAL;
#else
constexpr int kSendFlags = 0;
#endif

std::string errno_text(const char* what) {
    return std::string(what) + ": " + std::strerror(errno);
}

void tune_socket(int fd) {
    int one = 1;
    ::setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof one);
#ifdef SO_NOSIGPIPE
    ::setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, &one, sizeof one);
#endif
}

FrameHeader make_header(MsgType type, std::uint64_t correlation, std::size_t length, std::uint16_t flags) {
    if (length > max_payload(type))
        throw ProtocolError(std::string("send: ") + type_name(type) + " payload of " + std::to_string(length) +
                            " bytes exceeds the limit of " + std::to_string(max_payload(type)));
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

// Header checks shared by recv() and try_recv(). Runs before any payload allocation.
void validate_header(const FrameHeader& h, std::initializer_list<MsgType> allowed) {
    if (h.magic != kMagic) throw ProtocolError("bad frame magic");
    if (h.reserved != 0) throw ProtocolError("bad frame header: reserved field is not zero");
    if (!is_known(h.type)) throw ProtocolError("unknown frame type " + std::to_string(h.type));
    const auto type = static_cast<MsgType>(h.type);
    const bool listed = std::find(allowed.begin(), allowed.end(), type) != allowed.end();
    if (!listed && type != MsgType::Error)
        throw ProtocolError(std::string("unexpected frame type ") + type_name(type));
    if (h.length > max_payload(type))
        throw ProtocolError(std::string(type_name(type)) + " frame declares " + std::to_string(h.length) +
                            " payload bytes, limit " + std::to_string(max_payload(type)));
}

// Completed frame -> Frame, or ProtocolError for an Error frame the caller did not expect.
Frame finish_frame(const FrameHeader& h, std::string payload, std::initializer_list<MsgType> allowed) {
    const auto type = static_cast<MsgType>(h.type);
    if (type == MsgType::Error && std::find(allowed.begin(), allowed.end(), type) == allowed.end())
        throw ProtocolError("peer error: " + payload);
    Frame f;
    f.type = type;
    f.flags = h.flags;
    f.correlation = h.correlation;
    f.payload = std::move(payload);
    return f;
}

void wait_fd(int fd, short events) {
    pollfd p{fd, events, 0};
    while (::poll(&p, 1, -1) < 0) {
        if (errno != EINTR) throw ProtocolError(errno_text("poll"));
    }
}

// Blocking read of exactly n bytes (waits on EAGAIN when the fd is non-blocking).
void read_exact(int fd, char* dst, std::size_t n, std::uint64_t& counter) {
    std::size_t got = 0;
    while (got < n) {
        const ssize_t r = ::recv(fd, dst + got, n - got, 0);
        if (r > 0) {
            got += static_cast<std::size_t>(r);
            counter += static_cast<std::uint64_t>(r);
        } else if (r == 0) {
            if (got == 0) throw PeerClosed("connection closed by peer");
            throw ProtocolError("connection closed mid-frame");
        } else if (errno == EINTR) {
            continue;
        } else if (errno == EAGAIN || errno == EWOULDBLOCK) {
            wait_fd(fd, POLLIN);
        } else if (errno == ECONNRESET && got == 0) {
            throw PeerClosed(errno_text("recv"));
        } else {
            throw ProtocolError(errno_text("recv"));
        }
    }
}

// One sendmsg over the unsent tail of header + payload. Returns bytes written,
// 0 on EAGAIN; throws on a broken connection.
std::size_t send_some(int fd, const FrameHeader& h, std::string_view payload, std::size_t written) {
    iovec iov[2];
    int n = 0;
    if (written < sizeof h) {
        iov[n].iov_base = const_cast<char*>(reinterpret_cast<const char*>(&h)) + written;
        iov[n].iov_len = sizeof h - written;
        ++n;
        if (!payload.empty()) {
            iov[n].iov_base = const_cast<char*>(payload.data());
            iov[n].iov_len = payload.size();
            ++n;
        }
    } else {
        const std::size_t off = written - sizeof h;
        iov[n].iov_base = const_cast<char*>(payload.data()) + off;
        iov[n].iov_len = payload.size() - off;
        ++n;
    }
    msghdr msg{};
    msg.msg_iov = iov;
    msg.msg_iovlen = static_cast<decltype(msg.msg_iovlen)>(n);
    for (;;) {
        const ssize_t w = ::sendmsg(fd, &msg, kSendFlags);
        if (w >= 0) return static_cast<std::size_t>(w);
        if (errno == EINTR) continue;
        if (errno == EAGAIN || errno == EWOULDBLOCK) return 0;
        throw ProtocolError(errno_text("send"));
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
    : fd_(std::exchange(other.fd_, -1)),
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
        fd_ = std::exchange(other.fd_, -1);
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

Conn Conn::connect(const std::string& host, std::uint16_t port, int retry_ms) {
    addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    addrinfo* res = nullptr;
    const std::string service = std::to_string(port);
    const int gai = ::getaddrinfo(host.c_str(), service.c_str(), &hints, &res);
    if (gai != 0) throw ProtocolError("resolve " + host + ": " + ::gai_strerror(gai));

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(retry_ms);
    std::string last = "no address";
    for (;;) {
        for (addrinfo* ai = res; ai != nullptr; ai = ai->ai_next) {
            const int fd = ::socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
            if (fd < 0) {
                last = errno_text("socket");
                continue;
            }
            int rc;
            do rc = ::connect(fd, ai->ai_addr, ai->ai_addrlen);
            while (rc < 0 && errno == EINTR);
            if (rc == 0) {
                ::freeaddrinfo(res);
                tune_socket(fd);
                return Conn(fd);
            }
            last = errno_text("connect");
            ::close(fd);
        }
        if (std::chrono::steady_clock::now() >= deadline) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    ::freeaddrinfo(res);
    throw ProtocolError("connect " + host + ":" + service + " failed after " + std::to_string(retry_ms) +
                        " ms (" + last + ")");
}

void Conn::close() noexcept {
    if (fd_ >= 0) ::close(fd_);
    fd_ = -1;
    rx_header_ = FrameHeader{};
    rx_header_got_ = 0;
    rx_have_header_ = false;
    rx_payload_.clear();
    rx_payload_got_ = 0;
    tx_.clear();
}

void Conn::send(MsgType type, std::uint64_t correlation, std::string_view payload, std::uint16_t flags) {
    if (fd_ < 0) throw ProtocolError("send on a closed connection");
    // Frames already queued by enqueue() go first so the stream stays ordered.
    while (!flush()) wait_fd(fd_, POLLOUT);
    const FrameHeader h = make_header(type, correlation, payload.size(), flags);
    const std::size_t total = sizeof h + payload.size();
    std::size_t written = 0;
    while (written < total) {
        const std::size_t w = send_some(fd_, h, payload, written);
        if (w == 0) {
            wait_fd(fd_, POLLOUT);
            continue;
        }
        written += w;
        bytes_sent_ += w;
    }
}

Frame Conn::recv(std::initializer_list<MsgType> allowed) {
    if (fd_ < 0) throw ProtocolError("recv on a closed connection");
    if (rx_header_got_ != 0 || rx_have_header_)
        throw ProtocolError("recv: a non-blocking receive is in progress on this connection");
    FrameHeader h{};
    read_exact(fd_, reinterpret_cast<char*>(&h), sizeof h, bytes_received_);
    validate_header(h, allowed);
    // A recycled buffer keeps its capacity; its old bytes are overwritten.
    std::string payload;
    if (h.length != 0) {
        payload = std::exchange(rx_spare_, std::string());
        payload.resize(h.length);
        read_exact(fd_, payload.data(), h.length, bytes_received_);
    }
    return finish_frame(h, std::move(payload), allowed);
}

void Conn::set_nonblocking(bool on) {
    if (fd_ < 0) throw ProtocolError("set_nonblocking on a closed connection");
    const int fl = ::fcntl(fd_, F_GETFL, 0);
    if (fl < 0) throw ProtocolError(errno_text("fcntl"));
    const int want = on ? (fl | O_NONBLOCK) : (fl & ~O_NONBLOCK);
    if (want != fl && ::fcntl(fd_, F_SETFL, want) < 0) throw ProtocolError(errno_text("fcntl"));
}

void Conn::enqueue(MsgType type, std::uint64_t correlation, std::string payload, std::uint16_t flags) {
    if (fd_ < 0) throw ProtocolError("enqueue on a closed connection");
    Pending p;
    p.header = make_header(type, correlation, payload.size(), flags);
    p.payload = std::move(payload);
    tx_.push_back(std::move(p));
}

bool Conn::flush() {
    while (!tx_.empty()) {
        if (fd_ < 0) throw ProtocolError("flush on a closed connection");
        Pending& p = tx_.front();
        const std::size_t total = sizeof p.header + p.payload.size();
        const std::size_t w = send_some(fd_, p.header, p.payload, p.written);
        if (w == 0) return false;
        p.written += w;
        bytes_sent_ += w;
        if (p.written == total) {
            if (p.payload.capacity() > tx_spare_.capacity()) tx_spare_ = std::move(p.payload);
            tx_.pop_front();
        }
    }
    return true;
}

std::optional<Frame> Conn::try_recv(std::initializer_list<MsgType> allowed) {
    if (fd_ < 0) throw ProtocolError("try_recv on a closed connection");
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
            const ssize_t r = ::recv(fd_, dst, want, 0);
            const bool mid = rx_header_got_ != 0;
            if (r == 0) {
                if (!mid) throw PeerClosed("connection closed by peer");
                throw ProtocolError("connection closed mid-frame");
            }
            if (r < 0) {
                if (errno == EINTR) continue;
                if (errno == EAGAIN || errno == EWOULDBLOCK) return std::nullopt;
                if (errno == ECONNRESET && !mid) throw PeerClosed(errno_text("recv"));
                throw ProtocolError(errno_text("recv"));
            }
            bytes_received_ += static_cast<std::uint64_t>(r);
            if (!rx_have_header_) rx_header_got_ += static_cast<std::size_t>(r);
            else rx_payload_got_ += static_cast<std::size_t>(r);
        }
        if (!rx_have_header_) {
            if (rx_header_got_ < sizeof rx_header_) continue;
            validate_header(rx_header_, allowed);   // before the payload is sized
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
        return finish_frame(h, std::move(payload), allowed);
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
    if (fd_ >= 0) ::close(fd_);
}

Listener::Listener(Listener&& other) noexcept : fd_(std::exchange(other.fd_, -1)) {}

Listener& Listener::operator=(Listener&& other) noexcept {
    if (this != &other) {
        if (fd_ >= 0) ::close(fd_);
        fd_ = std::exchange(other.fd_, -1);
    }
    return *this;
}

Listener Listener::bind(const std::string& host, std::uint16_t port) {
    addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    addrinfo* res = nullptr;
    const std::string service = std::to_string(port);
    const int gai = ::getaddrinfo(host.empty() ? nullptr : host.c_str(), service.c_str(), &hints, &res);
    if (gai != 0) throw ProtocolError("resolve " + host + ": " + ::gai_strerror(gai));
    std::string last = "no address";
    for (addrinfo* ai = res; ai != nullptr; ai = ai->ai_next) {
        const int fd = ::socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
        if (fd < 0) {
            last = errno_text("socket");
            continue;
        }
        int one = 1;
        ::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);
        if (::bind(fd, ai->ai_addr, ai->ai_addrlen) == 0 && ::listen(fd, 16) == 0) {
            ::freeaddrinfo(res);
            Listener l;
            l.fd_ = fd;
            return l;
        }
        last = errno_text("bind/listen");
        ::close(fd);
    }
    ::freeaddrinfo(res);
    throw ProtocolError("listen on " + host + ":" + service + " failed (" + last + ")");
}

// On a non-blocking listener with nothing pending, returns a closed Conn.
Conn Listener::accept() {
    if (fd_ < 0) throw ProtocolError("accept on a closed listener");
    for (;;) {
        const int fd = ::accept(fd_, nullptr, nullptr);
        if (fd >= 0) {
            tune_socket(fd);
            return Conn(fd);
        }
        if (errno == EINTR || errno == ECONNABORTED) continue;
        if (errno == EAGAIN || errno == EWOULDBLOCK) return Conn();
        throw ProtocolError(errno_text("accept"));
    }
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

} // namespace mapdemo::net
