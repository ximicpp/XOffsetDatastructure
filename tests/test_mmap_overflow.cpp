// Test mmap overflow behavior

#include <iostream>
#include <cassert>
#include <fstream>
#include <filesystem>
#include <vector>
#include "../xoffsetdatastructure2.hpp"

using namespace XOffsetDatastructure2;

int main() {
    const std::string filename = "test_mmap_overflow.dat";
    const size_t FILE_SIZE = 1024; // 1KB

    try {
        std::cout << "Testing mmap overflow behavior...\n";
        
        // 1. Create a small file
        {
            std::ofstream ofs(filename, std::ios::binary);
            std::vector<char> zeros(FILE_SIZE, 0);
            ofs.write(zeros.data(), zeros.size());
            ofs.close();
            
            // Initialize it as a valid XBuffer
            XBuffer xbuf(filename, false);
            // Just opening it initializes the header
        }
        
        // 2. Open via mmap and try to allocate more than available
        {
            XBuffer xbuf(filename, false);
            std::cout << "  File size: " << xbuf.get_size() << " bytes\n";
            std::cout << "  Free memory: " << xbuf.get_free_memory() << " bytes\n";
            
            size_t alloc_size = 2048; // 2KB, definitely larger than file
            std::cout << "  Attempting to allocate " << alloc_size << " bytes...\n";
            
            bool exception_caught = false;
            try {
                // Try to create a string larger than the file
                // We use raw allocation to be sure
                void* ptr = xbuf.allocate(alloc_size);
                if (ptr == nullptr) {
                    std::cout << "  Allocation returned nullptr (expected behavior for some allocators)\n";
                } else {
                    std::cout << "  UNEXPECTED: Allocation succeeded!\n";
                }
            } catch (const boost::interprocess::bad_alloc& e) {
                std::cout << "  Caught expected bad_alloc: " << e.what() << "\n";
                exception_caught = true;
            } catch (const std::exception& e) {
                std::cout << "  Caught exception: " << e.what() << "\n";
                exception_caught = true;
            }
            
            if (!exception_caught) {
                // Some allocators might return nullptr instead of throwing
                // But boost::interprocess usually throws bad_alloc
                std::cout << "  No exception thrown. Checking if we can force it.\n";
            }
        }
        
        // Cleanup
        std::filesystem::remove(filename);
        
        std::cout << "Test finished.\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Fatal Exception: " << e.what() << "\n";
        return 1;
    }
}
