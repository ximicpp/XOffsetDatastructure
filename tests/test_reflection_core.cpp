// ============================================================================
// Test: Reflection Core — P2996 Basics
// Purpose: Tests ^^ reflection operator, member introspection, and [: :] splice.
//
// Consolidated from:
//   - test_reflection_operators.cpp  (type/member/builtin/container reflection)
//   - test_member_iteration.cpp      (nonstatic_data_members_of, member queries)
//   - test_splice_operations.cpp     (direct/pointer/type/expression splice)
// ============================================================================

#include "../xoffsetdatastructure.hpp"
#include <iostream>
#include <experimental/meta>
#include <utility>
#include <cassert>

using namespace XOffsetDatastructure;
using namespace std::meta;

// ---------------------------------------------------------------------------
// Shared test structures
// ---------------------------------------------------------------------------

struct Point { int x; int y; };

struct GameItem {
    uint32_t item_id;
    uint32_t item_type;
    uint32_t quantity;
    XString  name;

    template <typename Allocator>
    GameItem(Allocator allocator)
        : item_id(0), item_type(0), quantity(0), name(allocator) {}
};

struct SimpleStruct { int x; double y; float z; };

// ---------------------------------------------------------------------------
// Compile-time helpers
// ---------------------------------------------------------------------------

template<typename T>
consteval size_t member_count() {
    return nonstatic_data_members_of(^^T, access_context::unchecked()).size();
}

struct MemberInfo { const char* name; const char* type; };

template<typename T, size_t I>
consteval MemberInfo member_info_at() {
    auto members = nonstatic_data_members_of(^^T, access_context::unchecked());
    if (I < members.size()) {
        auto m = members[I];
        return { display_string_of(m).data(), display_string_of(type_of(m)).data() };
    }
    return {"", ""};
}

// ---------------------------------------------------------------------------
// Test 1: Type & Member Reflection (^^ operator)
// ---------------------------------------------------------------------------

void test_type_and_member_reflection() {
    std::cout << "[Test 1] Type & Member Reflection\n";
    std::cout << std::string(40, '-') << "\n";

    // Type reflection
    constexpr auto game_refl = ^^GameItem;
    std::cout << "  GameItem: " << display_string_of(game_refl) << "\n";

    // Member reflection
    constexpr auto id_refl   = ^^GameItem::item_id;
    constexpr auto name_refl = ^^GameItem::name;
    std::cout << "  item_id type:  " << display_string_of(type_of(id_refl)) << "\n";
    std::cout << "  name type:     " << display_string_of(type_of(name_refl)) << "\n";

    // Built-in types
    std::cout << "  int:     " << display_string_of(^^int) << "\n";
    std::cout << "  double:  " << display_string_of(^^double) << "\n";
    std::cout << "  uint32:  " << display_string_of(^^uint32_t) << "\n";

    // Container types
    std::cout << "  XVector<int>:       " << display_string_of(^^XVector<int>) << "\n";
    std::cout << "  XMap<int,double>:   " << display_string_of(^^XMap<int, double>) << "\n";

    std::cout << "[PASS]\n\n";
}

// ---------------------------------------------------------------------------
// Test 2: Member Iteration (nonstatic_data_members_of)
// ---------------------------------------------------------------------------

void test_member_iteration() {
    std::cout << "[Test 2] Member Iteration\n";
    std::cout << std::string(40, '-') << "\n";

    constexpr auto gc = member_count<GameItem>();
    constexpr auto sc = member_count<SimpleStruct>();
    std::cout << "  GameItem members:     " << gc << "\n";
    std::cout << "  SimpleStruct members: " << sc << "\n";

    // Print GameItem members with type info
    std::cout << "  GameItem details:\n";
    std::cout << "    [0] " << member_info_at<GameItem,0>().name
              << " : " << member_info_at<GameItem,0>().type << "\n";
    std::cout << "    [1] " << member_info_at<GameItem,1>().name
              << " : " << member_info_at<GameItem,1>().type << "\n";
    std::cout << "    [2] " << member_info_at<GameItem,2>().name
              << " : " << member_info_at<GameItem,2>().type << "\n";
    std::cout << "    [3] " << member_info_at<GameItem,3>().name
              << " : " << member_info_at<GameItem,3>().type << "\n";

    // Filter by type — use dealias_of to resolve typedef aliases
    constexpr bool id_is_u32 = (dealias(type_of(^^GameItem::item_id)) == dealias(^^uint32_t));
    constexpr bool name_is_xs = (dealias(type_of(^^GameItem::name)) == dealias(^^XString));
    static_assert(id_is_u32,  "item_id must be uint32_t");
    static_assert(name_is_xs, "name must be XString");
    std::cout << "  item_id is uint32_t: " << id_is_u32 << "\n";
    std::cout << "  name is XString:     " << name_is_xs << "\n";

    std::cout << "[PASS]\n\n";
}

// ---------------------------------------------------------------------------
// Test 3: Splice — Direct, Pointer, Type, Expression
// ---------------------------------------------------------------------------

void test_splice_operations() {
    std::cout << "[Test 3] Splice Operations [: :]\n";
    std::cout << std::string(40, '-') << "\n";

    // 3a. Direct member splice (read + write)
    Point p{10, 20};
    p.[:^^Point::x:] = 100;
    p.[:^^Point::y:] = 200;
    assert(p.x == 100 && p.y == 200);
    std::cout << "  Direct splice:  (" << p.x << ", " << p.y << ") [OK]\n";

    // 3b. Member-pointer splice
    int Point::*xp = &[:^^Point::x:];
    int Point::*yp = &[:^^Point::y:];
    Point p2{30, 40};
    p2.*xp = 300; p2.*yp = 400;
    assert(p2.x == 300 && p2.y == 400);
    std::cout << "  Pointer splice: (" << p2.x << ", " << p2.y << ") [OK]\n";

    // 3c. Type splice
    using PointAlias = [:^^Point:];
    PointAlias p3{50, 60};
    assert(p3.x == 50);
    std::cout << "  Type splice:    (" << p3.x << ", " << p3.y << ") [OK]\n";

    // 3d. Expression splice
    Point p4{15, 25};
    auto sum  = p4.[:^^Point::x:] + p4.[:^^Point::y:];
    auto prod = p4.[:^^Point::x:] * p4.[:^^Point::y:];
    assert(sum == 40 && prod == 375);
    std::cout << "  Expr splice:    sum=" << sum << " prod=" << prod << " [OK]\n";

    // 3e. Const member splice
    struct ConstTest { int normal; const int constant = 42; };
    ConstTest ct{10, 42};
    ct.[:^^ConstTest::normal:] = 20;
    assert(ct.normal == 20 && ct.[:^^ConstTest::constant:] == 42);
    std::cout << "  Const splice:   normal=" << ct.normal
              << " const=" << ct.constant << " [OK]\n";

    std::cout << "[PASS]\n\n";
}

// ---------------------------------------------------------------------------
// Test 4: Reflection with XBuffer Instances
// ---------------------------------------------------------------------------

void test_reflection_with_xbuffer() {
    std::cout << "[Test 4] Reflection with XBuffer\n";
    std::cout << std::string(40, '-') << "\n";

    XBuffer xbuf(2048);
    auto* item = xbuf.make<GameItem>();
    item->item_id   = 1001;
    item->item_type = 2;
    item->quantity   = 50;
    item->name       = "Sword";

    // Read via reflection
    assert((*item).[:^^GameItem::item_id:] == 1001);
    assert((*item).[:^^GameItem::quantity:] == 50);

    // Write via splice
    (*item).[:^^GameItem::item_id:] = 2002;
    assert(item->item_id == 2002);

    std::cout << "  item_id via splice: " << item->item_id << " [OK]\n";
    std::cout << "  quantity via splice: " << item->quantity << " [OK]\n";

    // SimpleStruct splice on stack
    SimpleStruct s{10, 20.5, 30.5f};
    assert(s.[:^^SimpleStruct::x:] == 10);
    assert(s.[:^^SimpleStruct::y:] == 20.5);
    std::cout << "  SimpleStruct splice: x=" << s.x << " y=" << s.y << " [OK]\n";

    std::cout << "[PASS]\n\n";
}

// ---------------------------------------------------------------------------
// Test 5: Member Property Queries (is_public, is_nonstatic_data_member)
// Merged from test_type_introspection.cpp
// ---------------------------------------------------------------------------

void test_member_properties() {
    std::cout << "[Test 5] Member Property Queries\n";
    std::cout << std::string(40, '-') << "\n";

    // is_public
    constexpr bool x_pub = is_public(^^SimpleStruct::x);
    constexpr bool y_pub = is_public(^^SimpleStruct::y);
    static_assert(x_pub, "SimpleStruct::x should be public");
    static_assert(y_pub, "SimpleStruct::y should be public");
    std::cout << "  SimpleStruct::x is_public: " << x_pub << " [OK]\n";

    // is_nonstatic_data_member
    constexpr bool x_nsdm = is_nonstatic_data_member(^^SimpleStruct::x);
    constexpr bool y_nsdm = is_nonstatic_data_member(^^SimpleStruct::y);
    static_assert(x_nsdm, "SimpleStruct::x should be nonstatic data member");
    static_assert(y_nsdm, "SimpleStruct::y should be nonstatic data member");
    std::cout << "  SimpleStruct::x is_nonstatic_data_member: " << x_nsdm << " [OK]\n";

    // is_static_member (should be false for data members)
    constexpr bool x_static = is_static_member(^^SimpleStruct::x);
    static_assert(!x_static, "SimpleStruct::x should not be static");
    std::cout << "  SimpleStruct::x is_static_member: " << x_static << " [OK]\n";

    // GameItem member properties
    constexpr bool id_pub = is_public(^^GameItem::item_id);
    constexpr bool name_nsdm = is_nonstatic_data_member(^^GameItem::name);
    static_assert(id_pub && name_nsdm);
    std::cout << "  GameItem::item_id is_public: " << id_pub << " [OK]\n";
    std::cout << "  GameItem::name is_nonstatic_data_member: " << name_nsdm << " [OK]\n";

    std::cout << "[PASS]\n\n";
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------

int main() {
    std::cout << "========================================\n";
    std::cout << "  Reflection Core Test\n";
    std::cout << "========================================\n\n";

    test_type_and_member_reflection();
    test_member_iteration();
    test_splice_operations();
    test_reflection_with_xbuffer();
    test_member_properties();

    std::cout << "========================================\n";
    std::cout << "[SUCCESS] All reflection core tests passed!\n";
    return 0;
}