// tools/schema_freeze (xoffset_schema_freeze.hpp): freezing two releases
// writes the checked-in snapshots and lock file byte for byte (the snapshots
// are compiled into this test, so their pins hold), the second snapshot
// refers to the first for unchanged types, and `check` passes or fails as the
// lock rules say.
//
//   test_schema_freeze <source dir of tests/schema_freeze>
#include "schema_freeze/schemas.hpp"
#include "schema_freeze/game_v2.hpp"
#include "../xoffset_schema_freeze.hpp"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <unistd.h>

using namespace XOffsetDatastructure;

namespace {
int failures = 0;
#define CHECK(cond)                                                                \
    do {                                                                           \
        if (!(cond)) {                                                             \
            std::fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #cond); \
            ++failures;                                                            \
        }                                                                          \
    } while (0)

std::string read_file(const std::filesystem::path& p) {
    std::ifstream in(p, std::ios::binary);
    std::ostringstream s;
    s << in.rdbuf();
    return s.str();
}
} // namespace

// Release 3 in development: changed layout, both releases in its lineage.
namespace game3 {
using namespace XOffsetDatastructure;
struct Player {
    XString name;
    std::int64_t level;
    std::int32_t slots[2][3];
    XVector<game1::Item> items;
    XMap<std::int32_t, XString> notes;
    XSet<game1::Rarity> seen;
    XVector<std::byte> blob;
    std::int32_t guild_id;
};
}
// Changed layout, but release 2 is missing from the lineage.
namespace game4 {
using namespace XOffsetDatastructure;
struct Player { XString name; std::int64_t level; std::int32_t extra; };
}
// Release 2's layout with a field renamed (level -> rank).
namespace game_renamed {
using namespace XOffsetDatastructure;
struct Player {
    XString name;
    std::int64_t rank;
    std::int32_t slots[2][3];
    XVector<game1::Item> items;
    XMap<std::int32_t, XString> notes;
    XSet<game1::Rarity> seen;
    XVector<std::byte> blob;
};
}
// The same rename, declared as keeping the bytes' meaning.
namespace game_aliased {
using namespace XOffsetDatastructure;
struct Player {
    XString name;
    std::int64_t rank;
    std::int32_t slots[2][3];
    XVector<game1::Item> items;
    XMap<std::int32_t, XString> notes;
    XSet<game1::Rarity> seen;
    XVector<std::byte> blob;
    // same layout as game_renamed::Player; distinct type for its own alias
};
}

XOFFSET_REGISTER_SCHEMA_NAME(game3::Player, "test.freeze.Player")
XOFFSET_REGISTER_SCHEMA_NAME(game4::Player, "test.freeze.Player")
XOFFSET_REGISTER_SCHEMA_NAME(game_renamed::Player, "test.freeze.Player")
XOFFSET_REGISTER_SCHEMA_NAME(game_aliased::Player, "test.freeze.Player")
XOFFSET_SCHEMA_LINEAGE(game2::Player, game_v1::Player)
XOFFSET_SCHEMA_LINEAGE(game3::Player, game_v1::Player, game_v2::Player)
XOFFSET_SCHEMA_LINEAGE(game4::Player, game_v1::Player)
XOFFSET_SCHEMA_NAME_ALIAS(game_aliased::Player, ::XOffsetDatastructure::schema_name_digest_v<game2::Player>)

// The snapshots have the layouts of the releases they froze.
static_assert(wire_schema_hash_v<game_v1::Player>() == wire_schema_hash_v<game1::Player>());
static_assert(wire_schema_hash_v<game_v2::Player>() == wire_schema_hash_v<game2::Player>());
static_assert(schema_name_digest_v<game_v2::Player> == schema_name_digest_v<game2::Player>);
static_assert(std::is_same_v<game_v2::Item, game_v1::Item>, "unchanged types refer to the previous snapshot");
static_assert(std::is_same_v<game_v2::Rarity, game_v1::Rarity>);

int main(int argc, char** argv) {
    namespace fs = std::filesystem;
    if (argc != 2) {
        std::fprintf(stderr, "usage: %s <tests/schema_freeze source dir>\n", argv[0]);
        return 2;
    }
    const fs::path golden = fs::absolute(argv[1]);
    const fs::path work = fs::temp_directory_path() / ("xoffset_schema_freeze_" + std::to_string(::getpid()));
    fs::remove_all(work);
    fs::create_directories(work / "tests" / "schema_freeze");
    fs::current_path(work);

    const std::string lock = "tests/schema_freeze/schema.lock";
    CHECK(detail::freeze::freeze<game1::Player>("v1", "game_v1", "tests/schema_freeze/game_v1.hpp", lock) == 0);
    CHECK(detail::freeze::freeze<game2::Player>("v2", "game_v2", "tests/schema_freeze/game_v2.hpp", lock) == 0);
    for (const char* f : {"game_v1.hpp", "game_v2.hpp", "schema.lock"}) {
        const bool same = read_file(work / "tests" / "schema_freeze" / f) == read_file(golden / f);
        if (!same) std::fprintf(stderr, "%s differs from the checked-in copy\n", f);
        CHECK(same);
    }
    // A version or namespace already in the lock is refused.
    CHECK(detail::freeze::freeze<game2::Player>("v2", "game_v9", "x.hpp", lock) == 2);
    CHECK(detail::freeze::freeze<game2::Player>("v9", "game_v2", "x.hpp", lock) == 2);

    CHECK(detail::freeze::check<game2::Player>(lock) == 0);        // equals v2
    CHECK(detail::freeze::check<game3::Player>(lock) == 0);        // next version, v2 in the lineage
    CHECK(detail::freeze::check<game4::Player>(lock) == 1);        // v2 not in the lineage
    CHECK(detail::freeze::check<game_renamed::Player>(lock) == 1); // same layout as v2, other names
    CHECK(detail::freeze::check<game_aliased::Player>(lock) == 0); // ... unless aliased
    CHECK(detail::freeze::check<game2::Player>("does-not-exist.lock") == 0);  // nothing frozen yet

    // The command line.
    char prog[] = "schema_freeze", check[] = "check", lockarg[] = "tests/schema_freeze/schema.lock", bad[] = "bogus";
    char* ok_args[] = {prog, check, lockarg};
    CHECK(schema_freeze_main<game2::Player>(3, ok_args) == 0);
    char* bad_args[] = {prog, bad};
    CHECK(schema_freeze_main<game2::Player>(2, bad_args) == 2);
    {
        std::ofstream(lock, std::ios::app) << "garbage line\n";
        CHECK(detail::freeze::check<game2::Player>(lock) == 2);
    }

    fs::current_path(golden);
    fs::remove_all(work);
    if (failures) {
        std::fprintf(stderr, "test_schema_freeze: %d failure(s)\n", failures);
        return 1;
    }
    std::puts("test_schema_freeze: all passed");
    return 0;
}
