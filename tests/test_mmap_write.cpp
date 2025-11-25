// Test mmap write (Modify via mmap)

#include <iostream>
#include <cassert>
#include <fstream>
#include <filesystem>
#include "../xoffsetdatastructure2.hpp"

using namespace XOffsetDatastructure2;

struct alignas(BASIC_ALIGNMENT) MmapWriteTestType {
    template <typename Allocator>
    MmapWriteTestType(Allocator allocator) : name(allocator) {}
    
    int id;
    XString name;
};

// Reflection hint
struct alignas(BASIC_ALIGNMENT) MmapWriteTestTypeReflectionHint {
    int32_t id;
    XString name;
};

// Manual type signature
namespace XTypeSignature {
    template<>
    struct TypeSignature<MmapWriteTestTypeReflectionHint> {
        static constexpr auto calculate() noexcept {
            return CompileString{"struct[s:40,a:8]{@0:i32[s:4,a:4],@8:string[s:32,a:8]}"};
        }
    };
}

int main() {
    try {
        std::cout << "Testing mmap write...\n";
        const std::string filename = "test_mmap_write.dat";
        
        // 1. Create and save initial data
        {
            std::cout << "  Creating initial data file...\n";
            XBuffer xbuf(4096);
            auto* obj = xbuf.make_root<MmapWriteTestType>("WriteObj");
            obj->id = 100;
            obj->name.assign("Initial");
            
            std::ofstream ofs(filename, std::ios::binary);
            auto* buf = xbuf.get_buffer();
            ofs.write(buf->data(), buf->size());
            ofs.close();
        }
        
        // 2. Open via mmap (Read-Write) and Modify
        {
            std::cout << "  Opening via mmap (Read-Write)...\n";
            // read_only = false
            XBuffer xbuf(filename, false); 
            
            auto [obj, found] = xbuf.find_root<MmapWriteTestType>("WriteObj");
            assert(found);
            
            std::cout << "  Original value: " << obj->name.c_str() << "\n";
            assert(obj->name == "Initial");
            
            // Modify data
            std::cout << "  Modifying data...\n";
            obj->id = 200;
            // Note: We can only modify existing string content if it fits, 
            // or assign new string if allocator supports it.
            // Since XString uses the allocator from the buffer, and the buffer is mmapped,
            // allocating new memory might be tricky if the file size is fixed.
            // However, XManagedMemory usually manages the free space within the file.
            // Let's try assigning a string of same or smaller length first.
            obj->name.assign("Modified");
            
            std::cout << "  Modified value: " << obj->name.c_str() << "\n";
            
            // Flush is automatic on close, but we can force it if we had access to mapped_region
            // xbuf destructor will close it.
        }
        
        // 3. Verify persistence
        {
            std::cout << "  Verifying persistence...\n";
            XBuffer xbuf(filename, true); // Read-only
            
            auto [obj, found] = xbuf.find_root<MmapWriteTestType>("WriteObj");
            assert(found);
            
            std::cout << "  Persisted ID: " << obj->id << "\n";
            std::cout << "  Persisted Name: " << obj->name.c_str() << "\n";
            
            assert(obj->id == 200);
            assert(obj->name == "Modified");
            std::cout << "  Verification successful!\n";
        }
        
        // Cleanup
        std::filesystem::remove(filename);
        
        std::cout << "All mmap write tests passed\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << "\n";
        return 1;
    }
}
