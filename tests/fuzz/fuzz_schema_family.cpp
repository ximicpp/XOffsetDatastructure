// libFuzzer target for the add-only type evolution (docs/SCHEMA_EVOLUTION.md
// section 9): the in-place pair. Three versions of one family read the same
// bytes, so every input is given to each of them and they must agree. The
// first input byte picks the mode, the rest is its input:
//
//   0  a whole wire of the family's root (tests/schema_family/player_versions.hpp).
//      A later version only adds fields, and a field it adds may be checked
//      (a bool), so it accepts no wire an earlier version rejects: v3 accepts
//      => v2 accepts => v1 accepts, on the full check (try_load_verified) and
//      on the admit tier (XReceiver<server_peer>). A wire two versions accept
//      re-saves to the same bytes from both, and an edit by the earliest one
//      (a plain field) reaches the others with every other byte kept.
//   1  a shared delta change body of a unit record in three versions: the
//      same rule for check_change, the same end, and applied on equal
//      records the same whole bodies after.
//   2  a shared delta whole body of the same record: the same as 1 for
//      check_whole and apply_whole.
//
// Field lists, so the client build (C++20) runs it too. Built like
// fuzz_wire_schemas.cpp: as the fuzzer, with XOFFSET_FUZZ_SEED_MAIN as
// fuzz_schema_family_seeds, and with replay_main.cpp as
// fuzz_schema_family_replay (any compiler).
#include "v2_fuzz_core.hpp"
#include "../../xoffset_shared_delta.hpp"
#include "../schema_family/player_versions.hpp"

#include <filesystem>

namespace ff {
namespace v1 {
struct Unit {
    std::uint32_t hp{};
    bool on{};
    std::uint8_t kind{};
    xo::XSpare<2> more{};
    xo::XString name;
    xo::XVector<std::uint16_t> marks;
    xo::XSpare<8> spare{};
};
} // namespace v1
namespace v2 {
struct Unit {
    std::uint32_t hp{};
    bool on{};
    std::uint8_t kind{};
    std::uint8_t team{};
    bool elite{};
    xo::XString name;
    xo::XVector<std::uint16_t> marks;
    std::uint32_t shield{};
    xo::XSpare<4> spare{};
};
} // namespace v2
namespace v3 {
struct Unit {
    std::uint32_t hp{};
    bool on{};
    std::uint8_t kind{};
    std::uint8_t team{};
    bool elite{};
    xo::XString name;
    xo::XVector<std::uint16_t> marks;
    std::uint32_t shield{};
    float speed{};
};
struct Units { xo::XVector<Unit> units; };
} // namespace v3
} // namespace ff
XOFFSET_RECORD(ff::v1::Unit, hp, on, kind, more, name, marks, spare)
XOFFSET_RECORD(ff::v2::Unit, hp, on, kind, team, elite, name, marks, shield, spare)
XOFFSET_RECORD(ff::v3::Unit, hp, on, kind, team, elite, name, marks, shield, speed)
XOFFSET_RECORD(ff::v3::Units, units)
XOFFSET_SCHEMA_FAMILY(ff::v2::Unit, ff::v1::Unit)
XOFFSET_SCHEMA_FAMILY(ff::v3::Unit, ff::v2::Unit)

namespace {
using P1 = fam::v1::Player;
using P2 = fam::v2::Player;
using P3 = fam::v3::Player;
using U1 = ff::v1::Unit;
using U2 = ff::v2::Unit;
using U3 = ff::v3::Unit;
static_assert(sizeof(U1) == sizeof(U2) && sizeof(U2) == sizeof(U3));

xo::XBuffer::MaximumCapacity grow_cap() {
    return xo::XBuffer::maximum_capacity(kLimit).with_growth(xo::XGrowPolicy::Automatic);
}

// ---- mode 0: whole wires -----------------------------------------------------------

template <class P>
xo::XStatus admit(const std::string& w) {
    static xo::XReceiver<xo::XLink::server_peer> rx(xo::XBuffer::maximum_capacity(kLimit));
    static xo::XBuffer target(4096, xo::XBuffer::maximum_capacity(kLimit));
    return rx.reload<P>(target, w);
}

void wires(const std::string& w) {
    std::string e1, e2, e3;
    auto l1 = typed_load<P1>(w, &e1);
    auto l2 = typed_load<P2>(w, &e2);
    auto l3 = typed_load<P3>(w, &e3);
    outcome_add("1", bool(l1), e1);
    outcome_add("2", bool(l2), e2);
    outcome_add("3", bool(l3), e3);
    if ((l3 && !l2) || (l2 && !l1)) fail("full check: a later version accepts a wire an earlier one rejects");
    const xo::XStatus a1 = admit<P1>(w), a2 = admit<P2>(w), a3 = admit<P3>(w);
    if ((a3 && !a2) || (a2 && !a1)) fail("admit: a later version accepts a wire an earlier one rejects");
    if (l1 && !a1) fail("admit rejects a wire the full check accepts (v1)");
    if (!l1) return;
    const xo::XSaveOptions image{xo::XWireEmission::free_runs};
    const std::string s1 = l1->save_verified(image);
    if (l2 && l2->save_verified(image) != s1) fail("v1 and v2 re-save the same wire differently");
    if (l3 && l3->save_verified(image) != s1) fail("v1 and v3 re-save the same wire differently");
    // The earliest version edits a field all three know; the later ones see
    // the edit and keep every byte of theirs.
    l1->plain_root().gold += 1;
    const std::string e = l1->save_verified(image);
    if (l2) {
        l2->plain_root().gold += 1;
        auto r2 = typed_load<P2>(e);
        if (!r2) fail("v2 rejects the wire v1 wrote after an edit of a wire v2 accepted");
        if (l2->save_verified(image) != e || r2->save_verified(image) != e) fail("v1's edit lost bytes of v2");
    }
    if (l3) {
        l3->plain_root().gold += 1;
        auto r3 = typed_load<P3>(e);
        if (!r3) fail("v3 rejects the wire v1 wrote after an edit of a wire v3 accepted");
        if (l3->save_verified(image) != e || r3->save_verified(image) != e) fail("v1's edit lost bytes of v3");
    }
}

// ---- modes 1 and 2: shared delta bodies --------------------------------------------

// Three units, one per version, in one buffer (containers need one).
struct Units {
    xo::XBuffer buf = xo::XBuffer::create<ff::v3::Units>(std::size_t(1) << 16, grow_cap());
    Units() { buf.root<ff::v3::Units>().units.resize(3); }
    template <class U> U& at(std::size_t i) { return reinterpret_cast<U&>(buf.root<ff::v3::Units>().units[i]); }
};

template <class U>
std::string whole_of(const U& u) {
    std::string out(XOffsetDatastructure::xsync::XRecordBody<U>::whole_room(u) + 64, '\0');
    char* e = XOffsetDatastructure::xsync::XRecordBody<U>::put_whole(out.data(), u);
    out.resize(std::size_t(e - out.data()));
    return out;
}

template <bool Change>
void bodies(const std::string& body) {
    namespace xsd = XOffsetDatastructure::xsync;
    const char* b = body.data();
    const char* end = b + body.size();
    auto check = [&]<class U>() {
        if constexpr (Change) return xsd::XRecordBody<U>::check_change(b, end);
        else return xsd::XRecordBody<U>::check_whole(b, end);
    };
    const char* c1 = check.template operator()<U1>();
    const char* c2 = check.template operator()<U2>();
    const char* c3 = check.template operator()<U3>();
    outcome_add("1", c1 != nullptr, "rejected");
    outcome_add("2", c2 != nullptr, "rejected");
    outcome_add("3", c3 != nullptr, "rejected");
    if ((c3 && !c2) || (c2 && !c1)) fail("a later version accepts a body an earlier one rejects");
    if ((c2 && c2 != c1) || (c3 && c3 != c1)) fail("two versions end the same body at different places");
    if (!c1) return;
    // Applied on equal records (a seed record per version, the same bytes),
    // the versions that accept the body hold the same bytes after.
    Units s;
    U1& u1 = s.at<U1>(0);
    U2& u2 = s.at<U2>(1);
    U3& u3 = s.at<U3>(2);
    u3.hp = 7;
    u3.on = true;
    u3.team = 2;
    u3.name = std::string_view("seed-unit");
    u3.marks.push_back(5);
    u3.speed = 1.5f;
    const std::string seed = whole_of(u3);
    xsd::XRecordBody<U1>::apply_whole(seed.data(), u1);
    xsd::XRecordBody<U2>::apply_whole(seed.data(), u2);
    auto apply = [&]<class U>(U& u) {
        if constexpr (Change) return xsd::XRecordBody<U>::apply_change(b, u);
        else return xsd::XRecordBody<U>::apply_whole(b, u);
    };
    if (apply(u1) != c1) fail("v1 applies a body to another end than its check");
    const std::string w1 = whole_of(u1);
    if (c2 && (apply(u2) != c2 || whole_of(u2) != w1)) fail("v1 and v2 hold different bytes after the same body");
    if (c3 && (apply(u3) != c3 || whole_of(u3) != w1)) fail("v1 and v3 hold different bytes after the same body");
}

// ---- seeds -----------------------------------------------------------------------------

template <class P>
void fill(P& p, std::mt19937_64& rng) {
    p.uid = rng();
    p.gold = std::int32_t(rng() % 100000);
    p.nick = random_text(rng, 24);
    for (int i = int(rng() % 4); i > 0; --i) {
        auto& h = p.heroes.emplace_back();
        h.id = std::int32_t(rng() % 1000);
        h.name = random_text(rng, 16);
        for (int k = int(rng() % 3); k > 0; --k) h.skills.push_back(std::int32_t(rng() % 50));
        h.stats.atk = std::int32_t(rng() % 300);
        if constexpr (requires { h.awake; }) { h.awake = rng() % 2; h.star = std::uint8_t(rng() % 6); }
    }
    p.buffs[std::int32_t(rng() % 10)].atk = 3;
    if constexpr (requires { p.vip; }) p.vip = std::int32_t(rng() % 10);
    if constexpr (requires { p.runes.emplace_back().level; }) {
        for (int i = int(rng() % 3); i > 0; --i) p.runes.emplace_back().level = std::int32_t(rng() % 9);
    }
}

template <class P>
std::string seed_wire(std::mt19937_64& rng) {
    auto b = xo::TypedXBuffer<P>::create(4096, grow_cap());
    fill(b.plain_root(), rng);
    return b.save_verified(xo::XSaveOptions{rng() % 2 ? xo::XWireEmission::free_runs : xo::XWireEmission::dense});
}

void write(const std::string& dir, int i, char tag, const std::string& bytes) {
    const auto path = dir + "/seed-" + std::to_string(i) + ".bin";
    std::FILE* f = std::fopen(path.c_str(), "wb");
    if (!f || std::fwrite(&tag, 1, 1, f) != 1 || std::fwrite(bytes.data(), 1, bytes.size(), f) != bytes.size() ||
        std::fclose(f) != 0)
        fail("cannot write " + path);
}

// 36 seeds: wires of each version, change and whole bodies of each version.
int write_family_seeds(const std::string& dir) {
    namespace xsd = XOffsetDatastructure::xsync;
    std::filesystem::create_directories(dir);
    std::mt19937_64 rng(20261124);
    int n = 0;
    for (int i = 0; i < 12; ++i) {
        const std::string w = i % 3 == 0 ? seed_wire<P1>(rng) : i % 3 == 1 ? seed_wire<P2>(rng) : seed_wire<P3>(rng);
        wires(w);
        write(dir, n++, 0, w);
    }
    Units s;
    U3& now = s.at<U3>(0);
    U3& prev = s.at<U3>(1);
    for (int i = 0; i < 12; ++i) {
        now.hp = std::uint32_t(rng());
        now.elite = rng() % 2;
        now.team = std::uint8_t(rng());
        now.name = random_text(rng, 20);
        if (rng() % 2) now.marks.push_back(std::uint16_t(rng()));
        now.speed = float(rng() % 100);
        const std::uint64_t mask = xsd::XRecordBody<U3>::changed(now, prev);
        std::string change(xsd::XRecordBody<U3>::change_room(now, mask) + 8, '\0');
        change.resize(std::size_t(xsd::XRecordBody<U3>::put_change(change.data(), now, mask) - change.data()));
        bodies<true>(change);
        write(dir, n++, 1, change);
        const std::string whole = whole_of(now);
        bodies<false>(whole);
        write(dir, n++, 2, whole);
        xsd::XRecordBody<U3>::apply_whole(whole.data(), prev);
    }
    return n;
}
} // namespace

extern "C" int XOffsetFuzzWriteSeeds(const char* dir) {
    std::cout << "fuzz_schema_family: wrote " << write_family_seeds(dir) << " seeds to " << dir << "\n";
    return 0;
}

#ifdef XOFFSET_FUZZ_SEED_MAIN
int main(int argc, char** argv) {
    if (argc != 2) { std::cerr << "usage: fuzz_schema_family_seeds <dir>\n"; return 2; }
    return XOffsetFuzzWriteSeeds(argv[1]);
}
#else
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    inject_crash_drill(data, size);
    outcome_begin(data, size);
    if (size == 0) { outcome_end(); return 0; }
    const std::string rest(reinterpret_cast<const char*>(data) + 1, size - 1);
    switch (data[0] % 3) {
    case 0: wires(rest); break;
    case 1: bodies<true>(rest); break;
    default: bodies<false>(rest); break;
    }
    outcome_end();
    return 0;
}
#endif
