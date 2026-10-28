// Device mesh demo: the control wires (PeerInfo, Welcome, Roster) and the
// build line, shared by xo_peer and xo_hub. OWNER: network thread.
//
// Every call here returns an Error instead of throwing, so xo_peer_noexc
// can use it. Control wires are built with save_verified and loaded with
// try_load_verified at protocol::kControlCap.
#pragma once
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
#if defined(__APPLE__)
#include <TargetConditionals.h>
#endif
#include "xmesh/protocol.hpp"
#include "xmesh/result.hpp"
#include "xmesh/schema.hpp"

namespace xmesh::control {

// ---- the build line -------------------------------------------------------------

inline const char* os_name() noexcept {
#if defined(__ANDROID__)
    return "android";
#elif defined(__APPLE__) && defined(TARGET_OS_SIMULATOR) && TARGET_OS_SIMULATOR
    return "ios-sim";
#elif defined(__APPLE__) && defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE
    return "ios";
#elif defined(__APPLE__)
    return "macos";
#elif defined(_WIN32)
    return "windows";
#elif defined(__linux__)
    return "linux";
#else
    return "unknown";
#endif
}

inline const char* arch_name() noexcept {
#if defined(__aarch64__) || defined(_M_ARM64)
    return "arm64";
#elif defined(__x86_64__) || defined(_M_X64)
    return "x64";
#else
    return "other";
#endif
}

// Short compiler name for the default device name: msvc, clang-cl, mingw,
// apple-clang, clang18, gcc13, ...
inline std::string compiler_short() {
#if defined(__clang__) && defined(_MSC_VER)
    return "clang-cl";
#elif defined(_MSC_VER)
    return "msvc";
#elif defined(__MINGW32__)
    return "mingw";
#elif defined(__apple_build_version__)
    return "apple-clang";
#elif defined(__ANDROID__)
    return "ndk";
#elif defined(__clang__)
    return "clang" + std::to_string(__clang_major__);
#elif defined(__GNUC__)
    return "gcc" + std::to_string(__GNUC__);
#else
    return "cc";
#endif
}

// One line naming how this program was built (PeerInfo::build, Welcome::hub_build).
inline std::string build_info() {
    std::string s = "compiler=";
#if defined(__clang__) && defined(_MSC_VER)
    s += "clang-cl-" + std::to_string(__clang_major__) + "." + std::to_string(__clang_minor__);
#elif defined(__apple_build_version__)
    s += "apple-clang-" + std::to_string(__clang_major__) + "." + std::to_string(__clang_minor__);
#elif defined(__clang__)
    s += "clang-" + std::to_string(__clang_major__) + "." + std::to_string(__clang_minor__);
#elif defined(_MSC_VER)
    s += "msvc-" + std::to_string(_MSC_FULL_VER);
#elif defined(__GNUC__)
    s += "gcc-" + std::to_string(__GNUC__) + "." + std::to_string(__GNUC_MINOR__);
#else
    s += "unknown";
#endif
    s += " os=";
    s += os_name();
#if defined(__ANDROID__)
    s += "-" + std::to_string(__ANDROID_API__);
#endif
#if defined(__MINGW32__)
    s += "-mingw";
#endif
    s += " arch=";
    s += arch_name();
#if defined(_MSVC_LANG)
    s += " cxx=" + std::to_string(_MSVC_LANG);
#else
    s += " cxx=" + std::to_string(__cplusplus);
#endif
    s += XOFFSET_SCHEMA_REFLECTION ? " schema=reflection" : " schema=field-lists";
    s += " schema_version=" + std::to_string(XMESH_SCHEMA_VERSION);
    s += XMESH_EXCEPTIONS ? " exceptions=on" : " exceptions=off";
    return s;
}

// <os>-<arch>-<compiler>[-noexc], e.g. "windows-x64-msvc-noexc".
inline std::string default_name() {
    std::string s = std::string(os_name()) + "-" + arch_name() + "-" + compiler_short();
    if (!XMESH_EXCEPTIONS) s += "-noexc";
    return s;
}

inline std::uint32_t own_flags(bool evil) noexcept {
    std::uint32_t f = 0;
    if (XMESH_EXCEPTIONS) f |= protocol::kPeerExceptions;
    if (XOFFSET_SCHEMA_REFLECTION) f |= protocol::kPeerReflection;
    if (evil) f |= protocol::kPeerEvil;
    return f;
}

// ---- control wires ----------------------------------------------------------------

inline XBuffer::MaximumCapacity control_cap() noexcept {
    return XBuffer::maximum_capacity(protocol::kControlCap).with_growth(XGrowPolicy::Automatic);
}

template <class Root>
Result<XBuffer> load_control(std::string_view wire) {
    XResult<XBuffer> b = XBuffer::try_load_verified<Root>(
        std::span<const std::byte>(reinterpret_cast<const std::byte*>(wire.data()), wire.size()),
        XBuffer::maximum_capacity(protocol::kControlCap));
    if (!b) return Error{b.error().message};
    return std::move(*b);
}

struct Peer {
    std::string name;
    std::string build;
    std::uint32_t flags = 0;
    std::uint32_t max_frame = 0;
};

inline std::string peer_info_wire(const Peer& p) {
    XBuffer b = XBuffer::create<PeerInfo>(1024, control_cap());
    auto& r = b.root<PeerInfo>();
    r.name = std::string_view(p.name);
    r.build = std::string_view(p.build);
    r.flags = p.flags;
    r.max_frame = p.max_frame;
    return b.save_verified<PeerInfo>();
}

inline Result<Peer> read_peer_info(std::string_view wire) {
    Result<XBuffer> b = load_control<PeerInfo>(wire);
    if (!b) return std::move(b).error();
    auto& r = b->root<PeerInfo>();
    return Peer{std::string(r.name.view()), std::string(r.build.view()), r.flags, r.max_frame};
}

struct WelcomeInfo {
    std::uint32_t device_id = 0;
    std::string hub_build;
    std::string run_label;
};

inline std::string welcome_wire(const WelcomeInfo& w) {
    XBuffer b = XBuffer::create<Welcome>(1024, control_cap());
    auto& r = b.root<Welcome>();
    r.device_id = w.device_id;
    r.reserved0 = 0;
    r.hub_build = std::string_view(w.hub_build);
    r.run_label = std::string_view(w.run_label);
    return b.save_verified<Welcome>();
}

inline Result<WelcomeInfo> read_welcome(std::string_view wire) {
    Result<XBuffer> b = load_control<Welcome>(wire);
    if (!b) return std::move(b).error();
    auto& r = b->root<Welcome>();
    if (r.device_id == protocol::kHubId) return Error{"Welcome: device id 0"};
    return WelcomeInfo{r.device_id, std::string(r.hub_build.view()), std::string(r.run_label.view())};
}

struct RosterInfo {
    struct Entry {
        std::uint32_t id = 0;
        std::uint32_t flags = 0;
        std::string name;
        std::string build;
    };
    std::uint64_t version = 0;
    std::vector<Entry> devices;
};

inline std::string roster_wire(const RosterInfo& info) {
    XBuffer b = XBuffer::create<Roster>(4096, control_cap());
    auto& r = b.root<Roster>();
    r.version = info.version;
    r.reserved0 = 0;
    r.devices.reserve(std::uint32_t(info.devices.size()));
    for (const auto& d : info.devices) {
        auto& e = r.devices.emplace_back();
        e.id = d.id;
        e.flags = d.flags;
        e.name = std::string_view(d.name);
        e.build = std::string_view(d.build);
    }
    return b.save_verified<Roster>();
}

inline Result<RosterInfo> read_roster(std::string_view wire) {
    Result<XBuffer> b = load_control<Roster>(wire);
    if (!b) return std::move(b).error();
    auto& r = b->root<Roster>();
    RosterInfo out;
    out.version = r.version;
    for (const auto& e : r.devices)
        out.devices.push_back({e.id, e.flags, std::string(e.name.view()), std::string(e.build.view())});
    return out;
}

} // namespace xmesh::control
