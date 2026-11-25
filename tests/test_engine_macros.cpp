// The client headers compile after an engine has defined function-like
// macros with common names. Unreal Engine's CoreMinimal.h defines check,
// checkf, checkSlow, verify, verifyf, ensure, ensureMsgf, LIKELY, UNLIKELY
// and TEXT before any third-party header is included, and Apple's
// AssertMacros.h defines check, verify, require, require_action,
// check_noerr, verify_noerr and nrequire when its unprefixed names are on.
// A library identifier with one of these names followed by '(' would be
// expanded by the preprocessor and break the build.
//
// The standard and platform headers are included first, so the test covers
// the library's own identifiers, not the toolchain's.

#include <algorithm>
#include <array>
#include <atomic>
#include <bit>
#include <cassert>
#include <cctype>
#include <cerrno>
#include <charconv>
#include <chrono>
#include <climits>
#include <cmath>
#include <compare>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <functional>
#include <iterator>
#include <limits>
#include <map>
#include <memory>
#include <mutex>
#include <new>
#include <optional>
#include <ostream>
#include <span>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <typeinfo>
#include <unordered_map>
#include <utility>
#include <vector>
#include <version>
#if __has_include(<memory_resource>)
#include <memory_resource>
#endif
#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <intrin.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif
#if defined(__APPLE__)
#include <TargetConditionals.h>
#endif

// Unreal Engine (AssertionMacros.h, Platform.h, the shapes of the checked
// build): statements in braces, so a use as an expression fails too.
#define check(expr) { if (!(expr)) std::abort(); }
#define checkf(expr, ...) { if (!(expr)) std::abort(); }
#define checkSlow(expr) { if (!(expr)) std::abort(); }
#define verify(expr) { if (!(expr)) std::abort(); }
#define verifyf(expr, ...) { if (!(expr)) std::abort(); }
#define ensure(expr) (!!(expr))
#define ensureMsgf(expr, ...) (!!(expr))
#define LIKELY(x) (!!(x))
#define UNLIKELY(x) (!!(x))
#define TEXT(x) x
// Apple AssertMacros.h with __ASSERT_MACROS_DEFINE_VERSIONS_WITHOUT_UNDERSCORES.
#define require(assertion, exceptionLabel) do { if (!(assertion)) goto exceptionLabel; } while (0)
#define require_action(assertion, exceptionLabel, action) \
    do { if (!(assertion)) { action; goto exceptionLabel; } } while (0)
#define check_noerr(errorCode) do { (void)(errorCode); } while (0)
#define verify_noerr(errorCode) do { (void)(errorCode); } while (0)
#define nrequire(assertion, exceptionLabel) require(!(assertion), exceptionLabel)

#include "../xoffsetdatastructure.hpp"
#include "../xoffset_compactor.hpp"
#include "../xoffset_dump.hpp"
#include "../xoffset_fixed_message.hpp"
#include "../xoffset_handshake.hpp"
#include "../xoffset_receiver.hpp"
#include "../xoffset_shared_delta.hpp"
#include "../xoffset_shared_publisher.hpp"

using namespace XOffsetDatastructure;

namespace engine_macros {

struct Unit {
    std::uint32_t id;
    std::int32_t hp;
    XString name;
};

struct World {
    std::uint64_t tick;
    XVector<Unit> units;
};

} // namespace engine_macros

XOFFSET_RECORD(engine_macros::Unit, id, hp, name)
XOFFSET_RECORD(engine_macros::World, tick, units)
XOFFSET_REGISTER_SCHEMA_NAME(engine_macros::World, "test.engine_macros.World")

namespace {

int failures = 0;

void expect(bool ok, const char* what) {
    if (!ok) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        ++failures;
    }
}

} // namespace

int main() {
    using engine_macros::World;
    auto world = TypedXBuffer<World>::create(4096, XBuffer::maximum_capacity(1 << 20));
    auto unit = world.root().units().emplace_back();
    unit.id() = 7;
    unit.hp() = 100;
    unit.name() = "engine client";
    world.root().tick() = 3;
    expect(world.ok(), "build the state through proxies");

    std::string wire;
    expect(static_cast<bool>(world.try_save_verified_into(wire)), "save");

    XReceiver<XLink::from_server> rx{XBuffer::maximum_capacity(1 << 20)};
    auto held = TypedXBuffer<World>::create(4096, XBuffer::maximum_capacity(1 << 20));
    expect(static_cast<bool>(rx.reload(held, wire)), "trusted reload");
    held.read([&](const World& w) {
        expect(w.tick == 3 && w.units.size() == 1 && w.units[0].name.view() == "engine client",
               "received state");
    });

    XReceiver<XLink::from_client> up{XBuffer::maximum_capacity(1 << 20)};
    auto view = up.view<World>(wire);
    expect(view.has_value() && view->root().units[0].hp == 100, "uplink view");
    if (view) expect(view->tier() == (XOFFSET_CHECK_UPLINK ? XCheck::bounds : XCheck::admit), "view tier");

    if (failures != 0) return 1;
    std::printf("engine macros: OK\n");
    return 0;
}
