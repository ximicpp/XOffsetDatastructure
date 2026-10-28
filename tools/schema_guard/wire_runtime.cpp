#if defined(__APPLE__)
#include <TargetConditionals.h>
#endif
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
#include <bit>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include "bench/slg/include/slg/model_xoffset.hpp"

#ifndef XSG_WIRE_INPUT_HASH
#error "XSG_WIRE_INPUT_HASH must identify the current wire fixture sources"
#endif

#ifndef XSG_BUILD_ID
#error "XSG_BUILD_ID must bind the fixture build manifest"
#endif
#define main xsg_original_wire_main
#include "../../tests/test_cross_compiler_wire.cpp"
#undef main


namespace xsg_slg {
namespace xm = slg::xm;
namespace xo = XOffsetDatastructure;
auto cap() { return xo::XBuffer::maximum_capacity(32u << 20).with_growth(xo::XGrowPolicy::Automatic); }
void city(xm::City& c) {
    c.city_id=100001; c.level=12;
    c.buildings.insert_or_assign(5, 7); c.buildings.insert_or_assign(9, 2);
    c.resources.insert_or_assign(1, 123456789); c.resources.insert_or_assign(3, 555);
}
void hero(xm::Hero& h, int i) {
    h.id=100+i; h.level=20+i; h.star=3; h.name="Cross platform hero " + std::to_string(i);
    auto& skill=h.skills.emplace_back(); skill.id=40+i; skill.level=6;
    auto& buff=h.buffs.emplace_back(); buff.id=9+i; buff.stacks=2; buff.expire_ms=1234567+i;
}
void army(xm::Army& a, int n) {
    a.army_id=1000+n; a.owner_id=2000+n; a.x=n; a.y=-n;
    hero(a.heroes.emplace_back(), n); hero(a.heroes.emplace_back(), n+1);
    auto& t=a.troops.emplace_back(); t.type=n+1; t.count=10000; t.wounded=37;
    auto& b=a.buffs.emplace_back(); b.id=n+12; b.stacks=1; b.expire_ms=100000;
}
void report(xm::Report& r) {
    r.battle_id=999111; r.summary="SLG cross-platform battle report";
    for(int i=0;i<3;++i) {
        auto& e=r.events.emplace_back();
        e.round=i; e.hero_id=100+i; e.skill_id=40+i; e.target=200+i; e.damage=1000-i*3;
    }
}
template<class T> void fill(T& r);
template<> void fill(xm::PlayerState& r) {
    r.player_id=201; r.name="SLG player across systems"; city(r.city);
    army(r.armies.emplace_back(),1); hero(r.heroes.emplace_back(),4);
    r.research.insert_or_assign(7,11); r.items.insert_or_assign(19,567890123);
}
template<> void fill(xm::BattleMsg& r) {
    r.battle_id=999111; r.seed=20261028; r.winner=1;
    army(r.attacker,1); army(r.defender,2); report(r.report);
}
template<> void fill(xm::ShardState& r) {
    r.shard_id=3; r.tick=987654321; r.width=256;
    auto& tile=r.tiles.emplace_back(); tile.x=8; tile.y=-4; tile.terrain=2; tile.level=5; tile.owner=201;
    city(r.cities.emplace_back());
    auto& march=r.marches.emplace_back(); march.march_id=99; march.owner=201;
    march.from_x=1; march.from_y=2; march.to_x=3; march.to_y=4; march.eta=55;
    auto& t=march.troops.emplace_back(); t.type=3; t.count=222; t.wounded=5;
}
template<> void fill(xm::Report& r) { report(r); }
template<class T> bool values_equal(const T& a, const T& b);
template<class T, std::size_t I> bool one_field_equal(const T& a, const T& b) {
    using namespace std::meta;
    constexpr auto member=nonstatic_data_members_of(^^T, access_context::unchecked())[I];
    return values_equal(a.[:member:],b.[:member:]);
}
template<class T, std::size_t... I>
bool fields_equal(const T& a, const T& b, std::index_sequence<I...>) {
    return (one_field_equal<T,I>(a,b) && ...);
}
template<class T> bool values_equal(const T& a, const T& b) {
    namespace d=xo::detail;
    // Floating values compare by representation so NaN payloads and -0.0 count.
    if constexpr (std::is_floating_point_v<T>) {
        if constexpr (sizeof(T)==sizeof(std::uint32_t)) return std::bit_cast<std::uint32_t>(a)==std::bit_cast<std::uint32_t>(b);
        else if constexpr (sizeof(T)==sizeof(std::uint64_t)) return std::bit_cast<std::uint64_t>(a)==std::bit_cast<std::uint64_t>(b);
        else return a==b || (a!=a && b!=b);
    }
    else if constexpr (std::is_arithmetic_v<T> || std::is_enum_v<T> || std::is_same_v<T,std::byte>) return a==b;
    else if constexpr (std::is_array_v<T>) {
        for(std::size_t i=0;i<std::extent_v<T>;++i) if(!values_equal(a[i],b[i])) return false;
        return true;
    }
    else if constexpr (requires { typename T::value_type; std::tuple_size<T>::value; }) {
        for(std::size_t i=0;i<std::tuple_size_v<T>;++i) if(!values_equal(a[i],b[i])) return false;
        return true;
    }
    else if constexpr (d::is_fixed_string_v<T>) return std::string_view(a.data(),a.size())==std::string_view(b.data(),b.size());
    else if constexpr (d::is_fixed_flat_map_v<T>) {
        if(a.size()!=b.size()) return false;
        for(const auto& entry:a) {
            auto it=b.find(entry.first);
            if(it==b.end() || !values_equal(entry.second,it->second)) return false;
        }
        return true;
    } else if constexpr (d::is_fixed_vector_v<T> || d::is_fixed_flat_set_v<T>) {
        if(a.size()!=b.size()) return false;
        auto i=a.begin(), j=b.begin();
        for(;i!=a.end();++i,++j) if(!values_equal(*i,*j)) return false;
        return true;
    } else return fields_equal(a,b,std::make_index_sequence<d::reflected_member_count<T>()>{});
}
template<class T> void run(const std::filesystem::path& dir, const char* name, bool write) {
    using Buffer=xo::TypedXBuffer<T>;
    auto b=Buffer::create(1u<<20,cap()); fill(b.root());
    const auto expected=b.save_verified();
    const auto path=dir/(std::string(name)+".xwire");
    if(write) {
        std::ofstream f(path,std::ios::binary); f.write(expected.data(),static_cast<std::streamsize>(expected.size()));
        if(!f) fail(std::string("cannot write ")+name);
    } else {
        const auto wire=read_file(path);
        auto loaded=Buffer::load(wire,cap());
        if(!values_equal(b.root(),loaded.root())) fail(std::string(name)+": loaded field values mismatch");
        if(wire!=expected || loaded.save_verified()!=wire) fail(std::string(name)+": SLG wire mismatch");
    }
    std::cout << name << (write ? " written" : " accepted and byte-identical") << std::endl;
}
void run_all(const char* dir, bool write) {
    const std::filesystem::path p(dir);
    run<xm::PlayerState>(p,"slg_player",write);
    run<xm::BattleMsg>(p,"slg_battle",write);
    run<xm::ShardState>(p,"slg_shard",write);
    run<xm::Report>(p,"slg_report",write);
}
} // namespace xsg_slg

int main(int argc, char** argv) {
    if (argc == 2 && std::strcmp(argv[1], "--xsg-identify") == 0) {
#if defined(_WIN32)
# if defined(__x86_64__) || defined(_M_X64)
        const char* target = "windows-x64";
# elif defined(__aarch64__) || defined(_M_ARM64)
        const char* target = "windows-arm64";
# else
        const char* target = "unsupported";
# endif
#elif defined(__ANDROID__)
# if defined(__x86_64__)
        const char* target = "android-x64";
# elif defined(__aarch64__)
        const char* target = "android-arm64";
# else
        const char* target = "unsupported";
# endif
#elif defined(__APPLE__) && TARGET_OS_IPHONE
# if TARGET_OS_SIMULATOR
#  if defined(__x86_64__)
        const char* target = "ios-simulator-x64";
#  elif defined(__aarch64__)
        const char* target = "ios-simulator-arm64";
#  else
        const char* target = "unsupported";
#  endif
        if (!std::getenv("SIMULATOR_UDID")) return 2;
# elif defined(__aarch64__)
        const char* target = "ios-arm64";
# else
        const char* target = "unsupported";
# endif
#elif defined(__APPLE__)
# if defined(__x86_64__)
        const char* target = "macos-x64";
# elif defined(__aarch64__)
        const char* target = "macos-arm64";
# else
        const char* target = "unsupported";
# endif
        if (std::getenv("SIMULATOR_UDID")) return 2;
#elif defined(__linux__)
# if defined(__x86_64__)
        const char* target = "linux-x64";
# elif defined(__aarch64__)
        const char* target = "linux-arm64";
# else
        const char* target = "unsupported";
# endif
#else
        const char* target = "unsupported";
#endif
        const char* endian = std::endian::native == std::endian::little ? "le" : "other";
        #if defined(_WIN32)
        const char* environment = GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "wine_get_version") ? "wine" : "windows";
#elif defined(__ANDROID__)
        const char* environment = "android";
#elif defined(__APPLE__) && TARGET_OS_IPHONE
        const char* environment = TARGET_OS_SIMULATOR ? "ios-simulator" : "ios";
#elif defined(__APPLE__)
        const char* environment = "macos";
#elif defined(__linux__)
        const char* environment = "linux";
#else
        const char* environment = "unsupported";
#endif
        std::printf("XSG_RUNTIME_V2 %s %zu %s %s %s %s\n", target, sizeof(void*) * 8, endian, XSG_WIRE_INPUT_HASH, environment, XSG_BUILD_ID);
        return 0;
    }
    const int rc=xsg_original_wire_main(argc, argv);
    if (rc == 0 && argc == 3 &&
        (std::strcmp(argv[1], "--write") == 0 || std::strcmp(argv[1], "--read") == 0))
        xsg_slg::run_all(argv[2], std::strcmp(argv[1], "--write") == 0);
    return rc;
}
