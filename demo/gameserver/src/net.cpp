// Game server demo: TCP framing and a poll() event loop (see net.hpp).
#include "gs/net.hpp"

#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#ifdef __linux__
#include <sys/epoll.h>
#endif

namespace gs {

namespace {

bool set_nonblocking(int fd) {
    const int flags = fcntl(fd, F_GETFL, 0);
    return flags >= 0 && fcntl(fd, F_SETFL, flags | O_NONBLOCK) == 0;
}

void set_nodelay(int fd) {
    int one = 1;
    (void)setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof one);
#ifdef SO_NOSIGPIPE
    (void)setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, &one, sizeof one);
#endif
}

#ifdef MSG_NOSIGNAL
constexpr int kSendFlags = MSG_NOSIGNAL;
#else
constexpr int kSendFlags = 0;
#endif

#ifdef __linux__
constexpr std::uint64_t kWakeTag = ~std::uint64_t{0};
constexpr std::uint64_t kListenerTag = std::uint64_t{1} << 62;
#endif

}  // namespace

int listen_tcp(std::uint16_t port) {
    const int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return -1;
    int one = 1;
    (void)setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port);
    if (bind(fd, reinterpret_cast<sockaddr*>(&addr), sizeof addr) != 0 || ::listen(fd, 1024) != 0 ||
        !set_nonblocking(fd)) {
        ::close(fd);
        return -1;
    }
    return fd;
}

int connect_tcp(const std::string& host, std::uint16_t port) {
    addrinfo hints{};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    addrinfo* res = nullptr;
    if (getaddrinfo(host.c_str(), std::to_string(port).c_str(), &hints, &res) != 0 || !res) return -1;
    const int fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (fd < 0) {
        freeaddrinfo(res);
        return -1;
    }
    const int rc = ::connect(fd, res->ai_addr, res->ai_addrlen);
    freeaddrinfo(res);
    if (rc != 0 || !set_nonblocking(fd)) {
        ::close(fd);
        return -1;
    }
    set_nodelay(fd);
    return fd;
}

bool parse_host_port(const std::string& text, std::string& host, std::uint16_t& port) {
    const auto colon = text.rfind(':');
    if (colon == std::string::npos || colon == 0) return false;
    host = text.substr(0, colon);
    const long p = std::strtol(text.c_str() + colon + 1, nullptr, 10);
    if (p <= 0 || p > 65535) return false;
    port = std::uint16_t(p);
    return true;
}

Loop::Loop() {
    signal(SIGPIPE, SIG_IGN);
    if (pipe(wake_) == 0) {
        set_nonblocking(wake_[0]);
        set_nonblocking(wake_[1]);
    }
#ifdef __linux__
    ep_ = epoll_create1(EPOLL_CLOEXEC);
    epoll_event ev{};
    ev.events = EPOLLIN;
    ev.data.u64 = kWakeTag;
    epoll_ctl(ep_, EPOLL_CTL_ADD, wake_[0], &ev);
#endif
}

Loop::~Loop() {
    for (auto& [id, c] : conns_)
        if (c->fd >= 0) ::close(c->fd);
    for (auto& l : listeners_) ::close(l.fd);
    if (wake_[0] >= 0) ::close(wake_[0]);
    if (wake_[1] >= 0) ::close(wake_[1]);
    if (ep_ >= 0) ::close(ep_);
}

void Loop::listen(int fd, std::size_t max_frame, OnAccept on_accept) {
    listeners_.push_back(Listener{fd, max_frame, std::move(on_accept)});
#ifdef __linux__
    epoll_event ev{};
    ev.events = EPOLLIN;
    ev.data.u64 = kListenerTag | (listeners_.size() - 1);
    epoll_ctl(ep_, EPOLL_CTL_ADD, fd, &ev);
#endif
}

Conn& Loop::adopt(int fd, std::size_t max_frame) {
    auto c = std::make_unique<Conn>();
    c->id = next_id_++;
    c->fd = fd;
    c->max_frame = max_frame;
    c->last_recv_us = now_us();
    Conn& ref = *c;
    conns_.emplace(ref.id, std::move(c));
#ifdef __linux__
    epoll_event ev{};
    ev.events = EPOLLIN;
    ev.data.u64 = ref.id;
    epoll_ctl(ep_, EPOLL_CTL_ADD, fd, &ev);
#endif
    return ref;
}

Conn* Loop::find(std::uint64_t id) {
    auto it = conns_.find(id);
    return it == conns_.end() || it->second->dead ? nullptr : it->second.get();
}

void Loop::queue(Conn& c) {
    if (c.queued) return;
    c.queued = true;
    queued_.push_back(c.id);
}

void Loop::send(Conn& c, std::uint8_t type, std::string_view body) { send(c, type, {}, body); }

void Loop::send(Conn& c, std::uint8_t type, std::string_view head, std::string_view body) {
    if (c.dead || c.closing) return;
    put_u32(c.out, std::uint32_t(1 + head.size() + body.size()));
    put_u8(c.out, type);
    c.out.append(head);
    c.out.append(body);
    queue(c);
}

void Loop::close(Conn& c) {
    if (c.dead) return;
    c.dead = true;
    dead_.push_back(c.id);
    if (on_close_) on_close_(c);
}

void Loop::close_after_send(Conn& c) {
    c.closing = true;
    queue(c);
}

void Loop::post(std::function<void()> f) {
    bool was_empty;
    {
        std::lock_guard<std::mutex> lock(posted_mutex_);
        was_empty = posted_.empty();
        posted_.push_back(std::move(f));
    }
    if (was_empty && wake_[1] >= 0) {
        const char b = 1;
        if (::write(wake_[1], &b, 1) < 0) {
            // the pipe is full: the loop is already awake
        }
    }
}

void Loop::drain_posted() {
    char buf[256];
    while (::read(wake_[0], buf, sizeof buf) > 0) {
    }
    std::vector<std::function<void()>> work;
    {
        std::lock_guard<std::mutex> lock(posted_mutex_);
        work.swap(posted_);
    }
    for (auto& f : work) f();
}

void Loop::read_ready(Conn& c) {
    char buf[64 * 1024];
    for (;;) {
        const ssize_t n = ::recv(c.fd, buf, sizeof buf, 0);
        if (n > 0) {
            c.in.append(buf, std::size_t(n));
            c.bytes_in += std::size_t(n);
            c.last_recv_us = now_us();
            if (std::size_t(n) < sizeof buf) break;
            continue;
        }
        if (n == 0) {
            close(c);
            return;
        }
        if (errno == EAGAIN || errno == EWOULDBLOCK) break;
        if (errno == EINTR) continue;
        close(c);
        return;
    }
    std::size_t pos = 0;
    while (!c.dead && c.in.size() - pos >= 5) {
        Reader r{std::string_view(c.in).substr(pos, 4)};
        const std::uint32_t len = r.u32();
        if (len == 0 || len > c.max_frame + 1) {  // before buffering anything more
            close(c);
            return;
        }
        if (c.in.size() - pos - 4 < len) break;
        const std::uint8_t type = std::uint8_t(c.in[pos + 4]);
        const std::string_view body(c.in.data() + pos + 5, len - 1);
        pos += 4 + std::size_t(len);
        if (on_frame_) on_frame_(c, type, body);
    }
    c.in.erase(0, pos);
}

void Loop::set_write_interest(Conn& c, bool on) {
    if (c.want_write == on) return;
    c.want_write = on;
#ifdef __linux__
    epoll_event ev{};
    ev.events = EPOLLIN | (on ? EPOLLOUT : 0u);
    ev.data.u64 = c.id;
    epoll_ctl(ep_, EPOLL_CTL_MOD, c.fd, &ev);
#endif
}

void Loop::write_ready(Conn& c) {
    while (c.out_pos < c.out.size()) {
        const ssize_t n = ::send(c.fd, c.out.data() + c.out_pos, c.out.size() - c.out_pos, kSendFlags);
        if (n > 0) {
            c.out_pos += std::size_t(n);
            c.bytes_out += std::size_t(n);
            continue;
        }
        if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) break;
        if (n < 0 && errno == EINTR) continue;
        close(c);
        return;
    }
    if (c.out_pos == c.out.size()) {
        c.out.clear();
        c.out_pos = 0;
        set_write_interest(c, false);
        if (c.closing) close(c);
    } else {
        if (c.out_pos > (1 << 20)) {
            c.out.erase(0, c.out_pos);
            c.out_pos = 0;
        }
        set_write_interest(c, true);  // the socket is full: wait until it drains
    }
}

// Frames queued while handling input go out in the same iteration.
void Loop::flush_queued() {
    std::vector<std::uint64_t> ids;
    ids.swap(queued_);
    for (std::uint64_t id : ids) {
        auto it = conns_.find(id);
        if (it == conns_.end()) continue;
        Conn& c = *it->second;
        c.queued = false;
        if (c.dead) continue;
        if (c.out_pos < c.out.size()) write_ready(c);
        else if (c.closing) close(c);
    }
}

void Loop::accept_ready(std::size_t i) {
    for (;;) {
        const int fd = ::accept(listeners_[i].fd, nullptr, nullptr);
        if (fd < 0) break;
        set_nonblocking(fd);
        set_nodelay(fd);
        Conn& c = adopt(fd, listeners_[i].max_frame);
        if (listeners_[i].on_accept) listeners_[i].on_accept(c);
    }
}

void Loop::reap() {
    for (std::uint64_t id : dead_) {
        auto it = conns_.find(id);
        if (it == conns_.end()) continue;
        ::close(it->second->fd);  // also leaves the epoll set
        conns_.erase(it);
    }
    dead_.clear();
}

void Loop::run(int tick_ms, const std::function<void()>& tick) {
    std::uint64_t next_tick = now_us() + std::uint64_t(tick_ms) * 1000;
#ifdef __linux__
    std::vector<epoll_event> events(1024);
#else
    std::vector<pollfd> fds;
    std::vector<std::uint64_t> ids;
#endif
    while (!stopping_) {
        const std::uint64_t now = now_us();
        const int timeout = now >= next_tick ? 0 : int((next_tick - now) / 1000) + 1;
#ifdef __linux__
        const int n = epoll_wait(ep_, events.data(), int(events.size()), timeout);
        if (n < 0 && errno != EINTR) break;
        for (int i = 0; i < n; ++i) {
            const std::uint64_t tag = events[std::size_t(i)].data.u64;
            const std::uint32_t re = events[std::size_t(i)].events;
            if (tag == kWakeTag) {
                drain_posted();
                continue;
            }
            if (tag & kListenerTag) {
                accept_ready(std::size_t(tag & ~kListenerTag));
                continue;
            }
            auto it = conns_.find(tag);
            if (it == conns_.end() || it->second->dead) continue;
            Conn& c = *it->second;
            if (re & (EPOLLIN | EPOLLHUP | EPOLLERR)) read_ready(c);
            if (!c.dead && (re & EPOLLOUT)) write_ready(c);
        }
#else
        fds.clear();
        ids.clear();
        fds.push_back(pollfd{wake_[0], POLLIN, 0});
        for (auto& l : listeners_) fds.push_back(pollfd{l.fd, POLLIN, 0});
        for (auto& [id, c] : conns_) {
            fds.push_back(pollfd{c->fd, short(POLLIN | (c->want_write ? POLLOUT : 0)), 0});
            ids.push_back(id);
        }
        const int rc = ::poll(fds.data(), nfds_t(fds.size()), timeout);
        if (rc < 0 && errno != EINTR) break;
        if (fds[0].revents) drain_posted();
        for (std::size_t i = 0; i < listeners_.size(); ++i)
            if (fds[1 + i].revents) accept_ready(i);
        const std::size_t base = 1 + listeners_.size();
        for (std::size_t i = 0; i < ids.size(); ++i) {
            const short re = fds[base + i].revents;
            if (!re) continue;
            auto it = conns_.find(ids[i]);
            if (it == conns_.end() || it->second->dead) continue;
            Conn& c = *it->second;
            if (re & (POLLIN | POLLHUP | POLLERR)) read_ready(c);
            if (!c.dead && (re & POLLOUT)) write_ready(c);
        }
#endif
        flush_queued();
        if (now_us() >= next_tick) {
            if (tick) tick();
            flush_queued();
            next_tick = now_us() + std::uint64_t(tick_ms) * 1000;
        }
        reap();
    }
}

}  // namespace gs
