// Test mmap loading (Zero-Copy)

#include <iostream>
#include <cassert>
#include <fstream>
#include <filesystem>
#include "../xoffsetdatastructure2.hpp"

using namespace XOffsetDatastructure2;

struct alignas(BASIC_ALIGNMENT) MmapTestType {
    template <typename Allocator>
    MmapTestType(Allocator allocator) : name(allocator) {}
    
    int id;
    XString name;
};

// Reflection hint
struct alignas(BASIC_ALIGNMENT) MmapTestTypeReflectionHint {
    int32_t id;
    XString name;
};

// Manual type signature
namespace XTypeSignature {
    template<>
    struct TypeSignature<MmapTestTypeReflectionHint> {
        static constexpr auto calculate() noexcept {
            return CompileString{"struct[s:40,a:8]{@0:i32[s:4,a:4],@8:string[s:32,a:8]}"};
        }
    };
}

int main() {
    try {
        std::cout << "Testing mmap loading...\n";
        const std::string filename = "test_mmap.dat";
        
        // 1. Create and save data
        {
            std::cout << "  Creating data file...\n";
            XBuffer xbuf(4096);
            auto* obj = xbuf.make_root<MmapTestType>("MmapObj");
            obj->id = 12345;
            obj->name.assign("ZeroCopy");
            
            // Save to file
            std::ofstream ofs(filename, std::ios::binary);
            auto* buf = xbuf.get_buffer();
            ofs.write(buf->data(), buf->size());
            ofs.close();
        }
        
        // 2. Load using mmap (Zero-Copy)
        {
            std::cout << "  Loading via mmap...\n";
            // Use the new constructor
            XBuffer xbuf(filename, true); // read_only = true
            
            // Verify internal state
            assert(xbuf.get_buffer() == nullptr); // Should be nullptr in mmap mode
            std::cout << "  Verified: get_buffer() returns nullptr (Mapped Mode)\n";
            
            // Access data
            auto [obj, found] = xbuf.find_root<MmapTestType>("MmapObj");
            assert(found);
            assert(obj != nullptr);
            
            std::cout << "  Object found: ID=" << obj->id << ", Name=" << obj->name.c_str() << "\n";
            
            assert(obj->id == 12345);
            assert(obj->name == "ZeroCopy");
            
            // Verify address range
            const void* base_addr = xbuf.get_address();
            size_t size = xbuf.get_size();
            const void* obj_addr = (const void*)obj;
            
            std::cout << "  Base Address: " << base_addr << "\n";
            std::cout << "  Object Address: " << obj_addr << "\n";
            std::cout << "  Size: " << size << "\n";
            
            bool in_range = (obj_addr >= base_addr) && 
                            ((char*)obj_addr < (char*)base_addr + size);
            
            assert(in_range);
            std::cout << "  Verified: Object address is within mapped region\n";
        }
        
        // Cleanup
        std::filesystem::remove(filename);
        
        std::cout << "All mmap tests passed\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << "\n";
        return 1;
    }
}
