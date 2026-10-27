// Schema handshake over a byte stream: the framing the library leaves to the
// application, in its simplest form.
//
//   frame = u32 little-endian length + that many bytes (an XOffset wire)
//
// The first frame each way is the handshake (hello, then ack); every later
// frame is one message. Compression, if any, goes inside a frame. The
// "connection" here is two in-memory byte queues so the example runs alone.
#include "../xoffset_handshake.hpp"

#include <cstdint>
#include <cstdio>
#include <deque>
#include <string>

using namespace XOffsetDatastructure;

struct Player {
    XString name;
    std::int32_t level;
    XVector<std::int32_t> items;
};
XOFFSET_RECORD(Player, name, level, items)
XOFFSET_REGISTER_SCHEMA_NAME(Player, "example.Player")

namespace {

using Stream = std::deque<char>;

void send_frame(Stream& out, const std::string& payload) {
    const auto n = static_cast<std::uint32_t>(payload.size());
    for (int i = 0; i < 4; ++i) out.push_back(static_cast<char>((n >> (8 * i)) & 0xFF));
    out.insert(out.end(), payload.begin(), payload.end());
}

// One complete frame, or false when the stream does not hold one yet.
bool receive_frame(Stream& in, std::string& payload, std::uint32_t max_bytes) {
    if (in.size() < 4) return false;
    std::uint32_t n = 0;
    for (int i = 0; i < 4; ++i) n |= std::uint32_t(static_cast<unsigned char>(in[i])) << (8 * i);
    if (n > max_bytes) throw XException("frame larger than the receiver accepts");
    if (in.size() < 4 + std::size_t(n)) return false;
    payload.assign(in.begin() + 4, in.begin() + 4 + n);
    in.erase(in.begin(), in.begin() + 4 + n);
    return true;
}

} // namespace

int main() {
    Stream to_server, to_client;

    // Client: the hello is the first frame.
    send_frame(to_server, make_schema_hello<Player>("example-client-1.0"));

    // Server: verify the hello, answer with the ack.
    std::string frame, ack;
    if (!receive_frame(to_server, frame, XSCHEMA_HELLO_MAX_BYTES)) return 1;
    XSchemaHelloResult peer = accept_schema_hello<Player>(frame, ack, "example-server-1.0");
    if (!peer.valid) {
        std::printf("server: bad hello: %s\n", peer.message.c_str());
        return 1;
    }
    send_frame(to_client, ack);
    if (!peer.accepted()) {
        std::printf("server: client %s needs an update\n", peer.label.c_str());
        return 1;
    }

    // Client: read the ack before sending anything else.
    if (!receive_frame(to_client, frame, XSCHEMA_HELLO_MAX_BYTES)) return 1;
    XSchemaAckResult r = read_schema_ack<Player>(frame);
    if (!r) {
        std::printf("client: %s\n", r.message.c_str());
        return 1;
    }

    // From here on: ordinary messages.
    auto player = TypedXBuffer<Player>::create();
    player.root().name = "susu";
    player.root().level = 3;
    player.root().items.push_back(42);
    send_frame(to_client, player.save_compact());

    if (!receive_frame(to_client, frame, 1u << 20)) return 1;
    auto received = TypedXBuffer<Player>::load_verified(frame, XBuffer::maximum_capacity(1u << 20));
    std::printf("handshake ok (%s <-> %s); %s level %d\n", peer.label.c_str(), "example-server-1.0",
                std::string(received.root().name.view()).c_str(), received.root().level);
    return 0;
}
