// SC-8: the non-throwing receive entries (try_load_verified,
// try_reload_verified, try_begin_verified_reload / try_finish and their
// TypedXBuffer forms). Built twice: as test_no_exceptions with exceptions on,
// where every rejection must equal what the throwing entry throws (code and
// message), and as test_no_exceptions_noexc with -fno-exceptions (MSVC
// /EHs-c-), where the same rejections must come back with the same codes and
// messages and the buffer must be left as the throwing entry leaves it.
//
// usage: test_no_exceptions            the checks below
//        test_no_exceptions --fatal    typed access without a root: exceptions
//                                      on, it throws; off, the fatal handler
//                                      runs (prints "fatal no_root: ...")
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <string_view>
#include "../xoffsetdatastructure.hpp"
using namespace XOffsetDatastructure;

struct NoExcRoot {
    std::uint64_t id{};
    XString name;
    XVector<std::uint32_t> values;
    XMap<std::uint32_t, XString> labels;
};
XOFFSET_REGISTER_SCHEMA_NAME(NoExcRoot, "test.NoExcRoot")

struct NoExcOther { std::uint32_t x{}; };
XOFFSET_REGISTER_SCHEMA_NAME(NoExcOther, "test.NoExcOther")

namespace {
int failures = 0;
void need(bool ok, const std::string& what) {
    if (!ok) {
        std::cerr << "FAIL: " << what << "\n";
        ++failures;
    }
}

constexpr std::size_t kMax = 1u << 22;
auto cap() { return XBuffer::maximum_capacity(kMax).with_growth(XGrowPolicy::Automatic); }

std::string make_wire(std::uint64_t id, std::size_t n) {
    auto b = XBuffer::create<NoExcRoot>(4096, cap());
    auto& r = b.root<NoExcRoot>();
    r.id = id;
    r.name = "no-exceptions-" + std::to_string(id) + std::string(40, 'x');
    for (std::size_t i = 0; i < n; ++i) r.values.push_back(std::uint32_t(id * 1000 + i));
    for (std::uint32_t k = 0; k < 4; ++k) r.labels[k] = "label " + std::to_string(k) + std::string(30, 'y');
    return b.save_verified<NoExcRoot>();
}

XWireHeaderV2 header_of(const std::string& w) {
    XWireHeaderV2 h{};
    std::memcpy(&h, w.data(), sizeof(h));
    return h;
}
std::string with_header(std::string w, const XWireHeaderV2& h) {
    std::memcpy(w.data(), &h, sizeof(h));
    return w;
}

bool holds(XBuffer& b, std::uint64_t id, std::size_t n) {
    if (!b.has_root<NoExcRoot>()) return false;
    const auto& r = b.root<NoExcRoot>();
    return r.id == id && r.values.size() == n && r.labels.size() == 4 &&
           std::string_view(r.name.data(), r.name.size()).starts_with("no-exceptions-");
}

struct Rejection {
    const char* name;
    std::string wire;
    XErrc code;
    const char* message; // exact; nullptr: the code alone is pinned
};

std::string describe(const XError& e) { return std::string(to_string(e.code)) + " (" + e.message + ")"; }

void check_error(const char* where, const Rejection& r, const XError& e) {
    need(e.code == r.code, std::string(where) + " " + r.name + ": " + describe(e) + ", expected " + to_string(r.code));
    if (r.message) need(e.message == r.message, std::string(where) + " " + r.name + ": message \"" + e.message + "\"");
    need(!e.message.empty(), std::string(where) + " " + r.name + ": empty message");
}

#if !XOFFSET_NO_EXCEPTIONS
// The throwing entry's code and message for the same input.
template <class F>
XError thrown_by(F&& f) {
    try {
        f();
    } catch (const XException& e) {
        return {e.code(), e.what()};
    }
    return {};
}
#endif

void load_rejections(const std::vector<Rejection>& cases) {
    for (const auto& r : cases) {
        auto loaded = XBuffer::try_load_verified<NoExcRoot>(r.wire, cap());
        need(!loaded, std::string("try_load_verified accepted ") + r.name);
        if (loaded) continue;
        check_error("try_load_verified", r, loaded.error());
#if !XOFFSET_NO_EXCEPTIONS
        const auto thrown = thrown_by([&] { (void)XBuffer::load_verified<NoExcRoot>(r.wire, cap()); });
        need(thrown.code == loaded.code() && thrown.message == loaded.error().message,
             std::string("load_verified threw ") + describe(thrown) + " for " + r.name);
#endif
        auto typed = TypedXBuffer<NoExcRoot>::try_load_verified(r.wire, cap());
        need(!typed && typed.code() == loaded.code() && typed.error().message == loaded.error().message,
             std::string("TypedXBuffer::try_load_verified differs for ") + r.name);
    }
}

void reload_rejections(const std::vector<Rejection>& cases, const std::string& good) {
    auto target = XBuffer::create<NoExcRoot>(4096, cap());
    for (const auto& r : cases) {
        need(bool(target.try_reload_verified<NoExcRoot>(good)) && holds(target, 7, 300),
             std::string("reload of the good wire before ") + r.name);
        const auto status = target.try_reload_verified<NoExcRoot>(r.wire);
        need(!status, std::string("try_reload_verified accepted ") + r.name);
        if (status) continue;
        // A wire shorter than 4 bytes or not ours is refused the same way;
        // after any refusal the buffer is empty and has no root.
        check_error("try_reload_verified", r, status.error());
        need(!target.has_root<NoExcRoot>(), std::string("root after a refused reload of ") + r.name);
#if !XOFFSET_NO_EXCEPTIONS
        auto other = XBuffer::create<NoExcRoot>(4096, cap());
        const auto thrown = thrown_by([&] { other.reload_verified<NoExcRoot>(r.wire); });
        need(thrown.code == status.code() && thrown.message == status.error().message,
             std::string("reload_verified threw ") + describe(thrown) + " for " + r.name);
#endif
    }
    // The same buffer accepts again after every refusal.
    need(bool(target.try_reload_verified<NoExcRoot>(good)) && holds(target, 7, 300), "reload after refusals");
    TypedXBuffer<NoExcRoot> typed(XBuffer::create<NoExcRoot>(4096, cap()));
    need(bool(typed.try_reload_verified(good)) && holds(typed, 7, 300), "TypedXBuffer::try_reload_verified");
    need(!typed.try_reload_verified(cases.front().wire) && !typed.has_root(),
         "TypedXBuffer::try_reload_verified refusal");
}

// Direct reload delivered from a whole wire (dense: no extent table).
XStatus direct(XBuffer& target, const std::string& w, bool complete_all = true) {
    const auto h = header_of(w);
    auto begun = target.try_begin_verified_reload<NoExcRoot>(h.used_bytes, h.hole_count, h.extent_count);
    if (!begun) return begun.error();
    auto& token = *begun;
    std::memcpy(token.header_bytes().data(), w.data(), sizeof(XWireHeaderV2));
    const auto payload = token.payload_bytes();
    const auto holes = token.hole_bytes();
    if (sizeof(XWireHeaderV2) + payload.size() + holes.size() != w.size())
        return XError{XErrc::size_mismatch, "test: wire length"};
    std::memcpy(payload.data(), w.data() + sizeof(XWireHeaderV2), payload.size());
    if (!holes.empty()) std::memcpy(holes.data(), w.data() + sizeof(XWireHeaderV2) + payload.size(), holes.size());
    token.complete(sizeof(XWireHeaderV2), complete_all ? payload.size() : payload.size() - 8, holes.size(), 0);
    return token.try_finish();
}

void direct_reload(const std::string& good, const std::string& corrupt) {
    auto target = XBuffer::create<NoExcRoot>(4096, cap());
    need(bool(direct(target, good)) && holds(target, 7, 300), "direct reload of the good wire");
    const auto refused = direct(target, corrupt);
    need(!refused && refused.code() == XErrc::invalid_payload, "direct reload of a corrupt payload: " +
                                                                   describe(refused.error()));
    need(!target.has_root<NoExcRoot>(), "root after a refused direct reload");
    const auto incomplete = direct(target, good, false);
    need(!incomplete && incomplete.code() == XErrc::incomplete_input &&
             incomplete.error().message == "verified reload: incomplete input",
         "direct reload with a short delivery: " + describe(incomplete.error()));
    need(bool(direct(target, good)) && holds(target, 7, 300), "direct reload after refusals");
    // Counts that cannot fit the payload are refused by begin.
    auto begun = target.try_begin_verified_reload<NoExcRoot>(64, 9, 0);
    need(!begun && begun.code() == XErrc::size_mismatch &&
             begun.error().message == "reload_verified<T>(): used size mismatch",
         "try_begin_verified_reload with too many holes");
    {
        auto open = target.try_begin_verified_reload<NoExcRoot>(64);
        need(bool(open), "try_begin_verified_reload");
        auto again = target.try_begin_verified_reload<NoExcRoot>(64);
        need(!again && again.code() == XErrc::invalid_state, "a second open direct reload");
    } // the token is dropped: the reload is abandoned
    need(!target.has_root<NoExcRoot>(), "root after an abandoned direct reload");
}

[[noreturn]] void print_fatal(XErrc code, const char* message) {
    std::printf("fatal %s: %s\n", to_string(code), message);
    std::fflush(stdout);
    std::_Exit(0);
}

int fatal_mode() {
    set_fatal_handler(&print_fatal);
    auto b = XBuffer::create<NoExcRoot>(4096, cap());
    (void)b.try_reload_verified<NoExcRoot>(std::string("not a wire"));
#if XOFFSET_NO_EXCEPTIONS
    (void)b.root<NoExcRoot>(); // the handler ends the process
    std::puts("root<T>() returned without a root");
    return 1;
#else
    try {
        (void)b.root<NoExcRoot>();
    } catch (const XException& e) {
        std::printf("fatal %s: %s\n", to_string(e.code()), e.what());
        return 0;
    }
    return 1;
#endif
}
} // namespace

int main(int argc, char** argv) {
    if (argc > 1 && std::string_view(argv[1]) == "--fatal") return fatal_mode();
    std::cout << "test_no_exceptions: exceptions " << (XOFFSET_NO_EXCEPTIONS ? "off" : "on") << "\n";

    const auto good = make_wire(7, 300);
    {
        auto loaded = XBuffer::try_load_verified<NoExcRoot>(good, cap());
        need(bool(loaded) && loaded.code() == XErrc::ok, "try_load_verified of a good wire");
        if (loaded) {
            need(holds(*loaded, 7, 300), "loaded content");
            need(loaded->save_verified<NoExcRoot>() == good, "loaded re-save");
        }
        auto typed = TypedXBuffer<NoExcRoot>::try_load_verified(good, cap());
        need(bool(typed) && holds(*typed, 7, 300), "TypedXBuffer::try_load_verified of a good wire");
        auto view = XBuffer::try_load_verified<NoExcRoot>(
            std::span<const std::byte>(reinterpret_cast<const std::byte*>(good.data()), good.size()));
        need(bool(view), "try_load_verified(span) of a good wire");
    }

    const auto h = header_of(good);
    std::string corrupt = good;
    {
        // The name's heap block offset (after id, at the root) pointed
        // outside the payload.
        const auto at = sizeof(XWireHeaderV2) + (h.root_offset - XWIRE_PAYLOAD_BEGIN) + 8;
        std::memset(corrupt.data() + at, 0x7f, 16);
    }
    auto magic = good; magic[0] = 'Q';
    auto v1 = good; std::memcpy(v1.data(), "XOFFV1\0\0", 8);
    auto flags = h; flags.flags = 0x10;
    auto used = h; used.used_bytes += 4;
    auto root = h; root.root_offset = used.used_bytes + 64;
    auto schema = h; schema.schema_hash ^= 1;
    auto type = h; type.root_type_id ^= 1;

    const std::vector<Rejection> cases = {
        {"truncated", good.substr(0, 20), XErrc::truncated, "load_verified<T>(): truncated wire header"},
        {"bad magic", magic, XErrc::bad_magic, "load_verified<T>(): not an XOffset wire (bad magic)"},
        {"format 1", v1, XErrc::unsupported_version,
         "load_verified<T>(): wire format 1 is not supported; this build reads 2.1"},
        {"flags", with_header(good, flags), XErrc::unsupported_flags,
         "load_verified<T>(): unsupported wire flags 0x00000010"},
        {"used size", with_header(good, used), XErrc::size_mismatch, "load_verified<T>(): used size mismatch"},
        {"root offset", with_header(good, root), XErrc::root_offset_out_of_bounds,
         "load_verified<T>(): root offset out of bounds"},
        {"schema", with_header(good, schema), XErrc::schema_mismatch, nullptr},
        {"root type", with_header(good, type), XErrc::root_type_mismatch, nullptr},
        {"payload", corrupt, XErrc::invalid_payload, nullptr},
    };
    load_rejections(cases);

    // reload_verified names itself in its messages.
    std::vector<Rejection> reload_cases;
    for (const auto& r : cases) {
        Rejection c = r;
        c.message = nullptr;
        reload_cases.push_back(c);
    }
    reload_rejections(reload_cases, good);
    direct_reload(good, corrupt);

    // A loaded result of another root type is refused like load_verified.
    {
        auto other = XBuffer::create<NoExcOther>(4096, cap());
        other.root<NoExcOther>().x = 5;
        const auto wire = other.save_verified<NoExcOther>();
        auto loaded = XBuffer::try_load_verified<NoExcRoot>(wire, cap());
        need(!loaded && loaded.code() == XErrc::root_type_mismatch, "another root type: " + describe(loaded.error()));
        need(bool(XBuffer::try_load_verified<NoExcOther>(wire, cap())), "the other root type loads as itself");
    }
    // More than the receiver's maximum capacity.
    {
        auto loaded = XBuffer::try_load_verified<NoExcRoot>(good, XBuffer::maximum_capacity(512));
        need(!loaded && loaded.code() == XErrc::capacity_exceeded &&
                 loaded.error().message == "load_verified<T>(): wire used_bytes " + std::to_string(h.used_bytes) +
                                               " exceeds maximum_capacity 512",
             "capacity: " + describe(loaded.error()));
#if !XOFFSET_NO_EXCEPTIONS
        const auto thrown = thrown_by([&] { (void)XBuffer::load_verified<NoExcRoot>(good, XBuffer::maximum_capacity(512)); });
        need(thrown.code == loaded.code() && thrown.message == loaded.error().message, "capacity: load_verified threw " +
                                                                                           describe(thrown));
#endif
    }
    // A null upstream is an argument error, not a payload error.
    {
        auto loaded = XBuffer::try_load_verified_with_resource<NoExcRoot>(
            std::span<const std::byte>(reinterpret_cast<const std::byte*>(good.data()), good.size()), cap(),
            nullptr);
        need(!loaded && loaded.code() == XErrc::invalid_argument &&
                 loaded.error().message == "validation scratch: null upstream",
             "null upstream: " + describe(loaded.error()));
    }

    if (failures) {
        std::cerr << "test_no_exceptions: " << failures << " failure(s)\n";
        return 1;
    }
    std::cout << "test_no_exceptions: " << cases.size() << " load rejections, " << reload_cases.size()
              << " reload rejections, direct reload: ok\n";
    return 0;
}
