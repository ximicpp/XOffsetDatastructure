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
    
    // Migration function for memory compaction
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

// ============================================================================
// Data Structure Visualization
// ============================================================================
void visualizeDataStructure(TestType* obj) {
    std::cout << "\n[Data Structure Summary]\n";
    std::cout << "  - mInt:           " << obj->mInt << "\n";
    std::cout << "  - mFloat:         " << obj->mFloat << "\n";
    std::cout << "  - mVector:        " << obj->mVector.size() << " elements\n";
    std::cout << "  - mStringVector:  " << obj->mStringVector.size() << " strings\n";
    std::cout << "  - mStringSet:     " << obj->mStringSet.size() << " unique strings\n";
    std::cout << "  - mSet:           " << obj->mSet.size() << " unique integers\n";
    std::cout << "  - mString:        \"" << obj->mString.c_str() << "\"\n";
    std::cout << "  - mComplexMap:    " << obj->mComplexMap.size() << " entries";
    
    if (!obj->mComplexMap.empty()) {
        size_t total_map_elements = 0;
        for (const auto& pair : obj->mComplexMap) {
            total_map_elements += pair.second.mVector.size();
        }
        std::cout << " (" << total_map_elements << " total sub-elements)";
    }
    std::cout << "\n";
    
    std::cout << "  - mXXTypeVector:  " << obj->mXXTypeVector.size() << " objects";
    if (!obj->mXXTypeVector.empty()) {
        size_t total_vec_elements = 0;
        for (const auto& inner : obj->mXXTypeVector) {
            total_vec_elements += inner.mVector.size();
        }
        std::cout << " (" << total_vec_elements << " total sub-elements)";
    }
    std::cout << "\n";
}

void visualizeMemoryAndData(XBuffer& xbuf, const std::string& stage) {
    std::cout << "\n[" << stage << "]\n";
    std::cout << "Memory: ";
    XBufferVisualizer::print_stats(xbuf);
    
    auto* obj = xbuf.find<TestType>("MyTest").first;
    if (obj) {
        visualizeDataStructure(obj);
    }
}

// ============================================================================
// Stage 1: Fill Basic Fields
// ============================================================================
void fillBasicFields(TestType* obj, XBuffer& xbuf) {
    obj->mInt = 123;
    obj->mFloat = 3.14f;
    obj->mString = XString("abcdefghijklmnopqrstuvwxyz", xbuf.get_segment_manager());
    
    // Add a few initial values
    obj->mVector.push_back(1);
    obj->mVector.push_back(3);
    obj->mSet.insert(3);
    obj->mSet.insert(4);
    
    // Inner object
    obj->TestTypeInnerObj.mInt = 246;
    obj->TestTypeInnerObj.mVector.push_back(23);
    obj->TestTypeInnerObj.mVector.push_back(21);
}

// ============================================================================
// Stage 2: Fill Small Containers
// ============================================================================
void fillSmallContainers(TestType* obj, XBuffer& xbuf) {
    // Expand vector
    for (auto i = 0; i < 1; ++i) {
        obj->mVector.push_back(i);
        obj->TestTypeInnerObj.mVector.push_back(64 + i);
    }

    for (auto i = 0; i < 6; ++i) {
        obj->mVector.push_back(1024 + i);
        obj->mSet.insert(1024 - i);
    }

    // String containers
    for (auto i = 0; i < 10; ++i) {
        obj->mStringVector.emplace_back("abcdefghijklmnopqrstuvwxyz", xbuf.get_segment_manager());
    }

    for (auto i = 0; i < 13; ++i) {
        obj->mStringSet.emplace("stringinset", xbuf.get_segment_manager());
        obj->mSet.insert(i);
    }
}

// ============================================================================
// Stage 3: Fill Large Nested Data
// ============================================================================
void fillLargeNestedData(TestType* obj, XBuffer& xbuf) {
    // Complex map with nested vectors
    for (auto i = 0; i < 7; ++i) {
        std::string key = "stringinset" + std::to_string(i);
        XString xkey(key.c_str(), xbuf.get_segment_manager());
        TestType::TestTypeInner xvalue(xbuf.get_segment_manager());
        obj->mComplexMap.emplace(xkey, xvalue);
        auto& vec = obj->mComplexMap.find(xkey)->second.mVector;
        for (int j = 0; j < 100; ++j) {
            vec.insert(vec.end(), {7, 8, 9, 10, 11, 12});
        }
    }

    // Vector of objects
    for (auto i = 0; i < 6; ++i) {
        obj->mXXTypeVector.emplace_back(xbuf.get_segment_manager());
        obj->mXXTypeVector.back().mInt = i;
        auto& vec = obj->mXXTypeVector.back().mVector;
        vec = {1, 2, 3, 4, 5, 6};
        for (auto j = 0; j < 12; ++j) {
            vec.push_back(j);
        }
    }
}

// ============================================================================
// Create Test Data with Staged Visualization
// ============================================================================
XBuffer createTestData(bool showVisualization) {
    XBuffer xbuf(4096);
    
    if (showVisualization) {
        std::cout << "\n" << std::string(70, '=') << "\n";
        std::cout << "  DATA CREATION PROCESS\n";
        std::cout << std::string(70, '=') << "\n";
        std::cout << "\n[Stage 0: Initial Buffer]\n";
        std::cout << "Memory: ";
        XBufferVisualizer::print_stats(xbuf);
    }
    
    // Create object
    TestType* obj = xbuf.construct<TestType>("MyTest")(xbuf.get_segment_manager());
    
    // Stage 1: Basic fields
    fillBasicFields(obj, xbuf);
    if (showVisualization) {
        visualizeMemoryAndData(xbuf, "Stage 1: Basic Fields Added");
    }
    
    // Stage 2: Small containers
    fillSmallContainers(obj, xbuf);
    if (showVisualization) {
        visualizeMemoryAndData(xbuf, "Stage 2: Small Containers Filled");
    }
    
    // Stage 3: Grow buffer
    xbuf.grow(1024 * 32);
    obj = xbuf.find_or_construct<TestType>("MyTest")(xbuf.get_segment_manager());
    if (showVisualization) {
        std::cout << "\n[Stage 3: Buffer Growth]\n";
        std::cout << "Memory: ";
        XBufferVisualizer::print_stats(xbuf);
        std::cout << "  >> Buffer expanded to 36KB to accommodate large data\n";
    }
    
    // Stage 4: Large nested data
    fillLargeNestedData(obj, xbuf);
    if (showVisualization) {
        visualizeMemoryAndData(xbuf, "Stage 4: Large Nested Data Added");
    }
    
    return xbuf;
}

// ============================================================================
// Memory Compaction
// ============================================================================
XBuffer compactMemory(XBuffer& xbuf, bool showVisualization) {
    if (showVisualization) {
        std::cout << "\n" << std::string(70, '=') << "\n";
        std::cout << "  MEMORY COMPACTION\n";
        std::cout << std::string(70, '=') << "\n";
        std::cout << "\n[Before Compaction]\n";
        std::cout << "Memory: ";
        XBufferVisualizer::print_stats(xbuf);
    }
    
    // Approach 1: compact_manual (C++17 if constexpr)
    // - Requires: User must define T::migrate() method manually
    XBuffer compact_buf = XBufferCompactor::compact_manual<TestType>(xbuf);
    
    // Approach 2: compact_automatic (C++26 reflection - NOT YET AVAILABLE)
    // - Requires: Nothing! Uses std::meta::members_of(^T) to auto-generate migration
    // - Benefit: No need to write T::migrate() - fully automatic
    // - Status: Waiting for C++26 reflection support
    // XBuffer compact_buf = XBufferCompactor::compact_automatic<TestType>(xbuf);
    
    if (showVisualization && compact_buf.get_size() > 0) {
        std::cout << "\n[After Compaction]\n";
        std::cout << "Memory: ";
        XBufferVisualizer::print_stats(compact_buf);
        
        auto old_stats = XBufferVisualizer::get_memory_stats(xbuf);
        auto new_stats = XBufferVisualizer::get_memory_stats(compact_buf);
        double reduction = (1.0 - (double)new_stats.total_size / old_stats.total_size) * 100.0;
        
        std::cout << "\n[Compaction Results]\n";
        std::cout << "  - Size reduction:   " << old_stats.total_size << " -> " 
                  << new_stats.total_size << " bytes (" 
                  << std::fixed << std::setprecision(1) << reduction << "% smaller)\n";
        std::cout << "  - Memory efficiency: " << old_stats.usage_percent() << "% -> " 
                  << new_stats.usage_percent() << "%\n";
    }
    
    return compact_buf;
}

// ============================================================================
// Save to File
// ============================================================================
void saveToFile(XBuffer& xbuf, const std::string& filepath) {
    std::ofstream ofs(filepath, std::ios::binary);
    const char* data = static_cast<const char*>(xbuf.get_address());
    std::size_t size = xbuf.get_size();
    ofs.write(data, size);
    ofs.close();
}

// ============================================================================
// Main Write Function
// ============================================================================
std::size_t writeData(const std::string& datafile, boost::container::vector<double>& writeTimes, bool writeFile = true, bool showVisualization = false, bool enableCompaction = false)
{
    auto start = std::chrono::high_resolution_clock::now();
    
    // Create test data with staged visualization
    XBuffer xbuf = createTestData(showVisualization);
    
    // Optional: Compact memory
    if (enableCompaction) {
        xbuf = compactMemory(xbuf, showVisualization);
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed_seconds = end - start;
    writeTimes.push_back(elapsed_seconds.count());
    
    // Save to file
    if (writeFile) {
        saveToFile(xbuf, datafile);
    }

    return xbuf.get_size();
}

// ============================================================================
// Data Validation
// ============================================================================
bool validateData(TestType* obj) {
    bool valid = true;
    
    if (obj->mInt != 123) {
        std::cerr << "[X] Validation failed: mInt (expected 123, got " << obj->mInt << ")\n";
        valid = false;
    }
    if (std::abs(obj->mFloat - 3.14f) > 0.01f) {
        std::cerr << "[X] Validation failed: mFloat (expected 3.14, got " << obj->mFloat << ")\n";
        valid = false;
    }
    if (obj->mString != "abcdefghijklmnopqrstuvwxyz") {
        std::cerr << "[X] Validation failed: mString\n";
        valid = false;
    }
    if (obj->TestTypeInnerObj.mInt != 246) {
        std::cerr << "[X] Validation failed: TestTypeInnerObj.mInt\n";
        valid = false;
    }
    
    return valid;
}

// ============================================================================
// Print Data Summary (concise output)
// ============================================================================
void printDataSummary(TestType* obj) {
    std::cout << "\n" << std::string(70, '=') << "\n";
    std::cout << "  DATA VALIDATION & SUMMARY\n";
    std::cout << std::string(70, '=') << "\n";
    
    // Validation
    bool valid = validateData(obj);
    if (valid) {
        std::cout << "\n[OK] Data Validation: PASSED\n";
    } else {
        std::cout << "\n[FAILED] Data Validation: FAILED\n";
    }
    
    // Summary
    visualizeDataStructure(obj);
}

// ============================================================================
// Print Full Data (verbose output)
// ============================================================================
void printFullData(TestType* obj) {
    std::cout << "\n" << std::string(70, '=') << "\n";
    std::cout << "  DETAILED DATA OUTPUT\n";
    std::cout << std::string(70, '=') << "\n\n";
    
    std::cout << "mInt: " << obj->mInt << std::endl;
    std::cout << "mFloat: " << obj->mFloat << std::endl;
    
    std::cout << "mVector (" << obj->mVector.size() << " elements): ";
    for (int i : obj->mVector) {
        std::cout << i << " ";
    }
    std::cout << std::endl;

    std::cout << "mStringVector (" << obj->mStringVector.size() << " strings): ";
    for (const auto& str : obj->mStringVector) {
        std::cout << str << " ";
    }
    std::cout << std::endl;

    std::cout << "\nmComplexMap (" << obj->mComplexMap.size() << " entries):\n";
    for (const auto& pair : obj->mComplexMap) {
        std::cout << "  " << pair.first << ": mInt=" << pair.second.mInt 
                  << ", mVector[" << pair.second.mVector.size() << "]=[";
        // Print first few elements
        size_t count = 0;
        for (int i : pair.second.mVector) {
            if (count++ >= 10) {
                std::cout << "... (" << pair.second.mVector.size() - 10 << " more)";
                break;
            }
            std::cout << i << " ";
        }
        std::cout << "]\n";
    }

    std::cout << "\nmSet (" << obj->mSet.size() << " elements): ";
    for (int i : obj->mSet) {
        std::cout << i << " ";
    }
    std::cout << std::endl;

    std::cout << "\nmString: \"" << obj->mString.c_str() << "\"" << std::endl;

    std::cout << "\nTestTypeInnerObj:\n";
    std::cout << "  mInt: " << obj->TestTypeInnerObj.mInt << std::endl;
    std::cout << "  mVector: ";
    for (int i : obj->TestTypeInnerObj.mVector) {
        std::cout << i << " ";
    }
    std::cout << std::endl;

    std::cout << "\nmXXTypeVector (" << obj->mXXTypeVector.size() << " objects):\n";
    for (const auto& inner : obj->mXXTypeVector) {
        std::cout << "  [mInt=" << inner.mInt << ", mVector[" 
                  << inner.mVector.size() << "]]: ";
        for (int i : inner.mVector) {
            std::cout << i << " ";
        }
        std::cout << std::endl;
    }
}

// ============================================================================
// Main Read Function
// ============================================================================
std::size_t readData(const std::string& datafile, boost::container::vector<double>& readTimes, bool verbose = false)
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

    // Display data based on verbosity
    if (verbose) {
        printFullData(mytest);
    } else {
        printDataSummary(mytest);
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
    bool verboseOutput = false;
    
    // Parse command line arguments
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--visualize" || arg == "-v") {
            showVisualization = true;
        } else if (arg == "--compact" || arg == "-c") {
            enableCompaction = true;
        } else if (arg == "--verbose") {
            verboseOutput = true;
        } else if (arg == "--help" || arg == "-h") {
            std::cout << "Usage: demo [options] [datafile]\n"
                      << "Options:\n"
                      << "  -v, --visualize    Show memory visualization during data creation\n"
                      << "  -c, --compact      Enable memory compaction\n"
                      << "  --verbose          Show detailed data output\n"
                      << "  -h, --help         Show this help message\n";
            return 0;
        } else {
            // Assume it's a datafile path
            datafile = arg;
        }
    }
    
    std::cout << "Datafile: " << datafile << std::endl;
    if (showVisualization) {
        std::cout << "Memory visualization: ENABLED\n";
    }
    if (enableCompaction) {
        std::cout << "Memory compaction: ENABLED\n";
    }
    if (verboseOutput) {
        std::cout << "Verbose output: ENABLED\n";
    }
    std::cout << "======================================================================\n";

    const int numRuns = 1;
    std::size_t storageSize = 0;
    boost::container::vector<double> writeTimes(numRuns);
    boost::container::vector<double> readTimes(numRuns);

    // Initial write and read
    storageSize = writeData(datafile, writeTimes, true, showVisualization, enableCompaction);
    readData(datafile, readTimes, verboseOutput);

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
