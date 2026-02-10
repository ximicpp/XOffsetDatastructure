// ============================================================================
// XOffsetDatastructure2 Signature Export Tool
//
// Exports type signatures for Player, Item, and GameData to .sig.hpp files.
// Compile with P2996 Clang, run: ./export_signatures [output_dir]
//
// Usage:
//   ./export_signatures              # Print to stdout
//   ./export_signatures tools/sigs/  # Write to tools/sigs/<platform>.sig.hpp
// ============================================================================

#include "../xoffsetdatastructure2.hpp"
#include "../examples/player.hpp"
#include "../examples/game_data.hpp"

#include <boost/typelayout/tools/sig_export.hpp>

TYPELAYOUT_EXPORT_TYPES(
    Player,
    Item,
    GameData
)