// ============================================================================
// Test: Static Assert with Reflection Hint
// Purpose: Test if static_assert triggers the issue
// ============================================================================

#include <iostream>
#include "../xoffsetdatastructure2.hpp"

using namespace XOffsetDatastructure2;

// Aggregate type (no constructors)
struct ItemReflectionHint {
    int32_t item_id;
    int32_t item_type;
    int32_t quantity;
    XString name;
};

// Aggregate type with vector of ItemReflectionHint
struct GameDataReflectionHint {
    int32_t player_id;
    int32_t level;
    float health;
    XString player_name;
    XVector<ItemReflectionHint> items;  // This line might trigger the issue!
    XSet<int32_t> achievements;
    XMap<XString, int32_t> quest_progress;
};

// This static_assert might trigger template instantiation
static_assert(sizeof(GameDataReflectionHint) > 0, "GameDataReflectionHint has size");

int main() {
    std::cout << "\n[TEST] Static Assert with Reflection Hint\n\n";
    std::cout << "If this compiles, the issue is elsewhere.\n";
    std::cout << "Size: " << sizeof(GameDataReflectionHint) << "\n";
    return 0;
}
