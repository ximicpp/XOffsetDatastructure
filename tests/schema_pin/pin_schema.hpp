// Schema shared by test_schema_pin and the compile-fail pin_mismatch check.
#ifndef XOFFSET_TEST_PIN_SCHEMA_HPP
#define XOFFSET_TEST_PIN_SCHEMA_HPP

#include "xoffsetdatastructure.hpp"

#include <cstdint>

namespace pin {
enum class Kind : std::uint8_t { a, b, c };
struct Inner {
    std::uint32_t id{};
    bool active{};
    XOffsetDatastructure::XString name;
};
struct Root {
    std::uint64_t epoch{};
    XOffsetDatastructure::XString title;
    XOffsetDatastructure::XVector<Inner> items;
    XOffsetDatastructure::XSet<std::int32_t> keys;
    XOffsetDatastructure::XMap<XOffsetDatastructure::XString, std::uint64_t> attrs;
    Kind kinds[3]{};
    double weight{};
    std::int16_t level{};
};
} // namespace pin
XOFFSET_RECORD_DERIVED(pin::Inner, (), id, active, name)
XOFFSET_RECORD_DERIVED(pin::Root, (), epoch, title, items, keys, attrs, kinds, weight, level)

XOFFSET_REGISTER_SCHEMA_NAME(pin::Root, "test.schema_pin.root")

#endif
