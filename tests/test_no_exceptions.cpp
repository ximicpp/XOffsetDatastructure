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
#include <limits>
#include <span>
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
XOFFSET_RECORD_DERIVED(NoExcRoot, (), id, name, values, labels)
XOFFSET_REGISTER_SCHEMA_NAME(NoExcRoot, "test.NoExcRoot")

// Floating-point keys and a vector for the limit checks (limit_entries).
struct LimitRoot {
    XSet<double> keys;
    XMap<float, std::int32_t> table;
    XVector<std::int64_t> values;
};
XOFFSET_RECORD_DERIVED(LimitRoot, (), keys, table, values)

struct NoExcOther { std::uint32_t x{}; };
XOFFSET_RECORD_DERIVED(NoExcOther, (), x)
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

XWireHeader header_of(const std::string& w) {
    XWireHeader h{};
    std::memcpy(&h, w.data(), sizeof(h));
    return h;
}
std::string with_header(std::string w, const XWireHeader& h) {
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

// Direct reload delivered from a whole wire (dense: the image, no extent
// table).
XStatus direct(XBuffer& target, const std::string& w, bool complete_all = true) {
    const auto h = header_of(w);
    auto begun = target.try_begin_verified_reload<NoExcRoot>(h.used_bytes, h.extent_count);
    if (!begun) return begun.error();
    auto& token = *begun;
    const auto image = token.image_bytes();
    if (image.size() != w.size()) return XError{XErrc::size_mismatch, "test: wire length"};
    std::memcpy(image.data(), w.data(), image.size());
    token.complete(complete_all ? image.size() : image.size() - 8, 0);
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
    auto begun = target.try_begin_verified_reload<NoExcRoot>(64, 9);
    need(!begun && begun.code() == XErrc::size_mismatch &&
             begun.error().message == "reload_verified<T>(): used size mismatch",
         "try_begin_verified_reload with too many extents");
    {
        auto open = target.try_begin_verified_reload<NoExcRoot>(64);
        need(bool(open), "try_begin_verified_reload");
        auto again = target.try_begin_verified_reload<NoExcRoot>(64);
        need(!again && again.code() == XErrc::invalid_state, "a second open direct reload");
    } // the token is dropped: the reload is abandoned
    need(!target.has_root<NoExcRoot>(), "root after an abandoned direct reload");
}

void create_and_clone_entries(const std::string& good) {
    // try_create happy paths (default capacity, explicit capacity, Relocating).
    {
        auto b = XBuffer::try_create<NoExcRoot>(4096);
        need(bool(b) && b->has_root<NoExcRoot>(), "XBuffer::try_create default");
        auto tb = TypedXBuffer<NoExcRoot>::try_create(4096, cap());
        need(bool(tb) && tb->has_root(), "TypedXBuffer::try_create explicit cap");
        if (tb) {
            const bool edited = tb->edit([](NoExcRoot& r) {
                r.id = 42;
                r.name = "created-noexc";
                r.values.push_back(11);
                r.values.push_back(22);
            });
            need(edited && tb->ok(), "TypedXBuffer::try_create edit");
            need(tb->plain_root().id == 42 && tb->plain_root().values.size() == 2,
                 "TypedXBuffer::try_create edited content");
        }
        auto rel = TypedXBuffer<NoExcRoot>::try_create(4096, XBuffer::relocating(1u << 20));
        need(bool(rel) && rel->has_root(), "TypedXBuffer::try_create relocating");
    }

    // try_create failure modes: payload ceiling, max < initial, invalid storage request, root too large for cap.
    {
        auto over = XBuffer::try_create<NoExcRoot>(std::size_t(XBuffer::MAX_PAYLOAD) + 8);
        need(!over && over.code() == XErrc::capacity_exceeded,
             "try_create above MAX_PAYLOAD: " + describe(over.error()));

        auto small_max = XBuffer::try_create<NoExcRoot>(4096, XBuffer::maximum_capacity(1024));
        need(!small_max && small_max.code() == XErrc::capacity_exceeded,
             "try_create max < initial: " + describe(small_max.error()));

        auto root_overflow = TypedXBuffer<NoExcRoot>::try_create(
            40, XBuffer::maximum_capacity(48).with_growth(XGrowPolicy::Automatic));
        need(!root_overflow && root_overflow.code() == XErrc::capacity_exceeded,
             "try_create root exceeds max: " + describe(root_overflow.error()));

        auto root_explicit = TypedXBuffer<NoExcRoot>::try_create(
            40, XBuffer::maximum_capacity(4096).with_growth(XGrowPolicy::Explicit));
        need(!root_explicit && root_explicit.code() == XErrc::out_of_storage,
             "try_create root exceeds explicit initial size: " + describe(root_explicit.error()));

        auto bad_storage = XBuffer::try_create<NoExcRoot>(
            4096, XBuffer::maximum_capacity(1u << 20).with_growth(XGrowPolicy::Automatic).with_storage(XStorage::Movable));
        need(!bad_storage && bad_storage.code() == XErrc::invalid_argument,
             "try_create invalid Movable storage request: " + describe(bad_storage.error()));
    }

    // try_clone and try_clone_into happy paths + failure modes.
    {
        auto src = TypedXBuffer<NoExcRoot>::try_load_verified(good, cap());
        need(bool(src), "try_load_verified for clone source");
        if (!src) return;

        auto copy = src->try_clone();
        need(bool(copy) && holds(*copy, 7, 300), "TypedXBuffer::try_clone content");
        if (copy) {
            need(copy->save_verified() == good, "TypedXBuffer::try_clone wire equality");
            // Mutate clone independently; source stays unchanged.
            need(copy->edit([](NoExcRoot& r) { r.id = 99; }), "edit cloned buffer");
            need(src->plain_root().id == 7 && copy->plain_root().id == 99, "clone independence");
        }

        auto dst_same = TypedXBuffer<NoExcRoot>::try_create(4096, cap());
        need(bool(dst_same), "dst_same try_create");
        if (dst_same) {
            const XStatus st = src->try_clone_into(*dst_same);
            need(bool(st) && holds(*dst_same, 7, 300), "TypedXBuffer::try_clone_into general");
            // Second clone hits in-place fast path (same logical size).
            const XStatus st2 = src->try_clone_into(*dst_same);
            need(bool(st2) && holds(*dst_same, 7, 300), "TypedXBuffer::try_clone_into in-place");
            need(dst_same->save_verified() == good, "try_clone_into wire equality");
        }

        // Self-clone fails with invalid_state and leaves source untouched.
        const XStatus self = src->try_clone_into(*src);
        need(!self && self.code() == XErrc::invalid_state && holds(*src, 7, 300),
             "try_clone_into self: " + describe(self.error()));
#if !XOFFSET_NO_EXCEPTIONS
        const auto thrown_self = thrown_by([&] { src->clone_into(*src); });
        need(thrown_self.code == self.code() && thrown_self.message == self.error().message,
             "clone_into self parity: " + describe(thrown_self));
#endif

        // Cloning an empty (no-arena) buffer fails cleanly.
        XBuffer empty;
        auto empty_clone = empty.try_clone();
        need(!empty_clone && empty_clone.code() == XErrc::invalid_state,
             "try_clone empty: " + describe(empty_clone.error()));
        if (dst_same) {
            const XStatus from_empty = empty.try_clone_into(*dst_same);
            need(!from_empty && from_empty.code() == XErrc::invalid_state && !dst_same->has_root(),
                 "try_clone_into from empty: " + describe(from_empty.error()));
        }

        // Destination capacity smaller than source used_end fails and resets dst.
        auto small_dst = TypedXBuffer<NoExcRoot>::try_create(256, XBuffer::maximum_capacity(512));
        need(bool(small_dst), "small_dst try_create");
        if (small_dst) {
            const XStatus too_big = src->try_clone_into(*small_dst);
            need(!too_big && too_big.code() == XErrc::capacity_exceeded && !small_dst->has_root(),
                 "try_clone_into exceeding dst max: " + describe(too_big.error()));
        }
    }
}

void save_delta_and_compact_entries(const std::string& good) {
    // ref<T>() and edit<T>() on empty/unverified/wrong-type buffers must not abort.
    {
        auto empty = XBuffer::try_create<NoExcRoot>(4096, cap());
        need(bool(empty), "empty setup");
        if (empty) {
            (void)empty->try_reload_verified<NoExcRoot>(std::string("bad"));
            auto r = empty->ref<NoExcRoot>();
            need(!r, "ref<T>() on buffer without root returns empty proxy");
            const bool edited = empty->edit<NoExcRoot>([](NoExcRoot& x) { x.id = 1; });
            need(!edited && empty->error() == XErrc::no_root, "edit<T>() without root records no_root");
        }
        auto other = TypedXBuffer<NoExcOther>::try_create(4096, cap());
        need(bool(other), "other setup");
        if (other) {
            auto wrong_ref = other->XBuffer::ref<NoExcRoot>();
            need(!wrong_ref, "ref<WrongT>() returns empty proxy");
            const bool wrong_edit = other->XBuffer::edit<NoExcRoot>([](NoExcRoot& x) { x.id = 2; });
            need(!wrong_edit && other->error() == XErrc::no_root, "edit<WrongT>() records no_root");
        }
    }

    // try_save_verified / try_save_compact / try_save_delta happy paths and rejections.
    {
        auto buf = TypedXBuffer<NoExcRoot>::try_load_verified(good, cap());
        need(bool(buf), "save/delta setup load");
        if (buf) {
            auto w1 = buf->try_save_verified();
            need(bool(w1) && *w1 == good, "TypedXBuffer::try_save_verified roundtrip");

            std::string w2;
            const XStatus s2 = buf->try_save_verified_into(w2);
            need(bool(s2) && w2 == good, "TypedXBuffer::try_save_verified_into roundtrip");

            auto c1 = buf->try_save_compact();
            need(bool(c1), "TypedXBuffer::try_save_compact ok");
            if (c1) {
                auto reloaded = TypedXBuffer<NoExcRoot>::try_load_verified(*c1, cap());
                need(bool(reloaded) && holds(*reloaded, 7, 300), "compact wire reloads identically");
            }

            std::string c2;
            const XStatus sc2 = buf->try_save_compact_into(c2);
            need(bool(sc2) && c1 && c2 == *c1, "TypedXBuffer::try_save_compact_into matches");

            // Composed try_save_compact_into: valid assignment and invalid out-of-root target.
            std::uint64_t override_id = 99;
            std::string composed;
            const XStatus sc_ok = buf->try_save_compact_into(
                composed, XCompactOptions{}, xassign(buf->plain_root().id, override_id));
            need(bool(sc_ok), "composed try_save_compact_into ok");
            if (sc_ok) {
                auto reloaded = TypedXBuffer<NoExcRoot>::try_load_verified(composed, cap());
                need(bool(reloaded) && holds(*reloaded, 99, 300), "composed wire carries override");
            }
            std::uint64_t foreign_target = 0;
            std::string bad_composed = "stale";
            const XStatus sc_bad = buf->try_save_compact_into(
                bad_composed, XCompactOptions{}, xassign(foreign_target, override_id));
            need(!sc_bad && sc_bad.code() == XErrc::invalid_argument && bad_composed.empty(),
                 "composed bad target rejected cleanly: " + describe(sc_bad.error()));

            // Wrong schema type rejection on try_save_verified / try_save_compact / try_save_delta.
            auto wrong_save = buf->XBuffer::try_save_verified<NoExcOther>();
            need(!wrong_save && wrong_save.code() == XErrc::schema_mismatch,
                 "try_save_verified wrong type: " + describe(wrong_save.error()));
            auto wrong_compact = buf->XBuffer::try_save_compact<NoExcOther>();
            need(!wrong_compact && wrong_compact.code() == XErrc::schema_mismatch,
                 "try_save_compact wrong type: " + describe(wrong_compact.error()));

            // Delta sync non-throwing emit + key_delta + receiver apply.
            XDeltaBaseline base;
            auto empty_key = base.try_key_delta();
            need(!empty_key && empty_key.code() == XErrc::invalid_state,
                 "empty baseline try_key_delta: " + describe(empty_key.error()));

            auto wrong_delta = buf->XBuffer::try_save_delta<NoExcOther>(base);
            need(!wrong_delta && wrong_delta.code() == XErrc::schema_mismatch,
                 "try_save_delta wrong type: " + describe(wrong_delta.error()));

            std::string delta1;
            auto ds1 = buf->try_save_delta_into(delta1, base);
            need(bool(ds1) && ds1->key && !delta1.empty(), "try_save_delta_into key delta");

            auto late_key = base.try_key_delta();
            need(bool(late_key) && !late_key->empty(), "baseline try_key_delta after first delta");

            auto rx = TypedXBuffer<NoExcRoot>::try_create(4096, cap());
            need(bool(rx), "delta rx create");
            XDeltaCursor cursor;
            if (rx && ds1) {
                need(bool(rx->try_apply_delta_verified(delta1, cursor)), "apply key delta");
                need(holds(*rx, 7, 300), "rx holds initial state");
                buf->edit([](NoExcRoot& r) { r.id = 123; });
                std::vector<char> span_buf(4096);
                auto dn = buf->try_save_delta_into(std::span<char>(span_buf), base);
                need(bool(dn) && *dn > 0 && *dn <= span_buf.size(), "try_save_delta_into(span) incremental");
                if (dn) {
                    need(bool(rx->try_apply_delta_verified(
                             std::span<const std::byte>(reinterpret_cast<const std::byte*>(span_buf.data()), *dn),
                             cursor)),
                         "apply incremental span delta");
                    need(holds(*rx, 123, 300), "rx holds updated state");
                }
            }

            // XCompactor::try_compact and compact (must not abort in -fno-exceptions builds).
            // Fragment the buffer first by replacing string/vector repeatedly.
            buf->edit([](NoExcRoot& r) {
                for (int i = 0; i < 6; ++i) {
                    r.name = std::string(120 + static_cast<std::size_t>(i * 17), 'x');
                    r.values.push_back(static_cast<std::uint32_t>(i));
                }
            });
            auto compacted = XCompactor::try_compact<NoExcRoot>(*buf);
            need(bool(compacted), "XCompactor::try_compact ok");
            if (compacted) {
                auto cw = compacted->save_verified<NoExcRoot>();
                auto loaded_c = TypedXBuffer<NoExcRoot>::try_load_verified(cw, cap());
                need(bool(loaded_c) && loaded_c->plain_root().id == 123 &&
                     loaded_c->plain_root().values.size() == 306,
                     "compacted buffer preserves content");
            }
            // Empty buffer (no root) compacts to empty buffer without aborting.
            auto no_root_buf = XBuffer::try_create<NoExcRoot>(4096, cap());
            if (no_root_buf) {
                (void)no_root_buf->try_reload_verified<NoExcRoot>(std::string("bad"));
                auto empty_compacted = XCompactor::try_compact<NoExcRoot>(*no_root_buf);
                need(bool(empty_compacted) && !empty_compacted->has_root<NoExcRoot>(),
                     "XCompactor::try_compact on empty buffer");
            }
        }
    }

    // Unverified buffer and sticky-error buffer must be rejected cleanly by try_save_* / try_save_delta_*.
    {
        auto unverified = XBuffer::load_unverified(good, cap());
        auto s1 = unverified.try_save_verified<NoExcRoot>();
        need(!s1 && s1.code() == XErrc::no_root, "unverified try_save_verified: " + describe(s1.error()));
        auto s2 = unverified.try_save_compact<NoExcRoot>();
        need(!s2 && s2.code() == XErrc::no_root, "unverified try_save_compact: " + describe(s2.error()));
        XDeltaBaseline b;
        auto s3 = unverified.try_save_delta<NoExcRoot>(b);
        need(!s3 && s3.code() == XErrc::no_root, "unverified try_save_delta: " + describe(s3.error()));

        auto tight = TypedXBuffer<NoExcRoot>::try_create(256, XBuffer::maximum_capacity(512));
        need(bool(tight), "tight try_create");
        if (tight) {
            const bool ok = tight->edit([](NoExcRoot& r) { r.values.resize(400); });
            need(!ok && !tight->ok(), "tight edit records sticky failure");
            std::string out = "keep";
            const XStatus sv = tight->try_save_verified_into(out);
            need(!sv && sv.code() == tight->error() && out.empty(),
                 "sticky-error try_save_verified_into: " + describe(sv.error()));
            out = "keep";
            const XStatus sc = tight->try_save_compact_into(out);
            need(!sc && sc.code() == tight->error() && out.empty(),
                 "sticky-error try_save_compact_into: " + describe(sc.error()));
            out = "keep";
            auto sd = tight->try_save_delta_into(out, b);
            need(!sd && sd.code() == tight->error() && out.empty(),
                 "sticky-error try_save_delta_into: " + describe(sd.error()));
        }
    }
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

// USER_GUIDE section 5.1: a NaN key is refused with invalid_argument and
// nothing is inserted; a request past maximum_capacity or a container's
// max_size() reports capacity_exceeded; an Explicit buffer that is full below
// maximum_capacity reports out_of_storage. Proxies and edit() scopes record the
// same codes with exceptions on and off; with exceptions on, plain references
// throw them.
static void limit_entries() {
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const float fnan = std::numeric_limits<float>::quiet_NaN();
    auto tb = TypedXBuffer<LimitRoot>::create(
        4096, XBuffer::maximum_capacity(1u << 16).with_growth(XGrowPolicy::Automatic));
    auto p = tb.root();

    need(!p.keys().insert(nan) && tb.error() == XErrc::invalid_argument && p.keys().size() == 0,
         "proxy set insert NaN");
    tb.clear_error();
    const double batch[] = {1.0, nan, 2.0};
    need(p.keys().insert(std::span<const double>(batch, 3)) == 0 && tb.error() == XErrc::invalid_argument &&
             p.keys().size() == 0,
         "proxy set batch insert with a NaN");
    tb.clear_error();
    need(!p.table().try_emplace(fnan, 1) && tb.error() == XErrc::invalid_argument && p.table().size() == 0,
         "proxy map try_emplace NaN");
    tb.clear_error();
    need(!p.table().insert_or_assign(-fnan, 1) && tb.error() == XErrc::invalid_argument && p.table().size() == 0,
         "proxy map insert_or_assign NaN");
    tb.clear_error();
    (void)p.table()[fnan];
    need(tb.error() == XErrc::invalid_argument && p.table().size() == 0, "proxy map operator[] NaN");
    tb.clear_error();
    need(!tb.edit([&](LimitRoot& r) { r.keys.insert(nan); }) && tb.error() == XErrc::invalid_argument &&
             tb.plain_root().keys.size() == 0,
         "edit set insert NaN");
    tb.clear_error();
    need(!tb.edit([&](LimitRoot& r) { r.table.insert_or_assign(fnan, 1); }) &&
             tb.error() == XErrc::invalid_argument && tb.plain_root().table.size() == 0,
         "edit map insert_or_assign NaN");
    tb.clear_error();
    // -0.0 and +0.0 are one key; an infinity is an ordinary key.
    need(p.keys().insert(0.0) && !p.keys().insert(-0.0) && p.keys().insert(std::numeric_limits<double>::infinity()) &&
             p.keys().size() == 2 && tb.ok(),
         "signed zeros are one key, infinity is a key");

    need(!p.values().reserve(1u << 20) && tb.error() == XErrc::capacity_exceeded,
         "proxy reserve past maximum_capacity: " + std::string(to_string(tb.error())));
    tb.clear_error();
    need(!tb.edit([](LimitRoot& r) { r.values.reserve(1u << 20); }) && tb.error() == XErrc::capacity_exceeded,
         "edit reserve past maximum_capacity: " + std::string(to_string(tb.error())));
    tb.clear_error();
    need(!tb.edit([](LimitRoot& r) { r.values.reserve(std::size_t(XBufferCore::MAX_PAYLOAD) / sizeof(std::int64_t) + 1); }) &&
             tb.error() == XErrc::capacity_exceeded,
         "edit reserve past max_size: " + std::string(to_string(tb.error())));
    tb.clear_error();
    need(!tb.edit([](LimitRoot& r) {
             for (std::int64_t i = 0; i < (1 << 14); ++i) r.values.push_back(i);
         }) && tb.error() == XErrc::capacity_exceeded,
         "edit push_back past maximum_capacity: " + std::string(to_string(tb.error())));
    tb.clear_error();

    auto ex = TypedXBuffer<LimitRoot>::create(
        4096, XBuffer::maximum_capacity(1u << 16).with_growth(XGrowPolicy::Explicit));
    need(!ex.edit([](LimitRoot& r) { r.values.reserve(4096); }) && ex.error() == XErrc::out_of_storage,
         "Explicit buffer full below maximum_capacity: " + std::string(to_string(ex.error())));
    ex.clear_error();

#if !XOFFSET_NO_EXCEPTIONS
    auto& r = tb.plain_root();
    const auto set_nan = thrown_by([&] { r.keys.insert(nan); });
    need(set_nan.code == XErrc::invalid_argument, "plain set insert NaN threw " + describe(set_nan));
    const auto map_nan = thrown_by([&] { r.table[fnan] = 1; });
    need(map_nan.code == XErrc::invalid_argument, "plain map operator[] NaN threw " + describe(map_nan));
    const auto cap_past = thrown_by([&] { r.values.reserve(1u << 20); });
    need(cap_past.code == XErrc::capacity_exceeded, "plain reserve past maximum_capacity threw " + describe(cap_past));
    const auto size_past =
        thrown_by([&] { r.values.reserve(std::size_t(XBufferCore::MAX_PAYLOAD) / sizeof(std::int64_t) + 1); });
    need(size_past.code == XErrc::capacity_exceeded, "plain reserve past max_size threw " + describe(size_past));
    auto& er = ex.plain_root();
    const auto full = thrown_by([&] { er.values.reserve(4096); });
    need(full.code == XErrc::out_of_storage, "plain reserve in a full Explicit buffer threw " + describe(full));
#endif
    need(tb.plain_root().keys.size() == 2 && tb.plain_root().table.size() == 0,
         "nothing inserted by the refused calls");
}

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
        // outside the image (a dense wire is the image).
        const auto at = XWIRE_PAYLOAD_BEGIN + 8;
        std::memset(corrupt.data() + at, 0x7f, 16);
    }
    auto magic = good; magic[0] = 'Q';
    auto v1 = good; std::memcpy(v1.data(), "XOFFV1\0\0", 8);
    auto size = h; size.header_bytes = 48;
    auto used = h; used.used_bytes += 4;
    auto root = h; root.used_bytes = XWIRE_PAYLOAD_BEGIN;
    auto schema = h; schema.schema_hash ^= 1;
    auto type = h; type.root_type_id ^= 1;

    const std::vector<Rejection> cases = {
        {"truncated", good.substr(0, 20), XErrc::truncated, "load_verified<T>(): truncated wire header"},
        {"bad magic", magic, XErrc::bad_magic, "load_verified<T>(): not an XOffset wire (bad magic)"},
        {"format 1", v1, XErrc::unsupported_version,
         "load_verified<T>(): wire format 1 is not supported; this build reads 3.0"},
        {"header size", with_header(good, size), XErrc::unsupported_flags,
         "load_verified<T>(): unsupported header size 0x00000030"},
        {"used size", with_header(good, used), XErrc::size_mismatch, "load_verified<T>(): used size mismatch"},
        {"no root", with_header(good.substr(0, XWIRE_PAYLOAD_BEGIN), root), XErrc::root_offset_out_of_bounds,
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
    limit_entries();
    create_and_clone_entries(good);
    save_delta_and_compact_entries(good);

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
        auto loaded = XBuffer::try_load_verified<NoExcRoot>(
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
