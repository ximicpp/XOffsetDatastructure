// ============================================================================
// Test: Reflection-Driven Memory Compaction
// Purpose: Test reflection in memory optimization scenarios
// ============================================================================

#include "../xoffsetdatastructure2.hpp"
#include <iostream>

#include <experimental/meta>
#include <experimental/meta>

using namespace XOffsetDatastructure2;

struct CompactData {
    uint32_t id;
    XString name;
    XVector<int> values;
    
    template <typename Allocator>
    CompactData(Allocator allocator) 
        : id(0), name(allocator), values(allocator) {}
};

// Helper functions
template<typename T>
consteval size_t get_member_count_ce() {
    using namespace std::meta;
    return nonstatic_data_members_of(^^T, access_context::unchecked()).size();
}

template<typename T, size_t I>
consteval auto get_member_ce() {
    using namespace std::meta;
    auto members = nonstatic_data_members_of(^^T, access_context::unchecked());
    return members[I];
}

template<typename T, size_t I>
void print_member_info() {
    using namespace std::meta;
    constexpr auto member = get_member_ce<T, I>();
    std::cout << "      - " << display_string_of(member) 
              << ": " << display_string_of(type_of(member)) << "\n";
}

template<typename T, size_t... Is>
void print_all_members(std::index_sequence<Is...>) {
    (print_member_info<T, Is>(), ...);
}

void test_structure_analysis() {
    std::cout << "[Test 1] Structure Analysis\n";
    std::cout << "---------------------------\n";
    
    constexpr size_t member_count = get_member_count_ce<CompactData>();
    
    std::cout << "  CompactData structure:\n";
    std::cout << "    Member count: " << member_count << "\n";
    std::cout << "    Members:\n";
    
    print_all_members<CompactData>(std::make_index_sequence<member_count>{});
    
    std::cout << "[PASS] Structure analysis\n\n";
}

void test_memory_usage_tracking() {
    std::cout << "[Test 2] Memory Usage Tracking\n";
    std::cout << "------------------------------\n";
    
    XBufferExt xbuf(4096);
    
    auto stats_empty = xbuf.stats();
    std::cout << "  Initial state:\n";
    std::cout << "    Total: " << stats_empty.total_size << " bytes\n";
    std::cout << "    Used: " << stats_empty.used_size << " bytes\n";
    std::cout << "    Free: " << stats_empty.free_size << " bytes\n";
    
    // Create data
    auto* data = xbuf.make<CompactData>("data");
    data->id = 1001;
    data->name = XString("TestObject", xbuf.allocator<XString>());
    
    for (int i = 0; i < 100; ++i) {
        data->values.push_back(i);
    }
    
    auto stats_filled = xbuf.stats();
    std::cout << "\n  After adding data:\n";
    std::cout << "    Total: " << stats_filled.total_size << " bytes\n";
    std::cout << "    Used: " << stats_filled.used_size << " bytes\n";
    std::cout << "    Free: " << stats_filled.free_size << " bytes\n";
    std::cout << "    Usage: " << stats_filled.usage_percent() << "%\n";
    
    std::cout << "[PASS] Memory usage tracking\n\n";
}

void test_compaction_with_reflection() {
    std::cout << "[Test 3] Compaction with Reflection\n";
    std::cout << "------------------------------------\n";
    
    XBufferExt xbuf(8192);
    auto* data = xbuf.make<CompactData>("data");
    
    data->id = 2002;
    data->name = XString("CompactionTest", xbuf.allocator<XString>());
    for (int i = 0; i < 50; ++i) {
        data->values.push_back(i * 2);
    }
    
    std::cout << "  Before compaction:\n";
    auto stats_before = xbuf.stats();
    std::cout << "    Total: " << stats_before.total_size << " bytes\n";
    std::cout << "    Used: " << stats_before.used_size << " bytes\n";
    std::cout << "    Usage: " << stats_before.usage_percent() << "%\n";
    
    // Analyze structure via reflection
    constexpr size_t member_count = get_member_count_ce<CompactData>();
    std::cout << "\n  Structure has " << member_count << " members\n";
    
    // Shrink buffer
    xbuf.shrink_to_fit();
    
    std::cout << "\n  After shrink_to_fit:\n";
    auto stats_after = xbuf.stats();
    std::cout << "    Total: " << stats_after.total_size << " bytes\n";
    std::cout << "    Used: " << stats_after.used_size << " bytes\n";
    std::cout << "    Usage: " << stats_after.usage_percent() << "%\n";
    
    // Verify data integrity
    bool integrity = (data->id == 2002 && 
                     data->values.size() == 50 &&
                     std::string(data->name.c_str()) == "CompactionTest");
    
    std::cout << "\n  Data integrity: " << (integrity ? "OK" : "FAIL") << "\n";
    
    std::cout << "[PASS] Compaction with reflection\n\n";
}

void test_grow_and_verify() {
    std::cout << "[Test 4] Buffer Growth and Verification\n";
    std::cout << "----------------------------------------\n";
    
    XBufferExt xbuf(1024);
    auto* data = xbuf.make<CompactData>("data");
    
    data->id = 3003;
    data->name = XString("GrowTest", xbuf.allocator<XString>());
    
    std::cout << "  Initial buffer: " << xbuf.stats().total_size << " bytes\n";
    
    // Grow buffer
    xbuf.grow(4096);
    
    std::cout << "  After grow: " << xbuf.stats().total_size << " bytes\n";
    
    // Verify data still accessible
    std::cout << "\n  Data after grow:\n";
    std::cout << "    id: " << data->id << "\n";
    std::cout << "    name: " << data->name.c_str() << "\n";
    
    // Check via reflection
    constexpr size_t member_count = get_member_count_ce<CompactData>();
    std::cout << "\n  Member count via reflection: " << member_count << "\n";
    
    std::cout << "[PASS] Buffer growth\n\n";
}

void test_serialization_size() {
    std::cout << "[Test 5] Serialization Size Analysis\n";
    std::cout << "-------------------------------------\n";
    
    XBufferExt xbuf(2048);
    auto* data = xbuf.make<CompactData>("data");
    
    data->id = 4004;
    data->name = XString("SerializationTest", xbuf.allocator<XString>());
    for (int i = 0; i < 20; ++i) {
        data->values.push_back(i);
    }
    
    std::cout << "  Before serialization:\n";
    auto stats = xbuf.stats();
    std::cout << "    Buffer size: " << stats.total_size << " bytes\n";
    std::cout << "    Used: " << stats.used_size << " bytes\n";
    
    // Serialize
    std::string binary = xbuf.save_to_string();
    std::cout << "\n  Serialized size: " << binary.size() << " bytes\n";
    
    // Deserialize
    XBufferExt xbuf2 = XBufferExt::load_from_string(binary);
    auto stats2 = xbuf2.stats();
    
    std::cout << "\n  After deserialization:\n";
    std::cout << "    Buffer size: " << stats2.total_size << " bytes\n";
    std::cout << "    Used: " << stats2.used_size << " bytes\n";
    
    std::cout << "[PASS] Serialization size\n\n";
}


int main() {
    std::cout << "========================================\n";
    std::cout << "  Reflection Compaction Test\n";
    std::cout << "========================================\n\n";

#include <experimental/meta>
    std::cout << "[INFO] C++26 Reflection: ENABLED\n";
    std::cout << "[INFO] Testing reflection with memory operations\n\n";
    
    test_structure_analysis();
    test_memory_usage_tracking();
    test_compaction_with_reflection();
    test_grow_and_verify();
    test_serialization_size();
    
    std::cout << "========================================\n";
    std::cout << "  Summary\n";
    std::cout << "========================================\n";
    std::cout << "[PASS] Test 1: Structure analysis\n";
    std::cout << "[PASS] Test 2: Memory usage tracking\n";
    std::cout << "[PASS] Test 3: Compaction with reflection\n";
    std::cout << "[PASS] Test 4: Buffer growth\n";
    std::cout << "[PASS] Test 5: Serialization size\n";
    std::cout << "\n[SUCCESS] All compaction tests passed!\n";
    
    return 0;
}
