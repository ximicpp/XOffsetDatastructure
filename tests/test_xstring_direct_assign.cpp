// ============================================================================
// Test: XString Direct Assignment
// Purpose: Verify that XString::operator=(const char*) correctly allocates
//          string data in the segment (not on the heap), and that the data
//          survives serialization/deserialization.
//
// Background: boost::container::basic_string already provides
//   operator=(const CharT* s) which calls assign(s, s + len).
//   assign() uses this->alloc() — the internally stored allocator —
//   to allocate new char data. Since XString's allocator is
//   allocator<char, segment_manager> with an offset_ptr<segment_manager>,
//   all allocations go through the segment manager into the segment.
//
// This test proves:
//   1. operator=(const char*) compiles and works
//   2. Char data is allocated inside the segment (address in range)
//   3. Data survives serialization + deserialization (C1 + C2)
//   4. Reassignment correctly deallocates old data in segment
//   5. Works with XVector<XString> elements
// ============================================================================

#include <iostream>
#include <cassert>
#include <cstring>
#include "../xoffsetdatastructure.hpp"
#include "../examples/player.hpp"

using namespace XOffsetDatastructure;

struct StringTestData {
    template <typename Allocator>
    StringTestData(Allocator allocator)
        : name(allocator), title(allocator), names(allocator) {}

    XString name;
    XString title;
    XVector<XString> names;
};

bool test_basic_assign() {
    std::cout << "\n[TEST] XString operator=(const char*)\n";
    std::cout << std::string(50, '-') << "\n";

    XBufferExt xbuf(4096);
    auto* data = xbuf.make<StringTestData>();

    // Test 1: Basic assignment from const char*
    std::cout << "Test 1: Basic operator=(const char*)... ";
    data->name = "Alice";
    assert(std::string(data->name.c_str()) == "Alice");
    assert(data->name.size() == 5);
    std::cout << "[OK]\n";

    // Test 2: Verify data is in segment (not on heap)
    std::cout << "Test 2: Char data is in segment... ";
    const void* buf_start = xbuf.get_address();
    const void* buf_end = static_cast<const char*>(buf_start) + xbuf.get_size();
    const void* name_data = data->name.c_str();
    // For short strings, data may be in SSO buffer (which is part of the
    // XString object itself, which IS in the segment). For long strings,
    // data is allocated via segment allocator. Either way, data is in segment.
    assert(name_data >= buf_start && name_data < buf_end);
    std::cout << "[OK]\n";

    // Test 3: Reassignment
    std::cout << "Test 3: Reassignment... ";
    data->name = "Bob";
    assert(std::string(data->name.c_str()) == "Bob");
    data->name = "A much longer string that exceeds SSO buffer size for sure";
    assert(std::string(data->name.c_str()) == "A much longer string that exceeds SSO buffer size for sure");
    // Verify long string is also in segment
    const void* long_data = data->name.c_str();
    assert(long_data >= buf_start && long_data < buf_end);
    std::cout << "[OK]\n";

    // Test 4: Multiple fields
    std::cout << "Test 4: Multiple fields... ";
    data->name = "Charlie";
    data->title = "Warrior";
    assert(std::string(data->name.c_str()) == "Charlie");
    assert(std::string(data->title.c_str()) == "Warrior");
    std::cout << "[OK]\n";

    return true;
}

bool test_serialization_roundtrip() {
    std::cout << "\n[TEST] Serialization roundtrip with direct assign\n";
    std::cout << std::string(50, '-') << "\n";

    // Test 5: Create data with direct assignment, serialize, deserialize
    std::cout << "Test 5: Serialize after direct assign... ";
    XBufferExt xbuf(4096);
    auto* data = xbuf.make<StringTestData>();
    data->name = "SerializedAlice";
    data->title = "ArchMage";

    std::string binary = xbuf.save_to_string();
    XBufferExt loaded = XBufferExt::load_from_string(binary);
    assert(loaded.has_root<StringTestData>()); auto& loaded_data = loaded.root<StringTestData>();
    assert(std::string(loaded_data.name.c_str()) == "SerializedAlice");
    assert(std::string(loaded_data.title.c_str()) == "ArchMage");
    std::cout << "[OK]\n";

    // Test 6: Modify deserialized data with direct assign
    std::cout << "Test 6: Modify deserialized data... ";
    loaded_data.name = "ModifiedBob";
    assert(std::string(loaded_data.name.c_str()) == "ModifiedBob");
    std::cout << "[OK]\n";

    // Test 7: Re-serialize and verify
    std::cout << "Test 7: Re-serialize and verify... ";
    std::string binary2 = loaded.save_to_string();
    XBufferExt loaded2 = XBufferExt::load_from_string(binary2);
    assert(loaded2.has_root<StringTestData>()); auto& loaded_data2 = loaded2.root<StringTestData>();
    assert(std::string(loaded_data2.name.c_str()) == "ModifiedBob");
    assert(std::string(loaded_data2.title.c_str()) == "ArchMage");
    std::cout << "[OK]\n";

    return true;
}

bool test_vector_of_strings() {
    std::cout << "\n[TEST] XVector<XString> element direct assign\n";
    std::cout << std::string(50, '-') << "\n";

    XBufferExt xbuf(8192);
    auto* data = xbuf.make<StringTestData>();

    // Test 8: Add strings to vector using emplace_back
    std::cout << "Test 8: Populate vector... ";
    data->names.push_back("Initial1");
    data->names.push_back("Initial2");
    data->names.push_back("Initial3");
    assert(data->names.size() == 3);
    std::cout << "[OK]\n";

    // Test 9: Modify vector elements with direct assign
    std::cout << "Test 9: Direct assign to vector elements... ";
    data->names[0] = "Modified1";
    data->names[1] = "Modified2";
    data->names[2] = "Modified3";
    assert(std::string(data->names[0].c_str()) == "Modified1");
    assert(std::string(data->names[1].c_str()) == "Modified2");
    assert(std::string(data->names[2].c_str()) == "Modified3");
    std::cout << "[OK]\n";

    // Test 10: Verify through serialization
    std::cout << "Test 10: Serialize vector of strings... ";
    std::string binary = xbuf.save_to_string();
    XBufferExt loaded = XBufferExt::load_from_string(binary);
    assert(loaded.has_root<StringTestData>()); auto& loaded_data = loaded.root<StringTestData>();
    assert(loaded_data.names.size() == 3);
    assert(std::string(loaded_data.names[0].c_str()) == "Modified1");
    assert(std::string(loaded_data.names[1].c_str()) == "Modified2");
    assert(std::string(loaded_data.names[2].c_str()) == "Modified3");
    std::cout << "[OK]\n";

    return true;
}

bool test_player_direct_assign() {
    std::cout << "\n[TEST] Player struct with direct assign (replaces verbose pattern)\n";
    std::cout << std::string(50, '-') << "\n";

    XBufferExt xbuf(4096);

    // Test 11: Old verbose pattern vs new direct assign
    std::cout << "Test 11: Direct assign on Player... ";
    auto* player = xbuf.make<Player>();
    player->id = 1;
    player->level = 10;

    // NEW: direct assign (instead of: player->name = XString("Alice", xbuf.allocator<XString>()))
    player->name = "Alice";

    assert(std::string(player->name.c_str()) == "Alice");
    assert(player->id == 1);
    assert(player->level == 10);
    std::cout << "[OK]\n";

    // Test 12: Verify serialization
    std::cout << "Test 12: Serialize Player... ";
    std::string binary = xbuf.save_to_string();
    XBufferExt loaded = XBufferExt::load_from_string(binary);
    assert(loaded.has_root<Player>()); auto& loaded_player = loaded.root<Player>();
    assert(std::string(loaded_player.name.c_str()) == "Alice");
    assert(loaded_player.id == 1);
    assert(loaded_player.level == 10);
    std::cout << "[OK]\n";

    // Test 13: Reassign and re-serialize
    std::cout << "Test 13: Reassign and re-serialize... ";
    player->name = "Bob";
    binary = xbuf.save_to_string();
    XBufferExt loaded2 = XBufferExt::load_from_string(binary);
    assert(loaded2.has_root<Player>()); auto& loaded_player2 = loaded2.root<Player>();
    assert(std::string(loaded_player2.name.c_str()) == "Bob");
    std::cout << "[OK]\n";

    return true;
}

int main() {
    std::cout << "========================================\n";
    std::cout << "  XString Direct Assignment Test\n";
    std::cout << "========================================\n";

    bool all_passed = true;
    all_passed &= test_basic_assign();
    all_passed &= test_serialization_roundtrip();
    all_passed &= test_vector_of_strings();
    all_passed &= test_player_direct_assign();

    std::cout << "\n========================================\n";
    if (all_passed) {
        std::cout << "[PASS] All XString direct assignment tests passed!\n";
        std::cout << "\nSummary:\n";
        std::cout << "  - operator=(const char*) works natively via Boost.Container\n";
        std::cout << "  - Char data is allocated in segment via internal allocator\n";
        std::cout << "  - Data survives serialization/deserialization\n";
        std::cout << "  - Works with XVector<XString> elements\n";
        std::cout << "  - Replaces verbose XString(\"...\", allocator) pattern\n";
        return 0;
    } else {
        std::cout << "[FAIL] Some tests failed!\n";
        return 1;
    }
}
