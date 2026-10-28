// SC-14 size probe: the smallest client that receives a server's wire, verifies it,
// reads it, changes it and sends it back, for XO_SCHEMAS distinct root schemas
// (0: the same program without XOffset, the baseline). Built -Os and stripped by
// tools/client_size/measure.sh; the difference to the baseline is what the library
// adds to an app, and the slope over XO_SCHEMAS is the cost of one more schema.
// It is never run for the measurement; run with a wire file it prints what it read.
#include <cstdint>
#include <cstdio>
#include <string>

#if XO_SCHEMAS > 0
#include "xoffsetdatastructure.hpp"

using namespace XOffsetDatastructure;

struct Item {
    std::int32_t id;
    std::int32_t count;
};
XOFFSET_RECORD(Item, id, count)

// One root schema per K: the same shape with a K-sized block, so every K has its own
// layout, signature and instantiations, like distinct messages of a real protocol.
#define XO_MSG(K)                                                       \
    struct Msg##K {                                                     \
        std::int64_t id;                                                \
        XString name;                                                   \
        XVector<Item> items;                                            \
        XVector<std::int32_t> scores;                                   \
        std::array<std::int32_t, K> extra;                              \
    };                                                                  \
    XOFFSET_RECORD(Msg##K, id, name, items, scores, extra)               \
    XOFFSET_REGISTER_SCHEMA_NAME(Msg##K, "size_client.Msg" #K)

XO_MSG(1) XO_MSG(2) XO_MSG(3) XO_MSG(4) XO_MSG(5) XO_MSG(6) XO_MSG(7) XO_MSG(8)

namespace {
template <class Msg>
long handle(const std::string& wire, std::string& reply) {
    auto loaded = XBuffer::try_load_verified<Msg>(wire, XBuffer::maximum_capacity(1u << 20));
    if (!loaded) return -1;
    XBuffer& buf = *loaded;
    Msg& m = buf.root<Msg>();
    long sum = static_cast<long>(m.id) + static_cast<long>(m.name.size());
    for (const Item& it : m.items) sum += it.id * it.count;
    for (std::int32_t s : m.scores) sum += s;
    for (std::int32_t e : m.extra) sum += e;
    // the client's small change before sending the state back
    if (!m.scores.empty()) m.scores[0] += 1;
    reply = buf.save_compact<Msg>();
    return sum;
}
}  // namespace
#endif

int main(int argc, char** argv) {
    if (argc < 2) return 2;
    std::FILE* f = std::fopen(argv[1], "rb");
    if (!f) return 2;
    std::string wire;
    char chunk[4096];
    for (std::size_t n; (n = std::fread(chunk, 1, sizeof chunk, f)) > 0;) wire.append(chunk, n);
    std::fclose(f);
    if (wire.empty()) return 2;
    long result = 0;
    std::string reply;
    // the first byte picks the message type, as a frame header would
    const unsigned char kind = static_cast<unsigned char>(wire[0]);
    wire.erase(0, 1);
#if XO_SCHEMAS == 0
    for (unsigned char c : wire) result += c;
    reply = wire;
    (void)kind;
#else
    switch (kind % XO_SCHEMAS) {
#define XO_CASE(K) \
    case K - 1: result = handle<Msg##K>(wire, reply); break;
        XO_CASE(1)
#if XO_SCHEMAS > 1
        XO_CASE(2)
#endif
#if XO_SCHEMAS > 2
        XO_CASE(3) XO_CASE(4)
#endif
#if XO_SCHEMAS > 4
        XO_CASE(5) XO_CASE(6) XO_CASE(7) XO_CASE(8)
#endif
    }
#endif
    std::printf("%ld %zu\n", result, reply.size());
    return 0;
}
