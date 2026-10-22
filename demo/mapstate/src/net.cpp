// Map state demo: framing bodies. OWNER: common-library thread. STUB (first task).
#include "mapdemo/net.hpp"

namespace mapdemo::net {
[[noreturn]] static void todo() { throw ProtocolError("net: not implemented"); }
std::uint32_t max_payload(MsgType) noexcept { return 0; }
const char* type_name(MsgType) noexcept { return "?"; }
Conn::~Conn() { close(); }
Conn::Conn(Conn&& other) noexcept : fd_(other.fd_) { other.fd_ = -1; }
Conn& Conn::operator=(Conn&& other) noexcept { if (this != &other) { close(); fd_ = other.fd_; other.fd_ = -1; } return *this; }
Conn Conn::connect(const std::string&, std::uint16_t, int) { todo(); }
void Conn::close() noexcept { fd_ = -1; }
void Conn::send(MsgType, std::uint64_t, std::string_view, std::uint16_t) { todo(); }
Frame Conn::recv(std::initializer_list<MsgType>) { todo(); }
void Conn::set_nonblocking(bool) { todo(); }
void Conn::enqueue(MsgType, std::uint64_t, std::string, std::uint16_t) { todo(); }
bool Conn::flush() { todo(); }
std::optional<Frame> Conn::try_recv(std::initializer_list<MsgType>) { todo(); }
Listener::~Listener() {}
Listener::Listener(Listener&& other) noexcept : fd_(other.fd_) { other.fd_ = -1; }
Listener& Listener::operator=(Listener&& other) noexcept { fd_ = other.fd_; other.fd_ = -1; return *this; }
Listener Listener::bind(const std::string&, std::uint16_t) { todo(); }
Conn Listener::accept() { todo(); }
std::string hello_payload() { todo(); }
bool hello_matches(std::string_view) { return false; }
std::uint64_t now_ns() noexcept { return 0; }
} // namespace mapdemo::net
