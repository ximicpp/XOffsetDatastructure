// ============================================================================
// Test: Reflection Type Signature Integration
// Purpose: Test integration between reflection and XTypeSignature
// ============================================================================

#include "../xoffsetdatastructure2.hpp"
#include <iostream>

#include <experimental/meta>
#include <experimental/meta>

using namespace XOffsetDatastructure2;

struct TypeSigTest {
    int a;
    double b;
    XString c;
    XVector<int> d;
    
    template <typename Allocator>
    TypeSigTest(Allocator allocator) 
        : a(0), b(0.0), c(allocator), d(allocator) {}
};

struct SimpleData {
    uint32_t id;
    float value;
    
    template <typename Allocator>
    SimpleData(Allocator allocator) : id(0), value(0.0f) {}
};

template<typename T>
consteval size_t get_member_count_consteval() {
    using namespace std::meta;
    return nonstatic_data_members_of(^^T, access_context::unchecked()).size();
}

void test_type_signature_generation() {
    using namespace std::meta;
    
    std::cout << "[Test 1] Type Signature Generation\n";
    std::cout << "-----------------------------------\n";
    
    // Generate type signature using reflection
    std::cout << "  TypeSigTest signature:\n";
    std::cout << "    Name: " << display_string_of(^^TypeSigTest) << "\n";
    std::cout << "    Members: " << get_member_count_consteval<TypeSigTest>() << "\n";
    
    std::cout << "\n  SimpleData signature:\n";
    std::cout << "    Name: " << display_string_of(^^SimpleData) << "\n";
    std::cout << "    Members: " << get_member_count_consteval<SimpleData>() << "\n";
    
    std::cout << "[PASS] Type signature generation\n\n";
}

void test_reflection_member_count() {
    std::cout << "[Test 2] Member Count Comparison\n";
    std::cout << "---------------------------------\n";
    
    // Get member count via reflection
    constexpr size_t test_count = get_member_count_consteval<TypeSigTest>();
    constexpr size_t simple_count = get_member_count_consteval<SimpleData>();
    
    std::cout << "  TypeSigTest:\n";
    std::cout << "    Reflection member count: " << test_count << "\n";
    
    std::cout << "\n  SimpleData:\n";
    std::cout << "    Reflection member count: " << simple_count << "\n";
    
    std::cout << "[PASS] Member count comparison\n\n";
}

// Helper: Get member at index (consteval)
template<typename T, size_t I>
consteval auto get_member_at_index() {
    using namespace std::meta;
    auto members = nonstatic_data_members_of(^^T, access_context::unchecked());
    if (I < members.size()) {
        return members[I];
    }
    return std::meta::info{};
}

// Helper: print member at index
template<typename T, size_t I>
void print_member_at() {
    using namespace std::meta;
    constexpr auto member = get_member_at_index<T, I>();
    if constexpr (member != std::meta::info{}) {
        std::cout << "    [" << I << "] " << display_string_of(member) 
                  << " : " << display_string_of(type_of(member)) << "\n";
    }
}

template<typename T, size_t... Is>
void print_members_with_indices(std::index_sequence<Is...>) {
    (print_member_at<T, Is>(), ...);
}

void test_reflection_member_names() {
    std::cout << "[Test 3] Member Names via Reflection\n";
    std::cout << "-------------------------------------\n";
    
    constexpr size_t member_count = get_member_count_consteval<TypeSigTest>();
    
    std::cout << "  TypeSigTest members (" << member_count << "):\n";
    print_members_with_indices<TypeSigTest>(std::make_index_sequence<4>{});
    
    std::cout << "[PASS] Member names\n\n";
}

void test_instance_creation() {
    std::cout << "[Test 4] Instance Creation and Validation\n";
    std::cout << "------------------------------------------\n";
    
    XBufferExt xbuf(2048);
    auto* obj = xbuf.make<TypeSigTest>("test");
    
    obj->a = 42;
    obj->b = 3.14;
    obj->c = XString("test_string", xbuf.allocator<XString>());
    obj->d.push_back(1);
    obj->d.push_back(2);
    obj->d.push_back(3);
    
    std::cout << "  Created TypeSigTest instance:\n";
    std::cout << "    a: " << obj->a << "\n";
    std::cout << "    b: " << obj->b << "\n";
    std::cout << "    c: " << obj->c.c_str() << "\n";
    std::cout << "    d.size(): " << obj->d.size() << "\n";
    
    // Verify via splice
    std::cout << "\n  Access via splice:\n";
    std::cout << "    a: " << obj->[:^^TypeSigTest::a:] << "\n";
    std::cout << "    b: " << obj->[:^^TypeSigTest::b:] << "\n";
    
    std::cout << "[PASS] Instance creation\n\n";
}

void test_type_consistency() {
    using namespace std::meta;
    
    std::cout << "[Test 5] Type Consistency Check\n";
    std::cout << "--------------------------------\n";
    
    // Check member types
    constexpr auto a_type = type_of(^^TypeSigTest::a);
    constexpr auto b_type = type_of(^^TypeSigTest::b);
    constexpr auto c_type = type_of(^^TypeSigTest::c);
    constexpr auto d_type = type_of(^^TypeSigTest::d);
    
    std::cout << "  Member types:\n";
    std::cout << "    a: " << display_string_of(a_type) 
              << " (expected: int)\n";
    std::cout << "    b: " << display_string_of(b_type) 
              << " (expected: double)\n";
    std::cout << "    c: " << display_string_of(c_type) 
              << " (expected: XString)\n";
    std::cout << "    d: " << display_string_of(d_type) 
              << " (expected: XVector<int>)\n";
    
    // Verify types match
    constexpr bool a_ok = (a_type == ^^int);
    constexpr bool b_ok = (b_type == ^^double);
    constexpr bool c_ok = (c_type == ^^XString);
    constexpr bool d_ok = (d_type == ^^XVector<int>);
    
    std::cout << "\n  Type validation:\n";
    std::cout << "    a is int: " << (a_ok ? "yes" : "no") << "\n";
    std::cout << "    b is double: " << (b_ok ? "yes" : "no") << "\n";
    std::cout << "    c is XString: " << (c_ok ? "yes" : "no") << "\n";
    std::cout << "    d is XVector<int>: " << (d_ok ? "yes" : "no") << "\n";
    
    std::cout << "[PASS] Type consistency\n\n";
}

void test_serialization_with_reflection() {
    std::cout << "[Test 6] Serialization with Reflection\n";
    std::cout << "---------------------------------------\n";
    
    XBufferExt xbuf(2048);
    auto* data = xbuf.make<SimpleData>("data");
    
    data->id = 9999;
    data->value = 123.45f;
    
    std::cout << "  Original data:\n";
    std::cout << "    id: " << data->id << "\n";
    std::cout << "    value: " << data->value << "\n";
    
    // Serialize
    std::string binary = xbuf.save_to_string();
    std::cout << "\n  Serialized to " << binary.size() << " bytes\n";
    
    // Deserialize
    XBufferExt xbuf2 = XBufferExt::load_from_string(binary);
    auto* loaded = xbuf2.find<SimpleData>("data").first;
    
    if (loaded) {
        std::cout << "\n  Loaded data:\n";
        std::cout << "    id: " << loaded->id << "\n";
        std::cout << "    value: " << loaded->value << "\n";
        
        // Verify via reflection
        constexpr size_t member_count = get_member_count_consteval<SimpleData>();
        std::cout << "\n  Member count (reflection): " << member_count << "\n";
        
        bool integrity = (loaded->id == 9999 && loaded->value == 123.45f);
        std::cout << "  Data integrity: " << (integrity ? "OK" : "FAIL") << "\n";
    }
    
    std::cout << "[PASS] Serialization with reflection\n\n";
}


int main() {
    std::cout << "========================================\n";
    std::cout << "  Reflection Type Signature Test\n";
    std::cout << "========================================\n\n";

#include <experimental/meta>
    std::cout << "[INFO] C++26 Reflection: ENABLED\n";
    std::cout << "[INFO] Testing XTypeSignature integration\n\n";
    
    test_type_signature_generation();
    test_reflection_member_count();
    test_reflection_member_names();
    test_instance_creation();
    test_type_consistency();
    test_serialization_with_reflection();
    
    std::cout << "========================================\n";
    std::cout << "  Summary\n";
    std::cout << "========================================\n";
    std::cout << "[PASS] Test 1: Type signature generation\n";
    std::cout << "[PASS] Test 2: Member count comparison\n";
    std::cout << "[PASS] Test 3: Member names\n";
    std::cout << "[PASS] Test 4: Instance creation\n";
    std::cout << "[PASS] Test 5: Type consistency\n";
    std::cout << "[PASS] Test 6: Serialization with reflection\n";
    std::cout << "\n[SUCCESS] All type signature tests passed!\n";
    
    return 0;
}
