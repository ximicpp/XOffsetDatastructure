// Smoke probe for a freshly built clang-p2996 toolchain (no XOffset headers).
// Checks reflection queries, splicing, expansion statements and the libc++
// hardening mode, then prints what it saw so the CI log records it.
#include <cstddef>
#include <cstdio>
#include <string_view>
#include <type_traits>
#include <version>

#if __has_include(<meta>)
#include <meta>
#else
#include <experimental/meta>
#endif

#ifndef _LIBCPP_VERSION
#error "expected libc++ (-stdlib=libc++)"
#endif
#if !defined(_LIBCPP_HARDENING_MODE) || _LIBCPP_HARDENING_MODE != _LIBCPP_HARDENING_MODE_NONE
#error "libc++ hardening mode is not none by default"
#endif

namespace probe {

struct Base {
    unsigned tag = 3;
};

struct Record : Base {
    int id = 7;
    double scale = 1.5;
    char name[4] = {'a', 'b', 'c', 0};
};

enum class Kind { alpha, beta, gamma };

constexpr auto ctx = std::meta::access_context::unchecked();

static_assert(std::meta::nonstatic_data_members_of(^^Record, ctx).size() == 3);
static_assert(std::meta::bases_of(^^Record, ctx).size() == 1);
static_assert(std::meta::identifier_of(std::meta::nonstatic_data_members_of(^^Record, ctx)[1]) == "scale");
static_assert(std::meta::enumerators_of(^^Kind).size() == 3);

template <class T, std::size_t I>
constexpr auto member_at() {
    return std::meta::nonstatic_data_members_of(^^T, ctx)[I];
}

// Splice through a constant, the same pattern the library uses.
template <class T, std::size_t I>
auto& field(T& value) {
    constexpr auto member = member_at<T, I>();
    return value.[:member:];
}

int sum_ints(const Record& r) {
    int total = 0;
    template for (constexpr auto member : std::define_static_array(
                      std::meta::nonstatic_data_members_of(^^Record, ctx))) {
        using M = [:std::meta::type_of(member):];
        if constexpr (std::is_same_v<M, int>) total += r.[:member:];
    }
    return total;
}

std::string_view kind_name(Kind k) {
    template for (constexpr auto e : std::define_static_array(std::meta::enumerators_of(^^Kind))) {
        if (k == [:e:]) return std::meta::identifier_of(e);
    }
    return "?";
}

} // namespace probe

int main() {
    probe::Record r;
    field<probe::Record, 0>(r) = 41;
    const int ints = probe::sum_ints(r);
    const auto beta = probe::kind_name(probe::Kind::beta);
    std::printf("libc++ %d, hardening mode %d (none=%d)\n", _LIBCPP_VERSION, _LIBCPP_HARDENING_MODE,
                _LIBCPP_HARDENING_MODE_NONE);
    std::printf("sum_ints=%d kind=%.*s base.tag=%u\n", ints, int(beta.size()), beta.data(), r.tag);
    if (ints != 41 || beta != "beta" || r.tag != 3) {
        std::puts("reflection probe: FAIL");
        return 1;
    }
    std::puts("reflection probe: OK");
    return 0;
}
