// ============================================================================
// Test: Platform Requirements
// Purpose: Verify platform meets requirements (64-bit, little-endian)
// ============================================================================

#include <iostream>
#include <cassert>
#include <cstdint>
#include <cstring>
#include "../xoffsetdatastructure2.hpp"

using namespace XOffsetDatastructure2;

// ============================================================================
// Platform Detection
// ============================================================================

bool is_64bit_platform() {
    return sizeof(void*) == 8;
}

bool is_little_endian() {
    uint32_t test = 0x01020304;
    uint8_t* bytes = reinterpret_cast<uint8_t*>(&test);
    return bytes[0] == 0x04;  // Little-endian: LSB first
}

const char* get_endianness_name() {
    return is_little_endian() ? "Little-Endian" : "Big-Endian";
}

// ============================================================================
// Type Size Verification
// ============================================================================

bool verify_type_sizes() {
    std::cout << "  Verifying type sizes...\n";
    
    bool all_correct = true;
    
    // Basic types
    if (sizeof(char) != 1) {
        std::cout << "    ❌ sizeof(char) = " << sizeof(char) << ", expected 1\n";
        all_correct = false;
    }
    
    if (sizeof(int) != 4) {
        std::cout << "    ❌ sizeof(int) = " << sizeof(int) << ", expected 4\n";
        all_correct = false;
    }
    
    if (sizeof(float) != 4) {
        std::cout << "    ❌ sizeof(float) = " << sizeof(float) << ", expected 4\n";
        all_correct = false;
    }
    
    if (sizeof(double) != 8) {
        std::cout << "    ❌ sizeof(double) = " << sizeof(double) << ", expected 8\n";
        all_correct = false;
    }
    
    if (sizeof(long long) != 8) {
        std::cout << "    ❌ sizeof(long long) = " << sizeof(long long) << ", expected 8\n";
        all_correct = false;
    }
    
    if (sizeof(void*) != 8) {
        std::cout << "    ❌ sizeof(void*) = " << sizeof(void*) << ", expected 8\n";
        all_correct = false;
    }
    
    // Container types
    if (sizeof(XString) != 32) {
        std::cout << "    ❌ sizeof(XString) = " << sizeof(XString) << ", expected 32\n";
        all_correct = false;
    }
    
    if (all_correct) {
        std::cout << "    ✓ All type sizes correct\n";
    }
    
    return all_correct;
}

// ============================================================================
// Endianness Verification
// ============================================================================

bool verify_endianness() {
    std::cout << "  Verifying endianness...\n";
    
    // Test 1: uint32_t byte order
    uint32_t test_u32 = 0x12345678;
    uint8_t* bytes_u32 = reinterpret_cast<uint8_t*>(&test_u32);
    
    if (bytes_u32[0] != 0x78 || bytes_u32[1] != 0x56 || 
        bytes_u32[2] != 0x34 || bytes_u32[3] != 0x12) {
        std::cout << "    ❌ uint32_t byte order incorrect\n";
        std::cout << "       Expected: 78 56 34 12\n";
        std::cout << "       Got:      " << std::hex 
                  << (int)bytes_u32[0] << " " << (int)bytes_u32[1] << " "
                  << (int)bytes_u32[2] << " " << (int)bytes_u32[3] << std::dec << "\n";
        return false;
    }
    
    // Test 2: double byte order
    double test_double = 1.0;
    uint8_t* bytes_double = reinterpret_cast<uint8_t*>(&test_double);
    // IEEE 754 double 1.0 in little-endian: 00 00 00 00 00 00 F0 3F
    if (bytes_double[7] != 0x3F || bytes_double[6] != 0xF0) {
        std::cout << "    ❌ double byte order incorrect\n";
        return false;
    }
    
    std::cout << "    ✓ Endianness is little-endian\n";
    return true;
}

// ============================================================================
// Alignment Verification
// ============================================================================

bool verify_alignment() {
    std::cout << "  Verifying struct alignment...\n";
    
    struct TestStruct {
        char c;
        int i;
        double d;
    };
    
    // Check natural alignment
    if (alignof(int) != 4) {
        std::cout << "    ❌ alignof(int) = " << alignof(int) << ", expected 4\n";
        return false;
    }
    
    if (alignof(double) != 8) {
        std::cout << "    ❌ alignof(double) = " << alignof(double) << ", expected 8\n";
        return false;
    }
    
    if (alignof(void*) != 8) {
        std::cout << "    ❌ alignof(void*) = " << alignof(void*) << ", expected 8\n";
        return false;
    }
    
    std::cout << "    ✓ Alignment requirements met\n";
    return true;
}

// ============================================================================
// Binary Compatibility Test
// ============================================================================

bool test_binary_compatibility() {
    std::cout << "  Testing binary data compatibility...\n";
    
    // Create buffer with known data
    XBufferExt xbuf(1024);
    
    // Write known values - use vector to avoid array construction issues
    typedef XOffsetDatastructure2::XVector<int> IntVector;
    auto* int_vec = xbuf.make<IntVector>("test_ints");
    int_vec->push_back(0x12345678);
    int_vec->push_back(0xABCDEF00);
    int_vec->push_back(0x11223344);
    int_vec->push_back(0x55667788);
    
    int* int_array = int_vec->data();
    
    // Verify byte order in buffer
    auto* buffer = xbuf.get_buffer();
    const uint8_t* data = reinterpret_cast<const uint8_t*>(buffer->data());
    
    // Find our test data in the buffer
    bool found = false;
    for (size_t i = 0; i < buffer->size() - 16; i++) {
        if (data[i] == 0x78 && data[i+1] == 0x56 && 
            data[i+2] == 0x34 && data[i+3] == 0x12) {
            found = true;
            
            // Verify all four integers
            const uint8_t expected[] = {
                0x78, 0x56, 0x34, 0x12,  // 0x12345678
                0x00, 0xEF, 0xCD, 0xAB,  // 0xABCDEF00
                0x44, 0x33, 0x22, 0x11,  // 0x11223344
                0x88, 0x77, 0x66, 0x55   // 0x55667788
            };
            
            bool all_match = true;
            for (int j = 0; j < 16; j++) {
                if (data[i + j] != expected[j]) {
                    all_match = false;
                    break;
                }
            }
            
            if (!all_match) {
                std::cout << "    ❌ Binary data layout incorrect\n";
                return false;
            }
            break;
        }
    }
    
    if (!found) {
        std::cout << "    ❌ Could not find test data in buffer\n";
        return false;
    }
    
    std::cout << "    ✓ Binary compatibility verified\n";
    return true;
}

// ============================================================================
// Cross-buffer Compatibility Test
// ============================================================================

bool test_cross_buffer_compatibility() {
    std::cout << "  Testing cross-buffer data transfer...\n";
    
    // Create source buffer with vector
    XBufferExt src_buf(1024);
    typedef XOffsetDatastructure2::XVector<int> IntVector;
    auto* src_vec = src_buf.make<IntVector>("numbers");
    for (int i = 0; i < 5; i++) {
        src_vec->push_back(i * 1000 + i);  // 0, 1001, 2002, 3003, 4004
    }
    
    // Serialize to binary
    auto* src_buffer = src_buf.get_buffer();
    std::vector<char> binary_data(src_buffer->begin(), src_buffer->end());
    
    // Load into new buffer
    XBufferExt dst_buf(binary_data);
    auto* dst_vec = dst_buf.find<IntVector>("numbers").first;
    
    if (!dst_vec) {
        std::cout << "    ❌ Failed to find data in destination buffer\n";
        return false;
    }
    
    // Verify data integrity
    for (size_t i = 0; i < dst_vec->size(); i++) {
        if ((*dst_vec)[i] != static_cast<int>(i * 1000 + i)) {
            std::cout << "    ❌ Data mismatch at index " << i 
                      << ": expected " << (i * 1000 + i)
                      << ", got " << (*dst_vec)[i] << "\n";
            return false;
        }
    }
    
    std::cout << "    ✓ Cross-buffer compatibility verified\n";
    return true;
}

// ============================================================================
// Main Test Function
// ============================================================================

bool test_platform_requirements() {
    std::cout << "\n[TEST] Platform Requirements Verification\n";
    std::cout << std::string(50, '-') << "\n";
    
    // Display platform information
    std::cout << "Platform Information:\n";
    std::cout << "  Architecture: " << (is_64bit_platform() ? "64-bit" : "32-bit") << "\n";
    std::cout << "  Endianness:   " << get_endianness_name() << "\n";
    std::cout << "  Pointer size: " << sizeof(void*) << " bytes\n";
    std::cout << "\n";
    
    // Check requirements
    std::cout << "Checking Requirements:\n";
    
    bool is_64bit = is_64bit_platform();
    bool is_le = is_little_endian();
    
    std::cout << "  64-bit architecture: " << (is_64bit ? "✓ YES" : "✗ NO") << "\n";
    std::cout << "  Little-endian:       " << (is_le ? "✓ YES" : "✗ NO") << "\n";
    std::cout << "\n";
    
    // If requirements not met, fail with clear message
    if (!is_64bit) {
        std::cout << "[FAIL] ❌ XOffsetDatastructure2 requires 64-bit architecture\n";
        std::cout << "       Current platform is " << (sizeof(void*) * 8) << "-bit\n";
        std::cout << "       This library is designed for 64-bit systems only.\n";
        return false;
    }
    
    if (!is_le) {
        std::cout << "[FAIL] ❌ XOffsetDatastructure2 requires little-endian architecture\n";
        std::cout << "       Current platform is big-endian\n";
        std::cout << "       This library stores data in little-endian format.\n";
        std::cout << "       Using it on big-endian systems will cause data corruption.\n";
        return false;
    }
    
    // Run verification tests
    std::cout << "Running Verification Tests:\n";
    
    if (!verify_type_sizes()) {
        std::cout << "[FAIL] Type size verification failed\n";
        return false;
    }
    
    if (!verify_endianness()) {
        std::cout << "[FAIL] Endianness verification failed\n";
        return false;
    }
    
    if (!verify_alignment()) {
        std::cout << "[FAIL] Alignment verification failed\n";
        return false;
    }
    
    if (!test_binary_compatibility()) {
        std::cout << "[FAIL] Binary compatibility test failed\n";
        return false;
    }
    
    if (!test_cross_buffer_compatibility()) {
        std::cout << "[FAIL] Cross-buffer compatibility test failed\n";
        return false;
    }
    
    std::cout << "\n";
    std::cout << "[PASS] ✓ Platform meets all requirements!\n";
    std::cout << "       Your system is compatible with XOffsetDatastructure2.\n";
    std::cout << "       - 64-bit architecture\n";
    std::cout << "       - Little-endian byte order\n";
    std::cout << "       - Correct type sizes and alignment\n";
    std::cout << "       - Binary data compatibility verified\n";
    
    return true;
}

int main() {
    try {
        return test_platform_requirements() ? 0 : 1;
    } catch (const std::exception& e) {
        std::cerr << "[FAIL] Exception: " << e.what() << "\n";
        return 1;
    }
}
