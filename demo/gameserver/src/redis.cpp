// Game server demo: minimal blocking Redis client (see redis.hpp).
#include "gs/redis.hpp"

#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <unistd.h>

namespace gs {

Redis::~Redis() {
    if (fd_ >= 0) ::close(fd_);
}

bool Redis::fail(std::string why) {
    ok_ = false;
    error_ = std::move(why);
    if (fd_ >= 0) ::close(fd_);
    fd_ = -1;
    return false;
}

bool Redis::connect(const std::string& host, std::uint16_t port) {
    addrinfo hints{};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    addrinfo* res = nullptr;
    if (getaddrinfo(host.c_str(), std::to_string(port).c_str(), &hints, &res) != 0 || !res)
        return fail("redis: cannot resolve " + host);
    fd_ = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    const int rc = fd_ < 0 ? -1 : ::connect(fd_, res->ai_addr, res->ai_addrlen);
    freeaddrinfo(res);
    if (rc != 0) return fail("redis: cannot connect to " + host + ":" + std::to_string(port));
    int one = 1;
    (void)setsockopt(fd_, IPPROTO_TCP, TCP_NODELAY, &one, sizeof one);
    ok_ = true;
    return true;
}

void Redis::append_command(std::initializer_list<std::string_view> args) {
    out_ += "*" + std::to_string(args.size()) + "\r\n";
    for (std::string_view a : args) {
        out_ += "$" + std::to_string(a.size()) + "\r\n";
        out_.append(a);
        out_ += "\r\n";
    }
}

bool Redis::flush_out() {
    std::size_t pos = 0;
    while (pos < out_.size()) {
        const ssize_t n = ::send(fd_, out_.data() + pos, out_.size() - pos, 0);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) return fail("redis: send failed");
        pos += std::size_t(n);
    }
    bytes_written_ += out_.size();
    out_.clear();
    return true;
}

bool Redis::read_exact(std::size_t n, std::string& out) {
    while (in_.size() - in_pos_ < n) {
        char buf[64 * 1024];
        const ssize_t r = ::recv(fd_, buf, sizeof buf, 0);
        if (r < 0 && errno == EINTR) continue;
        if (r <= 0) return fail("redis: connection closed");
        if (in_pos_ > 0) {
            in_.erase(0, in_pos_);
            in_pos_ = 0;
        }
        in_.append(buf, std::size_t(r));
    }
    out.assign(in_, in_pos_, n);
    in_pos_ += n;
    return true;
}

bool Redis::read_line(std::string& line) {
    for (;;) {
        const auto end = in_.find("\r\n", in_pos_);
        if (end != std::string::npos) {
            line.assign(in_, in_pos_, end - in_pos_);
            in_pos_ = end + 2;
            return true;
        }
        char buf[64 * 1024];
        const ssize_t r = ::recv(fd_, buf, sizeof buf, 0);
        if (r < 0 && errno == EINTR) continue;
        if (r <= 0) return fail("redis: connection closed");
        if (in_pos_ > 0) {
            in_.erase(0, in_pos_);
            in_pos_ = 0;
        }
        in_.append(buf, std::size_t(r));
    }
}

bool Redis::read_reply(std::optional<std::string>* bulk) {
    std::string line;
    if (!read_line(line) || line.empty()) return fail("redis: bad reply");
    switch (line[0]) {
        case '+':
        case ':': return true;
        case '-': ok_ = false; error_ = line.substr(1); return false;
        case '$': {
            const long n = std::strtol(line.c_str() + 1, nullptr, 10);
            if (n < 0) {
                if (bulk) bulk->reset();
                return true;
            }
            std::string data, crlf;
            if (!read_exact(std::size_t(n), data) || !read_exact(2, crlf)) return false;
            if (bulk) *bulk = std::move(data);
            return true;
        }
        default: return fail("redis: unexpected reply " + line);
    }
}

std::optional<std::string> Redis::get(std::string_view key) {
    ok_ = fd_ >= 0;
    if (!ok_) return std::nullopt;
    append_command({"GET", key});
    std::optional<std::string> v;
    if (!flush_out() || !read_reply(&v)) return std::nullopt;
    return v;
}

bool Redis::set(std::string_view key, std::string_view value) {
    if (fd_ < 0) return false;
    append_command({"SET", key, value});
    return flush_out() && read_reply(nullptr);
}

bool Redis::del(std::string_view key) {
    if (fd_ < 0) return false;
    append_command({"DEL", key});
    return flush_out() && read_reply(nullptr);
}

bool Redis::flushdb() {
    if (fd_ < 0) return false;
    append_command({"FLUSHDB"});
    return flush_out() && read_reply(nullptr);
}

bool Redis::set_many(const std::vector<std::pair<std::string, std::string>>& items) {
    if (fd_ < 0) return false;
    if (items.empty()) return true;
    for (const auto& [k, v] : items) append_command({"SET", k, v});
    if (!flush_out()) return false;
    bool all = true;
    for (std::size_t i = 0; i < items.size(); ++i) all = read_reply(nullptr) && all;
    return all;
}

}  // namespace gs
