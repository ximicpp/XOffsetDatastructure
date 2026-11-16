#include "xoffset_runner.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <span>
#include <stdexcept>
#if !defined(__APPLE__) && !defined(_WIN32)
#include <unistd.h>
#endif

namespace xo = XOffsetDatastructure;
namespace xm = slg::xm;

using LoginOps = slg::XOffsetRunnerT<slg::XoTrustedModel, false>::LoginOps;

template <class Order>
void check_order(const Order& actual, const slg::stdm::March& expected) {
    if (actual.march_id != expected.march_id || actual.owner != expected.owner ||
        actual.from_x != expected.from_x || actual.from_y != expected.from_y ||
        actual.to_x != expected.to_x || actual.to_y != expected.to_y ||
        actual.eta != expected.eta || actual.troops.size() != expected.troops.size())
        throw std::runtime_error("march prefix or troop count changed");

    for (std::size_t i = 0; i < expected.troops.size(); ++i) {
        const auto& a = actual.troops[i];
        const auto& b = expected.troops[i];
        if (a.type != b.type || a.count != b.count || a.wounded != b.wounded)
            throw std::runtime_error("troop fields changed");
    }
}

template <class Ops>
std::span<const std::byte> send(Ops& ops, int round, slg::Rng& rng, int64_t seq) {
    std::size_t size;
    if constexpr (requires { ops.up_encode_order(0, round, rng, seq); })
        size = ops.up_encode_order(0, round, rng, seq);
    else
        size = ops.up_encode([&](auto& order) { slg::fill_order(order, ops.held(0), rng, seq); });
    const auto bytes = ops.up_wire();
    if (bytes.size() != size) throw std::runtime_error("send size disagrees with its wire");
    return std::as_bytes(std::span<const char>(bytes.data(), bytes.size()));
}

int main() {
    try {
        const auto cap = xo::XBuffer::maximum_capacity(1024 * 1024);
        LoginOps ops;
        ops.srv.emplace_back(xo::XBuffer::create<xm::PlayerState>(128 * 1024, cap));
        ops.setup();
        xo::XReceiver<xo::XLink::from_client> receiver(cap);
        constexpr std::array army_counts{0, 1, 2, 3, 4, 5, 8, 15, 16, 17};
        constexpr std::array troop_counts{0, 1, 2, 3, 4, 5, 8, 128};
        constexpr std::array troop_limits{-1, 0, 1, 2, 151, 1034, (std::numeric_limits<int>::max)()};
        for (int shape = 0; shape < 512; ++shape) {
            slg::stdm::PlayerState source;
            source.player_id = -int64_t(shape) - 1;
            for (int a = 0; a < army_counts[shape % army_counts.size()]; ++a) {
                auto& army = source.armies.emplace_back();
                army.x = 123 + a * 37;
                army.y = 357 - a * 13;
                for (int t = 0; t < troop_counts[(shape + a) % troop_counts.size()]; ++t) {
                    auto& troop = army.troops.emplace_back();
                    troop.type = t - a * 10;
                    troop.count = troop_limits[(shape + t) % troop_limits.size()];
                    troop.wounded = a;
                }
            }
            slg::copy_player(ops.srv[0].reset<xm::PlayerState>(), source);
            ops.login_encode(0);
            ops.login_recv(0);
            for (int round = 0; round < 4; ++round) {
                auto seed = slg::mix_seed(20261021, std::uint64_t(shape) * 4 + round);
                if (shape % 32 == 0)
                    seed = (std::numeric_limits<std::uint64_t>::max)() - std::uint64_t(round);
                slg::Rng input(seed), reference(seed);
                slg::stdm::March expected;
                const auto seq = (std::int64_t(shape) << 40) + round;
                slg::fill_order(expected, source, reference, seq);
                const auto wire = send(ops, round, input, seq);
                if (input.state != reference.state)
                    throw std::runtime_error("order consumed a different random stream");
                const auto view = receiver.view<xm::March>(wire);
                auto full = xo::XBuffer::try_load_verified<xm::March>(wire, cap);
                if (!view || !full)
                    throw std::runtime_error("order failed bounds or full verification");
                check_order(view->root(), expected);
                check_order(full->root<xm::March>(), expected);
                const auto digest = slg::query_order(expected);
                if (ops.up_recv() != digest || ops.up_recv_full() != digest)
                    throw std::runtime_error("wire receive disagrees with reference order");
            }
        }
#if !defined(__APPLE__) && !defined(_WIN32)
        const auto page_size = ::sysconf(_SC_PAGESIZE);
        if (page_size <= 0 || !ops.um ||
            ops.um->reservation_limit() > (1u << 20) + static_cast<std::size_t>(page_size))
            throw std::runtime_error("uplink reserved more than the initial relocation arena");
#endif
        std::puts("2048 full-field orders and random streams agree across both receive modes");
        return 0;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "%s\n", e.what());
        return 1;
    }
}
