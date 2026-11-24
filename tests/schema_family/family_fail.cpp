// Must not compile: XOFFSET_SCHEMA_FAMILY(Next, Base) where Next is not an
// add-only successor of Base. One case per build, chosen by FAMILY_FAIL_<CASE>;
// tests/CMakeLists.txt checks each diagnostic. The cases that hold are in
// tests/test_schema_family.cpp.
#include "../../xoffsetdatastructure.hpp"

#include <cstdint>

using namespace XOffsetDatastructure;

namespace base {
struct Item { std::int32_t id{}; std::int32_t count{}; };
struct Hero {
    std::int32_t id{};
    std::int32_t level{};
    XString name;
    XVector<Item> bag;
    XVector<XSpare<16>> later;
    std::uint8_t flag{};          // three bytes of padding follow
    std::int32_t power{};
    XSpare<16> spare{};
};
} // namespace base
XOFFSET_RECORD(base::Item, id, count)
XOFFSET_RECORD(base::Hero, id, level, name, bag, later, flag, power, spare)

namespace next {
#if defined(FAMILY_FAIL_PADDING)          // a new field in what was padding
struct Item { std::int32_t id{}; std::int32_t count{}; };
struct Hero {
    std::int32_t id{}; std::int32_t level{}; XString name; XVector<Item> bag; XVector<XSpare<16>> later;
    std::uint8_t flag{}; std::uint8_t side{};
    std::int32_t power{}; XSpare<16> spare{};
};
#elif defined(FAMILY_FAIL_REMOVED)        // a field turned into spare bytes
struct Item { std::int32_t id{}; std::int32_t count{}; };
struct Hero {
    std::int32_t id{}; std::int32_t level{}; XString name; XVector<Item> bag; XVector<XSpare<16>> later;
    std::uint8_t flag{}; XSpare<7> was_power{}; XSpare<16> spare{};
};
#elif defined(FAMILY_FAIL_RENAMED)        // a field under another name
struct Item { std::int32_t id{}; std::int32_t count{}; };
struct Hero {
    std::int32_t id{}; std::int32_t rank{}; XString name; XVector<Item> bag; XVector<XSpare<16>> later;
    std::uint8_t flag{}; std::int32_t power{}; XSpare<16> spare{};
};
#elif defined(FAMILY_FAIL_TYPE)           // int32 -> uint32
struct Item { std::int32_t id{}; std::int32_t count{}; };
struct Hero {
    std::int32_t id{}; std::uint32_t level{}; XString name; XVector<Item> bag; XVector<XSpare<16>> later;
    std::uint8_t flag{}; std::int32_t power{}; XSpare<16> spare{};
};
#elif defined(FAMILY_FAIL_CONTAINER)      // a string taken from spare bytes
struct Item { std::int32_t id{}; std::int32_t count{}; };
struct Hero {
    std::int32_t id{}; std::int32_t level{}; XString name; XVector<Item> bag; XVector<XSpare<16>> later;
    std::uint8_t flag{}; std::int32_t power{}; XString title; XSpare<4> spare{};
};
#elif defined(FAMILY_FAIL_SPARE_PADDING)  // spare bytes left as padding
struct Item { std::int32_t id{}; std::int32_t count{}; };
struct Hero {
    std::int32_t id{}; std::int32_t level{}; XString name; XVector<Item> bag; XVector<XSpare<16>> later;
    std::uint8_t flag{}; std::int32_t power{};
    std::int32_t guild{}; std::uint8_t mood{}; std::int32_t vip{}; std::int32_t rank{};   // 3 spare bytes padding
};
#elif defined(FAMILY_FAIL_MOVED)          // two fields swapped
struct Item { std::int32_t id{}; std::int32_t count{}; };
struct Hero {
    std::int32_t level{}; std::int32_t id{}; XString name; XVector<Item> bag; XVector<XSpare<16>> later;
    std::uint8_t flag{}; std::int32_t power{}; XSpare<16> spare{};
};
#elif defined(FAMILY_FAIL_ELEMENT)        // a vector element grows: 8 -> 12 bytes
struct Item { std::int32_t id{}; std::int32_t count{}; std::int32_t extra{}; };
struct Hero {
    std::int32_t id{}; std::int32_t level{}; XString name; XVector<Item> bag; XVector<XSpare<16>> later;
    std::uint8_t flag{}; std::int32_t power{}; XSpare<16> spare{};
};
#elif defined(FAMILY_FAIL_SIZE)           // appended at the end: the record grows
struct Item { std::int32_t id{}; std::int32_t count{}; };
struct Hero {
    std::int32_t id{}; std::int32_t level{}; XString name; XVector<Item> bag; XVector<XSpare<16>> later;
    std::uint8_t flag{}; std::int32_t power{}; XSpare<16> spare{}; std::int32_t extra{};
};
#elif defined(FAMILY_FAIL_LIST)           // the kept list gets a record with a container
struct Item { std::int32_t id{}; std::int32_t count{}; };
struct Rune { XString name; std::int32_t level{}; };
struct Hero {
    std::int32_t id{}; std::int32_t level{}; XString name; XVector<Item> bag; XVector<Rune> later;
    std::uint8_t flag{}; std::int32_t power{}; XSpare<16> spare{};
};
#else
#error "define one FAMILY_FAIL_<CASE>"
#endif
} // namespace next

#if defined(FAMILY_FAIL_LIST)
XOFFSET_RECORD(next::Rune, name, level)
#endif
#if defined(FAMILY_FAIL_ELEMENT)
XOFFSET_RECORD(next::Item, id, count, extra)
#else
XOFFSET_RECORD(next::Item, id, count)
#endif
#if defined(FAMILY_FAIL_PADDING)
XOFFSET_RECORD(next::Hero, id, level, name, bag, later, flag, side, power, spare)
#elif defined(FAMILY_FAIL_REMOVED)
XOFFSET_RECORD(next::Hero, id, level, name, bag, later, flag, was_power, spare)
#elif defined(FAMILY_FAIL_RENAMED)
XOFFSET_RECORD(next::Hero, id, rank, name, bag, later, flag, power, spare)
#elif defined(FAMILY_FAIL_CONTAINER)
XOFFSET_RECORD(next::Hero, id, level, name, bag, later, flag, power, title, spare)
#elif defined(FAMILY_FAIL_SPARE_PADDING)
XOFFSET_RECORD(next::Hero, id, level, name, bag, later, flag, power, guild, mood, vip, rank)
#elif defined(FAMILY_FAIL_MOVED)
XOFFSET_RECORD(next::Hero, level, id, name, bag, later, flag, power, spare)
#elif defined(FAMILY_FAIL_SIZE)
XOFFSET_RECORD(next::Hero, id, level, name, bag, later, flag, power, spare, extra)
#else
XOFFSET_RECORD(next::Hero, id, level, name, bag, later, flag, power, spare)
#endif

XOFFSET_SCHEMA_FAMILY(next::Hero, base::Hero)

int main() { return 0; }
