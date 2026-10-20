#include "xoffsetdatastructure.hpp"
#include <cstdint>
#include <iostream>
#include <stdexcept>
namespace xo = XOffsetDatastructure;
namespace fd = xo::fixed_detail;
struct Attr { xo::XString key; xo::XVector<std::uint8_t> value; };
struct Root { xo::XVector<Attr> attrs; xo::XVector<std::uint32_t> trivial; };
void need(bool b, const char* why) { if (!b) throw std::runtime_error(why); }
auto make() { return xo::XBuffer::create<Root>(65536,
    xo::XBuffer::maximum_capacity(2u << 20).with_growth(xo::XGrowPolicy::Automatic)); }
int main() { try {
    xo::XVector<Attr> empty;
    int unknown = 7;
    need(!fd::move_assignment_owns_address(empty, &unknown), "unbound null traversal");
    auto a = make(); auto& r = a.root<Root>();
    r.attrs.reserve(4);
    need(fd::move_assignment_owns_address(r.attrs, r.attrs.data() + 3), "empty capacity range lost");
    need(!fd::move_assignment_owns_address(r.attrs, &unknown), "empty spare unknown owns");
    r.attrs.resize(2); r.attrs[0].key = "zero"; r.attrs[1].key = "later";
    r.attrs[0].value.resize(5, std::uint8_t{31}); r.attrs[1].value.resize(7, std::uint8_t{47});
    need(fd::move_assignment_owns_address(r.attrs, r.attrs[1].value.data() + 6), "later-child nested range lost");
    need(fd::move_assignment_owns_address(r.attrs, r.attrs[1].key.data()), "later-child string range lost");
    need(fd::move_assignment_owns_address(r.attrs, r.attrs.data() + 3), "live spare capacity range lost");
    need(!fd::move_assignment_owns_address(r.attrs, nullptr), "null target falsely owned");
    need(!fd::move_assignment_owns_address(r.attrs, &unknown), "unknown target falsely owned");
    auto b = make(); b.root<Root>().attrs.resize(1); b.root<Root>().attrs[0].value.resize(4);
    need(!fd::move_assignment_owns_address(r.attrs, b.root<Root>().attrs[0].value.data()), "foreign descendant falsely owned");
    const void* retired = r.attrs.data(); r.attrs.reserve(17);
    need(!fd::move_assignment_owns_address(r.attrs, retired), "old descriptor array cached across operation");
    need(fd::move_assignment_owns_address(r.attrs, r.attrs[1].value.data()), "reallocation descendant lost");
    auto moved = std::move(a); auto& v = moved.root<Root>().attrs;
    need(fd::move_assignment_owns_address(v, v[1].value.data()), "owner move changed exact owns");
    const void* released_child = v[1].value.data(); v.clear();
    need(!fd::move_assignment_owns_address(v, released_child), "empty live range scanned retired child");
    need(fd::move_assignment_owns_address(v, v.data() + 16), "clear lost capacity ownership");
    auto& trivial = moved.root<Root>().trivial; trivial.resize(4);
    need(fd::move_assignment_owns_address(trivial, trivial.data() + 3), "trivial capacity path changed");
    need(!fd::move_assignment_owns_address(trivial, &unknown), "trivial miss changed");
    std::cout << "exact owns null/empty/capacity/later-child/foreign/move/retire checks passed\n";
    return 0;
} catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; } }
