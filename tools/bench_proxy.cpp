// Growth policy part 2 gate: proxies (XRef) and pin scopes (edit) against
// plain references, on the workloads of the design's fast-proxy prototype
// (/mnt/project-files/grow-policy-20261110/fastproxy/).
//
//   raw    plain references into an Automatic buffer (64 MiB reservation,
//          never moves): today's fastest write path
//   proxy  XRef into a Relocating buffer (1 MiB reservation, moves when full)
//   edit   plain references inside buf.edit on a Relocating buffer
//
// Each sample builds a fresh buffer and times only the operations (the
// timing rule: data writes only). Best of 7 rounds; ns per operation.
// Gate: containers <= 1.15x raw, per-field record build <= 1.2x, edit <= 1.02x.
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <random>
#include <string>
#include <vector>
#include "xoffsetdatastructure.hpp"
using namespace XOffsetDatastructure;

struct Player {
    std::int32_t id{0};
    std::int32_t level{0};
    XString name;
    XVector<std::int32_t> items;
};
XOFFSET_RECORD(Player, id, level, name, items)
struct Root {
    XVector<std::int32_t> ints;
    XVector<Player> players;
    XMap<std::int32_t, std::int32_t> map;
};
XOFFSET_RECORD(Root, ints, players, map)
XOFFSET_REGISTER_SCHEMA_NAME(Root, "bench.proxy.Root")

namespace {
std::vector<std::string> g_names;
volatile std::int64_t g_sink;

#define NOINLINE __attribute__((noinline))

// ---- push_back loop
NOINLINE void ints_raw(Root& r, int n) {
    for (int i = 0; i < n; ++i) r.ints.push_back(i);
}
NOINLINE void ints_proxy(XRef<Root> r, int n) {
    auto v = r.field<&Root::ints>();
    for (int i = 0; i < n; ++i) v.push_back(i);
}
NOINLINE void opaque(int) { asm volatile(""); }
NOINLINE void ints_raw_call(Root& r, int n) {
    for (int i = 0; i < n; ++i) {
        r.ints.push_back(i);
        opaque(i);
    }
}
NOINLINE void ints_proxy_call(XRef<Root> r, int n) {
    auto v = r.field<&Root::ints>();
    for (int i = 0; i < n; ++i) {
        v.push_back(i);
        opaque(i);
    }
}

// ---- map insert
NOINLINE void map_raw(Root& r, const std::int32_t* keys, int n) {
    for (int i = 0; i < n; ++i) r.map.try_emplace(keys[i], i);
}
NOINLINE void map_proxy(XRef<Root> r, const std::int32_t* keys, int n) {
    auto m = r.field<&Root::map>();
    for (int i = 0; i < n; ++i) m.try_emplace(keys[i], i);
}

// ---- Player build, field by field
NOINLINE void players_raw(Root& r, int n) {
    for (int i = 0; i < n; ++i) {
        Player& p = r.players.emplace_back();
        p.id = i;
        p.level = i & 63;
        p.name = g_names[i];
        for (int k = 0; k < 4; ++k) p.items.push_back(i + k);
    }
}
NOINLINE void players_proxy(XRef<Root> r, int n) {
    auto players = r.field<&Root::players>();
    for (int i = 0; i < n; ++i) {
        auto p = players.emplace_back();
        p.field<&Player::id>() = i;
        p.field<&Player::level>() = i & 63;
        p.field<&Player::name>() = g_names[i];
        auto items = p.field<&Player::items>();
        for (int k = 0; k < 4; ++k) items.push_back(i + k);
    }
}

// ---- reads (report only)
NOINLINE std::int64_t levels_raw(const Root& r) {
    std::int64_t s = 0;
    for (const Player& p : r.players) s += p.level + p.items.size();
    return s;
}
NOINLINE std::int64_t levels_proxy(XRef<Root> r) {
    std::int64_t s = 0;
    for (auto p : r.field<&Root::players>()) s += p.field<&Player::level>() + p.field<&Player::items>().size();
    return s;
}

// raw / proxy_same: a pooled 64 MiB Automatic buffer (warm pages, never
// moves), as the prototype's baseline; proxy / edit: a Relocating buffer.
// raw / proxy / edit: a pooled 64 MiB Automatic buffer (warm pages, never
// moves), the prototype's baseline: what the access path itself costs.
// relocating: the same proxy / edit on a Relocating buffer (1 MiB first
// reservation, moves when full; the edit scope is told the growth).
enum class Mode { pooled, relocating };
XBufferPool& pool() {
    static XBufferPool p({16, 16ull * (64u << 20), 64u << 20});
    return p;
}
XBuffer make(Mode mode) {
    if (mode == Mode::pooled)
        return XBuffer::create<Root>(
            256, XBuffer::maximum_capacity(64u << 20).with_growth(XGrowPolicy::Automatic).with_pool(pool()));
    return XBuffer::create<Root>(256, XBuffer::relocating());
}

template <class Op>
double run(Mode mode, int n, int iters, Op&& op) {
    double best = 1e300;
    for (int round = 0; round < 7; ++round) {
        double total = 0;
        for (int k = 0; k < iters; ++k) {
            XBuffer b = make(mode);
            auto t0 = std::chrono::steady_clock::now();
            op(b);
            auto t1 = std::chrono::steady_clock::now();
            total += std::chrono::duration<double, std::nano>(t1 - t0).count();
            if (!b.ok()) {
                std::fprintf(stderr, "buffer failed: %s\n", to_string(b.error()));
                std::exit(1);
            }
            g_sink = b.root<Root>().ints.size() + b.root<Root>().players.size() + b.root<Root>().map.size();
        }
        best = std::min(best, total / iters / n);
    }
    return best;
}

int g_fail = 0;
void row(const char* name, int n, double raw, double proxy, double edit, double rproxy, double redit, double gate) {
    const double rp = proxy / raw, re = edit / raw;
    const bool pass = rp <= gate && re <= 1.02;
    std::printf("n=%-6d %-20s raw %7.2f | proxy %7.2f %.3fx (gate %.2f) | edit %7.2f %.3fx (gate 1.02) | "
                "relocating: proxy %.3fx, edit %.3fx | %s\n",
                n, name, raw, proxy, rp, gate, edit, re, rproxy / raw, redit / raw, pass ? "pass" : "OVER");
    if (!pass) ++g_fail;
}
} // namespace

int main(int argc, char** argv) {
    const bool quick = argc > 1 && std::string(argv[1]) == "quick";
    for (int i = 0; i < 100000; ++i) g_names.push_back("player_" + std::to_string(i * 7919 % 100000));
    const int sizes[2] = {1000, 16000};
    for (int n : sizes) {
        std::vector<std::int32_t> asc(n), rnd(n);
        for (int i = 0; i < n; ++i) asc[i] = i;
        std::mt19937 rng(42);
        for (int i = 0; i < n; ++i) rnd[i] = std::int32_t(rng() % (4u * n));
        const int iters = (n <= 1000 ? 400 : 30) / (quick ? 4 : 1);
        auto three = [&](const char* name, double gate, auto raw_op, auto proxy_op) {
            const double raw = run(Mode::pooled, n, iters, [&](XBuffer& b) { raw_op(b.root<Root>()); });
            const double proxy = run(Mode::pooled, n, iters, [&](XBuffer& b) { proxy_op(b.ref<Root>()); });
            const double edit = run(Mode::pooled, n, iters, [&](XBuffer& b) {
                b.edit<Root>([&](Root& r) { raw_op(r); });
            });
            const double rproxy = run(Mode::relocating, n, iters, [&](XBuffer& b) { proxy_op(b.ref<Root>()); });
            const double redit = run(Mode::relocating, n, iters, [&](XBuffer& b) {
                b.edit<Root>([&](Root& r) { raw_op(r); }, XBufferCore::size_type(n) * 96u);
            });
            row(name, n, raw, proxy, edit, rproxy, redit, gate);
        };
        three("ints push_back", 1.15, [&](Root& r) { ints_raw(r, n); }, [&](XRef<Root> r) { ints_proxy(r, n); });
        three("ints push_back+call", 1.15, [&](Root& r) { ints_raw_call(r, n); },
              [&](XRef<Root> r) { ints_proxy_call(r, n); });
        three("map insert asc", 1.15, [&](Root& r) { map_raw(r, asc.data(), n); },
              [&](XRef<Root> r) { map_proxy(r, asc.data(), n); });
        three("map insert rnd", 1.15, [&](Root& r) { map_raw(r, rnd.data(), n); },
              [&](XRef<Root> r) { map_proxy(r, rnd.data(), n); });
        three("players build", 1.20, [&](Root& r) { players_raw(r, n); },
              [&](XRef<Root> r) { players_proxy(r, n); });
        {   // reads: report only (the design keeps bulk reads in read scopes)
            auto b = make(Mode::relocating);
            players_raw(b.root<Root>(), n);
            auto time = [&](auto&& f) {
                double best = 1e300;
                for (int round = 0; round < 7; ++round) {
                    auto t0 = std::chrono::steady_clock::now();
                    for (int k = 0; k < iters; ++k) g_sink = f();
                    auto t1 = std::chrono::steady_clock::now();
                    best = std::min(best, std::chrono::duration<double, std::nano>(t1 - t0).count() / iters / n);
                }
                return best;
            };
            const double raw = time([&] { return levels_raw(b.root<Root>()); });
            const double proxy = time([&] { return levels_proxy(b.ref<Root>()); });
            const double scope = time([&] { return b.read<Root>([](const Root& r) { return levels_raw(r); }); });
            std::printf("n=%-6d %-22s raw %7.2f  proxy %7.2f (%.3fx, report only)  read %7.2f (%.3fx)\n", n,
                        "players read", raw, proxy, proxy / raw, scope, scope / raw);
        }
    }
    return g_fail ? 1 : 0;
}
