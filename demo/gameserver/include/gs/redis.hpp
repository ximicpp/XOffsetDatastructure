// Game server demo: a minimal blocking Redis client (RESP2), binary-safe.
//
// Only what the demo needs: GET, SET, DEL and pipelined SETs. One client per
// thread; the logic node gives each shard one for loads and its write-behind
// thread one for saves.
#ifndef GS_REDIS_HPP
#define GS_REDIS_HPP

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace gs {

class Redis {
public:
    Redis() = default;
    ~Redis();
    Redis(const Redis&) = delete;
    Redis& operator=(const Redis&) = delete;

    bool connect(const std::string& host, std::uint16_t port);
    bool connected() const { return fd_ >= 0; }

    // nullopt: the key does not exist, or the call failed (ok() says which).
    std::optional<std::string> get(std::string_view key);
    bool set(std::string_view key, std::string_view value);
    bool del(std::string_view key);
    // One round trip for all of them.
    bool set_many(const std::vector<std::pair<std::string, std::string>>& items);
    bool flushdb();

    bool ok() const { return ok_; }
    const std::string& error() const { return error_; }
    std::uint64_t bytes_written() const { return bytes_written_; }

private:
    void append_command(std::initializer_list<std::string_view> args);
    bool flush_out();
    // Reads one reply; for a bulk string fills `bulk` (nullopt for nil).
    bool read_reply(std::optional<std::string>* bulk);
    bool read_line(std::string& line);
    bool read_exact(std::size_t n, std::string& out);
    bool fail(std::string why);

    int fd_ = -1;
    std::string out_;
    std::string in_;
    std::size_t in_pos_ = 0;
    bool ok_ = true;
    std::string error_;
    std::uint64_t bytes_written_ = 0;
};

}  // namespace gs

#endif
