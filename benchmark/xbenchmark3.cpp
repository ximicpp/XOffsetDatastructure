#include "xtypes.hpp"
#include "xtypeholder.hpp"
#include <string>
#include <vector>
#include <stdint.h>
#include <chrono>
#include "test_generated.h"
// #include "test.pb.h"
#include "cista.h"
#include <typeinfo>
#include "test.capnp.h"
#include "character.capnp.h"
#include "capnp/message.h"
#include "capnp/serialize.h"
#include "zpp_bits.h"
#include <random>
#include <msgpack.hpp>
#include "zlibcompressor.hpp"
#include <fstream>
#include "character_generated.h"
#include <ctime>

constexpr bool WriteToFile = false;

namespace benchmark3
{
    struct Result
    {
        Result(std::string name, std::string version, size_t size, size_t compressedSize, int64_t time, int64_t time1 = 0, int64_t time2 = 0, int64_t time3 = 0)
            : name(name), version(version), size(size), compressedSize(compressedSize), time(time / 1000000.), time1(time1 / 1000000.), time2(time2 / 1000000.), time3(time3 / 1000000.)
        {
        }
        std::string name;
        std::string version;
        size_t size = 0;
        size_t compressedSize = 0;
        double time = 0;
        double time1 = 0;
        double time2 = 0;
        double time3 = 0;
        void print() const
        {
            std::cout << std::setw(20) << name << " " << std::setw(10) << version << " " << std::setw(10) << size << " " << std::setw(10) << compressedSize << " " << std::setw(10) << time << "ms " << std::setw(10) << time1 << "ms " << std::setw(10) << time2 << "ms " << std::setw(10) << time3 << "ms " << std::endl;
        }
    };
}

namespace benchmark3_offsetdatastructure
{
    struct Skill
    {
        uint32_t id;
        uint32_t level;
        bool operator==(const Skill &other) const
        {
            return std::tie(id, level) == std::tie(other.id, other.level);
        }
        bool operator!=(const Skill &other) const
        {
            return !(*this == other);
        }
    };
    struct Item
    {
        uint32_t id;
        uint32_t number;
        uint64_t uuid;
        uint64_t timestamp;
        bool operator==(const Item &other) const
        {
            return std::tie(id, uuid, number, timestamp) == std::tie(other.id, other.uuid, other.number, other.timestamp);
        }
        bool operator!=(const Item &other) const
        {
            return !(*this == other);
        }
    };

    struct Equip : public Item
    {
        template <typename Allocator>
        Equip(Allocator allocator) : attributes(allocator) {}
        uint32_t level;
        XOffsetDatastructure::XVector<float> attributes;
        bool operator==(const Equip &other) const
        {
            return std::tie(static_cast<const Item &>(*this), level, attributes) == std::tie(static_cast<const Item &>(other), other.level, other.attributes);
        }
        bool operator!=(const Equip &other) const
        {
            return !(*this == other);
        }
    };

    struct Position
    {
        float x;
        float y;
        float z;
        bool operator==(const Position &other) const
        {
            return std::tie(x, y, z) == std::tie(other.x, other.y, other.z);
        }
        bool operator!=(const Position &other) const
        {
            return !(*this == other);
        }
    };

    struct Character
    {
        template <typename Allocator>
        Character(Allocator allocator) : name(allocator), items(allocator), path(allocator), attributes(allocator), equips(allocator), skills(allocator) {}
        uint64_t id;
        XOffsetDatastructure::XString name;
        uint32_t level;
        float healthpoint;
        float manapoint;
        float speed;
        Position pos;
        XOffsetDatastructure::XMap<uint64_t, Item> items;
        XOffsetDatastructure::XVector<Position> path;
        XOffsetDatastructure::XVector<float> attributes;
        XOffsetDatastructure::XVector<Equip> equips;
        XOffsetDatastructure::XVector<Skill> skills;
        bool operator==(const Character &other) const
        {
            return std::tie(id, name, level, healthpoint, manapoint, speed, pos, items, path, attributes, equips, skills) == std::tie(other.id, other.name, other.level, other.healthpoint, other.manapoint, other.speed, other.pos, other.items, other.path, other.attributes, other.equips, other.skills);
        }
        bool operator!=(const Character &other) const
        {
            return !(*this == other);
        }
    };
}

namespace benchmark3_msgpack
{
    struct Skill
    {
        uint32_t id;
        uint32_t level;
        bool operator==(const Skill &other) const
        {
            return std::tie(id, level) == std::tie(other.id, other.level);
        }
        bool operator!=(const Skill &other) const
        {
            return !(*this == other);
        }
        MSGPACK_DEFINE(id, level);
    };
    struct Item
    {
        uint32_t id;
        uint32_t number;
        uint64_t uuid;
        uint64_t timestamp;
        bool operator==(const Item &other) const
        {
            return std::tie(id, uuid, number, timestamp) == std::tie(other.id, other.uuid, other.number, other.timestamp);
        }
        bool operator!=(const Item &other) const
        {
            return !(*this == other);
        }
        MSGPACK_DEFINE(id, uuid, number, timestamp);
    };

    struct Equip : public Item
    {
        uint32_t level;
        std::vector<float> attributes;
        bool operator==(const Equip &other) const
        {
            return std::tie(static_cast<const Item &>(*this), level, attributes) == std::tie(static_cast<const Item &>(other), other.level, other.attributes);
        }
        bool operator!=(const Equip &other) const
        {
            return !(*this == other);
        }
        MSGPACK_DEFINE(id, uuid, number, timestamp, level, attributes);
    };

    struct Position
    {
        float x;
        float y;
        float z;
        bool operator==(const Position &other) const
        {
            return std::tie(x, y, z) == std::tie(other.x, other.y, other.z);
        }
        bool operator!=(const Position &other) const
        {
            return !(*this == other);
        }
        MSGPACK_DEFINE(x, y, z);
    };

    struct Character
    {
        uint64_t id;
        std::string name;
        uint32_t level;
        float healthpoint;
        float manapoint;
        float speed;
        Position pos;
        std::map<uint64_t, Item> items;
        std::vector<Position> path;
        std::vector<float> attributes;
        std::vector<Equip> equips;
        std::vector<Skill> skills;
        bool operator==(const Character &other) const
        {
            return std::tie(id, name, level, healthpoint, manapoint, speed, pos, items, path, attributes, equips, skills) == std::tie(other.id, other.name, other.level, other.healthpoint, other.manapoint, other.speed, other.pos, other.items, other.path, other.attributes, other.equips, other.skills);
        }
        bool operator!=(const Character &other) const
        {
            return !(*this == other);
        }

        MSGPACK_DEFINE(id, name, level, healthpoint, manapoint, speed, pos, path, attributes, items, equips, skills);
    };
}

namespace benchmark3_zppbits
{
    struct Skill
    {
        uint32_t id;
        uint32_t level;
        bool operator==(const Skill &other) const
        {
            return std::tie(id, level) == std::tie(other.id, other.level);
        }
        bool operator!=(const Skill &other) const
        {
            return !(*this == other);
        }
    };
    struct Item
    {
        uint32_t id;
        uint32_t number;
        uint64_t uuid;
        uint64_t timestamp;
        bool operator==(const Item &other) const
        {
            return std::tie(id, uuid, number, timestamp) == std::tie(other.id, other.uuid, other.number, other.timestamp);
        }
        bool operator!=(const Item &other) const
        {
            return !(*this == other);
        }
    };

    struct Equip
    {
        uint32_t id;
        uint32_t number;
        uint64_t uuid;
        uint64_t timestamp;
        uint32_t level;
        std::vector<float> attributes;
        bool operator==(const Equip &other) const
        {
            return std::tie(id, uuid, number, timestamp, level, attributes) == std::tie(other.id, other.uuid, other.number, other.timestamp, other.level, other.attributes);
        }
        bool operator!=(const Equip &other) const
        {
            return !(*this == other);
        }
    };

    struct Position
    {
        float x;
        float y;
        float z;
        bool operator==(const Position &other) const
        {
            return std::tie(x, y, z) == std::tie(other.x, other.y, other.z);
        }
        bool operator!=(const Position &other) const
        {
            return !(*this == other);
        }
    };

    struct Character
    {
        uint64_t id;
        std::string name;
        uint32_t level;
        float healthpoint;
        float manapoint;
        float speed;
        Position pos;
        std::map<uint64_t, Item> items;
        std::vector<Position> path;
        std::vector<float> attributes;
        std::vector<Equip> equips;
        std::vector<Skill> skills;
        bool operator==(const Character &other) const
        {
            return std::tie(id, name, level, healthpoint, manapoint, speed, pos, items, path, attributes, equips, skills) == std::tie(other.id, other.name, other.level, other.healthpoint, other.manapoint, other.speed, other.pos, other.items, other.path, other.attributes, other.equips, other.skills);
        }
        bool operator!=(const Character &other) const
        {
            return !(*this == other);
        }
    };
}

namespace benchmark3_cistaraw
{
    struct Skill
    {
        uint32_t id;
        uint32_t level;
        bool operator==(const Skill &other) const
        {
            return std::tie(id, level) == std::tie(other.id, other.level);
        }
        bool operator!=(const Skill &other) const
        {
            return !(*this == other);
        }
    };

    struct Item
    {
        uint32_t id;
        uint32_t number;
        uint64_t uuid;
        uint64_t timestamp;
        bool operator==(const Item &other) const
        {
            return std::tie(id, uuid, number, timestamp) == std::tie(other.id, other.uuid, other.number, other.timestamp);
        }
        bool operator!=(const Item &other) const
        {
            return !(*this == other);
        }
    };

    struct Equip
    {
        uint32_t id;
        uint32_t number;
        uint64_t uuid;
        uint64_t timestamp;
        uint32_t level;
        cista::raw::vector<float> attributes;
        bool operator==(const Equip &other) const
        {
            return std::tie(id, uuid, number, timestamp, level, attributes) == std::tie(other.id, other.uuid, other.number, other.timestamp, other.level, other.attributes);
        }
        bool operator!=(const Equip &other) const
        {
            return !(*this == other);
        }
    };

    struct Position
    {
        float x;
        float y;
        float z;
        bool operator==(const Position &other) const
        {
            return std::tie(x, y, z) == std::tie(other.x, other.y, other.z);
        }
        bool operator!=(const Position &other) const
        {
            return !(*this == other);
        }
    };

    struct Character
    {
        uint64_t id;
        cista::raw::string name;
        uint32_t level;
        float healthpoint;
        float manapoint;
        float speed;
        Position pos;
        cista::raw::hash_map<uint64_t, Item> items;
        cista::raw::vector<Position> path;
        cista::raw::vector<float> attributes;
        cista::raw::vector<Equip> equips;
        cista::raw::vector<Skill> skills;
        bool operator==(const Character &other) const
        {
            return std::tie(id, name, level, healthpoint, manapoint, speed, pos, items, path, attributes, equips, skills) == std::tie(other.id, other.name, other.level, other.healthpoint, other.manapoint, other.speed, other.pos, other.items, other.path, other.attributes, other.equips, other.skills);
        }
        bool operator!=(const Character &other) const
        {
            return !(*this == other);
        }
    };
}

namespace benchmark3_cistaoffset
{
    struct Skill
    {
        uint32_t id;
        uint32_t level;
        bool operator==(const Skill &other) const
        {
            return std::tie(id, level) == std::tie(other.id, other.level);
        }
        bool operator!=(const Skill &other) const
        {
            return !(*this == other);
        }
    };
    struct Item
    {
        uint32_t id;
        uint32_t number;
        uint64_t uuid;
        uint64_t timestamp;
        bool operator==(const Item &other) const
        {
            return std::tie(id, uuid, number, timestamp) == std::tie(other.id, other.uuid, other.number, other.timestamp);
        }
        bool operator!=(const Item &other) const
        {
            return !(*this == other);
        }
    };

    struct Equip
    {
        uint32_t id;
        uint32_t number;
        uint64_t uuid;
        uint64_t timestamp;
        uint32_t level;
        cista::offset::vector<float> attributes;
        bool operator==(const Equip &other) const
        {
            return std::tie(id, uuid, number, timestamp, level, attributes) == std::tie(other.id, other.uuid, other.number, other.timestamp, other.level, other.attributes);
        }
        bool operator!=(const Equip &other) const
        {
            return !(*this == other);
        }
    };

    struct Position
    {
        float x;
        float y;
        float z;
        bool operator==(const Position &other) const
        {
            return std::tie(x, y, z) == std::tie(other.x, other.y, other.z);
        }
        bool operator!=(const Position &other) const
        {
            return !(*this == other);
        }
    };

    struct Character
    {
        uint64_t id;
        cista::offset::string name;
        uint32_t level;
        float healthpoint;
        float manapoint;
        float speed;
        Position pos;
        cista::offset::hash_map<uint64_t, Item> items;
        cista::offset::vector<Position> path;
        cista::offset::vector<float> attributes;
        cista::offset::vector<Equip> equips;
        cista::offset::vector<Skill> skills;
        bool operator==(const Character &other) const
        {
            return std::tie(id, name, level, healthpoint, manapoint, speed, pos, items, path, attributes, equips, skills) == std::tie(other.id, other.name, other.level, other.healthpoint, other.manapoint, other.speed, other.pos, other.items, other.path, other.attributes, other.equips, other.skills);
        }
        bool operator!=(const Character &other) const
        {
            return !(*this == other);
        }
    };
    class RecordOffset
    {
    public:
        cista::offset::vector<int> ids;
        cista::offset::vector<cista::offset::string> strings;

        bool operator==(const RecordOffset &other)
        {
            return (ids == other.ids && strings == other.strings);
        }

        bool operator!=(const RecordOffset &other)
        {
            return !(*this == other);
        }
    };
}

benchmark3::Result
zppbits_serialization_test3(size_t iterations)
{
    using namespace benchmark3_zppbits;
    std::random_device rd;
    std::mt19937_64 gen(0);
    std::uniform_int_distribution<uint64_t> disInt64;
    std::uniform_int_distribution<uint32_t> disInt32;
    std::uniform_real_distribution<float> disFloat;
    std::string charset = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    std::uniform_int_distribution<int> dis(0, charset.length() - 1);
    std::string randomString;
    for (int i = 0; i < 8; ++i)
    {
        randomString += charset[dis(gen)];
    }

    auto start1 = std::chrono::high_resolution_clock::now();
    Character r1;
    gen.seed(0);
    r1.id = disInt64(gen);
    r1.name = randomString.c_str();
    r1.level = disInt32(gen);
    r1.healthpoint = disFloat(gen);
    r1.manapoint = disFloat(gen);
    r1.speed = disFloat(gen);
    r1.pos = {disFloat(gen), disFloat(gen), disFloat(gen)};
    for (auto i = 0; i < 64; ++i)
    {
        r1.items.emplace(std::piecewise_construct, std::forward_as_tuple(disInt64(gen)), std::forward_as_tuple(disInt32(gen), disInt64(gen), disInt32(gen), disInt64(gen)));
    }
    for (auto i = 0; i < 16; ++i)
    {
        r1.path.emplace_back(disFloat(gen), disFloat(gen), disFloat(gen));
    }
    for (auto i = 0; i < 16; ++i)
    {
        r1.attributes.emplace_back(disFloat(gen));
    }
    for (auto i = 0; i < 8; ++i)
    {
        Equip equip;
        equip.id = disInt32(gen);
        equip.uuid = disInt64(gen);
        equip.number = disInt32(gen);
        equip.timestamp = disInt64(gen);
        equip.level = disInt32(gen);
        equip.attributes = {disFloat(gen), disFloat(gen), disFloat(gen), disFloat(gen), disFloat(gen), disFloat(gen), disFloat(gen), disFloat(gen)};
        r1.equips.push_back(equip);
    }
    for (auto i = 0; i < 4; ++i)
    {
        r1.skills.emplace_back(disInt32(gen), disInt32(gen));
    }
    auto x = disInt32(gen);

    if (x != 1837058832)
        throw std::logic_error("Random number sequence is inconsistent.");
    auto finish1 = std::chrono::high_resolution_clock::now();
    auto duration1 = std::chrono::duration_cast<std::chrono::nanoseconds>(finish1 - start1).count();

    // test write performance
    auto start2 = std::chrono::high_resolution_clock::now();
    for (size_t it = 0; it < iterations; it++)
    {
        r1.level += 1;
        r1.healthpoint += 1;
        r1.manapoint += 1;
        r1.speed += 1;
        r1.pos = {disFloat(gen), disFloat(gen), disFloat(gen)};
        for (auto i = 0; i < 4; ++i)
        {
            Equip equip;
            equip.id = disInt32(gen);
            equip.uuid = disInt64(gen);
            equip.number = disInt32(gen);
            equip.timestamp = disInt64(gen);
            equip.level = disInt32(gen);
            equip.attributes = {disFloat(gen), disFloat(gen), disFloat(gen), disFloat(gen), disFloat(gen), disFloat(gen), disFloat(gen), disFloat(gen)};
            r1.equips.push_back(equip);
            r1.equips.pop_back();
        }
        for (auto i = 0; i < 8; ++i)
        {
            r1.path.emplace_back(disFloat(gen), disFloat(gen), disFloat(gen));
        }
        r1.attributes.emplace_back(disFloat(gen));

        auto itemUuid0 = disInt64(gen);
        Item item = {disInt32(gen), disInt64(gen), disInt32(gen), disInt64(gen)};
        r1.items[itemUuid0] = item;
        r1.items.erase(itemUuid0);

        r1.attributes.pop_back();
        for (auto i = 0; i < 8; ++i)
        {
            r1.path.pop_back();
        }
        r1.speed -= 1;
        r1.manapoint -= 1;
        r1.healthpoint -= 1;
        r1.level -= 1;
    }
    auto finish2 = std::chrono::high_resolution_clock::now();
    auto duration2 = std::chrono::duration_cast<std::chrono::nanoseconds>(finish2 - start2).count();

    std::vector<std::byte> data;
    zpp::bits::out out{data};
    out(r1);
    Character r2;
    zpp::bits::in in{data};
    in(r2);

    auto len1 = out.position();
    auto len2 = in.position();
    assert(len1 == len2);

    if (r1 != r2)
    {
        throw std::logic_error("offsetdatastructure's case: deserialization failed");
    }

    if (WriteToFile)
    {
        std::string functionName = __func__;
        std::string fileName = functionName + ".bin";
        std::ofstream ofs(fileName, std::ios::binary);
        ofs.write(reinterpret_cast<char *>(data.data()), len1);
        ofs.close();
    }

    auto compressedSize = compressAndDecompress(data.data(), len1);

    auto start = std::chrono::high_resolution_clock::now();
    for (size_t it = 0; it < iterations; it++)
    {
        // Record r1;
        // for (size_t i = 0; i < kIntegers.size(); i++) {
        //     r1.ids.push_back(kIntegers[i]);
        // }
        // const char* strval = kStringValue.c_str();
        // for (size_t i = 0; i < kStringsCount; i++) {
        //     r1.strings.emplace_back(strval);
        // }
        std::vector<std::byte> data;
        zpp::bits::out out{data};
        out(r1);
        Character r2;
        zpp::bits::in in{data};
        in(r2);
    }
    auto finish = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(finish - start).count();
    return benchmark3::Result("zpp_bits", "1.0.0", len1, compressedSize, duration, duration1, duration2);
}

benchmark3::Result
flatbuffers_serialization_test3(size_t iterations)
{
    uint64_t duration, duration1, duration2;
    std::size_t originalSize, compressedSize;
    std::random_device rd;
    std::mt19937_64 gen(0);
    std::uniform_int_distribution<uint64_t> disInt64;
    std::uniform_int_distribution<uint32_t> disInt32;
    std::uniform_real_distribution<float> disFloat;
    std::string charset = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    std::uniform_int_distribution<int> dis(0, charset.length() - 1);
    std::string randomString;
    for (int i = 0; i < 8; ++i)
    {
        randomString += charset[dis(gen)];
    }
    {
        auto start1 = std::chrono::high_resolution_clock::now();
        benchmark3_msgpack::Character r1;
        gen.seed(0);
        r1.id = disInt64(gen);
        r1.name = randomString.c_str();
        r1.level = disInt32(gen);
        r1.healthpoint = disFloat(gen);
        r1.manapoint = disFloat(gen);
        r1.speed = disFloat(gen);
        r1.pos = {disFloat(gen), disFloat(gen), disFloat(gen)};
        for (auto i = 0; i < 64; ++i)
        {
            r1.items.emplace(std::piecewise_construct, std::forward_as_tuple(disInt64(gen)), std::forward_as_tuple(disInt32(gen), disInt64(gen), disInt32(gen), disInt64(gen)));
        }
        for (auto i = 0; i < 16; ++i)
        {
            r1.path.emplace_back(disFloat(gen), disFloat(gen), disFloat(gen));
        }
        for (auto i = 0; i < 16; ++i)
        {
            r1.attributes.emplace_back(disFloat(gen));
        }
        for (auto i = 0; i < 8; ++i)
        {
            benchmark3_msgpack::Equip equip;
            equip.id = disInt32(gen);
            equip.uuid = disInt64(gen);
            equip.number = disInt32(gen);
            equip.timestamp = disInt64(gen);
            equip.level = disInt32(gen);
            equip.attributes = {disFloat(gen), disFloat(gen), disFloat(gen), disFloat(gen), disFloat(gen), disFloat(gen), disFloat(gen), disFloat(gen)};
            r1.equips.push_back(equip);
        }
        for (auto i = 0; i < 4; ++i)
        {
            r1.skills.emplace_back(disInt32(gen), disInt32(gen));
        }
        auto x = disInt32(gen);
        if (x != 1837058832)
            throw std::logic_error("Random number sequence is inconsistent.");
        auto finish1 = std::chrono::high_resolution_clock::now();
        duration1 = std::chrono::duration_cast<std::chrono::nanoseconds>(finish1 - start1).count();

        // test write performance
        auto start2 = std::chrono::high_resolution_clock::now();
        // for (size_t it = 0; it < iterations; it++)
        //{
        //     r1.healthpoint += 1;
        //     r1.path.emplace_back(disFloat(gen), disFloat(gen), disFloat(gen));
        //     r1.attributes.emplace_back(disFloat(gen));
        //     auto itemUuid = disInt64(gen);
        //     r1.items.emplace(std::piecewise_construct, std::forward_as_tuple(itemUuid), std::forward_as_tuple(disInt32(gen), disInt64(gen), disInt32(gen), disInt64(gen)));
        //     r1.items.erase(itemUuid);
        //     r1.attributes.pop_back();
        //     r1.path.pop_back();
        //     r1.healthpoint -= 1;
        // }
        auto finish2 = std::chrono::high_resolution_clock::now();
        duration2 = std::chrono::duration_cast<std::chrono::nanoseconds>(finish2 - start2).count();
    }
    {
        flatbuffers::FlatBufferBuilder builder;
        flatbuffers_test::CharacterBuilder characterBuilder(builder);

        std::vector<flatbuffers::Offset<flatbuffers_test::Equip>> equips;
        for (auto i = 0; i < 8; ++i)
        {
            flatbuffers_test::EquipBuilder equipBuilder(builder);
            equipBuilder.add_id(disInt32(gen));
            equipBuilder.add_uuid(disInt64(gen));
            equipBuilder.add_number(disInt32(gen));
            equipBuilder.add_timestamp(disInt64(gen));
            equipBuilder.add_level(disInt32(gen));
            std::vector<float> attributes = {disFloat(gen), disFloat(gen), disFloat(gen), disFloat(gen), disFloat(gen), disFloat(gen), disFloat(gen), disFloat(gen)};
            auto attributesOffset = builder.CreateVector(attributes);
            equipBuilder.add_attributes(attributesOffset);
            flatbuffers::Offset<flatbuffers_test::Equip> equipOffset = equipBuilder.Finish();
            equips.push_back(equipOffset);
        }
        auto equipsOffset = builder.CreateVector(equips);
        characterBuilder.add_equips(equipsOffset);

        gen.seed(0);
        characterBuilder.add_id(disInt64(gen));
        characterBuilder.add_name(builder.CreateString(randomString.c_str()));
        characterBuilder.add_level(disInt32(gen));
        characterBuilder.add_healthpoint(disFloat(gen));
        characterBuilder.add_manapoint(disFloat(gen));
        characterBuilder.add_speed(disFloat(gen));
        flatbuffers_test::Position position(disFloat(gen), disFloat(gen), disFloat(gen));
        characterBuilder.add_pos(&position);

        std::vector<flatbuffers_test::ItemsEntry> itementries;
        for (auto i = 0; i < 64; ++i)
        {
            flatbuffers_test::Item item(disInt32(gen), disInt64(gen), disInt32(gen), disInt64(gen));
            flatbuffers_test::ItemsEntry itementry(disInt64(gen), item);
            itementries.push_back(itementry);
        }

        std::vector<flatbuffers_test::Position> path;
        for (auto i = 0; i < 16; ++i)
        {
            flatbuffers_test::Position position(disFloat(gen), disFloat(gen), disFloat(gen));
            path.push_back(position);
        }
        auto pathOffset = builder.CreateVectorOfStructs(path);
        characterBuilder.add_path(pathOffset);

        std::vector<float> attributes;
        for (auto i = 0; i < 16; ++i)
        {
            attributes.emplace_back(disFloat(gen));
        }
        auto attributesOffset2 = builder.CreateVector(attributes);
        characterBuilder.add_attributes(attributesOffset2);

        auto itementriesOffset = builder.CreateVectorOfStructs(itementries);
        characterBuilder.add_items(itementriesOffset);

        std::vector<flatbuffers_test::Skill> skills;
        for (auto i = 0; i < 4; ++i)
        {
            flatbuffers_test::Skill skill(disInt32(gen), disInt32(gen));
            skills.push_back(skill);
        }
        auto skillsOffset = builder.CreateVectorOfStructs(skills);
        characterBuilder.add_skills(skillsOffset);

        auto x = disInt32(gen);
        if (x != 23126686)
            throw std::logic_error("Random number sequence is inconsistent.");
        auto characterOffset = characterBuilder.Finish();
        builder.Finish(characterOffset);

        uint8_t *bufferPtr = builder.GetBufferPointer();
        originalSize = builder.GetSize();
        auto p = reinterpret_cast<char *>(builder.GetBufferPointer());
        auto sz = builder.GetSize();
        std::vector<char> buf(p, p + sz);

        flatbuffers::Verifier verifier(reinterpret_cast<const uint8_t *>(bufferPtr), originalSize);
        if (!flatbuffers_test::VerifyCharacterBuffer(verifier))
        {
            std::cout << "Serialized data is invalid! " << originalSize << std::endl;
        }
        // std::cout << "Serialized data is valid. "<< originalSize << std::endl;

        {
            const flatbuffers_test::Character *rcharacter = flatbuffers_test::GetCharacter(buf.data());
            auto requips = rcharacter->equips();
            // std::cout << requips->size() << std::endl;
            auto requip = requips->Get(1);
            auto rskills = rcharacter->skills();
            // std::cout << rskills->size() << std::endl;
            const flatbuffers_test::Skill *skill = rskills->Get(3);
            const flatbuffers::Vector<const flatbuffers_test::Position *> *rpath = rcharacter->path();
            // std::cout << rpath->size() << std::endl;
            const flatbuffers_test::Position *pos = rpath->Get(15);
            // std::cout << rpath->size() << " " << rskills->size() << std::endl;
            if (rpath->size() != 16 || rskills->size() != 4)
            {
                throw std::logic_error("flatbuffer's case: deserialization failed");
            }
            const flatbuffers::Vector<const flatbuffers_test::ItemsEntry *> *rItementries = rcharacter->items();
            const flatbuffers_test::ItemsEntry *rItementry = rItementries->Get(63);
            const flatbuffers_test::Item ritem = rItementry->value();
            // std::cout << rItementries->size() << " " << ritem.uuid() << std::endl;
        }

        if (WriteToFile)
        {
            std::string functionName = __func__;
            std::string fileName = functionName + ".bin";
            std::ofstream ofs(fileName, std::ios::binary);
            ofs.write(reinterpret_cast<char *>(bufferPtr), originalSize);
            ofs.close();
        }

        compressedSize = compressAndDecompress(bufferPtr, originalSize);

        auto start = std::chrono::high_resolution_clock::now();
        for (size_t it = 0; it < iterations; it++)
        {
            flatbuffers::FlatBufferBuilder builder;
            flatbuffers_test::CharacterBuilder characterBuilder(builder);

            gen.seed(0);
            std::vector<flatbuffers::Offset<flatbuffers_test::Equip>> equips;
            for (auto i = 0; i < 8; ++i)
            {
                flatbuffers_test::EquipBuilder equipBuilder(builder);
                equipBuilder.add_id(disInt32(gen));
                equipBuilder.add_uuid(disInt64(gen));
                equipBuilder.add_number(disInt32(gen));
                equipBuilder.add_timestamp(disInt64(gen));
                equipBuilder.add_level(disInt32(gen));
                std::vector<float> attributes = {disFloat(gen), disFloat(gen), disFloat(gen), disFloat(gen), disFloat(gen), disFloat(gen), disFloat(gen), disFloat(gen)};
                auto attributesOffset = builder.CreateVector(attributes);
                equipBuilder.add_attributes(attributesOffset);
                flatbuffers::Offset<flatbuffers_test::Equip> equipOffset = equipBuilder.Finish();
                equips.push_back(equipOffset);
            }
            auto equipsOffset = builder.CreateVector(equips);
            characterBuilder.add_equips(equipsOffset);

            gen.seed(0);
            characterBuilder.add_id(disInt64(gen));
            characterBuilder.add_name(builder.CreateString(randomString.c_str()));
            characterBuilder.add_level(disInt32(gen));
            characterBuilder.add_healthpoint(disFloat(gen));
            characterBuilder.add_manapoint(disFloat(gen));
            characterBuilder.add_speed(disFloat(gen));
            flatbuffers_test::Position position(disFloat(gen), disFloat(gen), disFloat(gen));
            characterBuilder.add_pos(&position);

            std::vector<flatbuffers_test::ItemsEntry> itementries;
            for (auto i = 0; i < 64; ++i)
            {
                flatbuffers_test::Item item(disInt32(gen), disInt64(gen), disInt32(gen), disInt64(gen));
                flatbuffers_test::ItemsEntry itementry(disInt64(gen), item);
                itementries.push_back(itementry);
            }
            std::vector<flatbuffers_test::Position> path;
            for (auto i = 0; i < 16; ++i)
            {
                flatbuffers_test::Position position(disFloat(gen), disFloat(gen), disFloat(gen));
                path.push_back(position);
            }
            auto pathOffset = builder.CreateVectorOfStructs(path);
            characterBuilder.add_path(pathOffset);

            std::vector<float> attributes;
            for (auto i = 0; i < 16; ++i)
            {
                attributes.emplace_back(disFloat(gen));
            }
            auto attributesOffset2 = builder.CreateVector(attributes);
            characterBuilder.add_attributes(attributesOffset2);

            auto itementriesOffset = builder.CreateVectorOfStructs(itementries);
            characterBuilder.add_items(itementriesOffset);

            std::vector<flatbuffers_test::Skill> skills;
            for (auto i = 0; i < 4; ++i)
            {
                flatbuffers_test::Skill skill(disInt32(gen), disInt32(gen));
                skills.push_back(skill);
            }
            auto x = disInt32(gen);
            if (x != 23126686)
                throw std::logic_error("Random number sequence is inconsistent.");
            auto skillsOffset = builder.CreateVectorOfStructs(skills);
            characterBuilder.add_skills(skillsOffset);

            auto characterOffset = characterBuilder.Finish();
            builder.Finish(characterOffset);

            uint8_t *bufferPtr = builder.GetBufferPointer();

            auto r2 = flatbuffers_test::GetCharacter(bufferPtr);
            (void)r2->path()[0];

            builder.Release();
        }
        auto finish = std::chrono::high_resolution_clock::now();
        duration = std::chrono::duration_cast<std::chrono::nanoseconds>(finish - start).count();
    }
    return benchmark3::Result("flatbuffers", "1.0.0", originalSize, compressedSize, duration, duration1, duration2);
}

benchmark3::Result
capnproto_serialization_test3(size_t iterations)
{
    std::random_device rd;
    std::mt19937_64 gen(0);
    std::uniform_int_distribution<uint64_t> disInt64;
    std::uniform_int_distribution<uint32_t> disInt32;
    std::uniform_real_distribution<float> disFloat;
    std::string charset = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    std::uniform_int_distribution<int> dis(0, charset.length() - 1);
    std::string randomString;
    for (int i = 0; i < 8; ++i)
    {
        randomString += charset[dis(gen)];
    }
    uint64_t duration, duration1, duration2;
    {
        auto start1 = std::chrono::high_resolution_clock::now();
        benchmark3_msgpack::Character r1;
        gen.seed(0);
        r1.id = disInt64(gen);
        r1.name = randomString.c_str();
        r1.level = disInt32(gen);
        r1.healthpoint = disFloat(gen);
        r1.manapoint = disFloat(gen);
        r1.speed = disFloat(gen);
        r1.pos = {disFloat(gen), disFloat(gen), disFloat(gen)};
        for (auto i = 0; i < 64; ++i)
        {
            r1.items.emplace(std::piecewise_construct, std::forward_as_tuple(disInt64(gen)), std::forward_as_tuple(disInt32(gen), disInt64(gen), disInt32(gen), disInt64(gen)));
        }
        for (auto i = 0; i < 16; ++i)
        {
            r1.path.emplace_back(disFloat(gen), disFloat(gen), disFloat(gen));
        }
        for (auto i = 0; i < 16; ++i)
        {
            r1.attributes.emplace_back(disFloat(gen));
        }
        for (auto i = 0; i < 8; ++i)
        {
            benchmark3_msgpack::Equip equip;
            equip.id = disInt32(gen);
            equip.uuid = disInt64(gen);
            equip.number = disInt32(gen);
            equip.timestamp = disInt64(gen);
            equip.level = disInt32(gen);
            equip.attributes = {disFloat(gen), disFloat(gen), disFloat(gen), disFloat(gen), disFloat(gen), disFloat(gen), disFloat(gen), disFloat(gen)};
            r1.equips.push_back(equip);
        }
        for (auto i = 0; i < 4; ++i)
        {
            r1.skills.emplace_back(disInt32(gen), disInt32(gen));
        }
        auto x = disInt32(gen);

        if (x != 1837058832)
            throw std::logic_error("Random number sequence is inconsistent.");
        auto finish1 = std::chrono::high_resolution_clock::now();
        duration1 = std::chrono::duration_cast<std::chrono::nanoseconds>(finish1 - start1).count();

        // test write performance
        auto start2 = std::chrono::high_resolution_clock::now();
        for (size_t it = 0; it < iterations; it++)
        {
            r1.level += 1;
            r1.healthpoint += 1;
            r1.manapoint += 1;
            r1.speed += 1;
            r1.pos = {disFloat(gen), disFloat(gen), disFloat(gen)};
            for (auto i = 0; i < 4; ++i)
            {
                benchmark3_msgpack::Equip equip;
                equip.id = disInt32(gen);
                equip.uuid = disInt64(gen);
                equip.number = disInt32(gen);
                equip.timestamp = disInt64(gen);
                equip.level = disInt32(gen);
                equip.attributes = {disFloat(gen), disFloat(gen), disFloat(gen), disFloat(gen), disFloat(gen), disFloat(gen), disFloat(gen), disFloat(gen)};
                r1.equips.push_back(equip);
                r1.equips.pop_back();
            }
            for (auto i = 0; i < 8; ++i)
            {
                r1.path.emplace_back(disFloat(gen), disFloat(gen), disFloat(gen));
            }
            r1.attributes.emplace_back(disFloat(gen));

            auto itemUuid0 = disInt64(gen);
            benchmark3_msgpack::Item item = {disInt32(gen), disInt64(gen), disInt32(gen), disInt64(gen)};
            r1.items[itemUuid0] = item;
            r1.items.erase(itemUuid0);

            r1.attributes.pop_back();
            for (auto i = 0; i < 8; ++i)
            {
                r1.path.pop_back();
            }
            r1.speed -= 1;
            r1.manapoint -= 1;
            r1.healthpoint -= 1;
            r1.level -= 1;
        }
        auto finish2 = std::chrono::high_resolution_clock::now();
        duration2 = std::chrono::duration_cast<std::chrono::nanoseconds>(finish2 - start2).count();
    }

    capnp::MallocMessageBuilder message;
    auto r1 = message.getRoot<capnp_character_test::Character>();
    gen.seed(0);
    r1.setId(disInt64(gen));
    r1.setName(randomString.c_str());
    r1.setLevel(disInt32(gen));
    r1.setHealthpoint(disFloat(gen));
    r1.setManapoint(disFloat(gen));
    r1.setSpeed(disFloat(gen));
    auto pos = r1.initPos();
    pos.setX(disFloat(gen));
    pos.setY(disFloat(gen));
    pos.setZ(disFloat(gen));
    auto items = r1.initItems();
    auto entries = items.initEntries(64);
    for (auto i = 0; i < 64; ++i)
    {
        auto entry = entries[i];
        entry.setKey(disInt64(gen));
        auto value = entry.initValue();
        value.setId(disInt32(gen));
        value.setUuid(disInt64(gen));
        value.setNumber(disInt32(gen));
        value.setTimestamp(disInt64(gen));
    }
    auto path = r1.initPath(16);
    for (auto i = 0; i < 16; ++i)
    {
        path[i].setX(disFloat(gen));
        path[i].setY(disFloat(gen));
        path[i].setZ(disFloat(gen));
    }
    auto attributes = r1.initAttributes(16);
    for (auto i = 0; i < 16; ++i)
    {
        attributes.set(i, disFloat(gen));
    }
    auto equips = r1.initEquips(8);
    for (auto i = 0; i < 8; ++i)
    {
        auto equip = equips[i];
        equip.setId(disInt32(gen));
        equip.setUuid(disInt64(gen));
        equip.setNumber(disInt32(gen));
        equip.setTimestamp(disInt64(gen));
        equip.setLevel(disInt32(gen));
        auto attributes = equip.initAttributes(8);
        for (auto i = 0; i < 8; ++i)
        {
            attributes.set(i, disFloat(gen));
        }
    }
    auto skills = r1.initSkills(4);
    for (auto i = 0; i < 4; ++i)
    {
        skills[i].setId(disInt32(gen));
        skills[i].setLevel(disInt32(gen));
    }
    // std::cout << r1.toString().flatten().cStr() << std::endl;
    // std::cout << r1.getEquips()[7].getTimestamp() << std::endl;
    // std::cout << r1.getSkills()[2].getLevel() << std::endl;
    auto x = disInt32(gen);
    if (x != 1837058832)
        throw std::logic_error("Random number sequence is inconsistent.");

    // check if we can deserialize back
    kj::ArrayPtr<const kj::ArrayPtr<const capnp::word>> serialized = message.getSegmentsForOutput();
    kj::Array<capnp::word> dataArr = capnp::messageToFlatArray(message);
    capnp::FlatArrayMessageReader reader(dataArr.asPtr());
    auto r2 = reader.getRoot<capnp_character_test::Character>();
    auto size = dataArr.size() * sizeof(capnp::word);

    if (WriteToFile)
    {
        std::string functionName = __func__;
        std::string fileName = functionName + ".bin";
        std::ofstream ofs(fileName, std::ios::binary);
        ofs.write(reinterpret_cast<char *>(dataArr.begin()), size);
        // ofs.write(serializedAllInOne.data(), serializedAllInOne.size());
        ofs.close();
    }

    auto compressedSize = compressAndDecompress(reinterpret_cast<char *>(dataArr.begin()), size);

    auto start = std::chrono::high_resolution_clock::now();
    for (size_t ii = 0; ii < iterations; ii++)
    {
        capnp::MallocMessageBuilder message;
        auto r1 = message.getRoot<capnp_character_test::Character>();
        gen.seed(0);
        r1.setId(disInt64(gen));
        r1.setName(randomString.c_str());
        r1.setLevel(disInt32(gen));
        r1.setHealthpoint(disFloat(gen));
        r1.setManapoint(disFloat(gen));
        r1.setSpeed(disFloat(gen));
        auto pos = r1.initPos();
        pos.setX(disFloat(gen));
        pos.setY(disFloat(gen));
        pos.setZ(disFloat(gen));
        auto items = r1.initItems();
        auto entries = items.initEntries(64);
        for (auto i = 0; i < 64; ++i)
        {
            auto entry = entries[i];
            entry.setKey(disInt64(gen));
            auto value = entry.initValue();
            value.setId(disInt32(gen));
            value.setUuid(disInt64(gen));
            value.setNumber(disInt32(gen));
            value.setTimestamp(disInt64(gen));
        }
        auto path = r1.initPath(16);
        for (auto i = 0; i < 16; ++i)
        {
            path[i].setX(disFloat(gen));
            path[i].setY(disFloat(gen));
            path[i].setZ(disFloat(gen));
        }
        auto attributes = r1.initAttributes(16);
        for (auto i = 0; i < 16; ++i)
        {
            attributes.set(i, disFloat(gen));
        }
        auto equips = r1.initEquips(8);
        for (auto i = 0; i < 8; ++i)
        {
            auto equip = equips[i];
            equip.setId(disInt32(gen));
            equip.setUuid(disInt64(gen));
            equip.setNumber(disInt32(gen));
            equip.setTimestamp(disInt64(gen));
            equip.setLevel(disInt32(gen));
            auto attributes = equip.initAttributes(8);
            for (auto i = 0; i < 8; ++i)
            {
                attributes.set(i, disFloat(gen));
            }
        }
        auto skills = r1.initSkills(4);
        for (auto i = 0; i < 4; ++i)
        {
            skills[i].setId(disInt32(gen));
            skills[i].setLevel(disInt32(gen));
        }
        auto x = disInt32(gen);
        // std::cout << r1.toString().flatten().cStr() << std::endl;
        if (x != 1837058832)
            throw std::logic_error("Random number sequence is inconsistent.");
        // serialization
        kj::ArrayPtr<const kj::ArrayPtr<const capnp::word>> serialized = message.getSegmentsForOutput();
        kj::Array<capnp::word> dataArr = capnp::messageToFlatArray(message);
        // deserialization
        capnp::FlatArrayMessageReader reader(dataArr.asPtr());
        auto r2 = reader.getRoot<capnp_character_test::Character>();
        assert(r1.getEquips()[0].getAttributes()[7], r2.getEquips()[0].getAttributes()[7]);
        // std::cout << r1.getEquips()[0].getAttributes()[7] << " " << r2.getEquips()[0].getAttributes()[7] << std::endl;
    }
    auto finish = std::chrono::high_resolution_clock::now();
    duration = std::chrono::duration_cast<std::chrono::nanoseconds>(finish - start).count();

    return benchmark3::Result("capnproto", std::to_string(CAPNP_VERSION), size, compressedSize, duration, duration1, duration2);
}

benchmark3::Result
msgpack_serialization_test3(size_t iterations, std::vector<benchmark3_msgpack::Character> &unitVector)
{
    using namespace benchmark3_msgpack;
    std::random_device rd;
    std::mt19937_64 gen(0);
    std::uniform_int_distribution<uint64_t> disInt64;
    std::uniform_int_distribution<uint32_t> disInt32;
    std::uniform_real_distribution<float> disFloat;
    std::string charset = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    std::uniform_int_distribution<int> dis(0, charset.length() - 1);
    std::string randomString;
    for (int i = 0; i < 8; ++i)
    {
        randomString += charset[dis(gen)];
    }
    auto start1 = std::chrono::high_resolution_clock::now();
    Character r1;
    gen.seed(0);
    r1.id = disInt64(gen);
    r1.name = randomString.c_str();
    r1.level = disInt32(gen);
    r1.healthpoint = disFloat(gen);
    r1.manapoint = disFloat(gen);
    r1.speed = disFloat(gen);
    r1.pos = {disFloat(gen), disFloat(gen), disFloat(gen)};
    for (auto i = 0; i < 64; ++i)
    {
        r1.items.emplace(std::piecewise_construct, std::forward_as_tuple(disInt64(gen)), std::forward_as_tuple(disInt32(gen), disInt64(gen), disInt32(gen), disInt64(gen)));
    }
    for (auto i = 0; i < 16; ++i)
    {
        r1.path.emplace_back(disFloat(gen), disFloat(gen), disFloat(gen));
    }
    for (auto i = 0; i < 16; ++i)
    {
        r1.attributes.emplace_back(disFloat(gen));
    }
    for (auto i = 0; i < 8; ++i)
    {
        Equip equip;
        equip.id = disInt32(gen);
        equip.uuid = disInt64(gen);
        equip.number = disInt32(gen);
        equip.timestamp = disInt64(gen);
        equip.level = disInt32(gen);
        equip.attributes = {disFloat(gen), disFloat(gen), disFloat(gen), disFloat(gen), disFloat(gen), disFloat(gen), disFloat(gen), disFloat(gen)};
        r1.equips.push_back(equip);
    }
    for (auto i = 0; i < 4; ++i)
    {
        r1.skills.emplace_back(disInt32(gen), disInt32(gen));
    }
    auto x = disInt32(gen);

    if (x != 1837058832)
        throw std::logic_error("Random number sequence is inconsistent.");
    auto finish1 = std::chrono::high_resolution_clock::now();
    auto duration1 = std::chrono::duration_cast<std::chrono::nanoseconds>(finish1 - start1).count();

    // test write performance
    auto start2 = std::chrono::high_resolution_clock::now();
    for (size_t it = 0; it < iterations; it++)
    {
        r1.level += 1;
        r1.healthpoint += 1;
        r1.manapoint += 1;
        r1.speed += 1;
        r1.pos = {disFloat(gen), disFloat(gen), disFloat(gen)};
        for (auto i = 0; i < 4; ++i)
        {
            Equip equip;
            equip.id = disInt32(gen);
            equip.uuid = disInt64(gen);
            equip.number = disInt32(gen);
            equip.timestamp = disInt64(gen);
            equip.level = disInt32(gen);
            equip.attributes = {disFloat(gen), disFloat(gen), disFloat(gen), disFloat(gen), disFloat(gen), disFloat(gen), disFloat(gen), disFloat(gen)};
            r1.equips.push_back(equip);
            r1.equips.pop_back();
        }
        for (auto i = 0; i < 8; ++i)
        {
            r1.path.emplace_back(disFloat(gen), disFloat(gen), disFloat(gen));
        }
        r1.attributes.emplace_back(disFloat(gen));

        auto itemUuid0 = disInt64(gen);
        Item item = {disInt32(gen), disInt64(gen), disInt32(gen), disInt64(gen)};
        r1.items[itemUuid0] = item;
        r1.items.erase(itemUuid0);

        r1.attributes.pop_back();
        for (auto i = 0; i < 8; ++i)
        {
            r1.path.pop_back();
        }
        r1.speed -= 1;
        r1.manapoint -= 1;
        r1.healthpoint -= 1;
        r1.level -= 1;
    }
    auto finish2 = std::chrono::high_resolution_clock::now();
    auto duration2 = std::chrono::duration_cast<std::chrono::nanoseconds>(finish2 - start2).count();

    unitVector.push_back(r1);

    Character r2;
    msgpack::sbuffer sbuf;
    msgpack::pack(sbuf, r1);
    std::string serialized(sbuf.data(), sbuf.size());
    msgpack::object_handle msg = msgpack::unpack(serialized.data(), serialized.size());
    msgpack::object obj = msg.get();
    obj.convert(r2);
    if (r1 != r2)
    {
        throw std::logic_error("msgpack's case: deserialization failed");
    }

    if (WriteToFile)
    {
        std::string functionName = __func__;
        std::string fileName = functionName + ".bin";
        std::ofstream ofs(fileName, std::ios::binary);
        ofs.write(serialized.data(), serialized.size());
        ofs.close();
    }

    auto compressedSize = compressAndDecompress(serialized.data(), serialized.size());

    auto start = std::chrono::high_resolution_clock::now();
    for (size_t i = 0; i < iterations; i++)
    {
        sbuf.clear();
        msgpack::pack(sbuf, r1);
        msgpack::object_handle msg = msgpack::unpack(sbuf.data(), sbuf.size());
        msgpack::object obj = msg.get();
        obj.convert(r2);
    }
    auto finish = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(finish - start).count();
    return benchmark3::Result("msgpack", msgpack_version(), serialized.size(), compressedSize, duration, duration1, duration2);
}

benchmark3::Result cistaraw_serialization_test3(size_t iterations)
{
    using namespace benchmark3_cistaraw;
    std::random_device rd;
    std::mt19937_64 gen(0);
    std::uniform_int_distribution<uint64_t> disInt64;
    std::uniform_int_distribution<uint32_t> disInt32;
    std::uniform_real_distribution<float> disFloat;
    std::string charset = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    std::uniform_int_distribution<int> dis(0, charset.length() - 1);
    std::string randomString;
    for (int i = 0; i < 8; ++i)
    {
        randomString += charset[dis(gen)];
    }
    auto start1 = std::chrono::high_resolution_clock::now();
    Character r1;
    gen.seed(0);
    r1.id = disInt64(gen);
    r1.name = randomString.c_str();
    r1.level = disInt32(gen);
    r1.healthpoint = disFloat(gen);
    r1.manapoint = disFloat(gen);
    r1.speed = disFloat(gen);
    r1.pos = {disFloat(gen), disFloat(gen), disFloat(gen)};
    for (auto i = 0; i < 64; ++i)
    {
        Item item = {disInt32(gen), disInt64(gen), disInt32(gen), disInt64(gen)};
        r1.items[disInt64(gen)] = item;
        // r1.items.emplace(std::piecewise_construct, std::forward_as_tuple(disInt64(gen)), std::forward_as_tuple(disInt32(gen), disInt64(gen), disInt32(gen), disInt64(gen)));
    }
    for (auto i = 0; i < 16; ++i)
    {
        r1.path.emplace_back(disFloat(gen), disFloat(gen), disFloat(gen));
    }
    for (auto i = 0; i < 16; ++i)
    {
        r1.attributes.emplace_back(disFloat(gen));
    }
    for (auto i = 0; i < 8; ++i)
    {
        Equip equip;
        equip.id = disInt32(gen);
        equip.uuid = disInt64(gen);
        equip.number = disInt32(gen);
        equip.timestamp = disInt64(gen);
        equip.level = disInt32(gen);
        equip.attributes = {disFloat(gen), disFloat(gen), disFloat(gen), disFloat(gen), disFloat(gen), disFloat(gen), disFloat(gen), disFloat(gen)};
        r1.equips.push_back(equip);
    }
    for (auto i = 0; i < 4; ++i)
    {
        r1.skills.emplace_back(disInt32(gen), disInt32(gen));
    }
    // for (size_t i = 0; i < benchmark3::kIntegers.size(); i++) {
    //     r1.ids.push_back(benchmark3::kIntegers[i]);
    // }
    // const char* strval = benchmark3::kStringValue.c_str();
    // for (size_t i = 0; i < benchmark3::kStringsCount; i++) {
    //     r1.strings.emplace_back(strval);
    // }
    auto x = disInt32(gen);
    if (x != 1837058832)
        throw std::logic_error("Random number sequence is inconsistent.");
    auto finish1 = std::chrono::high_resolution_clock::now();
    auto duration1 = std::chrono::duration_cast<std::chrono::nanoseconds>(finish1 - start1).count();

    std::vector<unsigned char> buf1 = cista::serialize(r1);

    // test write performance
    auto start2 = std::chrono::high_resolution_clock::now();
    for (size_t it = 0; it < iterations; it++)
    {
        r1.level += 1;
        r1.healthpoint += 1;
        r1.manapoint += 1;
        r1.speed += 1;
        r1.pos = {disFloat(gen), disFloat(gen), disFloat(gen)};
        for (auto i = 0; i < 4; ++i)
        {
            Equip equip;
            equip.id = disInt32(gen);
            equip.uuid = disInt64(gen);
            equip.number = disInt32(gen);
            equip.timestamp = disInt64(gen);
            equip.level = disInt32(gen);
            equip.attributes = {disFloat(gen), disFloat(gen), disFloat(gen), disFloat(gen), disFloat(gen), disFloat(gen), disFloat(gen), disFloat(gen)};
            r1.equips.push_back(equip);
            r1.equips.pop_back();
        }
        for (auto i = 0; i < 8; ++i)
        {
            r1.path.emplace_back(disFloat(gen), disFloat(gen), disFloat(gen));
        }
        r1.attributes.emplace_back(disFloat(gen));

        auto itemUuid0 = disInt64(gen);
        Item item = {disInt32(gen), disInt64(gen), disInt32(gen), disInt64(gen)};
        r1.items[itemUuid0] = item;
        r1.items.erase(itemUuid0);

        r1.attributes.pop_back();
        for (auto i = 0; i < 8; ++i)
        {
            r1.path.pop_back();
        }
        r1.speed -= 1;
        r1.manapoint -= 1;
        r1.healthpoint -= 1;
        r1.level -= 1;
    }
    auto finish2 = std::chrono::high_resolution_clock::now();
    auto duration2 = std::chrono::duration_cast<std::chrono::nanoseconds>(finish2 - start2).count();

    std::vector<unsigned char> buf2 = cista::serialize(r1);

    std::vector<unsigned char> buf;
    buf = cista::serialize(r1);
    Character *r2 = cista::raw::deserialize<Character>(buf);
    // Character* r2 = cista::raw::deserialize<Character>(buf);  // deserialize fail when gen.seed(0)
    if (r1 != *r2)
    {
        throw std::logic_error("offsetdatastructure's case: deserialization failed");
    }
    if (buf.size() != buf2.size())
    {
        throw std::logic_error("offsetdatastructure's case: deserialization failed");
    }

    if (WriteToFile)
    {
        std::string functionName = __func__;
        std::string fileName = functionName + ".bin";
        std::ofstream ofs(fileName, std::ios::binary);
        ofs.write(reinterpret_cast<char *>(buf.data()), buf.size());
        ofs.close();
    }

    auto compressedSize = compressAndDecompress(buf.data(), buf.size());

    auto start = std::chrono::high_resolution_clock::now();
    for (size_t it = 0; it < iterations; it++)
    {
        std::vector<unsigned char> buf;
        buf = cista::serialize(r1);
        Character r2 = *cista::raw::deserialize<Character>(buf);
        // r2.healthpoint += buf.size();
    }
    auto finish = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(finish - start).count();
    std::string libName = "cistaraw";
    return benchmark3::Result(libName, "1.0.0", buf.size(), compressedSize, duration, duration1, duration2);
}

benchmark3::Result cistaoffset_serialization_test3(size_t iterations)
{
    using namespace benchmark3_cistaoffset;
    std::random_device rd;
    std::mt19937_64 gen(0);
    std::uniform_int_distribution<uint64_t> disInt64;
    std::uniform_int_distribution<uint32_t> disInt32;
    std::uniform_real_distribution<float> disFloat;
    std::string charset = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    std::uniform_int_distribution<int> dis(0, charset.length() - 1);
    std::string randomString;
    for (int i = 0; i < 8; ++i)
    {
        randomString += charset[dis(gen)];
    }
    auto start1 = std::chrono::high_resolution_clock::now();
    Character r1;
    gen.seed(0);
    r1.id = disInt64(gen);
    r1.name = randomString.c_str();
    r1.level = disInt32(gen);
    r1.healthpoint = disFloat(gen);
    r1.manapoint = disFloat(gen);
    r1.speed = disFloat(gen);
    r1.pos = {disFloat(gen), disFloat(gen), disFloat(gen)};
    for (auto i = 0; i < 64; ++i)
    {
        Item item = {disInt32(gen), disInt64(gen), disInt32(gen), disInt64(gen)};
        r1.items[disInt64(gen)] = item;
        // r1.items.emplace(std::piecewise_construct, std::forward_as_tuple(disInt64(gen)), std::forward_as_tuple(disInt32(gen), disInt64(gen), disInt32(gen), disInt64(gen)));
    }
    for (auto i = 0; i < 16; ++i)
    {
        r1.path.emplace_back(disFloat(gen), disFloat(gen), disFloat(gen));
    }
    for (auto i = 0; i < 16; ++i)
    {
        r1.attributes.emplace_back(disFloat(gen));
    }
    for (auto i = 0; i < 8; ++i)
    {
        Equip equip;
        equip.id = disInt32(gen);
        equip.uuid = disInt64(gen);
        equip.number = disInt32(gen);
        equip.timestamp = disInt64(gen);
        equip.level = disInt32(gen);
        equip.attributes = {disFloat(gen), disFloat(gen), disFloat(gen), disFloat(gen), disFloat(gen), disFloat(gen), disFloat(gen), disFloat(gen)};
        r1.equips.push_back(equip);
    }
    for (auto i = 0; i < 4; ++i)
    {
        r1.skills.emplace_back(disInt32(gen), disInt32(gen));
    }
    auto x = disInt32(gen);

    if (x != 1837058832)
        throw std::logic_error("Random number sequence is inconsistent.");
    auto finish1 = std::chrono::high_resolution_clock::now();
    auto duration1 = std::chrono::duration_cast<std::chrono::nanoseconds>(finish1 - start1).count();

    // test write performance
    auto start2 = std::chrono::high_resolution_clock::now();
    for (size_t it = 0; it < iterations; it++)
    {
        r1.level += 1;
        r1.healthpoint += 1;
        r1.manapoint += 1;
        r1.speed += 1;
        r1.pos = {disFloat(gen), disFloat(gen), disFloat(gen)};
        for (auto i = 0; i < 4; ++i)
        {
            Equip equip;
            equip.id = disInt32(gen);
            equip.uuid = disInt64(gen);
            equip.number = disInt32(gen);
            equip.timestamp = disInt64(gen);
            equip.level = disInt32(gen);
            equip.attributes = {disFloat(gen), disFloat(gen), disFloat(gen), disFloat(gen), disFloat(gen), disFloat(gen), disFloat(gen), disFloat(gen)};
            r1.equips.push_back(equip);
            r1.equips.pop_back();
        }
        for (auto i = 0; i < 8; ++i)
        {
            r1.path.emplace_back(disFloat(gen), disFloat(gen), disFloat(gen));
        }
        r1.attributes.emplace_back(disFloat(gen));

        auto itemUuid0 = disInt64(gen);
        Item item = {disInt32(gen), disInt64(gen), disInt32(gen), disInt64(gen)};
        r1.items[itemUuid0] = item;
        r1.items.erase(itemUuid0);

        r1.attributes.pop_back();
        for (auto i = 0; i < 8; ++i)
        {
            r1.path.pop_back();
        }
        r1.speed -= 1;
        r1.manapoint -= 1;
        r1.healthpoint -= 1;
        r1.level -= 1;
    }
    auto finish2 = std::chrono::high_resolution_clock::now();
    auto duration2 = std::chrono::duration_cast<std::chrono::nanoseconds>(finish2 - start2).count();

    std::vector<unsigned char> buf;
    buf = cista::serialize(r1);
    Character *r2 = cista::offset::deserialize<Character>(buf);
    if (r1 != *r2)
    {
        throw std::logic_error("offsetdatastructure's case: deserialization failed");
    }

    if (WriteToFile)
    {
        std::string functionName = __func__;
        std::string fileName = functionName + ".bin";
        std::ofstream ofs(fileName, std::ios::binary);
        ofs.write(reinterpret_cast<char *>(buf.data()), buf.size());
        ofs.close();
    }

    auto compressedSize = compressAndDecompress(buf.data(), buf.size());

    auto start = std::chrono::high_resolution_clock::now();
    for (size_t it = 0; it < iterations; it++)
    {
        std::vector<unsigned char> buf;
        buf = cista::serialize(r1);
        Character r2 = *cista::offset::deserialize<Character>(buf);
    }
    auto finish = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(finish - start).count();
    std::string libName = "cistaoffset";
    return benchmark3::Result(libName, "1.0.0", buf.size(), compressedSize, duration, duration1, duration2);
}

benchmark3::Result offsetdatastructure_serialization_test3(size_t iterations, std::vector<XOffsetDatastructure::XTypeHolder<benchmark3_offsetdatastructure::Character>> &unitVector, bool ContianerPreAllocation = false)
{
    using namespace benchmark3_offsetdatastructure;
    benchmark3::Result ret("offsetdatastructure", "1.0.0", 0, 0, 0, 0, 0);
    std::random_device rd;
    std::mt19937_64 gen(0);
    std::uniform_int_distribution<uint64_t> disInt64;
    std::uniform_int_distribution<uint32_t> disInt32;
    std::uniform_real_distribution<float> disFloat;
    std::string charset = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    std::uniform_int_distribution<int> dis(0, charset.length() - 1);
    std::string randomString;
    for (int i = 0; i < 8; ++i)
    {
        randomString += charset[dis(gen)];
    }
    XOffsetDatastructure::XTypeHolder<Character> r1;
    r1.expand(1024 * 16);
    auto start1 = std::chrono::high_resolution_clock::now();
    auto rootptr = r1.getOffsetPtr();
    gen.seed(0);
    rootptr->id = disInt64(gen);
    rootptr->name = randomString.c_str();
    rootptr->level = disInt32(gen);
    rootptr->healthpoint = disFloat(gen);
    rootptr->manapoint = disFloat(gen);
    rootptr->speed = disFloat(gen);
    rootptr->pos = {disFloat(gen), disFloat(gen), disFloat(gen)};
    for (auto i = 0; i < 64; ++i)
    {
        rootptr->items.emplace(std::piecewise_construct, std::forward_as_tuple(disInt64(gen)), std::forward_as_tuple(disInt32(gen), disInt64(gen), disInt32(gen), disInt64(gen)));
    }
    if (ContianerPreAllocation)
    {
        rootptr->path.resize(16);
        for (auto i = 0; i < 16; ++i)
        {
            rootptr->path[i] = {disFloat(gen), disFloat(gen), disFloat(gen)};
        }
        rootptr->attributes.resize(16);
        for (auto i = 0; i < 16; ++i)
        {
            rootptr->attributes[i] = disFloat(gen);
        }
        rootptr->equips.resize(8, r1.getStorage());
        for (auto i = 0; i < 8; ++i)
        {
            auto &equip = rootptr->equips[i];
            equip.id = disInt32(gen);
            equip.uuid = disInt64(gen);
            equip.number = disInt32(gen);
            equip.timestamp = disInt64(gen);
            equip.level = disInt32(gen);
            auto attributesPtr = r1.getOffsetPtr<XOffsetDatastructure::XVector<float>>(equip.attributes);
            *attributesPtr = {disFloat(gen), disFloat(gen), disFloat(gen), disFloat(gen), disFloat(gen), disFloat(gen), disFloat(gen), disFloat(gen)};
        }
        rootptr->skills.resize(4);
        for (auto i = 0; i < 4; ++i)
        {
            rootptr->skills[i] = {disInt32(gen), disInt32(gen)};
        }
    }
    else
    {
        for (auto i = 0; i < 16; ++i)
        {
            rootptr->path.emplace_back(disFloat(gen), disFloat(gen), disFloat(gen));
        }
        for (auto i = 0; i < 16; ++i)
        {
            rootptr->attributes.emplace_back(disFloat(gen));
        }
        for (auto i = 0; i < 8; ++i)
        {
            rootptr->equips.emplace_back(r1.getStorage());
            auto &equip = rootptr->equips.back();
            equip.id = disInt32(gen);
            equip.uuid = disInt64(gen);
            equip.number = disInt32(gen);
            equip.timestamp = disInt64(gen);
            equip.level = disInt32(gen);
            auto attributesPtr = r1.getOffsetPtr<XOffsetDatastructure::XVector<float>>(equip.attributes);
            *attributesPtr = {disFloat(gen), disFloat(gen), disFloat(gen), disFloat(gen), disFloat(gen), disFloat(gen), disFloat(gen), disFloat(gen)};
        }
        for (auto i = 0; i < 4; ++i)
        {
            rootptr->skills.emplace_back(disInt32(gen), disInt32(gen));
        }
    }
    auto x = disInt32(gen);
    if (x != 1837058832)
        throw std::logic_error("Random number sequence is inconsistent.");

    auto finish1 = std::chrono::high_resolution_clock::now();
    auto duration1 = std::chrono::duration_cast<std::chrono::nanoseconds>(finish1 - start1).count();

    // std::cout << "path: " << rootptr->path.capacity() << " " << rootptr->path.size() << std::endl;
    // std::cout << "attributes: " << rootptr->attributes.capacity() << " " << rootptr->attributes.size() << std::endl;
    // std::cout << "items: " << rootptr->items.capacity() << " " << rootptr->items.size() << std::endl;
    // std::cout << "equips: " << rootptr->equips.capacity() << " " << rootptr->equips.size() << std::endl;
    // std::cout << "skills: " << rootptr->skills.capacity() << " " << rootptr->skills.size() << std::endl;

    // test write performance
    auto start2 = std::chrono::high_resolution_clock::now();
    for (size_t it = 0; it < iterations; it++)
    {
        rootptr->level += 1;
        rootptr->healthpoint += 1;
        rootptr->manapoint += 1;
        rootptr->speed += 1;
        rootptr->pos = {disFloat(gen), disFloat(gen), disFloat(gen)};
        for (auto i = 0; i < 4; ++i)
        {
            rootptr->equips.emplace_back(r1.getStorage());
            auto equip = rootptr->equips.back();
            equip.id = disInt32(gen);
            equip.uuid = disInt64(gen);
            equip.number = disInt32(gen);
            equip.timestamp = disInt64(gen);
            equip.level = disInt32(gen);
            auto attributesPtr = r1.getOffsetPtr<XOffsetDatastructure::XVector<float>>(equip.attributes);
            *attributesPtr = {disFloat(gen), disFloat(gen), disFloat(gen), disFloat(gen), disFloat(gen), disFloat(gen), disFloat(gen), disFloat(gen)};
            rootptr->equips.pop_back();
        }
        for (auto i = 0; i < 8; ++i)
        {
            rootptr->path.emplace_back(disFloat(gen), disFloat(gen), disFloat(gen));
        }
        rootptr->attributes.emplace_back(disFloat(gen));
        auto itemUuid0 = disInt64(gen);
        rootptr->items.emplace(std::piecewise_construct, std::forward_as_tuple(itemUuid0), std::forward_as_tuple(disInt32(gen), disInt64(gen), disInt32(gen), disInt64(gen)));
        rootptr->items.erase(itemUuid0);
        for (auto i = 0; i < 8; ++i)
        {
            rootptr->path.pop_back();
        }
        rootptr->attributes.pop_back();
        rootptr->speed -= 1;
        rootptr->manapoint -= 1;
        rootptr->healthpoint -= 1;
        rootptr->level -= 1;
    }
    auto finish2 = std::chrono::high_resolution_clock::now();
    auto duration2 = std::chrono::duration_cast<std::chrono::nanoseconds>(finish2 - start2).count();

    r1.trim();
    unitVector.push_back(r1);

    auto bufSize = r1.getBuffer().size();
    std::vector<std::byte> data2(r1.getBuffer().size());
    std::memcpy(data2.data(), r1.getBuffer().data(), r1.getBuffer().size());
    XOffsetDatastructure::XTypeHolder<Character> r2(data2);
    if (*r1.get() != *r2.get())
    {
        throw std::logic_error("offsetdatastructure's case: deserialization failed");
    }

    if (WriteToFile)
    {
        std::string functionName = __func__;
        std::string fileName = functionName + ".bin";
        std::ofstream ofs(fileName, std::ios::binary);
        ofs.write(reinterpret_cast<const char *>(r1.getBuffer().data()), r1.getBuffer().size());
        ofs.close();
    }

    auto compressedSize = compressAndDecompress(r1.getBuffer().data(), r1.getBuffer().size());

    auto start = std::chrono::high_resolution_clock::now();
    for (size_t it = 0; it < iterations; it++)
    {
        // XOffsetDatastructure::XTypeHolder<Character> r1;
        // r1.expand(1024 * 32);
        // auto rootptr = r1.getOffsetPtr();
        // gen.seed(0);
        // rootptr->id = disInt64(gen);
        // rootptr->name = randomString.c_str();
        // rootptr->level = disInt32(gen);
        // rootptr->healthpoint = disFloat(gen);
        // rootptr->manapoint = disFloat(gen);
        // rootptr->speed = disFloat(gen);
        // rootptr->pos = {disFloat(gen), disFloat(gen), disFloat(gen)};
        // for (auto i = 0; i < 64; ++i)
        // {
        //     rootptr->items.emplace(std::piecewise_construct, std::forward_as_tuple(disInt64(gen)), std::forward_as_tuple(disInt32(gen), disInt64(gen), disInt32(gen), disInt64(gen)));
        // }
        // for (auto i = 0; i < 16; ++i)
        // {
        //     rootptr->path.emplace_back(disFloat(gen), disFloat(gen), disFloat(gen));
        // }
        // for (auto i = 0; i < 16; ++i)
        // {
        //     rootptr->attributes.emplace_back(disFloat(gen));
        // }
        // for (auto i = 0; i < 8; ++i)
        // {
        //     rootptr->equips.emplace_back(r1.getStorage());
        //     auto equip = rootptr->equips.back();
        //     equip.id = disInt32(gen);
        //     equip.uuid = disInt64(gen);
        //     equip.number = disInt32(gen);
        //     equip.timestamp = disInt64(gen);
        //     equip.level = disInt32(gen);
        //     auto attributesPtr = r1.getOffsetPtr<XOffsetDatastructure::XVector<float>>(equip.attributes);
        //     *attributesPtr = {disFloat(gen), disFloat(gen), disFloat(gen), disFloat(gen), disFloat(gen), disFloat(gen), disFloat(gen), disFloat(gen)};
        // }
        // for (auto i = 0; i < 4; ++i)
        // {
        //     rootptr->skills.emplace_back(disInt32(gen), disInt32(gen));
        // }
        // r1.trim();
        // serialization
        const std::vector<std::byte> &buf = r1.getBuffer();

        // deserialization
        XOffsetDatastructure::XTypeHolder<Character> r2(const_cast<std::vector<std::byte> &>(buf));
    }
    auto finish = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(finish - start).count();

    return benchmark3::Result("offsetdatastructure", "1.0.0", bufSize, compressedSize, duration, duration1, duration2);
}

int main()
{
    auto iterNum = 0; // 100000;
    std::map<std::string, std::vector<benchmark3::Result>> results;
    std::vector<XOffsetDatastructure::XTypeHolder<benchmark3_offsetdatastructure::Character>> unitVectorXoffsetDatastructure;
    std::vector<benchmark3_msgpack::Character> unitVectorMsgpack;
    // for (auto i = 0; i < 1000; ++i)
    // {
    //     std::cout << "iteration: " << i << std::endl;
    //     // auto result = cistaraw_serialization_test3(iterNum);
    //     // results["cistaraw"].push_back(result);
    //     // result = cistaoffset_serialization_test3(iterNum);
    //     // results["cistaoffse"].push_back(result);
    //     // result = capnproto_serialization_test3(iterNum);
    //     // results["capnproto"].push_back(result);
    //     // result = flatbuffers_serialization_test3(iterNum);
    //     // results["flatbuffers"].push_back(result);
    //     auto result = offsetdatastructure_serialization_test3(iterNum, unitVectorXoffsetDatastructure);
    //     results["offsetdatastructure"].push_back(result);
    //     // result = msgpack_serialization_test3(iterNum, unitVectorMsgpack);
    //     // results["msgpack"].push_back(result);
    //     // result = zppbits_serialization_test3(iterNum);
    //     // results["zppbits"].push_back(result);
    // }
    for (auto i = 0; i < 1; ++i)
    {
        std::cout << "iteration: " << i << std::endl;
        auto result = offsetdatastructure_serialization_test3(iterNum, unitVectorXoffsetDatastructure);
        results["offsetdatastructure"].push_back(result);
    }
    for (auto i = 0; i < 1; ++i)
    {
        std::cout << "iteration: " << i << std::endl;
        auto result = offsetdatastructure_serialization_test3(iterNum, unitVectorXoffsetDatastructure, true);
        results["offsetdatastructure"].push_back(result);
    }
    // for (auto i = 0; i < 10000; ++i)
    // {
    //     std::cout << "iteration: " << i << std::endl;
    //     auto result = msgpack_serialization_test3(iterNum, unitVectorMsgpack);
    //     results["msgpack"].push_back(result);
    // }
    for (const auto &result : results)
    {
        std::cout << result.first << ":" << std::endl;
        for (const benchmark3::Result &r : result.second)
        {
            r.print();
        }
    }
    std::cout << "average:" << std::endl;
    for (const auto &result : results)
    {
        double time = 0;
        double time1 = 0;
        double time2 = 0;
        double time3 = 0;
        auto name = result.first;
        for (const benchmark3::Result &r : result.second)
        {
            time += r.time;
            time1 += r.time1;
            time2 += r.time2;
            time3 += r.time3;
        }
        time /= result.second.size();
        time1 /= result.second.size();
        time2 /= result.second.size();
        time3 /= result.second.size();
        std::cout << std::setw(20) << name << " " << std::setw(10) << time << "ms " << std::setw(10) << time1 << "ms " << std::setw(10) << time2 << "ms " << std::setw(10) << time3 << "ms " << std::endl;
    }
    return 0;
}