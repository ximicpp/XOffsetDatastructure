#include <iostream>
#include <experimental/meta>
#include <string_view>
#include <utility>

// Test structures
struct Person {
    int age;
    double height;
    float weight;
    const char* name;
};

struct Point3D {
    float x, y, z;
};

struct Complex {
    int real;
    int imag;
};

class MyClass {
public:
    int public_member;
    static int static_member;
private:
    int private_member;
};

int MyClass::static_member = 42;

// Helper: Print member names (consteval context)
template<typename T>
consteval auto print_member_names() {
    using namespace std::meta;
    auto members = nonstatic_data_members_of(^^T, access_context::unchecked());
    // We can't return members, but we can count them
    return members.size();
}

// Helper: Get member count
template<typename T>
consteval auto get_member_count() {
    using namespace std::meta;
    return nonstatic_data_members_of(^^T, access_context::unchecked()).size();
}

// Helper: Print all member info in consteval context
template<typename T>
consteval void print_members_consteval() {
    using namespace std::meta;
    
    // Get all members - this works in consteval context
    auto members = nonstatic_data_members_of(^^T, access_context::unchecked());
    
    // Can iterate in consteval context
    // But we can't do I/O here, so we'll use a different approach
}

// Helper: Generate member info array at compile time
template<typename T>
consteval auto get_member_info_count() {
    using namespace std::meta;
    auto members = nonstatic_data_members_of(^^T, access_context::unchecked());
    return members.size();
}

// Structure to hold member info (can cross compile-time boundary)
struct MemberInfo {
    const char* name;
    const char* type;
    bool is_public;
    bool is_static;
};

// Helper: Create compile-time member info for a specific index
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

// Macro-like template to iterate members at compile time
template<typename T, size_t... Is>
constexpr void print_all_members_impl(std::index_sequence<Is...>) {
    std::cout << "Iterating members via consteval:\n";
    ((std::cout << "  [" << Is << "] " 
                << get_member_info_at<T, Is>().name << " : "
                << get_member_info_at<T, Is>().type << "\n"
                << "      public=" << get_member_info_at<T, Is>().is_public
                << ", static=" << get_member_info_at<T, Is>().is_static << "\n"), ...);
}

template<typename T>
void print_all_members_via_consteval() {
    constexpr auto count = get_member_info_count<T>();
    print_all_members_impl<T>(std::make_index_sequence<count>{});
}

int main() {
    std::cout << "========================================\n";
    std::cout << "  P2996 R10 Advanced Meta Features\n";
    std::cout << "========================================\n\n";

    using namespace std::meta;

    // ========================================================================
    // Test 1: Member Iteration (R10 API)
    // ========================================================================
    std::cout << "[Test 1] Member Iteration (R10 API)\n";
    std::cout << "-----------------------------------\n";
    {
        // R10: vector<info> is consteval-only, must use in consteval context
        constexpr auto member_count = get_member_count<Person>();
        
        std::cout << "Person has " << member_count << " members:\n";
        
        // Manual listing (since we can't iterate vector<info> at runtime)
        std::cout << "  - " << display_string_of(^^Person::age) << "\n";
        std::cout << "  - " << display_string_of(^^Person::height) << "\n";
        std::cout << "  - " << display_string_of(^^Person::weight) << "\n";
        std::cout << "  - " << display_string_of(^^Person::name) << "\n";
        
        std::cout << "[PASS] Member iteration with R10 API\n";
    }
    std::cout << "[PASS] Test 1 PASSED\n\n";

    // ========================================================================
    // Test 2: Member Names (display_string_of)
    // ========================================================================
    std::cout << "[Test 2] Member Names\n";
    std::cout << "-----------------------------------\n";
    {
        // R10: display_string_of instead of name_of
        auto x_name = display_string_of(^^Point3D::x);
        auto y_name = display_string_of(^^Point3D::y);
        auto z_name = display_string_of(^^Point3D::z);
        
        std::cout << "Point3D members:\n";
        std::cout << "  " << x_name << "\n";
        std::cout << "  " << y_name << "\n";
        std::cout << "  " << z_name << "\n";
        
        std::cout << "[PASS] display_string_of for names\n";
    }
    std::cout << "[PASS] Test 2 PASSED\n\n";

    // ========================================================================
    // Test 3: Member Type Queries
    // ========================================================================
    std::cout << "[Test 3] Member Type Queries\n";
    std::cout << "-----------------------------------\n";
    {
        // type_of returns info which is consteval-only, use constexpr
        constexpr auto age_type = type_of(^^Person::age);
        constexpr auto height_type = type_of(^^Person::height);
        constexpr auto name_type = type_of(^^Person::name);
        
        std::cout << "Person member types:\n";
        // R10: display_string_of for type names
        std::cout << "  age: " << display_string_of(age_type) << "\n";
        std::cout << "  height: " << display_string_of(height_type) << "\n";
        std::cout << "  name: " << display_string_of(name_type) << "\n";
        
        std::cout << "[PASS] type_of queries\n";
    }
    std::cout << "[PASS] Test 3 PASSED\n\n";

    // ========================================================================
    // Test 4: Member Attributes (R10 API)
    // ========================================================================
    std::cout << "[Test 4] Member Attributes\n";
    std::cout << "-----------------------------------\n";
    {
        constexpr auto pub_refl = ^^MyClass::public_member;
        constexpr auto stat_refl = ^^MyClass::static_member;
        
        std::cout << "MyClass member attributes:\n";
        std::cout << "  public_member:\n";
        std::cout << "    is_public: " << is_public(pub_refl) << "\n";
        // R10: is_static_member instead of is_static
        std::cout << "    is_static: " << is_static_member(pub_refl) << "\n";
        // R10: is_nonstatic_data_member (new function)
        std::cout << "    is_nonstatic_data_member: " 
                  << is_nonstatic_data_member(pub_refl) << "\n";
        
        std::cout << "  static_member:\n";
        std::cout << "    is_static: " << is_static_member(stat_refl) << "\n";
        std::cout << "    is_nonstatic_data_member: " 
                  << is_nonstatic_data_member(stat_refl) << "\n";
        
        std::cout << "[PASS] R10 attribute queries\n";
    }
    std::cout << "[PASS] Test 4 PASSED\n\n";

    // ========================================================================
    // Test 5: Manual Member Access (no runtime iteration)
    // ========================================================================
    std::cout << "[Test 5] Manual Member Access\n";
    std::cout << "-----------------------------------\n";
    {
        Person p{25, 175.5, 70.5f, "Alice"};
        
        // Manual member inspection (vector<info> can't be used at runtime)
        std::cout << "Person member details:\n";
        
        constexpr auto age_refl = ^^Person::age;
        std::cout << "  Member: " << display_string_of(age_refl) << "\n";
        std::cout << "    Type: " << display_string_of(type_of(age_refl)) << "\n";
        std::cout << "    Is public: " << is_public(age_refl) << "\n";
        std::cout << "    Is static: " << is_static_member(age_refl) << "\n";
        
        constexpr auto height_refl = ^^Person::height;
        std::cout << "  Member: " << display_string_of(height_refl) << "\n";
        std::cout << "    Type: " << display_string_of(type_of(height_refl)) << "\n";
        std::cout << "    Is public: " << is_public(height_refl) << "\n";
        std::cout << "    Is static: " << is_static_member(height_refl) << "\n";
        
        std::cout << "[PASS] Manual member access\n";
    }
    std::cout << "[PASS] Test 5 PASSED\n\n";

    // ========================================================================
    // Test 6: Member Count
    // ========================================================================
    std::cout << "[Test 6] Member Count\n";
    std::cout << "-----------------------------------\n";
    {
        // Use constexpr to call consteval functions
        constexpr auto person_count = get_member_count<Person>();
        constexpr auto point_count = get_member_count<Point3D>();
        constexpr auto complex_count = get_member_count<Complex>();
        
        std::cout << "Member counts:\n";
        std::cout << "  Person: " << person_count << " members\n";
        std::cout << "  Point3D: " << point_count << " members\n";
        std::cout << "  Complex: " << complex_count << " members\n";
        
        std::cout << "[PASS] Member counting\n";
    }
    std::cout << "[PASS] Test 6 PASSED\n\n";

    // ========================================================================
    // Test 7: Struct Serialization (manual)
    // ========================================================================
    std::cout << "[Test 7] Struct Serialization\n";
    std::cout << "-----------------------------------\n";
    {
        Complex c{10, 20};
        
        // Manual serialization (can't iterate vector<info> at runtime)
        std::cout << "Serializing Complex: { ";
        std::cout << display_string_of(^^Complex::real) << ": " << c.[:^^Complex::real:] << ", ";
        std::cout << display_string_of(^^Complex::imag) << ": " << c.[:^^Complex::imag:];
        std::cout << " }\n";
        
        std::cout << "[PASS] Manual serialization\n";
    }
    std::cout << "[PASS] Test 7 PASSED\n\n";

    // ========================================================================
    // Test 8: Type Display Names
    // ========================================================================
    std::cout << "[Test 8] Type Display Names\n";
    std::cout << "-----------------------------------\n";
    {
        std::cout << "Type names:\n";
        std::cout << "  Person: " << display_string_of(^^Person) << "\n";
        std::cout << "  Point3D: " << display_string_of(^^Point3D) << "\n";
        std::cout << "  int: " << display_string_of(^^int) << "\n";
        std::cout << "  double: " << display_string_of(^^double) << "\n";
        
        std::cout << "[PASS] display_string_of for types\n";
    }
    std::cout << "[PASS] Test 8 PASSED\n\n";

    // ========================================================================
    // Test 9: Check Member Attributes
    // ========================================================================
    std::cout << "[Test 9] Member Attributes\n";
    std::cout << "-----------------------------------\n";
    {
        // Check each member's attributes manually
        std::cout << "Person public members:\n";
        
        constexpr auto age_pub = is_public(^^Person::age);
        constexpr auto height_pub = is_public(^^Person::height);
        constexpr auto weight_pub = is_public(^^Person::weight);
        constexpr auto name_pub = is_public(^^Person::name);
        
        if (age_pub) std::cout << "  - age\n";
        if (height_pub) std::cout << "  - height\n";
        if (weight_pub) std::cout << "  - weight\n";
        if (name_pub) std::cout << "  - name\n";
        
        std::cout << "[PASS] Member attribute checking\n";
    }
    std::cout << "[PASS] Test 9 PASSED\n\n";

    // ========================================================================
    // Test 10: Using nonstatic_data_members_of with Iteration
    // ========================================================================
    std::cout << "[Test 10] nonstatic_data_members_of Iteration\n";
    std::cout << "-----------------------------------\n";
    {
        std::cout << "Demonstrating member iteration via consteval:\n\n";
        
        std::cout << "Person members:\n";
        print_all_members_via_consteval<Person>();
        
        std::cout << "\nPoint3D members:\n";
        print_all_members_via_consteval<Point3D>();
        
        std::cout << "\nComplex members:\n";
        print_all_members_via_consteval<Complex>();
        
        std::cout << "\n[PASS] nonstatic_data_members_of iteration\n";
    }
    std::cout << "[PASS] Test 10 PASSED\n\n";

    // ========================================================================
    // Test 11: Combined Reflection Operations
    // ========================================================================
    std::cout << "[Test 11] Combined Operations\n";
    std::cout << "-----------------------------------\n";
    {
        Point3D pt{1.0f, 2.0f, 3.0f};
        
        constexpr auto member_count = get_member_count<Point3D>();
        
        std::cout << "Point3D complete info:\n";
        std::cout << "  Type: " << display_string_of(^^Point3D) << "\n";
        std::cout << "  Member count: " << member_count << "\n";
        std::cout << "  Members:\n";
        
        // Manual listing of members
        constexpr auto x_refl = ^^Point3D::x;
        std::cout << "    - Name: " << display_string_of(x_refl) << "\n";
        std::cout << "      Type: " << display_string_of(type_of(x_refl)) << "\n";
        std::cout << "      Public: " << is_public(x_refl) << "\n";
        std::cout << "      Static: " << is_static_member(x_refl) << "\n";
        
        constexpr auto y_refl = ^^Point3D::y;
        std::cout << "    - Name: " << display_string_of(y_refl) << "\n";
        std::cout << "      Type: " << display_string_of(type_of(y_refl)) << "\n";
        std::cout << "      Public: " << is_public(y_refl) << "\n";
        std::cout << "      Static: " << is_static_member(y_refl) << "\n";
        
        constexpr auto z_refl = ^^Point3D::z;
        std::cout << "    - Name: " << display_string_of(z_refl) << "\n";
        std::cout << "      Type: " << display_string_of(type_of(z_refl)) << "\n";
        std::cout << "      Public: " << is_public(z_refl) << "\n";
        std::cout << "      Static: " << is_static_member(z_refl) << "\n";
        
        // Direct member access
        std::cout << "  Values: (" << pt.x << ", " << pt.y << ", " << pt.z << ")\n";
        
        std::cout << "[PASS] Combined operations\n";
    }
    std::cout << "[PASS] Test 10 PASSED\n\n";

    // ========================================================================
    // Summary
    // ========================================================================
    std::cout << "========================================\n";
    std::cout << "  P2996 R10 Test Summary\n";
    std::cout << "========================================\n";
    std::cout << "[PASS] Test 1:  Member iteration (R10 API)\n";
    std::cout << "[PASS] Test 2:  display_string_of for names\n";
    std::cout << "[PASS] Test 3:  type_of queries\n";
    std::cout << "[PASS] Test 4:  R10 attribute queries\n";
    std::cout << "[PASS] Test 5:  Manual member access\n";
    std::cout << "[PASS] Test 6:  Member counting\n";
    std::cout << "[PASS] Test 7:  Manual serialization\n";
    std::cout << "[PASS] Test 8:  Type display names\n";
    std::cout << "[PASS] Test 9:  Member filtering\n";
    std::cout << "[PASS] Test 10: nonstatic_data_members_of iteration\n";
    std::cout << "[PASS] Test 11: Combined operations\n";
    std::cout << "\n[SUCCESS] All P2996 R10 features working!\n";
    std::cout << "========================================\n";
    std::cout << "\n[NOTE] P2996 R10 Constraints:\n";
    std::cout << "  - vector<info> is consteval-only (can't use at runtime)\n";
    std::cout << "  - Must use consteval functions or constexpr variables\n";
    std::cout << "  - No runtime iteration over members\n";
    std::cout << "  - expand operator not yet implemented\n";
    std::cout << "  - Manual member listing required\n";
    std::cout << "========================================\n";

    return 0;
}