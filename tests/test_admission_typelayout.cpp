// K0 admission cross test (C++26 only): XOffset's own admission
// (xoffset_admission.hpp, walking the schema field interface) must give the
// same answer as TypeLayout's is_byte_copy_safe, which the library used
// before it stopped including TypeLayout.
//
// Compared per type: the recursive predicate (what a member walk sees), and
// for everything but XOffset's containers the top-level trait as well. The
// containers are specialized by XOffset (element must be admitted), which
// TypeLayout no longer sees.

#include "xoffsetdatastructure.hpp"
#include <boost/typelayout.hpp>
#include "../tools/signature_catalog.hpp"
#include "../bench/slg/include/slg/model_xoffset.hpp"

#include <array>
#include <cstdint>
#include <iostream>
#include <utility>

namespace xo = XOffsetDatastructure;

namespace admission_cases {
    enum class Color : std::uint8_t { red, green };
    enum Legacy { legacy_a, legacy_b };
    struct Plain { std::int32_t a; double b; };
    struct WithPointer { std::int32_t a; int* p; };
    struct WithReference { int& r; };
    struct WithMemberPointer { int Plain::* m; };
    struct WithFunctionPointer { void (*f)(); };
    struct WithNull { std::nullptr_t n; std::int32_t a; };
    struct Poly { virtual ~Poly() = default; std::int32_t a; };
    struct HasPolyMember { Poly p; };
    struct Derived : Plain { std::int32_t c; };
    struct DerivedFromPoly : Poly { std::int32_t c; };
    struct Empty {};
    struct WithEmpty { [[no_unique_address]] Empty e; std::int32_t a; };
    struct DerivedFromEmpty : Empty { std::int32_t a; };
    struct Bits { std::uint32_t a : 3; std::uint32_t b : 29; };
    union ScalarUnion { std::int32_t i; float f; };
    union PointerUnion { std::int32_t i; int* p; };
    struct HasUnion { ScalarUnion u; };
    struct HasPointerUnion { PointerUnion u; };
    struct Arrays { std::int32_t a[4]; Plain b[2]; };
    struct PointerArray { int* p[2]; };
    struct Owning { xo::XString name; xo::XVector<std::int32_t> values; };
    struct OwningArray { xo::XString names[2]; };
    struct OwningNested { Owning inner; xo::XMap<xo::XString, Plain> by_name; xo::XSet<std::int32_t> ids; };
    struct StdTypes { std::array<std::int32_t, 3> a; std::pair<std::int32_t, float> p; };
    struct UserDestructor { std::int32_t a; ~UserDestructor() {} };
    struct Private { std::int32_t get() const { return a; } private: std::int32_t a = 0; };
    struct Base1 { std::int32_t x; };
    struct Base2 { int* q; };
    struct MultiDerived : Base1, Base2 { std::int32_t z; };
    struct Deep { Arrays arrays; HasUnion u; WithEmpty e; Bits bits; };
} // namespace admission_cases

namespace {
    int failures = 0;

    template <class T>
    void same(const char* name, bool top_level = true) {
        const bool xo_walk = xo::admission_detail::byte_copy_safe<T>();
        const bool tl_walk = boost::typelayout::detail::is_byte_copy_safe_impl<T>();
        if (xo_walk != tl_walk) {
            std::cout << "  MISMATCH " << name << ": XOffset walk " << xo_walk << ", TypeLayout " << tl_walk << "\n";
            ++failures;
        }
        if (top_level && xo::is_byte_copy_safe_v<T> != boost::typelayout::is_byte_copy_safe_v<T>) {
            std::cout << "  MISMATCH " << name << " (top level): XOffset " << xo::is_byte_copy_safe_v<T>
                      << ", TypeLayout " << boost::typelayout::is_byte_copy_safe_v<T> << "\n";
            ++failures;
        }
    }

    // XOffset's containers: walk equal, top level = walk && element(s) admitted.
    template <class C, class... E>
    void container(const char* name) {
        same<C>(name, false);
        const bool expected = (xo::admission_detail::byte_copy_safe<C>() && ... && xo::is_byte_copy_safe_v<E>);
        if (xo::is_byte_copy_safe_v<C> != expected) {
            std::cout << "  container " << name << ": top level " << xo::is_byte_copy_safe_v<C>
                      << ", expected " << expected << "\n";
            ++failures;
        }
    }
} // namespace

int main() {
    using namespace admission_cases;
#define XOFFSET_ADMISSION_SAME(Type, Name) same<Type>(Name);
    XOFFSET_SIGNATURE_TYPE_LIST(XOFFSET_ADMISSION_SAME)
#undef XOFFSET_ADMISSION_SAME

    same<bool>("bool"); same<char>("char"); same<std::byte>("byte"); same<std::int64_t>("i64");
    same<double>("f64"); same<long double>("long double"); same<char32_t>("char32");
    same<Color>("enum class"); same<Legacy>("legacy enum");
    same<int*>("int*"); same<void (*)()>("fn ptr"); same<int Plain::*>("member ptr");
    same<std::nullptr_t>("nullptr_t");
    same<std::int32_t[3]>("i32[3]"); same<int* [2]>("ptr[2]"); same<xo::XString[2]>("XString[2]");
    same<Plain>("Plain"); same<WithPointer>("WithPointer"); same<WithReference>("WithReference");
    same<WithMemberPointer>("WithMemberPointer"); same<WithFunctionPointer>("WithFunctionPointer");
    same<WithNull>("WithNull"); same<Poly>("Poly"); same<HasPolyMember>("HasPolyMember");
    same<Derived>("Derived"); same<DerivedFromPoly>("DerivedFromPoly"); same<Empty>("Empty");
    same<WithEmpty>("WithEmpty"); same<DerivedFromEmpty>("DerivedFromEmpty"); same<Bits>("Bits");
    same<ScalarUnion>("ScalarUnion"); same<PointerUnion>("PointerUnion"); same<HasUnion>("HasUnion");
    same<HasPointerUnion>("HasPointerUnion"); same<Arrays>("Arrays"); same<PointerArray>("PointerArray");
    same<Owning>("Owning"); same<OwningArray>("OwningArray"); same<OwningNested>("OwningNested");
    same<StdTypes>("StdTypes"); same<UserDestructor>("UserDestructor"); same<Private>("Private");
    same<MultiDerived>("MultiDerived"); same<Deep>("Deep"); same<const Plain>("const Plain");

    same<xo::XString>("XString");
    container<xo::XVector<std::int32_t>, std::int32_t>("XVector<i32>");
    container<xo::XVector<Owning>, Owning>("XVector<Owning>");
    container<xo::XVector<WithPointer>, WithPointer>("XVector<WithPointer>");
    container<xo::XSet<std::int32_t>, std::int32_t>("XSet<i32>");
    container<xo::XMap<xo::XString, Plain>, xo::XString, Plain>("XMap<XString, Plain>");
    container<xo::XMap<std::int32_t, WithPointer>, std::int32_t, WithPointer>("XMap<i32, WithPointer>");

    same<slg::xm::Buff>("slg Buff"); same<slg::xm::Hero>("slg Hero"); same<slg::xm::Army>("slg Army");
    same<slg::xm::City>("slg City"); same<slg::xm::PlayerState>("slg PlayerState");
    same<slg::xm::Report>("slg Report"); same<slg::xm::BattleMsg>("slg BattleMsg");
    same<slg::xm::March>("slg March"); same<slg::xm::ShardState>("slg ShardState");

    std::cout << "test_admission_typelayout: " << (failures ? "FAILED" : "passed") << "\n";
    return failures ? 1 : 0;
}
