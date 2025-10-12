#include <cstddef>
#include <cstring>
#include <memory>
#include <chrono>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <algorithm>
#include <numeric>
#include <iomanip>
#include "xoffsetdatastructure2.hpp"

using namespace XOffsetDatastructure2;

class TestType
{
public:
    template <typename Allocator>
    TestType(Allocator allocator) : mVector(allocator), mStringVector(allocator), mComplexMap(allocator), mStringSet(allocator), mSet(allocator), mString(allocator), TestTypeInnerObj(allocator), mXXTypeVector(allocator) {}
    
    int mInt{ 0 };
    float mFloat{ 0.f };
    XVector<int> mVector;
    XVector<XString> mStringVector;
    
    class TestTypeInner
    {
    public:
        template <typename Allocator>
        TestTypeInner(Allocator allocator) : mVector(allocator) {}
        int mInt{ 0 };
        XVector<int> mVector;
    } TestTypeInnerObj;
    
    XVector<TestTypeInner> mXXTypeVector;
    XMap<XString, TestTypeInner> mComplexMap;
    XSet<XString> mStringSet;
    XSet<int> mSet;
    XString mString;
    
    // Migration callback for memory compaction
    // NOTE: This manual migration is required because C++ lacks reflection.
    //       If compact_automatic were available (requires reflection/serialization),
    //       this method would not be needed.
    static void migrate(XBuffer& old_buf, XBuffer& new_buf) {
        // Find old object in fragmented buffer
        auto* old_obj = old_buf.find<TestType>("MyTest").first;
        if (!old_obj) return;
        
        // Create new object in compact buffer
        auto* new_obj = new_buf.construct<TestType>("MyTest")(new_buf.get_segment_manager());
        
        // Copy simple fields
        new_obj->mInt = old_obj->mInt;
        new_obj->mFloat = old_obj->mFloat;
        
        // Copy containers (automatic deep copy via Boost.Interprocess)
        new_obj->mVector = old_obj->mVector;
        new_obj->mSet = old_obj->mSet;
        
        // Copy XString containers (need explicit allocator)
        for (const auto& str : old_obj->mStringVector) {
            new_obj->mStringVector.emplace_back(str.c_str(), new_buf.get_segment_manager());
        }
        for (const auto& str : old_obj->mStringSet) {
            new_obj->mStringSet.emplace(str.c_str(), new_buf.get_segment_manager());
        }
        new_obj->mString = XString(old_obj->mString.c_str(), new_buf.get_segment_manager());
        
        // Copy nested objects
        new_obj->TestTypeInnerObj.mInt = old_obj->TestTypeInnerObj.mInt;
        new_obj->TestTypeInnerObj.mVector = old_obj->TestTypeInnerObj.mVector;
        
        // Copy complex map
        for (const auto& pair : old_obj->mComplexMap) {
            XString key(pair.first.c_str(), new_buf.get_segment_manager());
            TestTypeInner value(new_buf.get_segment_manager());
            value.mInt = pair.second.mInt;
            value.mVector = pair.second.mVector;
            new_obj->mComplexMap.emplace(key, value);
        }
        
        // Copy vector of nested objects
        for (const auto& inner : old_obj->mXXTypeVector) {
            new_obj->mXXTypeVector.emplace_back(new_buf.get_segment_manager());
            new_obj->mXXTypeVector.back().mInt = inner.mInt;
            new_obj->mXXTypeVector.back().mVector = inner.mVector;
        }
    }
};

std::size_t writeData(const std::string& datafile, boost::container::vector<double>& writeTimes, bool writeFile = true, bool showVisualization = false, bool enableCompaction = false)
{
    auto start = std::chrono::high_resolution_clock::now();
    XBuffer xbuf(4096);
    
    if (showVisualization) {
        std::cout << "\n[Memory Visualization] Initial XBuffer (4KB)\n";
        XBufferVisualizer::print_stats(xbuf);
    }
    
    TestType* mytest = xbuf.construct<TestType>("MyTest")(xbuf.get_segment_manager());
    mytest->mInt = 123;
    mytest->mFloat = 3.14f;
    mytest->mVector.push_back(1);
    mytest->mVector.push_back(3);
    mytest->mSet.insert(3);
    mytest->mSet.insert(4);
    mytest->mString = "abcdefghijklmnopqrstuvwxyz";
    mytest->TestTypeInnerObj.mInt = 246;
    mytest->TestTypeInnerObj.mVector.push_back(23);
    mytest->TestTypeInnerObj.mVector.push_back(21);
    
    for (auto i = 0; i < 1; ++i)
    {
        mytest->mVector.push_back(i);
        mytest->TestTypeInnerObj.mVector.push_back(64 + i);
    }

    for (auto i = 0; i < 6; ++i)
    {
        mytest->mVector.push_back(1024 + i);
        mytest->mSet.insert(1024 - i);
    }

    for (auto i = 0; i < 10; ++i)
    {
        mytest->mStringVector.emplace_back("abcdefghijklmnopqrstuvwxyz", xbuf.get_segment_manager());
    }

    for (auto i = 0; i < 13; ++i)
    {
        mytest->mStringSet.emplace("stringinset", xbuf.get_segment_manager());
        mytest->mSet.insert(i);
    }

    if (showVisualization) {
        std::cout << "\n[Memory Visualization] After adding basic data\n";
        XBufferVisualizer::print_stats(xbuf);
    }

    xbuf.grow(1024 * 32);
    mytest = xbuf.find_or_construct<TestType>("MyTest")(xbuf.get_segment_manager());

    if (showVisualization) {
        std::cout << "\n[Memory Visualization] After growing buffer to 36KB\n";
        XBufferVisualizer::print_stats(xbuf);
    }

    for (auto i = 0; i < 7; ++i)
    {
        std::string key = "stringinset" + std::to_string(i);
        XString xkey(key.c_str(), xbuf.get_segment_manager());
        TestType::TestTypeInner xvalue(xbuf.get_segment_manager());
        mytest->mComplexMap.emplace(xkey, xvalue);
        auto& vec = mytest->mComplexMap.find(xkey)->second.mVector;
        for (int j = 0; j < 100; ++j) {
            vec.insert(vec.end(), {7, 8, 9, 10, 11, 12});
        }
    }

    for (auto i = 0; i < 6; ++i)
    {
        mytest->mXXTypeVector.emplace_back(xbuf.get_segment_manager());
        mytest->mXXTypeVector.back().mInt = i;
        auto& vec = mytest->mXXTypeVector.back().mVector;
        vec = {1, 2, 3, 4, 5, 6};
        for (auto j = 0; j < 12; ++j)
        {
            vec.push_back(j);
        }
    }

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed_seconds = end - start;
    writeTimes.push_back(elapsed_seconds.count());

    if (showVisualization) {
        std::cout << "\n[Memory Visualization] Final state after all data added\n";
        XBufferVisualizer::print_stats(xbuf);
    }
    
    // ========================================================================
    // Memory Compaction
    // ========================================================================
    XBuffer* buffer_to_write = &xbuf;
    XBuffer compact_buf;
    
    if (enableCompaction) {
        if (showVisualization) {
            std::cout << "Before compaction: ";
            XBufferVisualizer::print_stats(xbuf);
        }
        
        compact_buf = XBufferCompactor::compact_manual<TestType>(xbuf);
        
        if (compact_buf.get_size() > 0) {
            buffer_to_write = &compact_buf;
            
            if (showVisualization) {
                std::cout << "After compaction:  ";
                XBufferVisualizer::print_stats(compact_buf);
            }
        } else {
            if (showVisualization) {
                std::cout << "Compaction skipped: Type doesn't have migrate method\n";
            }
        }
    }

    // Write buffer to file
    if (writeFile) {
        std::ofstream ofs(datafile, std::ios::binary);
        const char* data = static_cast<const char*>(buffer_to_write->get_address());
        std::size_t size = buffer_to_write->get_size();
        ofs.write(data, size);
        ofs.close();
    }

    return buffer_to_write->get_size();
}

std::size_t readData(const std::string& datafile, boost::container::vector<double>& readTimes)
{
    std::ifstream ifs(datafile, std::ios::binary);
    ifs.seekg(0, std::ios::end);
    std::size_t size = ifs.tellg();
    ifs.seekg(0, std::ios::beg);
    std::vector<char> buffer(size);
    ifs.read(buffer.data(), size);
    ifs.close();

    auto start = std::chrono::high_resolution_clock::now();
    XBuffer xbuf(buffer.data(), size);
    TestType* mytest = xbuf.find<TestType>("MyTest").first;

    std::cout << "mInt: " << mytest->mInt << std::endl;
    std::cout << "mFloat: " << mytest->mFloat << std::endl;
    
    std::cout << "mVector: ";
    for (int i : mytest->mVector) {
        std::cout << i << " ";
    }
    std::cout << std::endl;

    std::cout << "mStringVector: ";
    for (const auto& str : mytest->mStringVector) {
        std::cout << str << " ";
    }
    std::cout << std::endl;

    std::cout << "mComplexMap: " << std::endl;
    for (const auto& pair : mytest->mComplexMap) {
        std::cout << pair.first << ": " << pair.second.mInt << " [";
        for (int i : pair.second.mVector) {
            std::cout << i << " ";
        }
        std::cout << "]" << std::endl;
    }

    std::cout << "mSet: ";
    for (int i : mytest->mSet) {
        std::cout << i << " ";
    }
    std::cout << std::endl;

    std::cout << "mString: " << mytest->mString << std::endl;

    std::cout << "TestTypeInnerObj: " << std::endl;
    std::cout << "  mInt: " << mytest->TestTypeInnerObj.mInt << std::endl;
    std::cout << "  mVector: ";
    for (int i : mytest->TestTypeInnerObj.mVector) {
        std::cout << i << " ";
    }
    std::cout << std::endl;

    std::cout << "mXXTypeVector: " << std::endl;
    for (const auto& inner : mytest->mXXTypeVector) {
        std::cout << "  mInt: " << inner.mInt << std::endl;
        std::cout << "  mVector: ";
        for (int i : inner.mVector) {
            std::cout << i << " ";
        }
        std::cout << std::endl;
    }

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed_seconds = end - start;
    readTimes.push_back(elapsed_seconds.count());

    return size;
}

int main(int argc, char* argv[])
{
    std::string datafile = "data2.bin";
    bool showVisualization = false;
    bool enableCompaction = false;
    
    // Parse command line arguments
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--visualize" || arg == "-v") {
            showVisualization = true;
        } else if (arg == "--compact" || arg == "-c") {
            enableCompaction = true;
        } else {
            // Assume it's a datafile path
            datafile = arg;
        }
    }
    
    std::cout << "datafile: " << datafile << std::endl;
    if (showVisualization) {
        std::cout << "Memory visualization: ENABLED\n";
        std::cout << "======================================================================\n\n";
    }
    if (enableCompaction) {
        std::cout << "Memory compaction: ENABLED\n";
    }

    const int numRuns = 1;
    std::size_t storageSize = 0;
    boost::container::vector<double> writeTimes(numRuns);
    boost::container::vector<double> readTimes(numRuns);

    // Initial write and read
    storageSize = writeData(datafile, writeTimes, true, showVisualization, enableCompaction);
    readData(datafile, readTimes);

    // Additional runs if numRuns > 1
    for (int i = 1; i < numRuns; ++i)
    {
        storageSize = writeData(datafile, writeTimes, false);
        readData(datafile, readTimes);
    }

    if (numRuns > 0)
    {
        double writeTimeAvg = std::accumulate(writeTimes.begin(), writeTimes.end(), 0.0) / numRuns;
        double readTimeAvg = std::accumulate(readTimes.begin(), readTimes.end(), 0.0) / numRuns;
        std::cout << std::left << std::setw(25) << "Average writeData time:" << std::right << std::setw(10) << std::fixed << std::setprecision(3) << writeTimeAvg * 1000 << " ms" << std::endl;
        std::cout << std::left << std::setw(25) << "Average readData time:" << std::right << std::setw(10) << std::fixed << std::setprecision(3) << readTimeAvg * 1000 << " ms" << std::endl;
    }
    
    std::cout << "storageSize: " << storageSize << std::endl;
    
    return 0;
}
