// XOFFSET_ORDINARY_RECORDS: one declaration for several record types turns on
// wire_record_layout<T>::ordinary_members and StableEraseOrdinaryMembers<T>
// (SHIFT relocation, stable_erase_if). Records declared this way must behave
// and serialize exactly like undeclared twins. The build-failure cases
// (overlapping members, base class, bit-field) are ordinary_records_*_fail.
#include "xoffsetdatastructure.hpp"

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

namespace xo = XOffsetDatastructure;

namespace orec {
struct alignas(8) Skill { std::int32_t id{}; std::int32_t level{}; };
struct alignas(8) Buff { std::int32_t id{}; std::int32_t stacks{}; std::int64_t expire_ms{}; };
struct alignas(8) Hero {
    std::int32_t id{}; std::int32_t level{}; std::int32_t star{};
    xo::XString name;
    xo::XVector<Skill> skills;
    xo::XVector<Buff> buffs;
};
struct Root { xo::XVector<Hero> heroes; };

// Undeclared twins: per-element reference.
struct alignas(8) PlainSkill { std::int32_t id{}; std::int32_t level{}; };
struct alignas(8) PlainBuff { std::int32_t id{}; std::int32_t stacks{}; std::int64_t expire_ms{}; };
struct alignas(8) PlainHero {
    std::int32_t id{}; std::int32_t level{}; std::int32_t star{};
    xo::XString name;
    xo::XVector<PlainSkill> skills;
    xo::XVector<PlainBuff> buffs;
};
struct PlainRoot { xo::XVector<PlainHero> heroes; };
} // namespace orec

XOFFSET_ORDINARY_RECORDS(orec::Hero, orec::Skill, orec::Buff)
XOFFSET_REGISTER_SCHEMA_NAME(orec::Root, "test.ordinary_records.root")
XOFFSET_REGISTER_SCHEMA_NAME(orec::PlainRoot, "test.ordinary_records.plain")
// stable_erase_if alone, without the raw/SHIFT declaration.
template<> struct xo::StableEraseOrdinaryMembers<orec::PlainHero> : std::true_type {};

static_assert(xo::wire_record_layout<orec::Hero>::ordinary_members);
static_assert(xo::wire_record_layout<orec::Skill>::ordinary_members);
static_assert(xo::wire_record_layout<orec::Buff>::ordinary_members);
static_assert(xo::StableEraseOrdinaryMembers<orec::Hero>::value);
static_assert(xo::StableEraseOrdinaryMembers<orec::Buff>::value);
static_assert(xo::detail::shift_relocate_supported<orec::Hero>(), "declared Hero takes the SHIFT path");
static_assert(!xo::wire_record_layout<orec::PlainHero>::ordinary_members);
static_assert(!xo::detail::shift_relocate_supported<orec::PlainHero>());
// The mechanical checks on ordinary and non-ordinary layouts.
namespace orec_checks {
struct Empty {};
struct Overlap { [[no_unique_address]] Empty e; std::int64_t x{}; };
struct Disjoint { Empty e; std::int64_t x{}; };
struct Base { std::int32_t a{}; };
struct Derived : Base { std::int32_t b{}; };
struct Bits { std::uint32_t a : 4; std::uint32_t b : 28; };
} // namespace orec_checks
static_assert(xo::detail::ordinary_record_members_disjoint<orec::Hero>());
static_assert(xo::detail::ordinary_record_members_disjoint<orec_checks::Disjoint>());
static_assert(!xo::detail::ordinary_record_members_disjoint<orec_checks::Overlap>());
static_assert(xo::detail::ordinary_record_has_no_bases<orec::Hero>());
static_assert(!xo::detail::ordinary_record_has_no_bases<orec_checks::Derived>());
static_assert(xo::detail::ordinary_record_has_no_bit_fields<orec::Hero>());
static_assert(!xo::detail::ordinary_record_has_no_bit_fields<orec_checks::Bits>());

namespace {
[[noreturn]] void fail(const std::string& message) {
    std::cerr << "test_ordinary_records FAILED: " << message << "\n";
    std::exit(1);
}

struct Rng {
    std::uint64_t s;
    std::uint32_t below(std::uint32_t n) {
        s = s * 6364136223846793005ull + 1442695040888963407ull;
        return n ? std::uint32_t(s >> 33) % n : 0;
    }
};

template <class H> void fill(H& h, Rng& rng, std::int32_t id) {
    h.id = id;
    h.level = std::int32_t(rng.below(60));
    h.star = std::int32_t(rng.below(6));
    h.name = std::string(rng.below(2) ? 6 : 13 + rng.below(9), char('a' + id % 26));
    for (std::uint32_t k = 0, n = 3 + rng.below(2); k < n; ++k)
        h.skills.push_back({std::int32_t(k), std::int32_t(rng.below(10))});
    for (std::uint32_t k = 0, n = rng.below(3); k < n; ++k)
        h.buffs.push_back({std::int32_t(k), 1, std::int64_t(rng.below(100000))});
}

template <class Root> std::string describe(const Root& root) {
    std::string out;
    for (const auto& h : root.heroes) {
        out += std::to_string(h.id) + "/" + std::to_string(h.level) + "/" + std::string(h.name.view()) + "[";
        for (const auto& s : h.skills) out += std::to_string(s.id) + ":" + std::to_string(s.level) + ",";
        out += "|";
        for (const auto& b : h.buffs) out += std::to_string(b.id) + ":" + std::to_string(b.expire_ms) + ",";
        out += "];";
    }
    return out;
}

auto policy() {
    return xo::XBuffer::maximum_capacity(std::size_t(16) << 20).with_growth(xo::XGrowPolicy::Automatic);
}

// SLG-style season edits: recruit (append), dismiss (middle erase),
// stable_erase_if on low levels, checkpoints with full round trips.
template <class Root> std::vector<std::string> run() {
    std::vector<std::string> wires;
    Rng rng{20261022};
    xo::XBuffer buf = xo::XBuffer::create<Root>(8192, policy());
    std::int32_t next = 1;
    for (int i = 0; i < 30; ++i) fill(buf.root<Root>().heroes.emplace_back(), rng, next++);
    for (int op = 0; op < 600; ++op) {
        auto& heroes = buf.root<Root>().heroes;
        switch (rng.below(3)) {
        case 0: fill(heroes.emplace_back(), rng, next++); break;
        case 1:
            if (heroes.size() > 1) heroes.erase(heroes.begin() + std::ptrdiff_t(rng.below(heroes.size())));
            break;
        default:
            if (op % 50 == 0) {
                const auto cut = std::int32_t(rng.below(8));
                heroes.stable_erase_if([&](const auto& h) { return h.level < cut; });
            }
        }
        if (op % 61 == 0) {
            std::string wire = buf.save_verified<Root>();
            xo::XBuffer loaded = xo::XBuffer::load_verified<Root>(wire, policy());
            if (describe(loaded.root<Root>()) != describe(buf.root<Root>())) fail("reload content");
            if (loaded.save_verified<Root>() != wire) fail("write-read-write bytes");
            wires.push_back(std::move(wire));
            if (rng.below(2)) buf = std::move(loaded);
        }
    }
    wires.push_back(buf.save_verified<Root>());
    return wires;
}
} // namespace

int main() {
    const auto declared = run<orec::Root>();
    const auto plain = run<orec::PlainRoot>();
    if (declared.size() != plain.size()) fail("checkpoint count");
    constexpr std::size_t header = sizeof(xo::XWireHeaderV2);
    for (std::size_t i = 0; i < declared.size(); ++i)
        if (declared[i].size() != plain[i].size() ||
            declared[i].compare(header, std::string::npos, plain[i], header, std::string::npos) != 0)
            fail("declared and undeclared wires differ at checkpoint " + std::to_string(i));
    std::cout << "test_ordinary_records passed (" << declared.size() << " checkpoints byte-identical)\n";
    return 0;
}
