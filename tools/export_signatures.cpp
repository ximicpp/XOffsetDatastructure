// ============================================================================
// XOffsetDatastructure Signature Export Tool
//
// Exports type signatures for Player, Item, and GameData to .sig.hpp files.
// Compile with P2996 Clang, run: ./export_signatures [output_dir]
//
// Usage:
//   ./export_signatures              # Print to stdout
//   ./export_signatures tools/sigs/  # Write to tools/sigs/<platform>.sig.hpp
//
// Note: Player, Item, and GameData contain XString/XVector which are
// relocatable opaque types (not trivially copyable).  We use
// add_relocatable<T>() instead of add<T>().
// ============================================================================

#include "../xoffsetdatastructure.hpp"
#include "../examples/player.hpp"
#include "../examples/game_data.hpp"

#include <boost/typelayout/tools/sig_export.hpp>
#include <filesystem>

int main(int argc, char* argv[]) {
    ::boost::typelayout::SigExporter ex;

    // These types contain XString/XVector (relocatable opaque containers),
    // so they are not trivially_copyable.  add_relocatable<T>() skips
    // the trivially_copyable static_assert while still requiring pointer-free.
    ex.add_relocatable<Player>("Player");
    ex.add_relocatable<Item>("Item");
    ex.add_relocatable<GameData>("GameData");

    if (argc >= 2) {
        std::string dir = argv[1];
        std::filesystem::create_directories(dir);
        std::string path = dir;
        if (path.back() != '/') path += '/';
        path += ex.platform_name() + ".sig.hpp";
        return ex.write(path);
    }
    ex.write_stdout();
    return 0;
}