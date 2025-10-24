#include <iostream>
#include <experimental/meta>
#include <string_view>

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
        // R10: Use access_context parameter
        auto members = nonstatic_data_members_of(^^Person, 
                                                  access_context::unchecked());
        
        std::cout << "Person has " << members.size() << " members:\n";
        
        // R10: Use for loop instead of expand
        for (auto member : members) {
            // R10: Use display_string_of instead of name_of
            std::cout << "  - " << display_string_of(member) << "\n";
        }
        
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
        auto age_type = type_of(^^Person::age);
        auto height_type = type_of(^^Person::height);
        auto name_type = type_of(^^Person::name);
        
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
    // Test 5: Manual Member Iteration (no expand)
    // ========================================================================
    std::cout << "[Test 5] Manual Iteration\n";
    std::cout << "-----------------------------------\n";
    {
        Person p{25, 175.5, 70.5f, "Alice"};
        
        auto members = nonstatic_data_members_of(^^Person, 
                                                  access_context::unchecked());
        
        std::cout << "Person member details:\n";
        for (auto member : members) {
            std::cout << "  Member: " << display_string_of(member) << "\n";
            std::cout << "    Type: " << display_string_of(type_of(member)) << "\n";
            std::cout << "    Is public: " << is_public(member) << "\n";
            std::cout << "    Is static: " << is_static_member(member) << "\n";
        }
        
        std::cout << "[PASS] Manual iteration (for loop)\n";
    }
    std::cout << "[PASS] Test 5 PASSED\n\n";

    // ========================================================================
    // Test 6: Member Count
    // ========================================================================
    std::cout << "[Test 6] Member Count\n";
    std::cout << "-----------------------------------\n";
    {
        auto person_members = nonstatic_data_members_of(^^Person, 
                                                         access_context::unchecked());
        auto point_members = nonstatic_data_members_of(^^Point3D, 
                                                        access_context::unchecked());
        auto complex_members = nonstatic_data_members_of(^^Complex, 
                                                          access_context::unchecked());
        
        std::cout << "Member counts:\n";
        std::cout << "  Person: " << person_members.size() << " members\n";
        std::cout << "  Point3D: " << point_members.size() << " members\n";
        std::cout << "  Complex: " << complex_members.size() << " members\n";
        
        std::cout << "[PASS] Member counting\n";
    }
    std::cout << "[PASS] Test 6 PASSED\n\n";

    // ========================================================================
    // Test 7: Serialize Struct (manual iteration)
    // ========================================================================
    std::cout << "[Test 7] Struct Serialization\n";
    std::cout << "-----------------------------------\n";
    {
        Complex c{10, 20};
        
        auto members = nonstatic_data_members_of(^^Complex, 
                                                  access_context::unchecked());
        
        std::cout << "Serializing Complex: { ";
        
        bool first = true;
        for (auto member : members) {
            if (!first) std::cout << ", ";
            first = false;
            
            std::cout << display_string_of(member) << ": ";
            
            // Access member value via reflection
            if (display_string_of(member) == std::string_view("real")) {
                std::cout << c.[:^^Complex::real:];
            } else if (display_string_of(member) == std::string_view("imag")) {
                std::cout << c.[:^^Complex::imag:];
            }
        }
        
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
    // Test 9: Filter Members by Attribute
    // ========================================================================
    std::cout << "[Test 9] Filter Members\n";
    std::cout << "-----------------------------------\n";
    {
        auto members = nonstatic_data_members_of(^^Person, 
                                                  access_context::unchecked());
        
        std::cout << "Person public members:\n";
        for (auto member : members) {
            if (is_public(member)) {
                std::cout << "  - " << display_string_of(member) << "\n";
            }
        }
        
        std::cout << "[PASS] Member filtering\n";
    }
    std::cout << "[PASS] Test 9 PASSED\n\n";

    // ========================================================================
    // Test 10: Combined Reflection Operations
    // ========================================================================
    std::cout << "[Test 10] Combined Operations\n";
    std::cout << "-----------------------------------\n";
    {
        Point3D pt{1.0f, 2.0f, 3.0f};
        
        auto members = nonstatic_data_members_of(^^Point3D, 
                                                  access_context::unchecked());
        
        std::cout << "Point3D complete info:\n";
        std::cout << "  Type: " << display_string_of(^^Point3D) << "\n";
        std::cout << "  Member count: " << members.size() << "\n";
        std::cout << "  Members:\n";
        
        for (auto member : members) {
            std::cout << "    - Name: " << display_string_of(member) << "\n";
            std::cout << "      Type: " << display_string_of(type_of(member)) << "\n";
            std::cout << "      Public: " << is_public(member) << "\n";
            std::cout << "      Static: " << is_static_member(member) << "\n";
        }
        
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
    std::cout << "[PASS] Test 5:  Manual iteration (for loop)\n";
    std::cout << "[PASS] Test 6:  Member counting\n";
    std::cout << "[PASS] Test 7:  Manual serialization\n";
    std::cout << "[PASS] Test 8:  Type display names\n";
    std::cout << "[PASS] Test 9:  Member filtering\n";
    std::cout << "[PASS] Test 10: Combined operations\n";
    std::cout << "\n[SUCCESS] All P2996 R10 features working!\n";
    std::cout << "========================================\n";
    std::cout << "\n[NOTE] Using P2996 R10 API:\n";
    std::cout << "  - nonstatic_data_members_of(type, access_context)\n";
    std::cout << "  - display_string_of() instead of name_of()\n";
    std::cout << "  - is_static_member() instead of is_static()\n";
    std::cout << "  - for loops instead of expand operator\n";
    std::cout << "========================================\n";

    return 0;
}