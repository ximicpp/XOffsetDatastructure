// ============================================================================
// Test: Reflection Advanced — Serialization, Comparison & Validation
// Purpose: Uses P2996 reflection for structure analysis, serialization
//          round-trip, member-wise comparison, and version compatibility.
//
// Consolidated from:
//   - test_reflection_serialization.cpp  (struct→text, binary serialize, docs)
//   - test_reflection_comparison.cpp     (member count, equality, diff, version)
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

struct SerializableData {
    int id;
    double value;
    uint32_t flags;

    template <typename Allocator>
    SerializableData(Allocator allocator) : id(0), value(0.0), flags(0) {}
};

struct ComplexData {
    uint32_t type;
    XString  name;
    XVector<int> items;

    template <typename Allocator>
    ComplexData(Allocator allocator)
        : type(0), name(allocator), items(allocator) {}
};

struct Point3D { float x, y, z; };

// ---------------------------------------------------------------------------
// Compile-time helpers
// ---------------------------------------------------------------------------

template<typename T>
consteval size_t member_count() {
    return nonstatic_data_members_of(^^T, access_context::unchecked()).size();
}

template<typename T, size_t I>
consteval auto member_at() {
    return nonstatic_data_members_of(^^T, access_context::unchecked())[I];
}

template<typename T, size_t I>
void print_field() {
    constexpr auto m = member_at<T, I>();
    std::cout << "    " << display_string_of(m) << " : "
              << display_string_of(type_of(m)) << "\n";
}

template<typename T, size_t... Is>
void print_all_fields(std::index_sequence<Is...>) {
    (print_field<T, Is>(), ...);
}

// ---------------------------------------------------------------------------
// Test 1: Structure-to-Text via Reflection
// ---------------------------------------------------------------------------

void test_structure_to_text() {
    std::cout << "[Test 1] Structure to Text\n";
    std::cout << std::string(40, '-') << "\n";

    constexpr size_t mc = member_count<SerializableData>();
    std::cout << "  SerializableData (" << mc << " fields):\n";
    print_all_fields<SerializableData>(std::make_index_sequence<mc>{});

    constexpr size_t cc = member_count<ComplexData>();
    std::cout << "  ComplexData (" << cc << " fields):\n";
    print_all_fields<ComplexData>(std::make_index_sequence<cc>{});

    static_assert(mc == 3, "SerializableData should have 3 members");
    static_assert(cc == 3, "ComplexData should have 3 members");
    std::cout << "[PASS]\n\n";
}

// ---------------------------------------------------------------------------
// Test 2: Binary Serialization Round-Trip
// ---------------------------------------------------------------------------

void test_binary_serialization() {
    std::cout << "[Test 2] Binary Serialization Round-Trip\n";
    std::cout << std::string(40, '-') << "\n";

    XBuffer xbuf(1024);
    auto* data = xbuf.make<SerializableData>();
    data->id    = 999;
    data->value = 123.456;
    data->flags = 0xDEADBEEF;

    std::string binary = xbuf.save();
    std::cout << "  Serialized: " << binary.size() << " bytes\n";

    XBuffer xbuf2 = XBuffer::load(binary);
    auto& loaded = xbuf2.root<SerializableData>();

    assert(loaded.id    == 999);
    assert(loaded.value == 123.456);
    assert(loaded.flags == 0xDEADBEEF);
    std::cout << "  id="    << loaded.id
              << " value="  << loaded.value
              << " flags=0x" << std::hex << loaded.flags << std::dec << " [OK]\n";
    std::cout << "[PASS]\n\n";
}

// ---------------------------------------------------------------------------
// Test 3: Complex Structure Analysis + Serialization
// ---------------------------------------------------------------------------

void test_complex_structure() {
    std::cout << "[Test 3] Complex Structure Analysis\n";
    std::cout << std::string(40, '-') << "\n";

    XBuffer xbuf(2048);
    auto* data = xbuf.make<ComplexData>();
    data->type = 100;
    data->name = "TestObj";
    data->items.push_back(1);
    data->items.push_back(2);
    data->items.push_back(3);

    std::cout << "  type=" << data->type
              << " name=" << data->name.c_str()
              << " items=[";
    for (size_t i = 0; i < data->items.size(); ++i) {
        if (i > 0) std::cout << ",";
        std::cout << data->items[i];
    }
    std::cout << "]\n";

    // Serialize and reload
    std::string bin = xbuf.save();
    XBuffer xbuf2 = XBuffer::load(bin);
    auto& loaded = xbuf2.root<ComplexData>();
    assert(loaded.type == 100);
    assert(loaded.items.size() == 3);
    std::cout << "  Round-trip: type=" << loaded.type
              << " items.size=" << loaded.items.size() << " [OK]\n";
    std::cout << "[PASS]\n\n";
}

// ---------------------------------------------------------------------------
// Test 4: Compile-Time Member Count & Type Checks
// ---------------------------------------------------------------------------

void test_compile_time_checks() {
    std::cout << "[Test 4] Compile-Time Member Count & Type Checks\n";
    std::cout << std::string(40, '-') << "\n";

    constexpr auto sc = member_count<SerializableData>();
    constexpr auto pc = member_count<Point3D>();
    static_assert(sc == 3 && pc == 3);

    // Type-level member check: all Point3D fields are float
    constexpr bool all_float = [] consteval {
        auto ms = nonstatic_data_members_of(^^Point3D, access_context::unchecked());
        for (auto m : ms) if (type_of(m) != ^^float) return false;
        return true;
    }();
    static_assert(all_float, "Point3D members should all be float");

    std::cout << "  SerializableData: " << sc << " members\n";
    std::cout << "  Point3D:          " << pc << " members (all float: "
              << all_float << ")\n";
    std::cout << "[PASS]\n\n";
}

// ---------------------------------------------------------------------------
// Test 5: Member-Wise Comparison & Diff
// ---------------------------------------------------------------------------

void test_member_wise_comparison() {
    std::cout << "[Test 5] Member-Wise Comparison & Diff\n";
    std::cout << std::string(40, '-') << "\n";

    XBuffer xbuf1(1024), xbuf2(1024);
    auto* d1 = xbuf1.make<SerializableData>();
    auto* d2 = xbuf2.make<SerializableData>();

    d1->id = 10; d1->value = 20.5; d1->flags = 30;
    d2->id = 10; d2->value = 20.5; d2->flags = 30;

    assert(d1->id == d2->id && d1->value == d2->value && d1->flags == d2->flags);
    std::cout << "  Equal:     id=" << d1->id << " value=" << d1->value << " [OK]\n";

    d2->flags = 31;
    assert(d1->flags != d2->flags);
    std::cout << "  Diff:      flags " << d1->flags << " vs " << d2->flags << " [OK]\n";

    // Point3D diff
    Point3D p1{10.f, 20.f, 30.f}, p2{10.f, 25.f, 30.f};
    int diffs = (p1.x != p2.x) + (p1.y != p2.y) + (p1.z != p2.z);
    assert(diffs == 1);
    std::cout << "  Point3D:   " << diffs << "/3 fields differ [OK]\n";
    std::cout << "[PASS]\n\n";
}

// ---------------------------------------------------------------------------
// Test 6: Version Compatibility Simulation
// ---------------------------------------------------------------------------

void test_version_compatibility() {
    std::cout << "[Test 6] Version Compatibility\n";
    std::cout << std::string(40, '-') << "\n";

    constexpr size_t EXPECTED_V1 = 3;
    constexpr size_t current = member_count<SerializableData>();

    if constexpr (current == EXPECTED_V1) {
        std::cout << "  v1 (" << EXPECTED_V1 << " members) == current ("
                  << current << "): COMPATIBLE [OK]\n";
    } else if constexpr (current > EXPECTED_V1) {
        std::cout << "  FORWARD COMPATIBLE (new fields added)\n";
    } else {
        std::cout << "  INCOMPATIBLE (fields removed)\n";
    }

    std::cout << "[PASS]\n\n";
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------

int main() {
    std::cout << "========================================\n";
    std::cout << "  Reflection Advanced Test\n";
    std::cout << "========================================\n\n";

    test_structure_to_text();
    test_binary_serialization();
    test_complex_structure();
    test_compile_time_checks();
    test_member_wise_comparison();
    test_version_compatibility();

    std::cout << "========================================\n";
    std::cout << "[SUCCESS] All reflection advanced tests passed!\n";
    return 0;
}
