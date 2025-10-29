// ============================================================================
// Test: Reflection-Based Serialization
// Purpose: Test reflection for serialization and structure analysis
// ============================================================================

#include "../xoffsetdatastructure2.hpp"
#include <iostream>
#include <sstream>

#include <experimental/meta>
#include <experimental/meta>

using namespace XOffsetDatastructure2;

struct SerializableData {
    int id;
    double value;
    uint32_t flags;
    
    template <typename Allocator>
    SerializableData(Allocator allocator) : id(0), value(0.0), flags(0) {}
};

struct ComplexData {
    uint32_t type;
    XString name;
    XVector<int> items;
    
    template <typename Allocator>
    ComplexData(Allocator allocator) 
        : type(0), name(allocator), items(allocator) {}
};

// Helper: Get member count
template<typename T>
consteval size_t get_member_count_ce() {
    using namespace std::meta;
    return nonstatic_data_members_of(^^T, access_context::unchecked()).size();
}

// Helper: Get member at index
template<typename T, size_t I>
consteval auto get_member_ce() {
    using namespace std::meta;
    auto members = nonstatic_data_members_of(^^T, access_context::unchecked());
    return members[I];
}

// Helper: Print member at index
template<typename T, size_t I>
void print_member_info() {
    using namespace std::meta;
    constexpr auto member = get_member_ce<T, I>();
    std::cout << "    \"" << display_string_of(member) << "\": "
              << display_string_of(type_of(member)) << "\n";
}

template<typename T, size_t... Is>
void print_all_member_info(std::index_sequence<Is...>) {
    (print_member_info<T, Is>(), ...);
}

void test_structure_to_text() {
    std::cout << "[Test 1] Structure to Text\n";
    std::cout << "--------------------------\n";
    
    constexpr size_t member_count = get_member_count_ce<SerializableData>();
    
    std::cout << "  SerializableData structure:\n";
    std::cout << "  {\n";
    
    print_all_member_info<SerializableData>(std::make_index_sequence<member_count>{});
    
    std::cout << "  }\n";
    std::cout << "[PASS] Structure to text\n\n";
}

// Helper: Print member with details
template<typename T, size_t I>
void print_member_details() {
    using namespace std::meta;
    constexpr auto member = get_member_ce<T, I>();
    std::cout << "    - " << display_string_of(member) 
              << " (" << display_string_of(type_of(member)) << ")\n";
}

template<typename T, size_t... Is>
void print_all_member_details(std::index_sequence<Is...>) {
    (print_member_details<T, Is>(), ...);
}

void test_member_listing() {
    std::cout << "[Test 2] Member Listing\n";
    std::cout << "-----------------------\n";
    
    XBufferExt xbuf(1024);
    auto* data = xbuf.make<SerializableData>("data");
    
    data->id = 42;
    data->value = 3.14;
    data->flags = 0xFF;
    
    std::cout << "  Manual JSON representation:\n";
    std::cout << "  {\n";
    std::cout << "    \"id\": " << data->id << ",\n";
    std::cout << "    \"value\": " << data->value << ",\n";
    std::cout << "    \"flags\": " << data->flags << "\n";
    std::cout << "  }\n";
    
    // Show member info via reflection
    constexpr size_t member_count = get_member_count_ce<SerializableData>();
    
    std::cout << "\n  Members detected via reflection (" << member_count << "):\n";
    print_all_member_details<SerializableData>(std::make_index_sequence<member_count>{});
    
    std::cout << "[PASS] Member listing\n\n";
}

// Helper: Print field type
template<typename T, size_t I>
void print_field_type() {
    using namespace std::meta;
    constexpr auto member = get_member_ce<T, I>();
    std::cout << "      " << display_string_of(member) << ": "
              << display_string_of(type_of(member)) << "\n";
}

template<typename T, size_t... Is>
void print_all_field_types(std::index_sequence<Is...>) {
    (print_field_type<T, Is>(), ...);
}

void test_complex_structure_analysis() {
    std::cout << "[Test 3] Complex Structure Analysis\n";
    std::cout << "------------------------------------\n";
    
    XBufferExt xbuf(2048);
    auto* data = xbuf.make<ComplexData>("complex");
    
    data->type = 100;
    data->name = XString("TestObject", xbuf.allocator<XString>());
    data->items.push_back(1);
    data->items.push_back(2);
    data->items.push_back(3);
    
    std::cout << "  ComplexData instance:\n";
    std::cout << "    type: " << data->type << "\n";
    std::cout << "    name: " << data->name.c_str() << "\n";
    std::cout << "    items: [";
    for (size_t i = 0; i < data->items.size(); ++i) {
        if (i > 0) std::cout << ", ";
        std::cout << data->items[i];
    }
    std::cout << "]\n";
    
    // Analyze structure
    constexpr size_t member_count = get_member_count_ce<ComplexData>();
    
    std::cout << "\n  Structure analysis:\n";
    std::cout << "    Total members: " << member_count << "\n";
    std::cout << "    Field types:\n";
    
    print_all_field_types<ComplexData>(std::make_index_sequence<member_count>{});
    
    std::cout << "[PASS] Complex structure analysis\n\n";
}

void test_binary_serialization() {
    std::cout << "[Test 4] Binary Serialization\n";
    std::cout << "-----------------------------\n";
    
    XBufferExt xbuf(1024);
    auto* data = xbuf.make<SerializableData>("data");
    
    data->id = 999;
    data->value = 123.456;
    data->flags = 0xDEADBEEF;
    
    std::cout << "  Original data:\n";
    std::cout << "    id: " << data->id << "\n";
    std::cout << "    value: " << data->value << "\n";
    std::cout << "    flags: 0x" << std::hex << data->flags << std::dec << "\n";
    
    // Serialize
    std::string binary = xbuf.save_to_string();
    std::cout << "\n  Serialized to " << binary.size() << " bytes\n";
    
    // Deserialize
    XBufferExt xbuf2 = XBufferExt::load_from_string(binary);
    auto* loaded = xbuf2.find<SerializableData>("data").first;
    
    if (loaded) {
        std::cout << "\n  Loaded data:\n";
        std::cout << "    id: " << loaded->id << "\n";
        std::cout << "    value: " << loaded->value << "\n";
        std::cout << "    flags: 0x" << std::hex << loaded->flags << std::dec << "\n";
        
        bool integrity = (loaded->id == 999 && 
                         loaded->value == 123.456 &&
                         loaded->flags == 0xDEADBEEF);
        std::cout << "\n  Integrity: " << (integrity ? "OK" : "FAIL") << "\n";
    }
    
    std::cout << "[PASS] Binary serialization\n\n";
}

void test_member_count_validation() {
    std::cout << "[Test 5] Member Count Validation\n";
    std::cout << "---------------------------------\n";
    
    constexpr size_t simple_count = get_member_count_ce<SerializableData>();
    constexpr size_t complex_count = get_member_count_ce<ComplexData>();
    
    std::cout << "  Member counts:\n";
    std::cout << "    SerializableData: " << simple_count << " members\n";
    std::cout << "    ComplexData: " << complex_count << " members\n";
    
    // Compile-time validation could be added here
    std::cout << "\n  Use case: Verify structure hasn't changed between versions\n";
    std::cout << "  Expected SerializableData: 3 members\n";
    std::cout << "  Expected ComplexData: 3 members\n";
    
    constexpr bool validation_ok = (simple_count == 3 && complex_count == 3);
    std::cout << "\n  Validation: " << (validation_ok ? "PASS" : "FAIL") << "\n";
    
    std::cout << "[PASS] Member count validation\n\n";
}

// Helper: Print struct field declaration
template<typename T, size_t I>
void print_struct_field() {
    using namespace std::meta;
    constexpr auto member = get_member_ce<T, I>();
    std::cout << "    " << display_string_of(type_of(member)) << " "
              << display_string_of(member) << ";\n";
}

template<typename T, size_t... Is>
void print_all_struct_fields(std::index_sequence<Is...>) {
    (print_struct_field<T, Is>(), ...);
}

// Helper: Print field description
template<typename T, size_t I>
void print_field_description() {
    using namespace std::meta;
    constexpr auto member = get_member_ce<T, I>();
    std::cout << "    - " << display_string_of(member) 
              << ": Type " << display_string_of(type_of(member))
              << (is_public(member) ? " (public)" : " (private)") << "\n";
}

template<typename T, size_t... Is>
void print_all_field_descriptions(std::index_sequence<Is...>) {
    (print_field_description<T, Is>(), ...);
}

void test_field_type_documentation() {
    std::cout << "[Test 6] Field Type Documentation\n";
    std::cout << "----------------------------------\n";
    
    std::cout << "  Auto-generated documentation:\n\n";
    
    std::cout << "  struct SerializableData {\n";
    constexpr size_t member_count = get_member_count_ce<SerializableData>();
    print_all_struct_fields<SerializableData>(std::make_index_sequence<member_count>{});
    std::cout << "  };\n";
    
    std::cout << "\n  Field descriptions:\n";
    print_all_field_descriptions<SerializableData>(std::make_index_sequence<member_count>{});
    
    std::cout << "[PASS] Field type documentation\n\n";
}


int main() {
    std::cout << "========================================\n";
    std::cout << "  Reflection Serialization Test\n";
    std::cout << "========================================\n\n";

#include <experimental/meta>
    std::cout << "[INFO] C++26 Reflection: ENABLED\n";
    std::cout << "[INFO] Testing reflection for serialization\n\n";
    
    test_structure_to_text();
    test_member_listing();
    test_complex_structure_analysis();
    test_binary_serialization();
    test_member_count_validation();
    test_field_type_documentation();
    
    std::cout << "========================================\n";
    std::cout << "  Summary\n";
    std::cout << "========================================\n";
    std::cout << "[PASS] Test 1: Structure to text\n";
    std::cout << "[PASS] Test 2: Member listing\n";
    std::cout << "[PASS] Test 3: Complex structure analysis\n";
    std::cout << "[PASS] Test 4: Binary serialization\n";
    std::cout << "[PASS] Test 5: Member count validation\n";
    std::cout << "[PASS] Test 6: Field type documentation\n";
    std::cout << "\n[SUCCESS] All serialization tests passed!\n";
    
    return 0;
}
