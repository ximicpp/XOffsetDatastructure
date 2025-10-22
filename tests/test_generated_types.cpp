#include <iostream>
#include <cassert>
#include "xoffsetdatastructure2.hpp"
#include "test_types.hpp"

using namespace XOffsetDatastructure2;

void test_basic_creation() {
    std::cout << "Testing basic type creation..." << std::endl;
    
    XBufferExt xbuf(4096);
    
    auto* inner = xbuf.make<TestTypeInner>("Inner1");
    assert(inner != nullptr);
    assert(inner->mInt == 0);
    
    inner->mInt = 42;
    assert(inner->mInt == 42);
    
    std::cout << "  [OK] TestTypeInner created and modified" << std::endl;
}

void test_vector_operations() {
    std::cout << "Testing vector operations..." << std::endl;
    
    XBufferExt xbuf(4096);
    
    auto* inner = xbuf.make<TestTypeInner>("Inner2");
    inner->mVector.push_back(1);
    inner->mVector.push_back(2);
    inner->mVector.push_back(3);
    
    assert(inner->mVector.size() == 3);
    assert(inner->mVector[0] == 1);
    assert(inner->mVector[1] == 2);
    assert(inner->mVector[2] == 3);
    
    std::cout << "  [OK] XVector operations work correctly" << std::endl;
}

void test_complex_type() {
    std::cout << "Testing complex type with nested structures..." << std::endl;
    
    XBufferExt xbuf(8192);
    
    auto* test = xbuf.make<TestType>("ComplexTest");
    assert(test != nullptr);
    
    test->mInt = 100;
    test->mFloat = 3.14f;
    
    test->mVector.push_back(10);
    test->mVector.push_back(20);
    assert(test->mVector.size() == 2);
    
    test->TestTypeInnerObj.mInt = 999;
    assert(test->TestTypeInnerObj.mInt == 999);
    
    std::cout << "  [OK] TestType with nested TestTypeInner works" << std::endl;
}

void test_string_operations() {
    std::cout << "Testing XString operations..." << std::endl;
    
    XBufferExt xbuf(4096);
    
    auto* test = xbuf.make<TestType>("StringTest");
    
    const char* hello = "Hello, World!";
    test->mString.assign(hello);
    
    assert(test->mString.size() == strlen(hello));
    assert(strcmp(test->mString.c_str(), hello) == 0);
    
    std::cout << "  [OK] XString operations work correctly" << std::endl;
}

void test_string_vector() {
    std::cout << "Testing XVector<XString>..." << std::endl;
    
    XBufferExt xbuf(8192);
    
    auto* test = xbuf.make<TestType>("StringVectorTest");
    
    // Add strings to vector
    test->mStringVector.emplace_back("First", xbuf.allocator<XString>());
    test->mStringVector.emplace_back("Second", xbuf.allocator<XString>());
    test->mStringVector.emplace_back("Third", xbuf.allocator<XString>());
    
    assert(test->mStringVector.size() == 3);
    assert(strcmp(test->mStringVector[0].c_str(), "First") == 0);
    assert(strcmp(test->mStringVector[1].c_str(), "Second") == 0);
    assert(strcmp(test->mStringVector[2].c_str(), "Third") == 0);
    
    // Test modification
    test->mStringVector[1].assign("Modified");
    assert(strcmp(test->mStringVector[1].c_str(), "Modified") == 0);
    
    // Test iteration
    int count = 0;
    for (const auto& str : test->mStringVector) {
        assert(str.size() > 0);
        count++;
    }
    assert(count == 3);
    
    std::cout << "  [OK] XVector<XString> operations work correctly" << std::endl;
}

void test_nested_vector() {
    std::cout << "Testing XVector<TestTypeInner>..." << std::endl;
    
    XBufferExt xbuf(8192);
    
    auto* test = xbuf.make<TestType>("NestedVectorTest");
    
    // Add nested objects to vector using emplace_back
    test->mXXTypeVector.emplace_back(xbuf.allocator<TestTypeInner>(), 100);
    test->mXXTypeVector.emplace_back(xbuf.allocator<TestTypeInner>(), 200);
    test->mXXTypeVector.emplace_back(xbuf.allocator<TestTypeInner>(), 300);
    
    assert(test->mXXTypeVector.size() == 3);
    assert(test->mXXTypeVector[0].mInt == 100);
    assert(test->mXXTypeVector[1].mInt == 200);
    assert(test->mXXTypeVector[2].mInt == 300);
    
    // Test modification
    test->mXXTypeVector[1].mInt = 999;
    assert(test->mXXTypeVector[1].mInt == 999);
    
    // Add data to nested vectors
    test->mXXTypeVector[0].mVector.push_back(10);
    test->mXXTypeVector[0].mVector.push_back(20);
    assert(test->mXXTypeVector[0].mVector.size() == 2);
    assert(test->mXXTypeVector[0].mVector[0] == 10);
    assert(test->mXXTypeVector[0].mVector[1] == 20);
    
    // Test iteration
    int count = 0;
    int sum = 0;
    for (const auto& inner : test->mXXTypeVector) {
        sum += inner.mInt;
        count++;
    }
    assert(count == 3);
    assert(sum == 100 + 999 + 300); // 1399
    
    std::cout << "  [OK] XVector<TestTypeInner> operations work correctly" << std::endl;
}

void test_map_operations() {
    std::cout << "Testing XMap<XString, TestTypeInner>..." << std::endl;
    
    XBufferExt xbuf(8192);
    
    auto* test = xbuf.make<TestType>("MapTest");
    
    // Insert key-value pairs
    XString key1("first", xbuf.allocator<XString>());
    test->mComplexMap.emplace(key1, TestTypeInner(xbuf.allocator<TestTypeInner>(), 111));
    
    XString key2("second", xbuf.allocator<XString>());
    test->mComplexMap.emplace(key2, TestTypeInner(xbuf.allocator<TestTypeInner>(), 222));
    
    XString key3("third", xbuf.allocator<XString>());
    test->mComplexMap.emplace(key3, TestTypeInner(xbuf.allocator<TestTypeInner>(), 333));
    
    assert(test->mComplexMap.size() == 3);
    
    // Test find operations
    XString findKey1("first", xbuf.allocator<XString>());
    auto it1 = test->mComplexMap.find(findKey1);
    assert(it1 != test->mComplexMap.end());
    assert(it1->second.mInt == 111);
    
    XString findKey2("second", xbuf.allocator<XString>());
    auto it2 = test->mComplexMap.find(findKey2);
    assert(it2 != test->mComplexMap.end());
    assert(it2->second.mInt == 222);
    
    // Test modification
    it2->second.mInt = 999;
    XString findKey2b("second", xbuf.allocator<XString>());
    auto it2b = test->mComplexMap.find(findKey2b);
    assert(it2b->second.mInt == 999);
    
    // Test nested vector in map value
    it1->second.mVector.push_back(1);
    it1->second.mVector.push_back(2);
    it1->second.mVector.push_back(3);
    
    XString findKey1b("first", xbuf.allocator<XString>());
    auto it1b = test->mComplexMap.find(findKey1b);
    assert(it1b->second.mVector.size() == 3);
    assert(it1b->second.mVector[0] == 1);
    assert(it1b->second.mVector[1] == 2);
    assert(it1b->second.mVector[2] == 3);
    
    // Test iteration
    int count = 0;
    int sum = 0;
    for (const auto& pair : test->mComplexMap) {
        assert(pair.first.size() > 0);  // Key should not be empty
        sum += pair.second.mInt;
        count++;
    }
    assert(count == 3);
    assert(sum == 111 + 999 + 333); // 1443
    
    // Test count
    XString countKey("first", xbuf.allocator<XString>());
    assert(test->mComplexMap.count(countKey) == 1);
    
    XString noKey("nonexistent", xbuf.allocator<XString>());
    assert(test->mComplexMap.count(noKey) == 0);
    
    std::cout << "  [OK] XMap<XString, TestTypeInner> operations work correctly" << std::endl;
}

void test_set_operations() {
    std::cout << "Testing XSet operations..." << std::endl;
    
    XBufferExt xbuf(8192);
    
    auto* test = xbuf.make<TestType>("SetTest");
    
    // Test XSet<int>
    test->mSet.insert(10);
    test->mSet.insert(20);
    test->mSet.insert(30);
    test->mSet.insert(20);  // Duplicate
    
    assert(test->mSet.size() == 3);
    assert(test->mSet.count(10) == 1);
    assert(test->mSet.count(20) == 1);
    assert(test->mSet.count(99) == 0);
    
    std::cout << "  [OK] XSet<int> operations work correctly" << std::endl;
    
    // Test XSet<XString>
    XString str1("apple", xbuf.allocator<XString>());
    test->mStringSet.insert(str1);
    
    XString str2("banana", xbuf.allocator<XString>());
    test->mStringSet.insert(str2);
    
    XString str3("cherry", xbuf.allocator<XString>());
    test->mStringSet.insert(str3);
    
    XString str4("apple", xbuf.allocator<XString>());  // Duplicate
    test->mStringSet.insert(str4);
    
    assert(test->mStringSet.size() == 3);
    
    // Test count operations
    XString findStr1("apple", xbuf.allocator<XString>());
    assert(test->mStringSet.count(findStr1) == 1);
    
    XString findStr2("banana", xbuf.allocator<XString>());
    assert(test->mStringSet.count(findStr2) == 1);
    
    XString findStr3("notfound", xbuf.allocator<XString>());
    assert(test->mStringSet.count(findStr3) == 0);
    
    // Test iteration
    int count = 0;
    for (const auto& str : test->mStringSet) {
        assert(str.size() > 0);  // Should not be empty
        count++;
    }
    assert(count == 3);
    
    std::cout << "  [OK] XSet<XString> operations work correctly" << std::endl;
    std::cout << "  [OK] All XSet operations work correctly" << std::endl;
}

void test_default_values() {
    std::cout << "Testing default values..." << std::endl;
    
    XBufferExt xbuf(4096);
    
    auto* inner = xbuf.make<TestTypeInner>("DefaultInner");
    assert(inner->mInt == 0);
    
    auto* test = xbuf.make<TestType>("DefaultTest");
    assert(test->mInt == 0);
    assert(test->mFloat == 0.0f);
    
    std::cout << "  [OK] Default values initialized correctly" << std::endl;
}

void test_type_sizes() {
    std::cout << "Testing type sizes and alignment..." << std::endl;
    
    std::cout << "  TestTypeInner: size=" << sizeof(TestTypeInner) 
              << " align=" << alignof(TestTypeInner) << std::endl;
    std::cout << "  TestType: size=" << sizeof(TestType) 
              << " align=" << alignof(TestType) << std::endl;
    
    std::cout << "  TestTypeInnerReflectionHint: size=" << sizeof(TestTypeInnerReflectionHint)
              << " align=" << alignof(TestTypeInnerReflectionHint) << std::endl;
    std::cout << "  TestTypeReflectionHint: size=" << sizeof(TestTypeReflectionHint)
              << " align=" << alignof(TestTypeReflectionHint) << std::endl;
    
    assert(alignof(TestTypeInner) == 8);
    assert(alignof(TestType) == 8);
    assert(alignof(TestTypeInnerReflectionHint) == 8);
    assert(alignof(TestTypeReflectionHint) == 8);
    
    std::cout << "  [OK] All types properly aligned" << std::endl;
}

int main() {
    std::cout << "=== Generated Types Test Suite ===" << std::endl;
    std::cout << std::endl;
    
    try {
        test_basic_creation();
        test_vector_operations();
        test_complex_type();
        test_string_operations();
        test_string_vector();
        test_nested_vector();
        test_map_operations();
        test_set_operations();
        test_default_values();
        test_type_sizes();
        
        std::cout << std::endl;
        std::cout << "=== All Tests Passed! ===" << std::endl;
        return 0;
    }
    catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    }
}
