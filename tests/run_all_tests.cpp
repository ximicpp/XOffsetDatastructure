// ============================================================================
// Test Runner - Run All Tests
// Purpose: Execute all test cases and report results
// ============================================================================

#include <iostream>
#include <vector>
#include <string>
#include <functional>

// Test function declarations
bool test_basic_types();
bool test_vector_operations();
bool test_map_set_operations();
bool test_nested_structures();
bool test_memory_compaction();

struct TestCase {
    std::string name;
    std::function<bool()> test_func;
};

int main() {
    std::cout << "\n";
    std::cout << "========================================================================\n";
    std::cout << "  XOffsetDatastructure2 Test Suite\n";
    std::cout << "========================================================================\n";
    
    std::vector<TestCase> tests = {
        {"Basic Types", test_basic_types},
        {"Vector Operations", test_vector_operations},
        {"Map and Set Operations", test_map_set_operations},
        {"Nested Structures", test_nested_structures},
        {"Memory Compaction", test_memory_compaction}
    };
    
    int passed = 0;
    int failed = 0;
    std::vector<std::string> failed_tests;
    
    for (const auto& test : tests) {
        std::cout << "\nRunning: " << test.name << "\n";
        std::cout << std::string(70, '=') << "\n";
        
        try {
            if (test.test_func()) {
                passed++;
                std::cout << "\n[SUCCESS] " << test.name << " passed!\n";
            } else {
                failed++;
                failed_tests.push_back(test.name);
                std::cout << "\n[FAILURE] " << test.name << " failed!\n";
            }
        } catch (const std::exception& e) {
            failed++;
            failed_tests.push_back(test.name);
            std::cout << "\n[EXCEPTION] " << test.name << " threw exception: " 
                      << e.what() << "\n";
        }
    }
    
    // Summary
    std::cout << "\n";
    std::cout << "========================================================================\n";
    std::cout << "  TEST SUMMARY\n";
    std::cout << "========================================================================\n";
    std::cout << "Total Tests:  " << tests.size() << "\n";
    std::cout << "Passed:       " << passed << "\n";
    std::cout << "Failed:       " << failed << "\n";
    
    if (failed > 0) {
        std::cout << "\nFailed Tests:\n";
        for (const auto& name : failed_tests) {
            std::cout << "  - " << name << "\n";
        }
    }
    
    std::cout << "\nResult: " << (failed == 0 ? "[ALL TESTS PASSED]" : "[SOME TESTS FAILED]") << "\n";
    std::cout << "========================================================================\n\n";
    
    return failed == 0 ? 0 : 1;
}
