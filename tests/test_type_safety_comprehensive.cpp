#include "../xoffsetdatastructure2.hpp"
#include <iostream>
#include <cassert>

using namespace XOffsetDatastructure2;

// ============================================================================
// Test 1: Basic Types (Should PASS)
// ============================================================================

struct BasicTypes {
    int32_t i32;
    int64_t i64;
    uint32_t u32;
    uint64_t u64;
    float f32;
    double f64;
    bool flag;
    char ch;
    
    template<typename Allocator>
    BasicTypes(Allocator) : i32(0), i64(0), u32(0), u64(0), f32(0.0f), f64(0.0), flag(false), ch('\0') {}
};

static_assert(is_xbuffer_safe<BasicTypes>::value, "BasicTypes should be safe");

// ============================================================================
// Test 2: Container Types (Should PASS)
// ============================================================================

struct ContainerTypes {
    XString name;
    XVector<int32_t> numbers;
    XSet<int32_t> unique_ids;
    XMap<int32_t, XString> id_to_name;
    
    template<typename Allocator>
    ContainerTypes(Allocator alloc) 
        : name(alloc), numbers(alloc), unique_ids(alloc), id_to_name(alloc) {}
};

static_assert(is_xbuffer_safe<ContainerTypes>::value, "ContainerTypes should be safe");

// ============================================================================
// Test 3: Nested Containers (Should PASS)
// ============================================================================

struct NestedContainers {
    XVector<XVector<int32_t>> matrix;
    XMap<XString, XVector<int32_t>> name_to_numbers;
    XVector<XString> strings;
    
    template<typename Allocator>
    NestedContainers(Allocator alloc) 
        : matrix(alloc), name_to_numbers(alloc), strings(alloc) {}
};

static_assert(is_xbuffer_safe<NestedContainers>::value, "NestedContainers should be safe");

// ============================================================================
// Test 4: User-Defined Nested Types (Should PASS)
// ============================================================================

struct Point {
    float x;
    float y;
    float z;
};

struct Player {
    XString name;
    int32_t level;
    Point position;
    XVector<int32_t> inventory;
    
    template<typename Allocator>
    Player(Allocator alloc) 
        : name(alloc), level(0), position{}, inventory(alloc) {}
};

static_assert(is_xbuffer_safe<Point>::value, "Point should be safe");
static_assert(is_xbuffer_safe<Player>::value, "Player should be safe");

// ============================================================================
// Test 5: Complex Nested Structure (Should PASS)
// ============================================================================

struct Item {
    int32_t id;
    XString name;
    float weight;
    
    Item() = default;
    Item(int32_t i, XString n, float w) : id(i), name(n), weight(w) {}
};

struct Inventory {
    XVector<Item> items;
    XMap<int32_t, Item> quick_access;
    
    template<typename Allocator>
    Inventory(Allocator alloc) 
        : items(alloc), quick_access(alloc) {}
};

struct GameState {
    Player player;
    Inventory inventory;
    XVector<Point> waypoints;
    
    template<typename Allocator>
    GameState(Allocator alloc) 
        : player(alloc), inventory(alloc), waypoints(alloc) {}
};

static_assert(is_xbuffer_safe<Item>::value, "Item should be safe");
static_assert(is_xbuffer_safe<Inventory>::value, "Inventory should be safe");
static_assert(is_xbuffer_safe<GameState>::value, "GameState should be safe");

// ============================================================================
// Test 6: Types that SHOULD FAIL
// ============================================================================

// 6.1: Polymorphic type (has virtual function)
struct PolymorphicType {
    virtual void foo() {}  // Virtual function = NOT ALLOWED
    int32_t data;
};

static_assert(!is_xbuffer_safe<PolymorphicType>::value, 
    "PolymorphicType should NOT be safe (has virtual function)");

// 6.2: Contains raw pointer
struct WithRawPointer {
    int32_t* ptr;  // Raw pointer = NOT ALLOWED
    int32_t data;
};

static_assert(!is_xbuffer_safe<WithRawPointer>::value, 
    "WithRawPointer should NOT be safe (has raw pointer)");

// 6.3: Contains std::string
struct WithStdString {
    std::string name;  // std::string = NOT ALLOWED
    int32_t id;
};

static_assert(!is_xbuffer_safe<WithStdString>::value, 
    "WithStdString should NOT be safe (uses std::string)");

// 6.4: Contains std::vector
struct WithStdVector {
    std::vector<int32_t> data;  // std::vector = NOT ALLOWED
};

static_assert(!is_xbuffer_safe<WithStdVector>::value, 
    "WithStdVector should NOT be safe (uses std::vector)");

// 6.5: Nested unsafe type
struct UnsafeNested {
    Point position;  // Safe
    PolymorphicType bad;  // NOT SAFE - contains polymorphic type
};

static_assert(!is_xbuffer_safe<UnsafeNested>::value, 
    "UnsafeNested should NOT be safe (contains unsafe member)");

// ============================================================================
// Runtime Tests
// ============================================================================

void test_basic_types() {
    std::cout << "\n[Test] Basic Types..." << std::endl;
    
    XBufferExt xbuf(1024 * 1024);
    auto* data = xbuf.make<BasicTypes>("basic");
    
    data->i32 = -123;
    data->i64 = -9876543210LL;
    data->u32 = 456;
    data->u64 = 9876543210ULL;
    data->f32 = 3.14f;
    data->f64 = 2.718281828;
    data->flag = true;
    data->ch = 'X';
    
    // Verify
    assert(data->i32 == -123);
    assert(data->u64 == 9876543210ULL);
    assert(data->flag == true);
    assert(data->ch == 'X');
    
    std::cout << "  ✓ All basic types work correctly" << std::endl;
}

void test_container_types() {
    std::cout << "\n[Test] Container Types..." << std::endl;
    
    XBufferExt xbuf(1024 * 1024);
    auto* data = xbuf.make<ContainerTypes>("containers");
    
    // Test XString
    data->name = XString("TestName", xbuf.allocator<char>());
    
    // Test XVector
    data->numbers.push_back(10);
    data->numbers.push_back(20);
    data->numbers.push_back(30);
    
    // Test XSet
    data->unique_ids.insert(100);
    data->unique_ids.insert(200);
    data->unique_ids.insert(100);  // Duplicate, should be ignored
    
    // Test XMap - use emplace like in demo
    data->id_to_name.emplace(1, XString("Alice", xbuf.allocator<XString>()));
    data->id_to_name.emplace(2, XString("Bob", xbuf.allocator<XString>()));
    
    // Verify
    assert(data->name == "TestName");
    assert(data->numbers.size() == 3);
    assert(data->numbers[0] == 10);
    assert(data->unique_ids.size() == 2);  // Only unique values
    assert(data->id_to_name.size() == 2);
    assert(data->id_to_name[1] == "Alice");
    
    std::cout << "  ✓ All container types work correctly" << std::endl;
}

void test_nested_types() {
    std::cout << "\n[Test] Nested User Types..." << std::endl;
    
    XBufferExt xbuf(1024 * 1024);
    auto* player = xbuf.make<Player>("player");
    
    player->name = XString("Hero", xbuf.allocator<char>());
    player->level = 99;
    player->position.x = 10.5f;
    player->position.y = 20.3f;
    player->position.z = -5.7f;
    player->inventory.push_back(1001);
    player->inventory.push_back(2002);
    
    // Verify
    assert(player->name == "Hero");
    assert(player->level == 99);
    assert(player->position.x == 10.5f);
    assert(player->inventory.size() == 2);
    assert(player->inventory[1] == 2002);
    
    std::cout << "  ✓ Nested user types work correctly" << std::endl;
}

void test_complex_structure() {
    std::cout << "\n[Test] Complex Nested Structure..." << std::endl;
    
    XBufferExt xbuf(1024 * 1024);
    auto* game = xbuf.make<GameState>("game");
    
    // Setup player
    game->player.name = XString("Alice", xbuf.allocator<char>());
    game->player.level = 50;
    game->player.position = {1.0f, 2.0f, 3.0f};
    
    // Setup inventory
    Item sword{1, XString("Sword", xbuf.allocator<char>()), 5.5f};
    Item shield{2, XString("Shield", xbuf.allocator<char>()), 8.2f};
    
    game->inventory.items.push_back(sword);
    game->inventory.items.push_back(shield);
    game->inventory.quick_access.emplace(1, sword);
    
    // Setup waypoints
    game->waypoints.push_back({0.0f, 0.0f, 0.0f});
    game->waypoints.push_back({10.0f, 20.0f, 30.0f});
    
    // Verify
    assert(game->player.name == "Alice");
    assert(game->inventory.items.size() == 2);
    assert(game->inventory.items[0].id == 1);
    assert(game->inventory.quick_access[1].name == "Sword");
    assert(game->waypoints.size() == 2);
    assert(game->waypoints[1].x == 10.0f);
    
    std::cout << "  ✓ Complex structure works correctly" << std::endl;
}

void print_type_safety_info() {
    std::cout << "\n========================================" << std::endl;
    std::cout << "Type Safety Validation Results" << std::endl;
    std::cout << "========================================" << std::endl;
    
    std::cout << "\n✅ SAFE TYPES (Allowed):\n";
    std::cout << "  - BasicTypes:        " << (is_xbuffer_safe<BasicTypes>::value ? "SAFE" : "UNSAFE") << "\n";
    std::cout << "  - ContainerTypes:    " << (is_xbuffer_safe<ContainerTypes>::value ? "SAFE" : "UNSAFE") << "\n";
    std::cout << "  - NestedContainers:  " << (is_xbuffer_safe<NestedContainers>::value ? "SAFE" : "UNSAFE") << "\n";
    std::cout << "  - Point:             " << (is_xbuffer_safe<Point>::value ? "SAFE" : "UNSAFE") << "\n";
    std::cout << "  - Player:            " << (is_xbuffer_safe<Player>::value ? "SAFE" : "UNSAFE") << "\n";
    std::cout << "  - GameState:         " << (is_xbuffer_safe<GameState>::value ? "SAFE" : "UNSAFE") << "\n";
    
    std::cout << "\n❌ UNSAFE TYPES (Forbidden):\n";
    std::cout << "  - PolymorphicType:   " << (is_xbuffer_safe<PolymorphicType>::value ? "SAFE" : "UNSAFE") << "\n";
    std::cout << "  - WithRawPointer:    " << (is_xbuffer_safe<WithRawPointer>::value ? "SAFE" : "UNSAFE") << "\n";
    std::cout << "  - WithStdString:     " << (is_xbuffer_safe<WithStdString>::value ? "SAFE" : "UNSAFE") << "\n";
    std::cout << "  - WithStdVector:     " << (is_xbuffer_safe<WithStdVector>::value ? "SAFE" : "UNSAFE") << "\n";
    std::cout << "  - UnsafeNested:      " << (is_xbuffer_safe<UnsafeNested>::value ? "SAFE" : "UNSAFE") << "\n";
    
    std::cout << "\n========================================" << std::endl;
}

int main() {
    std::cout << "╔══════════════════════════════════════════╗" << std::endl;
    std::cout << "║  XBuffer Type Safety Comprehensive Test  ║" << std::endl;
    std::cout << "╚══════════════════════════════════════════╝" << std::endl;
    
    // Compile-time validation summary
    print_type_safety_info();
    
    // Runtime tests
    test_basic_types();
    test_container_types();
    test_nested_types();
    test_complex_structure();
    
    std::cout << "\n╔══════════════════════════════════════════╗" << std::endl;
    std::cout << "║        ✓ ALL TESTS PASSED!               ║" << std::endl;
    std::cout << "╚══════════════════════════════════════════╝" << std::endl;
    
    return 0;
}
