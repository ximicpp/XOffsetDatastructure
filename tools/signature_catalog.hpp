#ifndef XOFFSET_SIGNATURE_CATALOG_HPP
#define XOFFSET_SIGNATURE_CATALOG_HPP

#include "signature_type_list.hpp"

#include "../xoffsetdatastructure.hpp"

namespace xoffset::signature::catalog {

enum class WireState : std::uint32_t {
    offline = 0,
    online = 1,
    archived = 2,
};

struct ScalarRecord {
    std::int32_t id{0};
    std::uint64_t ticks{0};
    double score{0.0};
};

struct NestedValue {
    template <typename Alloc>
    explicit NestedValue(const Alloc& alloc)
        : name(alloc), tags(alloc) {}

    std::int32_t id{0};
    XOffsetDatastructure::XString name;
    XOffsetDatastructure::XVector<std::uint16_t> tags;
};

struct WireCatalogRoot {
    template <typename Alloc>
    explicit WireCatalogRoot(const Alloc& alloc)
        : title(alloc),
          payload(alloc),
          values(alloc),
          nested(alloc),
          ids(alloc),
          by_id(alloc),
          name_to_id(alloc) {}

    WireState state{WireState::offline};
    std::int32_t matrix[2][3] = {};
    XOffsetDatastructure::XString title;
    XOffsetDatastructure::XBlob payload;
    XOffsetDatastructure::XVector<std::int32_t> values;
    XOffsetDatastructure::XVector<NestedValue> nested;
    XOffsetDatastructure::XSet<std::int32_t> ids;
    XOffsetDatastructure::XMap<std::int32_t, NestedValue> by_id;
    XOffsetDatastructure::XMap<XOffsetDatastructure::XString, std::int32_t> name_to_id;
};

} // namespace xoffset::signature::catalog

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
