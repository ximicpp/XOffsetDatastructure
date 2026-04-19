#ifndef XOFFSET_SIGNATURE_CATALOG_HPP
#define XOFFSET_SIGNATURE_CATALOG_HPP

#include "signature_type_list.hpp"

#include "../examples/game_data.hpp"
#include "../examples/player.hpp"

namespace xoffset::signature {

template <class Exporter>
void add_exported_types(Exporter& exporter) {
    #define XOFFSET_ADD_TYPE(Type, Name) \
        exporter.template add_relocatable<Type>(Name);
    XOFFSET_SIGNATURE_TYPE_LIST(XOFFSET_ADD_TYPE)
    #undef XOFFSET_ADD_TYPE
}

template <class Visitor>
void visit_exported_types(Visitor&& visitor) {
    #define XOFFSET_VISIT_TYPE(Type, Name) \
        visitor.template operator()<Type>(Name);
    XOFFSET_SIGNATURE_TYPE_LIST(XOFFSET_VISIT_TYPE)
    #undef XOFFSET_VISIT_TYPE
}

} // namespace xoffset::signature

#endif // XOFFSET_SIGNATURE_CATALOG_HPP
