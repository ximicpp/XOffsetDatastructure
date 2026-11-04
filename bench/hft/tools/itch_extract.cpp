// Cuts a replay window out of a Nasdaq TotalView-ITCH 5.0 day file.
//
//   curl -sSL <emi.nasdaq.com/.../01302019.NASDAQ_ITCH50.gz> | gunzip -c |
//     itch_extract --from 09:30:00 --to 09:31:00 --out m.itch
//
// Input and output use the day file's framing: each message is preceded by its
// length as a 2-byte big-endian integer. The output holds, in this order:
//   1. every Stock Directory message ('R') seen before the window, so a reader
//      can map stock locate codes to symbols;
//   2. one Add Order ('A') per order still resting on the book when the window
//      opens, rebuilt from the replayed adds, executions, cancels, deletes and
//      replaces before it (timestamp = window start, original reference, side,
//      remaining shares, price and stock), so the window replays onto the same
//      book the exchange had;
//   3. every message whose timestamp lies in [from, to).
// It stops reading at the first message at or after `to`. A summary of the
// message types in the window goes to stderr.
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <unordered_map>
#include <vector>

namespace {

uint64_t be(const uint8_t* p, int n) {
    uint64_t v = 0;
    for (int i = 0; i < n; ++i) v = (v << 8) | p[i];
    return v;
}
void put_be(uint8_t* p, uint64_t v, int n) {
    for (int i = n - 1; i >= 0; --i) { p[i] = uint8_t(v & 0xff); v >>= 8; }
}
uint64_t parse_hms(const char* s) {
    int h = 0, m = 0, sec = 0;
    if (std::sscanf(s, "%d:%d:%d", &h, &m, &sec) != 3) { std::fprintf(stderr, "bad time %s\n", s); std::exit(2); }
    return ((uint64_t(h) * 60 + m) * 60 + sec) * 1000000000ull;
}

struct Resting { uint16_t locate; uint8_t side; uint32_t shares; uint32_t price; std::array<char, 8> stock; };

} // namespace

int main(int argc, char** argv) {
    uint64_t from = parse_hms("09:30:00"), to = parse_hms("09:31:00");
    const char* out_path = nullptr;
    for (int i = 1; i + 1 < argc; i += 2) {
        if (!std::strcmp(argv[i], "--from")) from = parse_hms(argv[i + 1]);
        else if (!std::strcmp(argv[i], "--to")) to = parse_hms(argv[i + 1]);
        else if (!std::strcmp(argv[i], "--out")) out_path = argv[i + 1];
    }
    if (!out_path || to <= from) { std::fprintf(stderr, "usage: itch_extract --from HH:MM:SS --to HH:MM:SS --out FILE < day.itch\n"); return 2; }
    std::FILE* out = std::fopen(out_path, "wb");
    if (!out) { std::perror(out_path); return 1; }

    std::vector<uint8_t> dir;                       // framed 'R' messages
    std::unordered_map<uint64_t, Resting> book;     // order reference -> resting order
    book.reserve(1 << 22);
    std::array<uint64_t, 256> in_window{};
    uint64_t total = 0, written = 0, window_bytes = 0;
    bool seeded = false;
    uint8_t len_buf[2];
    uint8_t msg[65536];

    auto write_framed = [&](const uint8_t* m, uint16_t n) {
        uint8_t l[2]; put_be(l, n, 2);
        std::fwrite(l, 1, 2, out); std::fwrite(m, 1, n, out);
        ++written;
    };
    auto seed = [&] {
        std::fwrite(dir.data(), 1, dir.size(), out);
        uint64_t n_dir = dir.size();
        uint8_t a[36];
        for (const auto& [ref, r] : book) {
            if (r.shares == 0) continue;
            a[0] = 'A'; put_be(a + 1, r.locate, 2); put_be(a + 3, 0, 2); put_be(a + 5, from, 6);
            put_be(a + 11, ref, 8); a[19] = r.side; put_be(a + 20, r.shares, 4);
            std::memcpy(a + 24, r.stock.data(), 8); put_be(a + 32, r.price, 4);
            write_framed(a, 36);
        }
        std::fprintf(stderr, "seed: %llu directory bytes, %llu resting orders\n",
                     (unsigned long long)n_dir, (unsigned long long)written);
        seeded = true;
    };

    while (std::fread(len_buf, 1, 2, stdin) == 2) {
        const uint16_t n = uint16_t(be(len_buf, 2));
        if (n == 0 || std::fread(msg, 1, n, stdin) != n) break;
        ++total;
        const uint8_t type = msg[0];
        const uint64_t ts = n >= 11 ? be(msg + 5, 6) : 0;
        if (ts >= to) break;
        if (ts >= from) {
            if (!seeded) seed();
            write_framed(msg, n);
            ++in_window[type];
            window_bytes += n;
            continue;
        }
        // Before the window: keep the directory and replay the book.
        switch (type) {
        case 'R': dir.insert(dir.end(), len_buf, len_buf + 2); dir.insert(dir.end(), msg, msg + n); break;
        case 'A': case 'F': {
            Resting r{uint16_t(be(msg + 1, 2)), msg[19], uint32_t(be(msg + 20, 4)), uint32_t(be(msg + 32, 4)), {}};
            std::memcpy(r.stock.data(), msg + 24, 8);
            book[be(msg + 11, 8)] = r;
            break;
        }
        case 'E': case 'C': case 'X': {
            auto it = book.find(be(msg + 11, 8));
            const uint32_t q = uint32_t(be(msg + 19, 4));
            if (it != book.end()) { it->second.shares = it->second.shares > q ? it->second.shares - q : 0; if (!it->second.shares) book.erase(it); }
            break;
        }
        case 'D': book.erase(be(msg + 11, 8)); break;
        case 'U': {
            auto it = book.find(be(msg + 11, 8));
            if (it != book.end()) {
                Resting r = it->second;
                book.erase(it);
                r.shares = uint32_t(be(msg + 27, 4));
                r.price = uint32_t(be(msg + 31, 4));
                book[be(msg + 19, 8)] = r;
            }
            break;
        }
        default: break;
        }
    }
    if (!seeded) seed();
    std::fclose(out);
    std::fprintf(stderr, "read %llu messages, wrote %llu (window %llu bytes)\nwindow types:",
                 (unsigned long long)total, (unsigned long long)written, (unsigned long long)window_bytes);
    for (int t = 0; t < 256; ++t)
        if (in_window[t]) std::fprintf(stderr, " %c=%llu", t, (unsigned long long)in_window[t]);
    std::fprintf(stderr, "\n");
    return 0;
}
