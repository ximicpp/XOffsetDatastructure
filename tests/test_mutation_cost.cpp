// Test mutation cost and fragmentation

#include <iostream>
#include <cassert>
#include <string>
#include <vector>
#include "../xoffsetdatastructure2.hpp"

using namespace XOffsetDatastructure2;

struct alignas(BASIC_ALIGNMENT) MutationTest {
    template <typename Allocator>
    MutationTest(Allocator allocator) : data(allocator) {}
    XString data;
};

// Reflection hint
struct alignas(BASIC_ALIGNMENT) MutationTestReflectionHint {
    XString data;
};

// Manual type signature for test
namespace XTypeSignature {
    template<>
    struct TypeSignature<MutationTestReflectionHint> {
        static constexpr auto calculate() noexcept {
            return CompileString{"struct[s:32,a:8]{@0:string[s:32,a:8]}"};
        }
    };
}

int main() {
    try {
        std::cout << "Testing mutation cost...\n";
        
        // 1. Create buffer
        XBuffer xbuf(1024 * 1024); // 1MB initial
        auto* obj = xbuf.make_root<MutationTest>("TestObj");
        
        size_t initial_used = xbuf.get_size() - xbuf.get_free_memory();
        std::cout << "Initial used memory: " << initial_used << " bytes\n";
        
        // 2. Mutation loop
        std::string long_str(1000, 'A'); // 1KB string
        std::string short_str = "Short";
        
        for (int i = 0; i < 100; ++i) {
            // Assign long string -> allocates new block
            obj->data.assign(long_str.c_str());
            
            // Assign short string -> allocates new block (usually) or reuses if allocator is smart
            obj->data.assign(short_str.c_str());
        }
        
        size_t final_used = xbuf.get_size() - xbuf.get_free_memory();
        std::cout << "Final used memory: " << final_used << " bytes\n";
        
        size_t growth = final_used - initial_used;
        std::cout << "Growth after 100 mutations: " << growth << " bytes\n";
        
        // Check if growth is significant
        if (growth > 50 * 1024) {
            std::cout << "WARNING: Significant memory growth detected! Fragmentation is high.\n";
            return 1; 
        } else {
            std::cout << "Memory growth is acceptable.\n";
        }
        
        // 3. Fragmentation test
        std::cout << "\nTesting fragmentation...\n";
        std::vector<XString*> strings;
        size_t before_bulk = xbuf.get_size();
        
        // Allocate 100 x 1KB strings
        for (int i = 0; i < 100; ++i) {
            auto* s = xbuf.construct<XString>(anonymous_instance)(long_str.c_str(), xbuf.allocator<char>());
            strings.push_back(s);
        }
        
        size_t peak_size = xbuf.get_size();
        std::cout << "Peak size (after bulk alloc): " << peak_size << " bytes\n";
        
        // Free all strings
        for (auto* s : strings) {
            xbuf.destroy_ptr(s);
        }
        
        size_t after_free = xbuf.get_size();
        std::cout << "Size after free: " << after_free << " bytes\n";
        
        if (after_free == peak_size) {
            std::cout << "OBSERVATION: File size did NOT shrink automatically (Expected behavior for vector-backed memory).\n";
        } else {
            std::cout << "OBSERVATION: File size shrank (Unexpected).\n";
        }
        
        // Verify free memory is available
        size_t free_mem = xbuf.get_free_memory();
        std::cout << "Free memory available: " << free_mem << " bytes\n";
        
        if (free_mem > 90 * 1024) {
             std::cout << "CONCLUSION: Memory is logically reclaimed but physically occupied.\n";
        }
        
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << "\n";
        return 1;
    }
}
