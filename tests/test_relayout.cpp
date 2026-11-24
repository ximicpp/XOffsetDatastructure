// XBuffer::relayout<T>() and relayout_advised<T>(): a sender that edits its
// content in place across ticks rewrites it as save_compact's image, in place.
//   - delta streams of every baseline kind from one churned sender (copy
//     baselines comparing everything or touched_only, touches baselines, a
//     digest; two of each kind on the same buffer), verified and XReceiver
//     receivers: byte-identical to the sender after every tick, with forced
//     relayouts and relayouts the hint advises;
//   - the values stay (save_compact with slack 0 before and after), the image
//     is save_compact's (with its slack) or, when that is not smaller, the
//     plain one or nothing; proxies (the root's and nested ones) and XHandles
//     read the same values; edits after it give the content a mirror gets;
//   - refusals change nothing: a pin scope (edit, read, build), a recorded
//     failure, no verified root of T, an injected storage failure, a failed
//     scratch allocation;
//   - the old image's tail: zero with XOFFSET_SCRUB_RESET (the _hardened
//     build), otherwise remembered like a reset's and reused correctly;
//   - relayout_advised: false fresh, true once the content doubled its
//     compact size, false after a relayout, measured again for new content
//     (reload, swap, clone).
#include "xoffsetdatastructure.hpp"
#include "xoffset_receiver.hpp"

#if !XOFFSET_NO_EXCEPTIONS
#include "host_failpoint.hpp"
#endif

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <random>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace xo = XOffsetDatastructure;

namespace rl {
struct Skill { std::uint32_t id, level; };
struct Item { std::uint32_t id; std::uint32_t number; std::uint64_t uuid; std::uint64_t stamp; };
struct Position { float x, y, z; };
struct Equip {
    std::uint32_t id; std::uint32_t number; std::uint64_t uuid; std::uint64_t stamp; std::uint32_t level;
    xo::XVector<float> attributes;
};
struct Hero {
    std::uint64_t id; xo::XString name; std::uint32_t level; float hp, mp, speed; Position pos;
    xo::XMap<std::uint64_t, Item> items; xo::XVector<Position> trail; xo::XVector<float> attributes;
    xo::XVector<Equip> equips; xo::XVector<Skill> skills;
};
struct Other { std::uint64_t id; xo::XVector<std::uint64_t> values; };
} // namespace rl
XOFFSET_RECORD(rl::Skill, id, level)
XOFFSET_RECORD(rl::Item, id, number, uuid, stamp)
XOFFSET_RECORD(rl::Position, x, y, z)
XOFFSET_RECORD(rl::Equip, id, number, uuid, stamp, level, attributes)
XOFFSET_RECORD(rl::Hero, id, name, level, hp, mp, speed, pos, items, trail, attributes, equips, skills)
XOFFSET_RECORD(rl::Other, id, values)
XOFFSET_REGISTER_SCHEMA_NAME(rl::Hero, "test.relayout.Hero")
XOFFSET_REGISTER_SCHEMA_NAME(rl::Other, "test.relayout.Other")

namespace {
using rl::Hero;
using Buffer = xo::TypedXBuffer<Hero>;

[[noreturn]] void fail(const std::string& message) {
    std::cerr << "test_relayout FAILED: " << message << "\n";
    std::exit(1);
}
void need(bool ok, const std::string& what) { if (!ok) fail(what); }

auto cap() { return xo::XBuffer::maximum_capacity(std::size_t(1) << 26).with_growth(xo::XGrowPolicy::Automatic); }
constexpr xo::XCompactOptions kPlain{.slack_div = 0};

std::uint32_t used_of(const xo::XBuffer& b) { return b.arena()->used_end; }
std::string image_of(const xo::XBuffer& b) { return std::string(b.data(), used_of(b)); }
bool same_image(const xo::XBuffer& a, const xo::XBuffer& b) {
    return used_of(a) == used_of(b) && std::memcmp(a.data(), b.data(), used_of(a)) == 0;
}

std::size_t count(double base, double k) { return std::size_t(base * k + 0.5); }

Buffer make_state(double k, std::uint64_t seed = 1) {
    std::mt19937_64 g(seed);
    auto b = Buffer::create(16 * 1024, cap());
    need(b.edit([&](Hero& r) {
        r.id = g(); r.name = "abcdefgh"; r.level = 7; r.hp = 1; r.mp = 2; r.speed = 3; r.pos = {1, 2, 3};
        for (std::size_t i = 0; i < count(64, k); ++i) {
            const std::uint64_t key = g();
            r.items[key] = rl::Item{std::uint32_t(g()), std::uint32_t(g()), g(), g()};
        }
        for (std::size_t i = 0; i < count(16, k); ++i) r.trail.push_back(rl::Position{float(i), 1.f, 2.f});
        for (std::size_t i = 0; i < count(16, k); ++i) r.attributes.push_back(float(i));
        for (std::size_t i = 0; i < count(8, k); ++i) {
            auto& e = r.equips.emplace_back();
            e.id = std::uint32_t(g()); e.uuid = g(); e.level = 3;
            for (int a = 0; a < 8; ++a) e.attributes.push_back(float(a));
        }
        for (std::size_t i = 0; i < count(4, k); ++i) r.skills.push_back(rl::Skill{std::uint32_t(i), 2});
    }), "make_state");
    return b;
}

// Edits that keep the content near its size and leave holes and spare
// capacity behind (premises' dirty_churn). With `touch`, every change is
// marked for touched_only and touches baselines.
struct Churn {
    double k;
    std::mt19937_64 rng;
    std::size_t ni, np, na, ne;
    explicit Churn(double k_, std::uint64_t seed = 11)
        : k(k_), rng(seed), ni(count(64, k_)), np(count(16, k_)), na(count(16, k_)), ne(count(8, k_)) {}
    static std::size_t lo(std::size_t base) { return base / 2; }
    static std::size_t hi(std::size_t base) { return (std::max<std::size_t>)(2, base + base / 2); }
    std::size_t rnd(std::size_t n) { return std::size_t(rng() % (std::max<std::size_t>)(1, n)); }
    void step(xo::XBuffer& b, bool touch) {
        const unsigned op = unsigned(rng() % 8);
        need(b.edit<Hero>([&](Hero& r) {
            r.level += 1;
            if (touch) b.touch(r.level);
            switch (op) {
            case 0: case 1: {
                const bool insert = (op == 0 && r.items.size() < hi(ni)) || r.items.size() <= lo(ni);
                if (insert) {
                    const std::uint64_t key = rng();
                    r.items[key] = rl::Item{std::uint32_t(key), 7, key ^ 1, key ^ 2};
                } else if (r.items.size()) {
                    r.items.erase_at(r.items.begin() + std::ptrdiff_t(rnd(r.items.size())));
                }
                if (touch) b.touch(r.items);
                break;
            }
            case 2: case 3: {
                const std::size_t d = 1 + rnd(4);
                const bool grow = (op == 2 && r.trail.size() + d <= hi(np)) || r.trail.size() < lo(np) + d;
                if (grow) for (std::size_t i = 0; i < d; ++i) r.trail.push_back(rl::Position{float(i), 1.f, 2.f});
                else r.trail.resize(r.trail.size() - std::uint32_t(d));
                if (touch) b.touch(r.trail);
                break;
            }
            case 4: {
                if (!r.equips.size()) break;
                auto& e = r.equips[std::uint32_t(rnd(r.equips.size()))];
                float tmp[12];
                const std::size_t n = 4 + rnd(9);
                for (std::size_t i = 0; i < n; ++i) tmp[i] = float(rng() % 1000);
                e.attributes.assign_range(std::span<const float>(tmp, n));
                if (touch) b.touch(e);
                break;
            }
            case 5: {
                if ((r.equips.size() < hi(ne) && (rng() & 1)) || r.equips.size() <= lo(ne)) {
                    auto& e = r.equips.emplace_back();
                    e.id = std::uint32_t(rng()); e.level = 3;
                    const std::size_t n = 4 + rnd(9);
                    for (std::size_t i = 0; i < n; ++i) e.attributes.push_back(float(i));
                } else if (r.equips.size()) {
                    r.equips.erase(r.equips.begin() + std::ptrdiff_t(rnd(r.equips.size())));
                }
                if (touch) b.touch(r.equips);
                break;
            }
            case 6: {
                if ((r.attributes.size() < hi(na) && (rng() & 1)) || r.attributes.size() <= lo(na))
                    r.attributes.push_back(float(rng() % 100));
                else
                    r.attributes.pop_back();
                if (touch) b.touch(r.attributes);
                break;
            }
            default: {
                const std::size_t n = 4 + rnd(17);
                std::string text(n, 'a');
                for (auto& c : text) c = char('a' + rnd(26));
                r.name = text.c_str();
                if (touch) b.touch(r.name);
                break;
            }
            }
        }), "churn edit");
    }
};

// A relayout that must succeed, checked: the values stay (the plain compact
// wire), the image is the one save_compact writes with its default slack (or
// the plain one, or the old image when neither is smaller), used_end never
// grows, the free state is exact and empty after a change.
void checked_relayout(Buffer& b, const char* where) {
    xo::XBuffer& x = b;
    const auto values = b.save_compact(kPlain);
    const auto slack = b.save_compact();
    const auto old_image = image_of(x);
    const auto epoch = x.epoch();
    const auto status = b.relayout();
    need(status.has_value(), std::string(where) + ": relayout failed: " + status.error().message);
    need(b.save_compact(kPlain) == values, std::string(where) + ": relayout changed the values");
    if (slack.size() < old_image.size()) {
        need(image_of(x) == slack, std::string(where) + ": image is not save_compact's");
    } else if (values.size() < old_image.size()) {
        need(image_of(x) == values, std::string(where) + ": image is not the plain compact one");
    } else {
        need(image_of(x) == old_image && x.epoch() == epoch, std::string(where) + ": a no-op relayout changed the buffer");
        return;
    }
    need(x.epoch() != epoch, std::string(where) + ": epoch did not advance");
    need(x.free_block_count() == 0 && x.free_block_bytes() == 0, std::string(where) + ": free blocks after relayout");
    need(x.free_table_complete(), std::string(where) + ": free table not exact after relayout");
    need(!b.relayout_advised(), std::string(where) + ": advised right after a relayout");
}

// Delta streams from one churned sender through forced and advised
// relayouts; every receiver equals the sender after every tick.
void streams(double k, int ticks, int every) {
    auto s = make_state(k);
    xo::XBuffer& sx = s;
    struct Stream {
        const char* name;
        xo::XDeltaBaseline base;
        xo::XDeltaOptions options;
        bool admit;
        Buffer rx{4096, cap()};
        xo::XDeltaCursor cursor{};
        int keys = 0;
    };
    xo::XDeltaOptions touched_only;
    touched_only.touched_only = true;
    xo::XDeltaOptions digest;
    digest.digest = true;
    std::vector<Stream> st;
    st.reserve(7);
    st.push_back({"compare_all", xo::XDeltaBaseline{}, xo::XDeltaOptions{}, false});
    st.push_back({"touched_only", xo::XDeltaBaseline{}, touched_only, true});
    st.push_back({"touched_only_2", xo::XDeltaBaseline{}, touched_only, false});
    st.push_back({"touches", xo::XDeltaBaseline(xo::XDeltaRecord::touches), xo::XDeltaOptions{}, false});
    st.push_back({"touches_2", xo::XDeltaBaseline(xo::XDeltaRecord::touches), xo::XDeltaOptions{}, true});
    st.push_back({"compare_all_digest", xo::XDeltaBaseline{}, digest, true});
    xo::XReceiver<xo::XLink::from_server> receiver(xo::XBuffer::maximum_capacity(std::size_t(1) << 26));
    auto ref = s.root();
    auto handle = s.handle();
    auto nested = s.root().equips()[2];
    Churn churn(k);
    std::string d;
    int relayouts = 0, advised = 0;
    for (int t = 0; t < ticks; ++t) {
        churn.step(s, true);
        const bool advise = s.relayout_advised();
        advised += advise;
        if ((every > 0 && t % every == every - 1) || advise) {
            const std::uint32_t level = s.plain_root().level;
            const auto equips = s.plain_root().equips.size();
            const auto nested_id = nested.id().get();
            const auto nested_n = nested.attributes().size();
            const auto before = used_of(sx);
            checked_relayout(s, "streams");
            relayouts += used_of(sx) < before;
            need(ref.level().get() == level && handle->level == level, "root proxy or handle after relayout");
            need(ref.equips().size() == equips, "proxy container after relayout");
            need(nested.id().get() == nested_id && nested.attributes().size() == nested_n,
                 "nested proxy after relayout");
        }
        for (auto& x : st) {
            const auto stats = s.save_delta_into(d, x.base, x.options);
            if (d.empty()) continue;
            x.keys += stats.key;
            const xo::XStatus r = x.admit ? receiver.apply_delta(x.rx, d, x.cursor)
                                          : x.rx.try_apply_delta_verified(d, x.cursor);
            need(r.has_value(), std::string(x.name) + ": apply rejected at tick " + std::to_string(t) + ": " +
                                    r.error().message);
            need(same_image(sx, x.rx), std::string(x.name) + ": receiver differs at tick " + std::to_string(t));
        }
    }
    need(relayouts > 0, "no relayout shed anything");
    // A touches baseline sends a key delta after each relayout (the first
    // delta is one too); a copy baseline needs none.
    for (auto& x : st) {
        if (x.base.record() == xo::XDeltaRecord::touches)
            need(x.keys >= relayouts + 1, std::string(x.name) + ": fewer key deltas than relayouts");
        else
            need(x.keys == 1, std::string(x.name) + ": a copy baseline sent a key delta after the first");
    }
    std::cout << "  streams k=" << k << " ticks=" << ticks << " relayouts=" << relayouts << " advised=" << advised
              << " used=" << used_of(sx) << "\n";
}

void refusals() {
    auto s = make_state(1);
    Churn churn(1);
    for (int i = 0; i < 3000; ++i) churn.step(s, false);
    xo::XBuffer& sx = s;
    const auto image = image_of(sx);
    const auto epoch = sx.epoch();
    auto unchanged = [&](const char* what) {
        need(image_of(sx) == image && sx.epoch() == epoch, std::string(what) + ": refused relayout changed the buffer");
    };
    // Pin scopes: the payload must not move.
    xo::XErrc in_edit = xo::XErrc::ok;
    need(s.edit([&](Hero&) { in_edit = s.relayout().code(); }), "edit around relayout");
    need(in_edit == xo::XErrc::borrowed && s.ok(), "relayout inside edit not refused with borrowed");
    unchanged("edit");
    const auto in_read = s.read([&](const Hero&) { return s.relayout().code(); });
    need(in_read == xo::XErrc::borrowed && s.ok(), "relayout inside read not refused with borrowed");
    unchanged("read");
    // A recorded failure: its code, nothing changed, nothing more recorded.
    sx.record_error(xo::XErrc::out_of_range);
    need(s.relayout().code() == xo::XErrc::out_of_range, "relayout of a failed buffer not refused with its error");
    need(sx.error() == xo::XErrc::out_of_range, "the recorded failure changed");
    unchanged("failed buffer");
    sx.clear_error();
    // Another root type, an unverified load, no arena.
    need(sx.relayout<rl::Other>().code() == xo::XErrc::no_root, "relayout<Other> of a Hero buffer");
    unchanged("wrong type");
    {
        auto u = xo::XBuffer::load_unverified(s.save_verified(), cap());
        need(u.relayout<Hero>().code() == xo::XErrc::no_root, "relayout of an unverified load");
        need(!u.relayout_advised<Hero>(), "relayout_advised of an unverified load");
        xo::XBuffer none;
        need(none.relayout<Hero>().code() == xo::XErrc::no_root, "relayout without an arena");
        need(!none.relayout_advised<Hero>(), "relayout_advised without an arena");
    }
    // An injected storage failure while the image is copied back.
    xo::detail::fail_next_vm_operation(xo::detail::VmOperation::Prepare);
    const auto storage = s.relayout().code();
    xo::detail::clear_vm_failures();
    need(storage == xo::XErrc::out_of_storage, "storage failure not reported as out_of_storage");
    need(s.ok(), "a storage failure was recorded");
    unchanged("storage failure");
#if !XOFFSET_NO_EXCEPTIONS
    // The scratch cannot grow: a buffer larger than any relayout so far,
    // and the first host allocation fails.
    {
        auto big = make_state(64, 3);
        Churn bc(64, 5);
        for (int i = 0; i < 2000; ++i) bc.step(big, false);
        xo::XBuffer& bx = big;
        const auto big_image = image_of(bx);
        const auto big_epoch = bx.epoch();
        xo::XErrc code = xo::XErrc::ok;
        {
            index_host::Arm arm(0);
            code = big.relayout().code();
        }
        need(code == xo::XErrc::host_allocation_failed, "scratch allocation failure not reported");
        need(image_of(bx) == big_image && bx.epoch() == big_epoch && big.ok(), "scratch failure changed the buffer");
        checked_relayout(big, "after a scratch failure");
    }
#endif
    // Refusals recorded nothing: it works now.
    checked_relayout(s, "after refusals");
    need(used_of(sx) < image.size(), "churned buffer did not shrink");
}

// Values and proxies across a relayout, and edits after it against a mirror
// that never relayouts.
void edits_after() {
    for (double k : {0.5, 1.0, 4.0}) {
        auto s = make_state(k, 7);
        auto mirror = make_state(k, 7);
        Churn a(k, 13), b(k, 13);
        for (int round = 0; round < 6; ++round) {
            for (int i = 0; i < 1500; ++i) {
                a.step(s, false);
                b.step(mirror, false);
            }
            checked_relayout(s, "edits_after");
            need(s.save_compact(kPlain) == mirror.save_compact(kPlain), "content differs from the mirror");
        }
        // The whole wire of the relaid sender loads as its content.
        auto back = Buffer::load(s.save_verified());
        need(back.save_compact(kPlain) == s.save_compact(kPlain), "wire after relayout");
        auto dense = Buffer::load(s.save_verified({.emission = xo::XWireEmission::dense}));
        need(dense.save_compact(kPlain) == s.save_compact(kPlain), "dense wire after relayout");
    }
}

// The old image's tail: zeroed with XOFFSET_SCRUB_RESET, otherwise left and
// taken by later blocks without leaking into them.
void tail() {
    auto s = make_state(4, 21);
    Churn churn(4, 23);
    for (int i = 0; i < 5000; ++i) churn.step(s, false);
    xo::XBuffer& sx = s;
    const auto old_used = used_of(sx);
    checked_relayout(s, "tail");
    const auto used = used_of(sx);
    need(used < old_used, "tail: nothing shed");
#if XOFFSET_SCRUB_RESET
    for (std::uint32_t i = used; i < old_used; ++i)
        need(sx.data()[i] == 0, "tail: byte past the new used_end not zero with XOFFSET_SCRUB_RESET");
#endif
    for (std::size_t i = old_used; i < sx.get_size(); ++i)
        need(sx.data()[i] == 0, "tail: byte past the old used_end not zero");
    // Growth into the old tail: a mirror loaded from the plain wire takes
    // the same edits; every wire form carries the same values.
    auto mirror = Buffer::load(s.save_compact(kPlain), cap());
    for (int i = 0; i < 400; ++i) {
        need(s.edit([&](Hero& r) {
            r.trail.push_back(rl::Position{float(i), 2.f, 3.f});
            auto& e = r.equips.emplace_back();
            e.id = std::uint32_t(i);
            for (int a = 0; a < 3; ++a) e.attributes.push_back(float(a));
        }), "tail growth");
        need(mirror.edit([&](Hero& r) {
            r.trail.push_back(rl::Position{float(i), 2.f, 3.f});
            auto& e = r.equips.emplace_back();
            e.id = std::uint32_t(i);
            for (int a = 0; a < 3; ++a) e.attributes.push_back(float(a));
        }), "mirror growth");
    }
    need(used_of(sx) > old_used, "tail: growth did not reach past the old image");
    need(s.save_compact(kPlain) == mirror.save_compact(kPlain), "tail: values after growth");
    for (auto emission : {xo::XWireEmission::dense, xo::XWireEmission::zero_runs, xo::XWireEmission::free_runs}) {
        auto loaded = Buffer::load(s.save_verified({.emission = emission}), cap());
        need(loaded.save_compact(kPlain) == mirror.save_compact(kPlain), "tail: wire after growth");
    }
}

// Fresh content, a tight Explicit buffer, a Relocating one.
void shapes() {
    {
        // Fresh: the slack image is larger than used_end; the plain one or
        // nothing, never growth.
        auto s = make_state(1);
        xo::XBuffer& sx = s;
        const auto used = used_of(sx);
        checked_relayout(s, "fresh");
        need(used_of(sx) <= used, "fresh: relayout grew the image");
        // Twice in a row: the second has nothing to shed.
        const auto image = image_of(sx);
        checked_relayout(s, "fresh again");
        need(image_of(sx) == image, "second relayout changed the image");
    }
    {
        // Explicit growth, the logical length just what the edits needed.
        auto s = Buffer::create(64 * 1024, xo::XBuffer::maximum_capacity(64 * 1024));
        need(s.edit([&](Hero& r) {
            for (int i = 0; i < 300; ++i) r.trail.push_back(rl::Position{1, 2, 3});
            r.trail.resize(10);
            for (std::uint64_t i = 0; i < 64; ++i) r.items[i] = rl::Item{};
        }), "explicit edits");
        xo::XBuffer& sx = s;
        const auto used = used_of(sx);
        checked_relayout(s, "explicit");
        need(used_of(sx) < used, "explicit: nothing shed");
    }
    {
        auto s = Buffer::create(4096, xo::XBuffer::maximum_capacity(std::size_t(1) << 24).with_growth(xo::XGrowPolicy::Relocating));
        auto mirror = Buffer::create(4096, cap());
        Churn a(2, 31), b(2, 31);
        for (int round = 0; round < 4; ++round) {
            for (int i = 0; i < 2000; ++i) {
                a.step(s, false);
                b.step(mirror, false);
            }
            checked_relayout(s, "relocating");
            need(s.save_compact(kPlain) == mirror.save_compact(kPlain), "relocating: values");
        }
    }
}

void hint() {
    struct EpochTagProbe : xo::XBufferCore {
        using xo::XBufferCore::relayout_epoch_tag;
    };
    static_assert(EpochTagProbe::relayout_epoch_tag(0) != 0);
    static_assert(EpochTagProbe::relayout_epoch_tag(0xffffffffull) != 0);
    static_assert(EpochTagProbe::relayout_epoch_tag(0x7fffffffull) != EpochTagProbe::relayout_epoch_tag(0x80000000ull));
    static_assert(EpochTagProbe::relayout_epoch_tag(0xffffffffull) != EpochTagProbe::relayout_epoch_tag(0x100000000ull));

    auto s = make_state(1);
    xo::XBuffer& sx = s;
    need(!s.relayout_advised(), "advised on fresh content");
    need(!s.relayout_advised(), "advised on fresh content (second call)");
    need(!sx.relayout_advised<rl::Other>(), "advised for another root type");
    // Churn until advised: the content stays near its size, so used_end
    // reaching twice the compact size takes holes and spare capacity.
    const auto fresh_mark = s.save_compact().size();  // the mark measured above
    Churn churn(1);
    int edits = 0;
    while (!s.relayout_advised()) {
        churn.step(s, false);
        need(++edits < 200000, "never advised under churn");
    }
    need(used_of(sx) >= 2 * fresh_mark && used_of(sx) < 2 * fresh_mark + 8192, "advised away from twice the mark");
    // The mark is measured once per content: churn on until used_end is
    // twice the content's compact size as it is now, so that a measure of
    // this content agrees.
    while (used_of(sx) < 2 * s.save_compact().size()) {
        churn.step(s, false);
        need(++edits < 200000, "used_end never reached twice the compact size");
    }
    need(s.relayout_advised(), "advised stopped without a relayout");
    // Even after the mark is cached and reached in this epoch, an untyped
    // buffer must still report false for a different root type.
    need(!sx.relayout_advised<rl::Other>(), "advised for another root type after mark reached");
    // A clone holds the same bytes: measured for itself, the same answer.
    auto copy = s.clone();
    need(copy.relayout_advised(), "clone of advised content not advised");
    // A dense reload keeps the layout: advised; a compact one: not.
    auto receiver = Buffer::create(4096, cap());
    receiver.reload_verified(s.save_verified({.emission = xo::XWireEmission::dense}));
    need(receiver.relayout_advised(), "dense reload of advised content not advised");
    receiver.reload_verified(s.save_compact());
    need(!receiver.relayout_advised(), "compact reload advised");
    // A swap: each buffer measures its new content.
    auto fresh = make_state(1);
    need(!fresh.relayout_advised(), "fresh advised");
    s.swap(fresh);
    need(!s.relayout_advised() && fresh.relayout_advised(), "swap: hints did not follow the content");
    s.swap(fresh);
    // A move: the target measures.
    Buffer moved(std::move(s));
    need(moved.relayout_advised(), "moved content not advised");
    need(!s.relayout_advised(), "moved-from buffer advised");
    checked_relayout(moved, "hint");
    need(!moved.relayout_advised(), "advised after relayout");
    // A reset: an empty root is never advised.
    (void)moved.reset();
    need(!moved.relayout_advised(), "advised after reset");
    // Large already-compact buffer (> 256 KiB, forcing fresh scratch allocation):
    // slack image would exceed used_end and is skipped without reserving/emitting
    // a larger slack image; relayout returns ok with the image unchanged.
    {
        auto big = Buffer::create(512 * 1024, cap());
        need(big.edit([&](Hero& r) {
            r.trail.reserve(30000);
            for (int i = 0; i < 30000; ++i) r.trail.push_back(rl::Position{float(i), 1.f, 2.f});
        }), "big fresh build");
        xo::XBuffer& bx = big;
        const auto used_before = used_of(bx);
        need(used_before > 300 * 1024, "big fresh buffer expected > 300 KiB");
        const auto image_before = image_of(bx);
        const auto st = big.relayout();
        need(bool(st), "big fresh relayout failed");
        need(used_of(bx) <= used_before, "big fresh relayout grew");
        need(big.save_compact(kPlain) == Buffer::load(image_before, cap()).save_compact(kPlain),
             "big fresh relayout altered values");
        // Second call has nothing left to shed and keeps bytes identical.
        const auto image_after = image_of(bx);
        need(bool(big.relayout()), "big fresh second relayout failed");
        need(image_of(bx) == image_after, "big fresh second relayout changed bytes");
    }
}

} // namespace

int main() {
    streams(1, 3000, 97);
    streams(4, 1500, 61);
    streams(0.5, 3000, 0);
    refusals();
    edits_after();
    tail();
    shapes();
    hint();
    std::cout << "test_relayout passed\n";
    return 0;
}
