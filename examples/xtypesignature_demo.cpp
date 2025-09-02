#include "xtypesignature.hpp"
#include <iostream>

namespace test {
    using namespace XTypeSignature;

    // Verify specific type sizes
    static_assert(sizeof(bool) <= ANY_SIZE, "bool size exceeds ANY_SIZE bytes");
    static_assert(sizeof(int) <= ANY_SIZE, "int size exceeds ANY_SIZE bytes");
    static_assert(sizeof(float) <= ANY_SIZE, "float size exceeds ANY_SIZE bytes");
    static_assert(sizeof(double) <= ANY_SIZE, "double size exceeds ANY_SIZE bytes");
    static_assert(sizeof(XString) <= ANY_SIZE, "string size exceeds ANY_SIZE bytes");
    static_assert(sizeof(XMap<int, int>) <= ANY_SIZE, "map size exceeds ANY_SIZE bytes");
    static_assert(sizeof(XVector<int>) <= ANY_SIZE, "vector size exceeds ANY_SIZE bytes");

    // Test structures
    struct Point {
        float x;
        float y;
    };

    struct Rectangle {
        Point top_left;
        Point bottom_right;
        XString name;
    };

    struct alignas(BASIC_ALIGNMENT) TestType {
        int32_t mInt;

        struct alignas(BASIC_ALIGNMENT) TestTypeInner {
            int32_t mInt;
            XVector<int32_t> mVector;
        } TestTypeInnerObj;

        XMap<XString, TestTypeInner> mComplexMap;
    };

    struct alignas(BASIC_ALIGNMENT) TestType2 {
        int32_t mInt;
        XString mStr;
        XVector<int32_t> mVector;
    };

    // Type signature verification
    static_assert(get_XTypeSignature<int32_t>() == "i32[s:4,a:4]");
    
    static_assert(get_XTypeSignature<float>() == "f32[s:4,a:4]");
    
    static_assert(get_XTypeSignature<Point>() == 
                 "struct[s:8,a:4]{@0:f32[s:4,a:4],@4:f32[s:4,a:4]}");
    
    static_assert(get_XTypeSignature<Rectangle>() == 
                 "struct[s:40,a:8]{@0:struct[s:8,a:4]{@0:f32[s:4,a:4],@4:f32[s:4,a:4]},"
                 "@8:struct[s:8,a:4]{@0:f32[s:4,a:4],@4:f32[s:4,a:4]},"
                 "@16:string[s:24,a:8]}");
    
    static_assert(get_XTypeSignature<TestType>() == 
                 "struct[s:64,a:8]{@0:i32[s:4,a:4],"
                 "@8:struct[s:32,a:8]{@0:i32[s:4,a:4],@8:vector[s:24,a:8]<i32[s:4,a:4]>},"
                 "@40:map[s:24,a:8]<string[s:24,a:8],struct[s:32,a:8]{@0:i32[s:4,a:4],"
                 "@8:vector[s:24,a:8]<i32[s:4,a:4]>}>}");
    
    static_assert(get_XTypeSignature<TestType2>() == 
                 "struct[s:56,a:8]{@0:i32[s:4,a:4],@8:string[s:24,a:8],"
                 "@32:vector[s:24,a:8]<i32[s:4,a:4]>}");
    
    static_assert(get_XTypeSignature<XVector<int32_t>>() == 
                 "vector[s:24,a:8]<i32[s:4,a:4]>");
    
    static_assert(get_XTypeSignature<XMap<XString, int32_t>>() == 
                 "map[s:24,a:8]<string[s:24,a:8],i32[s:4,a:4]>");
    
    static_assert(get_XTypeSignature<char[ANY_SIZE]>() == 
                 "bytes[s:64,a:1]");
    
    static_assert(get_XTypeSignature<void*>() == 
                 "ptr[s:8,a:8]");
    
    static_assert(get_XTypeSignature<any_equivalent>() ==
                 "struct[s:72,a:8]{@0:ptr[s:8,a:8],@8:bytes[s:64,a:1]}");
    
    static_assert(get_XTypeSignature<DynamicStruct>() ==
                 "map[s:24,a:8]<string[s:24,a:8],struct[s:72,a:8]{@0:ptr[s:8,a:8],"
                 "@8:bytes[s:64,a:1]}>");

} // namespace test

template <typename T>
void print_XTypeSignature(const char* name)
{
    using namespace XTypeSignature;
    constexpr auto sig = TypeSignature<T>::calculate();
    std::cout << name << ": ";
    sig.print();
    std::cout << '\n';
}

int main()
{
    std::cout << "Type signatures with size and alignment information:\n";

    print_XTypeSignature<int32_t>("int32_t");
    print_XTypeSignature<float>("float");
    print_XTypeSignature<test::Point>("Point");
    print_XTypeSignature<test::Rectangle>("Rectangle");
    print_XTypeSignature<test::TestType>("TestType");
    print_XTypeSignature<test::TestType2>("TestType2");
    print_XTypeSignature<XTypeSignature::XVector<int32_t>>("Vector<int32_t>");
    print_XTypeSignature<XTypeSignature::XMap<XTypeSignature::XString, int32_t>>("Map<XString, int32_t>");
    print_XTypeSignature<char[XTypeSignature::ANY_SIZE]>("char[ANY_SIZE]");
    print_XTypeSignature<void*>("void*");
    print_XTypeSignature<XTypeSignature::XAny>("XAny");
    print_XTypeSignature<XTypeSignature::DynamicStruct>("DynamicStruct");

    return 0;
}