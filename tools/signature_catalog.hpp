#ifndef XOFFSET_SIGNATURE_CATALOG_HPP
#define XOFFSET_SIGNATURE_CATALOG_HPP

#include "../examples/game_data.hpp"
#include "../examples/player.hpp"

namespace xoffset::signature {

template <class Exporter>
void add_exported_types(Exporter& exporter) {
    exporter.template add_relocatable<Player>("Player");
    exporter.template add_relocatable<Item>("Item");
    exporter.template add_relocatable<GameData>("GameData");
}

} // namespace xoffset::signature

#endif // XOFFSET_SIGNATURE_CATALOG_HPP
