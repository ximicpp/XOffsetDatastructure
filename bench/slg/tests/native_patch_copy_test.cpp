#include "xoffset_runner.hpp"
#include <stdexcept>
#include <cstdio>

template <class Patch>
uint64_t patch_digest(const Patch& patch) {
    slg::Digest d;
    d.u(patch.player_id);
    d.u(patch.seq);
    slg::dig_vec(d, patch.army_index, [](auto& hash, const auto& id) { hash.u(id); });
    slg::dig_vec(d, patch.armies, [](auto& hash, const auto& army) { slg::dig_army(hash, army); });
    slg::dig_map(d, patch.items);
    d.u(patch.city_changed);
    slg::dig_city(d, patch.city);
    return d.h;
}

int main() {
    namespace xo = XOffsetDatastructure;
    namespace xm = slg::xm;
    using xo::XBuffer;
    try {
        const auto cap = XBuffer::maximum_capacity(1024 * 1024).with_growth(xo::XGrowPolicy::Automatic);
        auto source = XBuffer::create<xm::PlayerState>(16 * 1024, cap);
        auto output = XBuffer::create<xm::PlayerPatch>(4096, cap);
        std::string wire;
        for (int seq = 0; seq < 256; ++seq) {
            auto original = slg::make_player(20261116 + seq, 17 + seq);
            auto& player = source.reset<xm::PlayerState>();
            slg::copy_player(player, original);
            slg::SelfDirty dirty;
            for (std::size_t i = 0; i < original.armies.size(); ++i)
                if ((i + seq) % 3 != 0) dirty.armies.push_back(int32_t(i));
            for (const auto& [key, value] : original.items)
                if ((key + seq) % 5 == 0) dirty.items.push_back(key);
            dirty.city = seq % 2 != 0;
            slg::stdm::PlayerPatch expected;
            slg::fill_patch(expected, original, dirty, seq);
            auto& patch = output.reset<xm::PlayerPatch>();
            slg::fill_patch(patch, player, dirty, seq);
            if (patch_digest(expected) != patch_digest(patch))
                throw std::runtime_error("native copy changed a patch field");
            output.save_verified_into<xm::PlayerPatch>(wire);
            auto checked = XBuffer::try_load_verified<xm::PlayerPatch>(wire, cap);
            if (!checked || patch_digest(expected) != patch_digest(checked->root<xm::PlayerPatch>()))
                throw std::runtime_error("full receive changed a patch field");
        }
        std::puts("256 full-field patch comparisons and verified reloads passed");
    } catch (const std::exception& e) {
        std::fprintf(stderr, "%s\n", e.what());
        return 1;
    }
}
