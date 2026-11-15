// ============================================================================
// XOffsetDatastructure Hello World
// The recommended style (README.md, docs/USER_GUIDE.md section 3): one
// XOFFSET_RECORD line per record, TypedXBuffer<T>, the root() proxy and its
// field accessors, edit() for hot loops, save_verified() and load().
// Builds the same with C++20, C++23 and C++26.
// ============================================================================

#include "../xoffsetdatastructure.hpp"

#include <cstddef>
#include <cstdint>
#include <iostream>
#include <string>

using namespace XOffsetDatastructure;

// A plain aggregate with library containers inside: the bytes of the buffer
// that holds it are its wire form.
struct Player {
    std::int32_t id{};
    std::int32_t level{};
    XString name;
    XVector<std::int32_t> items;
};
// Global scope, after the type: the field list (needed without C++26
// reflection; checked against reflection with it) and the schema name.
XOFFSET_RECORD(Player, id, level, name, items)
XOFFSET_REGISTER_SCHEMA_NAME(Player, "example.Player")

int main() {
    constexpr std::size_t limit = 8 * 1024 * 1024;   // hard per-buffer limit

    // 1. A buffer whose root is a Player.
    auto buffer = TypedXBuffer<Player>::create(4096, XBuffer::maximum_capacity(limit));

    // 2. Write through the proxy: it stays valid if the buffer moves to a
    //    larger reservation, and a failed allocation is recorded in
    //    buffer.error() instead of throwing.
    auto player = buffer.root();
    player.id() = 1;
    player.level() = 10;
    player.name() = "Alice";
    for (std::int32_t item : {101, 102, 103}) player.items().push_back(item);

    // 3. Hot loops: a pin scope hands out plain references at plain speed.
    buffer.edit([](Player& p) {
        p.level += 1;
        for (std::int32_t& item : p.items) item += 1000;
    });
    if (!buffer.ok()) {
        std::cerr << "write failed: " << static_cast<int>(buffer.error()) << "\n";
        return 1;
    }

    // 4. The wire: an owning copy of the arena bytes, no per-field encoding.
    std::string wire = buffer.save_verified();
    std::cout << "wire: " << wire.size() << " bytes\n";

    // 5. The receiver checks the whole wire, then reads plain members.
    auto restored = TypedXBuffer<Player>::load(wire, XBuffer::maximum_capacity(limit));
    const bool same = restored.read([](const Player& p) {
        std::cout << "loaded: " << std::string(p.name.view()) << ", id " << p.id
                  << ", level " << p.level << ", items";
        for (std::int32_t item : p.items) std::cout << " " << item;
        std::cout << "\n";
        return p.id == 1 && p.level == 11 && p.items.size() == 3 && p.items[0] == 1101;
    });
    std::cout << (same ? "OK\n" : "MISMATCH\n");
    return same ? 0 : 1;
}
