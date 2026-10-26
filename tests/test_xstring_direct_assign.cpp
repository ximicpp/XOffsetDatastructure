// ============================================================================
// Test: XString Direct Assignment
// Purpose: Verify that XString::operator=(const char*) correctly allocates
//          string data in the XOffset buffer arena (not on the heap), and that
//          the data
//          survives serialization/deserialization.
//
// Background: XString stores a bound arena allocator and uses it for
//   operator=(const char*) / assign() growth. The resulting char payload
//   must stay inside the XOffset buffer arena instead of escaping to the heap.
//
// This test proves:
//   1. operator=(const char*) compiles and works
//   2. Char data is allocated inside the buffer arena (address in range)
//   3. Data survives serialization + deserialization (C1 + C2)
//   4. Reassignment correctly deallocates old data in the buffer arena
//   5. Works with XVector<XString> elements
// ============================================================================

#include <iostream>
#include <cassert>
#include <cstring>
#include "test_allocator_ctors.hpp"
#include "../examples/player.hpp"

using namespace XOffsetDatastructure;

struct StringTestData {
#if XOFFSET_TEST_ALLOCATOR_CTORS
    template <typename Allocator>
    StringTestData(Allocator allocator)
        : name(allocator), title(allocator), names(allocator) {}
#endif

    XString name;
    XString title;
    XVector<XString> names;
};
#if !XOFFSET_TEST_ALLOCATOR_CTORS
XOFFSET_RECORD_DERIVED(StringTestData, (), name, title, names)
#endif

bool test_basic_assign() {
    std::cout << "\n[TEST] XString operator=(const char*)\n";
    std::cout << std::string(50, '-') << "\n";

    XBuffer xbuf(4096);
    auto* data = xbuf.make<StringTestData>();

    // Test 1: Basic assignment from const char*
    std::cout << "Test 1: Basic operator=(const char*)... ";
    data->name = "Alice";
    assert(std::string(data->name.c_str()) == "Alice");
    assert(data->name.size() == 5);
    std::cout << "[OK]\n";

    // Test 2: Verify data is in the buffer arena (not on heap)
    std::cout << "Test 2: Char data is in the buffer arena... ";
    const void* buf_start = xbuf.get_address();
    const void* buf_end = static_cast<const char*>(buf_start) + xbuf.get_size();
    const void* name_data = data->name.c_str();
    // For short strings, data may be inline inside XString, which still lives
    // inside the arena-backed buffer. For long strings, data is allocated from
    // the arena. Either way, data remains inside the buffer.
    assert(name_data >= buf_start && name_data < buf_end);
    std::cout << "[OK]\n";

    // Test 3: Reassignment
    std::cout << "Test 3: Reassignment... ";
    data->name = "Bob";
    assert(std::string(data->name.c_str()) == "Bob");
    data->name = "A much longer string that exceeds SSO buffer size for sure";
    assert(std::string(data->name.c_str()) == "A much longer string that exceeds SSO buffer size for sure");
    // Verify long string is also in the buffer arena
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
    XBuffer xbuf(4096);
    auto* data = xbuf.make<StringTestData>();
    data->name = "SerializedAlice";
    data->title = "ArchMage";

    std::string binary = xbuf.save();
    XBuffer loaded = XBuffer::load_unverified(binary);
    assert(loaded.unsafe_has_root<StringTestData>()); auto& loaded_data = loaded.unsafe_root<StringTestData>();
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
    std::string binary2 = loaded.save();
    XBuffer loaded2 = XBuffer::load_unverified(binary2);
    assert(loaded2.unsafe_has_root<StringTestData>()); auto& loaded_data2 = loaded2.unsafe_root<StringTestData>();
    assert(std::string(loaded_data2.name.c_str()) == "ModifiedBob");
    assert(std::string(loaded_data2.title.c_str()) == "ArchMage");
    std::cout << "[OK]\n";

    return true;
}

bool test_vector_of_strings() {
    std::cout << "\n[TEST] XVector<XString> element direct assign\n";
    std::cout << std::string(50, '-') << "\n";

    XBuffer xbuf(8192);
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
    std::string binary = xbuf.save();
    XBuffer loaded = XBuffer::load_unverified(binary);
    assert(loaded.unsafe_has_root<StringTestData>()); auto& loaded_data = loaded.unsafe_root<StringTestData>();
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

    XBuffer xbuf(4096);

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
    std::string binary = xbuf.save();
    XBuffer loaded = XBuffer::load_unverified(binary);
    assert(loaded.unsafe_has_root<Player>()); auto& loaded_player = loaded.unsafe_root<Player>();
    assert(std::string(loaded_player.name.c_str()) == "Alice");
    assert(loaded_player.id == 1);
    assert(loaded_player.level == 10);
    std::cout << "[OK]\n";

    // Test 13: Reassign and re-serialize
    std::cout << "Test 13: Reassign and re-serialize... ";
    player = &xbuf.root<Player>();  // re-acquire after save shrink
    player->name = "Bob";
    binary = xbuf.save();
    XBuffer loaded2 = XBuffer::load_unverified(binary);
    assert(loaded2.unsafe_has_root<Player>()); auto& loaded_player2 = loaded2.unsafe_root<Player>();
    assert(std::string(loaded_player2.name.c_str()) == "Bob");
    std::cout << "[OK]\n";

    return true;
}

// Reassigning a heap string that fits its block: the block is kept, the new
// characters and terminator are written, and every byte after the terminator
// up to the end of the block is zero (M4), for every old and new length and
// for views of the string's own characters.
bool test_heap_reassign_in_place() {
    std::cout << "\n[TEST] Heap XString reassignment in place\n";
    std::cout << std::string(50, '-') << "\n";

    constexpr std::size_t cap = 40;
    auto pattern = [](std::size_t length, unsigned seed) {
        std::string out;
        for (std::size_t i = 0; i < length; ++i) out.push_back(static_cast<char>(0x21 + (seed * 7 + i * 13) % 90));
        return out;
    };
    XBuffer xbuf(64 * 1024);
    auto* data = xbuf.make<StringTestData>();
    auto& s = data->name;
    s = std::string_view(pattern(cap, 1));
    assert(!s.is_inline() && s.capacity() == cap);
    const char* block = s.data();
    auto check = [&](const std::string& want) {
        assert(s.view() == want);
        assert(s.data() == block && s.capacity() == cap);
        for (std::size_t i = want.size(); i <= cap; ++i) assert(block[i] == '\0');
    };

    std::cout << "Test 14: Every old and new length... ";
    std::size_t cases = 0;
    for (std::size_t before = 1; before <= cap; ++before) {
        for (std::size_t length = 1; length <= cap; ++length) {
            const std::string old_value = pattern(before, unsigned(before));
            s = std::string_view(old_value);
            check(old_value);
            const std::string want = pattern(length, unsigned(length + 3));
            s = std::string_view(want);
            check(want);
            ++cases;
        }
    }
    std::cout << "[OK] " << cases << " cases\n";

    std::cout << "Test 15: Views of the string's own characters... ";
    cases = 0;
    for (std::size_t length = 1; length <= cap; ++length) {
        for (std::size_t from = 0; from < length; ++from) {
            for (std::size_t count = 1; from + count <= length; ++count) {
                const std::string old_value = pattern(length, unsigned(length));
                s = std::string_view(old_value);
                s = std::string_view(s.data() + from, count);
                check(old_value.substr(from, count));
                ++cases;
            }
        }
    }
    std::cout << "[OK] " << cases << " cases\n";

    std::cout << "Test 16: Longer than the block, then empty... ";
    const std::string longer = pattern(cap + 1, 9);
    s = std::string_view(longer);
    assert(s.view() == longer && s.capacity() == cap + 1);
    s = std::string_view();
    assert(s.empty() && s.data()[0] == '\0');
    std::cout << "[OK]\n";
    return true;
}

// Reassigning an inline string: a short value stays inline and its 12
// inline head bytes are the characters, zero bytes, then the flag and length
// byte (I1), for every old and new length and for views of the string's own
// inline characters. A longer value moves to the heap.
bool test_inline_reassign() {
    std::cout << "\n[TEST] Inline XString reassignment\n";
    std::cout << std::string(50, '-') << "\n";

    constexpr std::size_t inline_max = 10;
    const std::string chars = "abcdefghijklmnop";
    auto check = [&](const XString& s, std::string_view want) {
        assert(s.view() == want);
        if (want.size() > inline_max) {
            assert(!s.is_inline());
            return;
        }
        unsigned char expect[12] = {};
        std::memcpy(expect, want.data(), want.size());
        expect[11] = static_cast<unsigned char>(0x80u | want.size());
        assert(s.is_inline());
        assert(std::memcmp(reinterpret_cast<const char*>(&s) + 4, expect, 12) == 0);
    };

    std::cout << "Test 17: Every old and new length... ";
    std::size_t cases = 0;
    for (std::size_t before = 1; before <= inline_max; ++before) {
        for (std::size_t length = 0; length <= inline_max + 2; ++length) {
            XBuffer xbuf(4096);
            auto& s = xbuf.make<StringTestData>()->name;
            s = std::string_view(chars).substr(16 - before);
            check(s, std::string_view(chars).substr(16 - before));
            const std::string want = chars.substr(0, length);
            s = std::string_view(want);
            check(s, want);
            ++cases;
        }
    }
    std::cout << "[OK] " << cases << " cases\n";

    std::cout << "Test 18: Views of the string's own inline characters... ";
    cases = 0;
    for (std::size_t length = 1; length <= inline_max; ++length) {
        for (std::size_t from = 0; from < length; ++from) {
            for (std::size_t count = 1; from + count <= length; ++count) {
                XBuffer xbuf(4096);
                auto& s = xbuf.make<StringTestData>()->name;
                s = std::string_view(chars).substr(0, length);
                check(s, std::string_view(chars).substr(0, length));
                s = std::string_view(s.data() + from, count);
                check(s, std::string_view(chars).substr(from, count));
                ++cases;
            }
        }
    }
    std::cout << "[OK] " << cases << " cases\n";
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
    all_passed &= test_heap_reassign_in_place();
    all_passed &= test_inline_reassign();

    std::cout << "\n========================================\n";
    if (all_passed) {
        std::cout << "[PASS] All XString direct assignment tests passed!\n";
        std::cout << "\nSummary:\n";
        std::cout << "  - operator=(const char*) works natively via XOffset fixed containers\n";
        std::cout << "  - Char data is allocated in the buffer arena via the internal allocator\n";
        std::cout << "  - Data survives serialization/deserialization\n";
        std::cout << "  - Works with XVector<XString> elements\n";
        std::cout << "  - Replaces verbose XString(\"...\", allocator) pattern\n";
        return 0;
    } else {
        std::cout << "[FAIL] Some tests failed!\n";
        return 1;
    }
}
