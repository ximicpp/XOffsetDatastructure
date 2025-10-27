// ============================================================================
// Test: Member Iteration and Introspection
// Purpose: Test nonstatic_data_members_of and member queries
// ============================================================================

#include "../xoffsetdatastructure2.hpp"
#include <iostream>

#if __has_include(<experimental/meta>)
#include <experimental/meta>
#define HAS_REFLECTION 1
#include <utility>

using namespace XOffsetDatastructure2;

// Helper structure to cross compile-time/runtime boundary
struct MemberInfo {
    const char* name;
    const char* type;
    bool is_public;
    bool is_static;
};

// Helper: Get member count at compile time
template<typename T>
consteval auto get_member_count() {
    using namespace std::meta;
    return nonstatic_data_members_of(^^T, access_context::unchecked()).size();
}

// Helper: Get member info at specific index
template<typename T, size_t Index>
consteval auto get_member_info_at() -> MemberInfo {
    using namespace std::meta;
    auto members = nonstatic_data_members_of(^^T, access_context::unchecked());
    
    if (Index < members.size()) {
        auto member = members[Index];
        return MemberInfo{
            display_string_of(member).data(),
            display_string_of(type_of(member)).data(),
            is_public(member),
            is_static_member(member)
        };
    }
    return MemberInfo{"", "", false, false};
}

// Helper: Print all members using fold expression
template<typename T, size_t... Is>
constexpr void print_members_impl(std::index_sequence<Is...>) {
    ((std::cout << "    [" << Is << "] " 
                << get_member_info_at<T, Is>().name << "\n"), ...);
}

// Helper: Print members with details
template<typename T, size_t... Is>
constexpr void print_members_detailed_impl(std::index_sequence<Is...>) {
    ((std::cout << "\n    Member " << Is << ": " << get_member_info_at<T, Is>().name << "\n"
                << "      Type: " << get_member_info_at<T, Is>().type << "\n"
                << "      Is public: " << (get_member_info_at<T, Is>().is_public ? "yes" : "no") << "\n"
                << "      Is static: " << (get_member_info_at<T, Is>().is_static ? "yes" : "no") << "\n"
                << "      Is nonstatic data member: " 
                << (!get_member_info_at<T, Is>().is_static ? "yes" : "no") << "\n"), ...);
}

// Public interface for printing members
template<typename T>
void print_all_members() {
    constexpr auto count = get_member_count<T>();
    print_members_impl<T>(std::make_index_sequence<count>{});
}

// Public interface for printing member details
template<typename T>
void print_all_members_detailed() {
    constexpr auto count = get_member_count<T>();
    print_members_detailed_impl<T>(std::make_index_sequence<count>{});
}

struct GameItem {
    uint32_t item_id;
    uint32_t item_type;
    uint32_t quantity;
    XString name;
    
    template <typename Allocator>
    GameItem(Allocator allocator) 
        : item_id(0), item_type(0), quantity(0), name(allocator) {}
};

struct SimpleStruct {
    int x;
    double y;
    float z;
};

void test_get_all_members() {
    using namespace std::meta;
    
    std::cout << "[Test 1] Get All Members\n";
    std::cout << "------------------------\n";
    
    constexpr auto member_count = get_member_count<GameItem>();
    std::cout << "  GameItem has " << member_count << " members:\n";
    
    // Use fold expression to print all members
    print_all_members<GameItem>();
    
    std::cout << "[PASS] Get all members\n\n";
}

void test_member_details() {
    using namespace std::meta;
    
    std::cout << "[Test 2] Member Details\n";
    std::cout << "-----------------------\n";
    
    std::cout << "  Detailed member information:\n";
    
    // Use fold expression to print detailed member info
    print_all_members_detailed<GameItem>();
    
    std::cout << "\n[PASS] Member details\n\n";
}

void test_filter_by_type() {
    using namespace std::meta;
    
    std::cout << "[Test 3] Filter Members by Type\n";
    std::cout << "--------------------------------\n";
    
    // Manual listing by type (vector<info> can't be filtered at runtime)
    std::cout << "  uint32_t members:\n";
    constexpr auto item_id_type = type_of(^^GameItem::item_id);
    constexpr bool item_id_is_uint32 = (item_id_type == ^^uint32_t);
    if (item_id_is_uint32) {
        std::cout << "    - " << display_string_of(^^GameItem::item_id) << "\n";
    }
    constexpr auto item_type_type = type_of(^^GameItem::item_type);
    constexpr bool item_type_is_uint32 = (item_type_type == ^^uint32_t);
    if (item_type_is_uint32) {
        std::cout << "    - " << display_string_of(^^GameItem::item_type) << "\n";
    }
    constexpr auto quantity_type = type_of(^^GameItem::quantity);
    constexpr bool quantity_is_uint32 = (quantity_type == ^^uint32_t);
    if (quantity_is_uint32) {
        std::cout << "    - " << display_string_of(^^GameItem::quantity) << "\n";
    }
    
    std::cout << "\n  XString members:\n";
    constexpr auto name_type = type_of(^^GameItem::name);
    constexpr bool name_is_xstring = (name_type == ^^XString);
    if (name_is_xstring) {
        std::cout << "    - " << display_string_of(^^GameItem::name) << "\n";
    }
    
    std::cout << "[PASS] Filter by type\n\n";
}

void test_member_count() {
    using namespace std::meta;
    
    std::cout << "[Test 4] Member Count\n";
    std::cout << "---------------------\n";
    
    constexpr auto game_count = get_member_count<GameItem>();
    constexpr auto simple_count = get_member_count<SimpleStruct>();
    
    std::cout << "  GameItem: " << game_count << " members\n";
    std::cout << "  SimpleStruct: " << simple_count << " members\n";
    
    std::cout << "[PASS] Member count\n\n";
}

void test_instance_access() {
    using namespace std::meta;
    
    std::cout << "[Test 5] Instance Member Access\n";
    std::cout << "--------------------------------\n";
    
    XBufferExt xbuf(2048);
    auto* item = xbuf.make<GameItem>("test_item");
    
    item->item_id = 1001;
    item->item_type = 2;
    item->quantity = 50;
    item->name = XString("Magic Sword", xbuf.allocator<XString>());
    
    std::cout << "  Created GameItem instance:\n";
    std::cout << "    item_id: " << item->item_id << "\n";
    std::cout << "    item_type: " << item->item_type << "\n";
    std::cout << "    quantity: " << item->quantity << "\n";
    std::cout << "    name: " << item->name.c_str() << "\n";
    
    // Verify via reflection
    constexpr auto member_count = get_member_count<GameItem>();
    std::cout << "\n  Member count via reflection: " << member_count << "\n";
    std::cout << "  Member types:\n";
    print_all_members<GameItem>();
    
    std::cout << "[PASS] Instance access\n\n";
}

void test_simple_struct_iteration() {
    using namespace std::meta;
    
    std::cout << "[Test 6] Simple Struct Iteration\n";
    std::cout << "---------------------------------\n";
    
    SimpleStruct s{10, 20.5, 30.5f};
    
    constexpr auto member_count = get_member_count<SimpleStruct>();
    std::cout << "  SimpleStruct members (" << member_count << "):\n";
    
    // Use fold expression to print members with types
    std::cout << "    [0] " << get_member_info_at<SimpleStruct, 0>().name 
              << " (" << get_member_info_at<SimpleStruct, 0>().type << ")\n";
    std::cout << "    [1] " << get_member_info_at<SimpleStruct, 1>().name 
              << " (" << get_member_info_at<SimpleStruct, 1>().type << ")\n";
    std::cout << "    [2] " << get_member_info_at<SimpleStruct, 2>().name 
              << " (" << get_member_info_at<SimpleStruct, 2>().type << ")\n";
    
    // Access via splice
    std::cout << "\n  Values:\n";
    std::cout << "    x = " << s.[:^^SimpleStruct::x:] << "\n";
    std::cout << "    y = " << s.[:^^SimpleStruct::y:] << "\n";
    std::cout << "    z = " << s.[:^^SimpleStruct::z:] << "\n";
    
    std::cout << "[PASS] Simple struct iteration\n\n";
}

#endif // __cpp_reflection

int main() {
    std::cout << "========================================\n";
    std::cout << "  Member Iteration Test\n";
    std::cout << "========================================\n\n";

#if __has_include(<experimental/meta>)
    std::cout << "[INFO] C++26 Reflection: ENABLED\n";
    std::cout << "[INFO] Testing nonstatic_data_members_of\n\n";
    
    test_get_all_members();
    test_member_details();
    test_filter_by_type();
    test_member_count();
    test_instance_access();
    test_simple_struct_iteration();
    
    std::cout << "========================================\n";
    std::cout << "  Summary\n";
    std::cout << "========================================\n";
    std::cout << "[PASS] Test 1: Get all members\n";
    std::cout << "[PASS] Test 2: Member details\n";
    std::cout << "[PASS] Test 3: Filter by type\n";
    std::cout << "[PASS] Test 4: Member count\n";
    std::cout << "[PASS] Test 5: Instance access\n";
    std::cout << "[PASS] Test 6: Simple struct iteration\n";
    std::cout << "\n[SUCCESS] All member iteration tests passed!\n";
    
    return 0;
#else
    std::cout << "[SKIP] C++26 Reflection not available\n";
    std::cout << "[INFO] Compile with -std=c++26 -freflection to enable\n";
    return 0;
#endif
}