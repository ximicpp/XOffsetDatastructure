#include "xoffset_runner.hpp"   // slg::xm::MarchOrderPrefix
#include "xoffset_receiver.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <span>
#include <stdexcept>
#include <vector>

namespace xo = XOffsetDatastructure;
namespace xm = slg::xm;

struct FlatCountProbe {
    std::int64_t id{};
    xo::XVector<std::uint32_t> values;
};
XOFFSET_RECORD(FlatCountProbe, id, values)
XOFFSET_REGISTER_SCHEMA_NAME(FlatCountProbe, "slg.flat_count_probe")

template <class Root, class Prefix, std::size_t Field, class Element, xo::detail::FlatRecordFraming Mode,
          class Vector, class Fill, class Generate>
void verify(Vector vector, Fill fill, Generate generate) {
    xo::detail::FlatRecordWriter<Root, Field, Element, Mode> writer;
    if (!writer.wire().empty())
        throw std::runtime_error("unwritten output is not empty");

    const auto cap = xo::XBuffer::maximum_capacity(1 << 20);
    auto buffer = xo::XBuffer::create<Root>(128 * 1024, cap);
    auto& expected = buffer.template root<Root>();
    xo::XReceiver<xo::XLink::from_client> receiver(cap);
    constexpr std::array<std::size_t, 16> lengths{0, 1, 2, 4, 3, 1, 0, 4, 2, 257, 256, 254, 16, 1024, 0, 6};
    for (std::size_t seq = 1; seq <= 256; ++seq) {
        const auto count = lengths[(seq - 1) % lengths.size()];
        const auto prepare = [&](auto& root) {
            fill(root, seq);
            return count;
        };
        const auto element = [&](std::size_t i) noexcept { return generate(i, seq); };
        prepare(expected);
        auto& values = vector(expected);
        values.clear();
        values.reserve(count);
        for (std::size_t i = 0; i < count; ++i)
            values.emplace_back(element(i));
        xo::detail::zero_padding_bytes<Root>(reinterpret_cast<char*>(&expected));
        for (auto& value : values) xo::detail::zero_padding_bytes<Element>(reinterpret_cast<char*>(&value));
        const auto reference = buffer.template save_compact<Root>(xo::XCompactOptions{.slack_div = 0});
        const auto written = writer.template write<Prefix>(prepare, element);
        const auto ref = std::as_bytes(std::span<const char>(reference.data(), reference.size()));
        if (written != reference.size() || !std::ranges::equal(ref, writer.wire())) {
            std::fprintf(stderr, "mode=%d element=%zu seq=%zu count=%zu written=%zu expected=%zu\n",
                         static_cast<int>(Mode), sizeof(Element), seq, count, written, reference.size());
            for (std::size_t i = 0; i < (std::min)(ref.size(), writer.wire().size()); ++i)
                if (ref[i] != writer.wire()[i])
                    std::fprintf(stderr, "%zu: %02x -> %02x\n", i, std::to_integer<int>(ref[i]),
                                 std::to_integer<int>(writer.wire()[i]));
            throw std::runtime_error("output differs from save_compact");
        }
        const auto view = receiver.template view<Root>(writer.wire());
        auto checked = xo::XBuffer::try_load_verified<Root>(writer.wire(), cap);
        if (!view || !checked || vector(view->root()).size() != count ||
            vector(checked->template root<Root>()).size() != count)
            throw std::runtime_error("checked receive rejected output");
    }
    const std::vector<std::byte> before(writer.wire().begin(), writer.wire().end());
    const auto element = [&](std::size_t) noexcept { return Element{}; };
    bool threw = false;
    try {
        writer.template write<Prefix>([](Prefix&) -> std::size_t {
            throw std::runtime_error("preparation failed");
        }, element);
    } catch (const std::runtime_error&) {
        threw = true;
    }
    if (!threw || !std::ranges::equal(before, writer.wire()))
        throw std::runtime_error("failed preparation replaced output");
    threw = false;
    try {
        writer.template write<Prefix>([](Prefix&) {
            return (std::numeric_limits<std::size_t>::max)();
        }, element);
    } catch (const std::exception&) {
        threw = true;
    }
    if (!threw || !std::ranges::equal(before, writer.wire()))
        throw std::runtime_error("oversized input replaced output");
}

template <xo::detail::FlatRecordFraming Mode> void verify_mode() {
    verify<xm::March, xm::MarchOrderPrefix, 7, xm::Troop, Mode>(
        [](auto& x) -> auto& { return x.troops; },
        [](auto& x, std::size_t seq) {
            x.march_id = static_cast<std::int64_t>(seq) << 32;
            x.owner = -static_cast<std::int64_t>(seq);
            x.from_x = -static_cast<std::int32_t>(seq);
            x.from_y = static_cast<std::int32_t>(seq + 17);
            x.to_x = static_cast<std::int32_t>(seq * 3);
            x.to_y = -static_cast<std::int32_t>(seq * 19);
            x.eta = static_cast<std::int32_t>(seq * 11);
        },
        [](std::size_t i, std::size_t seq) noexcept {
            return xm::Troop{std::int32_t(i % 5), std::int32_t(i + seq), -std::int32_t(i % 3)};
        });
    verify<FlatCountProbe, FlatCountProbe, 1, std::uint32_t, Mode>(
        [](auto& x) -> auto& { return x.values; },
        [](auto& x, std::size_t seq) { x.id = static_cast<std::int64_t>(seq) << 32; },
        [](std::size_t i, std::size_t seq) noexcept { return std::uint32_t(i + seq); });
}

int main() {
    try {
        verify_mode<xo::detail::FlatRecordFraming::sized_slots>();
        std::puts("512 compact outputs match save_compact, verified loads and rollback passed");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
