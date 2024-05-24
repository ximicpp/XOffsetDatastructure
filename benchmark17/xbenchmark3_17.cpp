#include <string>
#include <vector>
#include <stdint.h>
#include <chrono>
#include "character.pb.h"
#include <typeinfo>
#include <random>
#include "zlibcompressor.hpp"
#include <fstream>
#include "test.pb.h"
#include <msgpack.hpp>

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
        void print()
        {
            std::cout << std::setw(20) << name << " " << std::setw(10) << version << " " << std::setw(10) << size << " " << std::setw(10) << compressedSize << " " << std::setw(10) << time << "ms " << std::setw(10) << time1 << "ms " << std::setw(10) << time2 << "ms " << std::setw(10) << time3 << "ms " << std::endl;
        }
    };
}

namespace benchmark3_zppbits
{
    struct Skill
    {
        uint32_t id;
        uint32_t level;
        Skill() : id(0), level(0) {}
        Skill(uint32_t id, uint32_t level) : id(id), level(level) {}
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
        Item() : id(0), uuid(0), number(0), timestamp(0) {}
        Item(uint32_t id, uint64_t uuid, uint32_t number, uint64_t timestamp) : id(id), uuid(uuid), number(number), timestamp(timestamp) {}
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
        Equip() : id(0), uuid(0), number(0), timestamp(0), level(0) {}
        Equip(uint32_t id, uint64_t uuid, uint32_t number, uint64_t timestamp, uint32_t level, const std::vector<float> &attributes) : id(id), uuid(uuid), number(number), timestamp(timestamp), level(level), attributes(attributes) {}
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
        Position() : x(0), y(0), z(0) {}
        Position(float x, float y, float z) : x(x), y(y), z(z) {}
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

namespace benchmark3_msgpack
{
    struct Skill
    {
        uint32_t id;
        uint32_t level;
        Skill() : id(0), level(0) {}
        Skill(uint32_t id, uint32_t level) : id(id), level(level) {}
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
        Item() : id(0), uuid(0), number(0), timestamp(0) {}
        Item(uint32_t id, uint64_t uuid, uint32_t number, uint64_t timestamp) : id(id), uuid(uuid), number(number), timestamp(timestamp) {}
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
        Equip() : Item(0, 0, 0, 0), level(0) {}
        Equip(uint32_t id, uint64_t uuid, uint32_t number, uint64_t timestamp, uint32_t level, const std::vector<float> &attributes) : Item(id, uuid, number, timestamp), level(level), attributes(attributes) {}
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
        Position() : x(0), y(0), z(0) {}
        Position(float x, float y, float z) : x(x), y(y), z(z) {}
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

benchmark3::Result
msgpack_serialization_test3(size_t iterations)
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
    for (auto i = 0; i < 16; ++i)
    {
        r1.path.emplace_back(disFloat(gen), disFloat(gen), disFloat(gen));
    }
    for (auto i = 0; i < 16; ++i)
    {
        r1.attributes.emplace_back(disFloat(gen));
    }
    for (auto i = 0; i < 64; ++i)
    {
        r1.items.emplace(std::piecewise_construct, std::forward_as_tuple(disInt64(gen)), std::forward_as_tuple(disInt32(gen), disInt64(gen), disInt32(gen), disInt64(gen)));
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

benchmark3::Result
protobuf_serialization_test3(size_t iterations)
{
    using namespace protobuf_test;
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

    protobuf_test::Character character;
    auto start1 = std::chrono::high_resolution_clock::now();
    gen.seed(0);
    // Create a Character message
    character.set_id(disInt64(gen));
    character.set_name(randomString.c_str());
    character.set_level(disInt32(gen));
    character.set_healthpoint(disFloat(gen));
    character.set_manapoint(disFloat(gen));
    character.set_speed(disFloat(gen));
    // Set Position
    protobuf_test::Position *position = character.mutable_pos();
    position->set_x(disFloat(gen));
    position->set_y(disFloat(gen));
    position->set_z(disFloat(gen));
    for (auto i = 0; i < 16; ++i)
    {
        // Add Path
        protobuf_test::Position *path1 = character.add_path();
        path1->set_x(disFloat(gen));
        path1->set_y(disFloat(gen));
        path1->set_z(disFloat(gen));
    }
    auto attributes = character.mutable_attributes();
    for (auto i = 0; i < 16; ++i)
    {
        attributes->Add(disFloat(gen));
    }
    auto items = character.mutable_items();
    for (auto i = 0; i < 64; ++i)
    {
        auto uuid = disInt64(gen);
        protobuf_test::Item item1;
        item1.set_id(disInt32(gen));
        item1.set_uuid(disInt64(gen));
        item1.set_number(disInt32(gen));
        item1.set_timestamp(disInt64(gen));
        (*items)[uuid] = item1;
    }
    for (auto i = 0; i < 8; ++i)
    {
        protobuf_test::Equip *equip = character.add_equips();
        equip->set_id(disInt32(gen));
        equip->set_uuid(disInt64(gen));
        equip->set_number(disInt32(gen));
        equip->set_timestamp(disInt64(gen));
        equip->set_level(disInt32(gen));
        auto attributes = equip->mutable_attributes();
        for (auto i = 0; i < 8; ++i)
        {
            attributes->Add(disFloat(gen));
        }
    }
    for (auto i = 0; i < 4; ++i)
    {
        // Add Skills
        protobuf_test::Skill *skill1 = character.add_skills();
        skill1->set_id(disInt32(gen));
        skill1->set_level(disInt32(gen));
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
        character.set_level(character.level() + 1);
        character.set_healthpoint(character.healthpoint() + 1);
        character.set_manapoint(character.manapoint() + 1);
        character.set_speed(character.speed() + 1);
        protobuf_test::Position *position = character.mutable_pos();
        position->set_x(disFloat(gen));
        position->set_y(disFloat(gen));
        position->set_z(disFloat(gen));
        auto equips = character.mutable_equips();
        for (auto i = 0; i < 4; ++i)
        {
            protobuf_test::Equip *equip = character.add_equips();
            equip->set_id(disInt32(gen));
            equip->set_uuid(disInt64(gen));
            equip->set_number(disInt32(gen));
            equip->set_timestamp(disInt64(gen));
            equip->set_level(disInt32(gen));
            auto attributes = equip->mutable_attributes();
            for (auto i = 0; i < 8; ++i)
            {
                attributes->Add(disFloat(gen));
            }
            equips->RemoveLast();
        }
        for (auto i = 0; i < 8; ++i)
        {
            protobuf_test::Position *path1 = character.add_path();
            path1->set_x(disFloat(gen));
            path1->set_y(disFloat(gen));
            path1->set_z(disFloat(gen));
        }
        auto attributes = character.mutable_attributes();
        attributes->Add(disFloat(gen));
        auto items = character.mutable_items();
        auto itemUuid0 = disInt64(gen);
        protobuf_test::Item item;
        item.set_id(disInt32(gen));
        item.set_uuid(disInt64(gen));
        item.set_number(disInt32(gen));
        item.set_timestamp(disInt64(gen));
        (*items)[itemUuid0] = item;
        items->erase(itemUuid0);

        attributes->RemoveLast();
        auto path = character.mutable_path();
        for (auto i = 0; i < 8; ++i)
        {
            path->RemoveLast();
        }
        character.set_level(character.level() - 1);
        character.set_healthpoint(character.healthpoint() - 1);
        character.set_manapoint(character.manapoint() - 1);
        character.set_speed(character.speed() - 1);
    }
    auto finish2 = std::chrono::high_resolution_clock::now();
    auto duration2 = std::chrono::duration_cast<std::chrono::nanoseconds>(finish2 - start2).count();


    // Write the Character message to a file
    // std::ofstream output("character.bin", std::ios::binary);
    // character.SerializeToOstream(&output);
    // output.close();
    // // Read the Character message from the file
    // std::ifstream input("character.bin", std::ios::binary);
    // protobuf_test::Character readCharacter;
    // readCharacter.ParseFromIstream(&input);
    // input.close();
    // Access the fields of the read Character message
    // std::cout << "ID: " << readCharacter.id() << std::endl;
    // std::cout << "Name: " << readCharacter.name() << std::endl;
    // std::cout << "Level: " << readCharacter.level() << std::endl;
    // std::cout << "Health Point: " << readCharacter.healthpoint() << std::endl;
    // std::cout << "Mana Point: " << readCharacter.manapoint() << std::endl;
    // std::cout << "Speed: " << readCharacter.speed() << std::endl;
    // const protobuf_test::Position& readPosition = readCharacter.pos();
    // std::cout << "Position: (" << readPosition.x() << ", " << readPosition.y() << ", " << readPosition.z() << ")" << std::endl;
    // for (const protobuf_test::Position& path : readCharacter.path()) {
    //     std::cout << "Path: (" << path.x() << ", " << path.y() << ", " << path.z() << ")" << std::endl;
    // }
    // for (float attribute : readCharacter.attributes()) {
    //     std::cout << "Attribute: " << attribute << std::endl;
    // }
    // for (const auto& item : readCharacter.items()) {
    //     auto readItem = item.second;
    //     std::cout << "Item ID: " << readItem.id() << std::endl;
    //     std::cout << "Item UUID: " << readItem.uuid() << std::endl;
    //     std::cout << "Item Number: " << readItem.number() << std::endl;
    //     std::cout << "Item Timestamp: " << readItem.timestamp() << std::endl;
    // }
    // for (auto& equip : readCharacter.equips()) {
    //     std::cout << "Equip ID: " << equip.id() << std::endl;
    //     std::cout << "Equip UUID: " << equip.uuid() << std::endl;
    //     std::cout << "Equip Number: " << equip.number() << std::endl;
    //     std::cout << "Equip Timestamp: " << equip.timestamp() << std::endl;
    //     std::cout << "Equip Level: " << equip.level() << std::endl;
    //     for (float attribute : equip.attributes()) {
    //         std::cout << "Equip Attribute: " << attribute << std::endl;
    //     }
    // }
    // for (auto& skill : readCharacter.skills()) {
    //     std::cout << "Skill ID: " << skill.id() << std::endl;
    //     std::cout << "Skill Level: " << skill.level() << std::endl;
    // }

    std::string serialized;
    character.SerializeToString(&serialized);

    // check if we can deserialize back
    Character r2;
    bool ok = r2.ParseFromString(serialized);
    if (!ok /*|| r2 != r1*/)
    {
        throw std::logic_error("protobuf's case: deserialization failed");
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
        serialized.clear();
        // protobuf_test::Character character;
        // gen.seed(0);
        // // Create a Character message
        // character.set_id(disInt64(gen));
        // character.set_name(randomString.c_str());
        // character.set_level(disInt32(gen));
        // character.set_healthpoint(disFloat(gen));
        // character.set_manapoint(disFloat(gen));
        // character.set_speed(disFloat(gen));
        // // Set Position
        // protobuf_test::Position* position = character.mutable_pos();
        // position->set_x(disFloat(gen));
        // position->set_y(disFloat(gen));
        // position->set_z(disFloat(gen));
        // for (auto i = 0; i < 16; ++i)
        // {
        //     // Add Path
        //     protobuf_test::Position* path1 = character.add_path();
        //     path1->set_x(disFloat(gen));
        //     path1->set_y(disFloat(gen));
        //     path1->set_z(disFloat(gen));
        // }
        // auto attributes = character.mutable_attributes();
        // for (auto i = 0; i < 16; ++i)
        // {
        //     attributes->Add(disFloat(gen));
        // }
        // auto items = character.mutable_items();
        // for (auto i = 0; i < 64; ++i)
        // {
        //     auto uuid = disInt64(gen);
        //     protobuf_test::Item item1;
        //     item1.set_id(disInt32(gen));
        //     item1.set_uuid(disInt64(gen));
        //     item1.set_number(disInt32(gen));
        //     item1.set_timestamp(disInt64(gen));
        //     (*items)[uuid] = item1;
        // }
        // for (auto i = 0; i < 8; ++i)
        // {
        //     protobuf_test::Equip* equip = character.add_equips();
        //     equip->set_id(disInt32(gen));
        //     equip->set_uuid(disInt64(gen));
        //     equip->set_number(disInt32(gen));
        //     equip->set_timestamp(disInt64(gen));
        //     equip->set_level(disInt32(gen));
        //     auto attributes = equip->mutable_attributes();
        //     for (auto i = 0; i < 8; ++i)
        //     {
        //         attributes->Add(disFloat(gen));
        //     }
        // }
        // for (auto i = 0; i < 4; ++i)
        // {
        //     // Add Skills
        //     protobuf_test::Skill* skill1 = character.add_skills();
        //     skill1->set_id(disInt32(gen));
        //     skill1->set_level(disInt32(gen));
        // }
        // auto x = disInt32(gen);
        // if (x != 1837058832)  throw std::logic_error("Random number sequence is inconsistent.");
        character.SerializeToString(&serialized);
        r2.ParseFromString(serialized);
    }
    auto finish = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(finish - start).count();
    return benchmark3::Result("protobuf", std::to_string(GOOGLE_PROTOBUF_VERSION), serialized.size(), compressedSize, duration, duration1, duration2);
}

int main()
{
    auto iterNum = 100000;
    for (auto i = 0; i < 2; ++i)
    {
        auto result = msgpack_serialization_test3(iterNum);
        result.print();
        result = protobuf_serialization_test3(iterNum);
        result.print();
    }
    return 0;
}
