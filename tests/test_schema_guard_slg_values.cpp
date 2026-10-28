#define XSG_WIRE_INPUT_HASH "semantic-test"
#define XSG_BUILD_ID "semantic-test"
#include "../tools/schema_guard/wire_runtime.cpp"
#include <array>
#include <limits>
template<class T,class F> void check(F mutate) {
    using B=XOffsetDatastructure::TypedXBuffer<T>;
    auto a=B::create(1u<<20,xsg_slg::cap()), b=B::create(1u<<20,xsg_slg::cap());
    xsg_slg::fill(a.root()); xsg_slg::fill(b.root());
    if(!xsg_slg::values_equal(a.root(),b.root())) fail("semantic comparison rejected equal roots");
    mutate(b.root());
    if(xsg_slg::values_equal(a.root(),b.root())) fail("semantic comparison missed changed field");
}
struct SemanticRegression { SemanticRegression() {
    check<slg::xm::PlayerState>([](auto& r){r.armies[0].heroes[0].level+=1;});
    check<slg::xm::PlayerState>([](auto& r){r.research.insert_or_assign(7,99);});
    check<slg::xm::BattleMsg>([](auto& r){r.report.events[0].damage+=1;});
    check<slg::xm::ShardState>([](auto& r){r.marches[0].troops[0].count+=1;});
    check<slg::xm::Report>([](auto& r){r.summary="incorrect summary";});
    const double nan=std::numeric_limits<double>::quiet_NaN();
    if(!xsg_slg::values_equal(nan,nan) || xsg_slg::values_equal(0.0,-0.0)) fail("floating comparison is not bitwise");
    const std::array<int,3> x{1,2,3}, y{1,2,4};
    const int u[2]={5,6}, v[2]={5,7};
    if(!xsg_slg::values_equal(x,x) || xsg_slg::values_equal(x,y)) fail("std::array comparison");
    if(!xsg_slg::values_equal(u,u) || xsg_slg::values_equal(u,v)) fail("array comparison");
    std::cout<<"SLG field and query checks passed\n";
}};
SemanticRegression semantic_regression;
