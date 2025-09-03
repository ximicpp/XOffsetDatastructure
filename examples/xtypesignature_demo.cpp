#include "xtypesignature/xtypesignature.hpp"
#include "xoffsetdatastructure/xtypesignature.hpp"
#include "xoffsetdatastructure2/xtypesignature2.hpp"
#include <iostream>
#include <cstdint>

namespace test {
    using XTypeSignature::ANY_SIZE;
    using XTypeSignature::BASIC_ALIGNMENT;
    using XTypeSignature::get_XTypeSignature;
    using XTypeSignature::TypeSignature;
    using XOffsetDatastructure::XString;
    using XOffsetDatastructure::XVector;
    using XOffsetDatastructure::XMap;
    using XOffsetDatastructure::XSet;

    // XOffsetDatastructure2 类型别名
    namespace XOD2 = XOffsetDatastructure2;
    using XString2 = XOD2::XString;
    using XVector2 = XOD2::XVector<int32_t>;
    using XMap2 = XOD2::XMap<XString2, int32_t>;
    using XSet2 = XOD2::XSet<int32_t>;

    // 基本测试结构
    struct Point {
        float x;
        float y;
    };

    struct Rectangle {
        Point top_left;
        Point bottom_right;
        XString name;
    };

    // XOffsetDatastructure 测试类型
    struct alignas(BASIC_ALIGNMENT) TestTypeInner {
        int32_t mInt;
        XVector<int32_t> mVector;
    };

    struct alignas(BASIC_ALIGNMENT) TestType {
        int32_t mInt;
        float mFloat;
        XVector<int32_t> mVector;
        XVector<XString> mStringVector;
        XVector<TestTypeInner> mTypeVector;
        XMap<XString, TestTypeInner> mComplexMap;
        XSet<XString> mStringSet;
        XSet<int32_t> mSet;
        XString mString;
        TestTypeInner innerObj;
    };

    // XOffsetDatastructure2 测试类型
    struct alignas(BASIC_ALIGNMENT) TestTypeInner2 {
        int32_t mInt{ 0 };
        XVector2 mVector;
    };

    struct alignas(BASIC_ALIGNMENT) TestType2 {
        int32_t mInt{ 0 };
        float mFloat{ 0.f };
        XVector2 mVector;
        XOD2::XVector<XString2> mStringVector;
        XOD2::XVector<TestTypeInner2> mTypeVector;
        XOD2::XMap<XString2, TestTypeInner2> mComplexMap;
        XOD2::XSet<XString2> mStringSet;
        XOD2::XSet<int32_t> mSet;
        XString2 mString;
        TestTypeInner2 innerObj;
    };

    // 类型签名验证
    static_assert(get_XTypeSignature<int32_t>() == "i32[s:4,a:4]");
    
    static_assert(get_XTypeSignature<float>() == "f32[s:4,a:4]");
    
    static_assert(get_XTypeSignature<Point>() == 
                 "struct[s:8,a:4]{@0:f32[s:4,a:4],@4:f32[s:4,a:4]}");
    
    static_assert(get_XTypeSignature<Rectangle>() == 
                 "struct[s:48,a:8]{@0:struct[s:8,a:4]{@0:f32[s:4,a:4],@4:f32[s:4,a:4]},@8:struct[s:8,a:4]{@0:f32[s:4,a:4],@4:f32[s:4,a:4]},@16:string[s:32,a:8]}");

    static_assert(get_XTypeSignature<TestTypeInner>() == 
                 "struct[s:40,a:8]{@0:i32[s:4,a:4],@8:vector[s:32,a:8]<i32[s:4,a:4]>}");
    
    static_assert(get_XTypeSignature<TestType>() == 
                 "struct[s:272,a:8]{"
                 "@0:i32[s:4,a:4],"
                 "@4:f32[s:4,a:4],"
                 "@8:vector[s:32,a:8]<i32[s:4,a:4]>,"
                 "@40:vector[s:32,a:8]<string[s:32,a:8]>,"
                 "@72:vector[s:32,a:8]<struct[s:40,a:8]{@0:i32[s:4,a:4],@8:vector[s:32,a:8]<i32[s:4,a:4]>}>,"
                 "@104:map[s:32,a:8]<string[s:32,a:8],struct[s:40,a:8]{@0:i32[s:4,a:4],@8:vector[s:32,a:8]<i32[s:4,a:4]>}>,"
                 "@136:set[s:32,a:8]<string[s:32,a:8]>,"
                 "@168:set[s:32,a:8]<i32[s:4,a:4]>,"
                 "@200:string[s:32,a:8],"
                 "@232:struct[s:40,a:8]{@0:i32[s:4,a:4],@8:vector[s:32,a:8]<i32[s:4,a:4]>}"
                 "}");

    // static_assert(get_XTypeSignature<TestType2>() == 
    //              "struct[s:272,a:8]{"
    //              "@0:i32[s:4,a:4],"
    //              "@4:f32[s:4,a:4],"
    //              "@8:vector[s:32,a:8]<i32[s:4,a:4]>,"
    //              "@40:vector[s:32,a:8]<string[s:32,a:8]>,"
    //              "@72:vector[s:32,a:8]<struct[s:40,a:8]{@0:i32[s:4,a:4],@8:vector[s:32,a:8]<i32[s:4,a:4]>}>,"
    //              "@104:map[s:32,a:8]<string[s:32,a:8],struct[s:40,a:8]{@0:i32[s:4,a:4],@8:vector[s:32,a:8]<i32[s:4,a:4]>}>,"
    //              "@136:set[s:32,a:8]<string[s:32,a:8]>,"
    //              "@168:set[s:32,a:8]<i32[s:4,a:4]>,"
    //              "@200:string[s:32,a:8],"
    //              "@232:struct[s:40,a:8]{@0:i32[s:4,a:4],@8:vector[s:32,a:8]<i32[s:4,a:4]>}"
    //              "}");

    // XOffsetDatastructure 容器类型签名验证
    static_assert(get_XTypeSignature<XVector<int32_t>>() == "vector[s:32,a:8]<i32[s:4,a:4]>");
    
    static_assert(get_XTypeSignature<XMap<XString, int32_t>>() == "map[s:32,a:8]<string[s:32,a:8],i32[s:4,a:4]>");

    // XOffsetDatastructure2 容器类型签名验证
    static_assert(get_XTypeSignature<XVector2>() == "vector[s:32,a:8]<i32[s:4,a:4]>");

    static_assert(get_XTypeSignature<XString2>() == "string[s:32,a:8]");

    static_assert(get_XTypeSignature<XMap2>() == "map[s:32,a:8]<string[s:32,a:8],i32[s:4,a:4]>");

    static_assert(get_XTypeSignature<XSet2>() == "set[s:32,a:8]<i32[s:4,a:4]>");

    // XOffsetDatastructure2 TestTypeInner 类型签名验证
    static_assert(get_XTypeSignature<TestTypeInner2>() == 
                 "struct[s:40,a:8]{@0:i32[s:4,a:4],@8:vector[s:32,a:8]<i32[s:4,a:4]>}");

    // 验证两个库的类型签名是否相同
    static_assert(get_XTypeSignature<XVector<int32_t>>() == get_XTypeSignature<XVector2>());
    static_assert(get_XTypeSignature<XString>() == get_XTypeSignature<XString2>());
    static_assert(get_XTypeSignature<XMap<XString, int32_t>>() == get_XTypeSignature<XMap2>());
    static_assert(get_XTypeSignature<XSet<int32_t>>() == get_XTypeSignature<XSet2>());
    static_assert(get_XTypeSignature<TestTypeInner>() == get_XTypeSignature<TestTypeInner2>());
    // static_assert(get_XTypeSignature<TestType>() == get_XTypeSignature<TestType2>());
    
    static_assert(get_XTypeSignature<char[ANY_SIZE]>() == "bytes[s:64,a:1]");
    
    static_assert(get_XTypeSignature<void*>() == "ptr[s:8,a:8]");

} // namespace test

template <typename T>
void print_XTypeSignature(const char* name)
{
    constexpr auto sig = XTypeSignature::TypeSignature<T>::calculate();
    std::cout << name << ": ";
    sig.print();
    std::cout << '\n';
}

int main()
{
    std::cout << "Type signatures with size and alignment information:\n\n";

    print_XTypeSignature<int32_t>("int32_t");
    print_XTypeSignature<float>("float");
    print_XTypeSignature<test::Point>("Point");
    print_XTypeSignature<test::Rectangle>("Rectangle");
    print_XTypeSignature<test::TestTypeInner>("TestTypeInner");
    print_XTypeSignature<test::TestTypeInner2>("TestTypeInner2");
    print_XTypeSignature<test::TestType>("TestType");
    // print_XTypeSignature<test::TestType2>("TestType2");
    print_XTypeSignature<XOffsetDatastructure::XVector<int32_t>>("Vector<int32_t>");
    print_XTypeSignature<XOffsetDatastructure::XMap<XOffsetDatastructure::XString, int32_t>>("Map<XString, int32_t>");
    print_XTypeSignature<XOffsetDatastructure2::XVector<int32_t>>("Vector2<int32_t>");
    print_XTypeSignature<XOffsetDatastructure2::XMap<XOffsetDatastructure2::XString, int32_t>>("Map2<XString2, int32_t>");
    print_XTypeSignature<char[XTypeSignature::ANY_SIZE]>("char[ANY_SIZE]");
    print_XTypeSignature<void*>("void*");

    return 0;
}