#include "xoffset_snapshot_index.hpp"
#include "index_host_failpoint.hpp"
#include <array>
#include <iostream>
#include <stdexcept>

namespace xo = XOffsetDatastructure;
namespace fd = xo::fixed_detail;
namespace si = xo::snapshot_index_detail;
using Evidence = xo::SnapshotIndexEvidence;
struct IndexCell { xo::XString label; xo::XVector<xo::XString> nested; };
struct IndexRoot { xo::XVector<IndexCell> cells; xo::XVector<std::byte> opaque; };
XOFFSET_REGISTER_SCHEMA_NAME(IndexRoot, "four-paths.snapshot-array-index.v1")
using Snapshot = xo::VerifiedSnapshot<IndexRoot>;
using Index = xo::VerifiedSnapshotArrayIndex<IndexRoot, xo::XVector<IndexCell>>;
namespace {
void need(bool b, const char* why) { if (!b) throw std::runtime_error(why); }
auto policy() { return xo::XBuffer::maximum_capacity(4u << 20).with_growth(xo::XGrowPolicy::Automatic); }
Snapshot make() {
    auto source = xo::TypedXBuffer<IndexRoot>::create(65536, policy());
    auto& root = source.root(); root.cells.resize(5); root.opaque.resize(64);
    for (unsigned i = 0; i < 5; ++i) {
        root.cells[i].label = "stable-index-payload";
        root.cells[i].nested.resize(2);
        root.cells[i].nested[0] = "first"; root.cells[i].nested[1] = "second";
    }
    return xo::freeze_verified(std::move(source));
}
void supported_domain_and_fallback() {
    const auto before = si::process_requested_bytes();
    {
        auto owner = make(); const auto& cells = owner.root().cells;
        auto index = xo::make_verified_snapshot_array_index(owner, cells);
        need(bool(index) && index.requested_bytes() <= si::owner_limit, "index preparation disabled or exceeded cap");
        for (unsigned i = 0; i < 5; ++i) for (unsigned j = 0; j < 5; ++j) {
            const auto expected = i == j ? Evidence::unknown : Evidence::strict_negative;
            need(index.evidence(cells[i].label, &cells[j].nested) == expected, "declared same-array evidence differs");
            need(index.owns_address(cells[i].label, &cells[j].nested) ==
                 fd::move_assignment_owns_address(cells[i].label, &cells[j].nested), "negative differs from original predicate");
        }
        // Known positive and out-of-line heads must go through the old walk.
        need(index.evidence(cells[0].nested, &cells[0].nested[1]) == Evidence::unknown,
             "out-of-line target was incorrectly inferred from an inline address");
        need(index.owns_address(cells[0].nested, &cells[0].nested[1]), "positive original predicate was skipped");
        need(index.evidence(cells, &cells[1].label) == Evidence::unknown &&
             index.owns_address(cells, &cells[1].label), "parent positive failed closed incorrectly");
        need(index.evidence(cells[0].label, owner.root().opaque.data()) == Evidence::unknown,
             "opaque bytes were treated as a declared head");
        need(index.evidence(cells[0].label, cells.data() + cells.size()) == Evidence::unknown,
             "spare or end storage was treated as a live element");
        auto foreign = make();
        need(index.evidence(cells[0].label, &foreign.root().cells[1].label) == Evidence::unknown,
             "foreign reservation was treated as this parent");
        auto wrong = xo::make_verified_snapshot_array_index(owner, foreign.root().cells);
        need(!wrong, "foreign declared parent entered the index");
        xo::XVector<IndexCell> empty;
        auto absent = xo::make_verified_snapshot_array_index(owner, empty);
        need(!absent, "unbound empty parent entered the index");
    }
    need(si::process_requested_bytes() == before, "index host requested bytes leaked");
}
void owner_and_index_moves() {
    const auto before = si::process_requested_bytes();
    {
        auto snapshot = make(); const auto* data = snapshot.root().cells.data();
        auto index = xo::make_verified_snapshot_array_index(snapshot, snapshot.root().cells);
        const auto charged = si::process_requested_bytes();
        auto copy = index; auto moved = std::move(index); snapshot = Snapshot{};
        need(si::process_requested_bytes() == charged, "index copy or move allocated a second host index");
        need(copy.snapshot().root().cells.data() == data && moved.snapshot().root().cells.data() == data,
             "copy/move lost the immutable payload lease");
        index = Index{}; copy = Index{};
        const auto& cells = moved.snapshot().root().cells;
        need(moved.evidence(cells[0].label, &cells[4].nested) == Evidence::strict_negative,
             "moved index lost complete parent evidence");
        auto io = moved.snapshot().acquire_io_lease();
        const auto segments = io.segments(); io.cancel(); moved = Index{};
        need(io.cancellation_requested() && io.segments()[1].data() == segments[1].data(),
             "cancel incorrectly released the real completion owner");
        io.complete();
    }
    need(si::process_requested_bytes() == before, "last index release did not return its host budget");
}
void actual_oom_and_budget_arithmetic() {
    auto owner = make(); const auto& cells = owner.root().cells;
    const auto before = si::process_requested_bytes();
    {
        Index disabled;
        { index_host::Arm arm(0); disabled = Index::prepare(owner, cells); }
        need(!disabled && index_host::failures == 1 && index_host::attempts == 1,
             "real index allocation failure was not tested");
        need(si::process_requested_bytes() == before, "failed allocation retained an index reservation");
        need(disabled.snapshot().root().cells.size() == 5 &&
             disabled.owns_address(cells, &cells[0].label), "OOM disabled original positive behavior or owner lifetime");
    }
    // No synthetic malloc claims: exercise just the fixed cap accounting with
    // unique real stack identities, then return every reservation.
    std::array<std::byte, 18> keys{};
    need(si::reserve(&keys[0], si::owner_limit), "per-owner cap boundary was rejected");
    need(!si::reserve(&keys[0], 1), "per-owner cap was exceeded");
    std::size_t successes = 1;
    for (std::size_t i = 1; i < keys.size(); ++i)
        if (si::reserve(&keys[i], si::owner_limit)) ++successes;
    need(successes == 15 && si::process_requested_bytes() <= si::process_limit,
         "process budget omitted registry bytes or exceeded 32MiB");
    for (std::size_t i = 0; i < successes; ++i) si::release(&keys[i], si::owner_limit);
    need(si::process_requested_bytes() == before, "budget boundary test did not return reservations");
    std::array<std::byte, 257> slots{};
    for (std::size_t i=0;i<256;++i)need(si::reserve(&slots[i],1),"fixed owner slot unavailable before full");
    need(!si::reserve(&slots[256],1),"full fixed owner registry was bypassed");
    for (std::size_t i=0;i<256;++i)si::release(&slots[i],1);
    need(si::process_requested_bytes()==before,"full owner table did not return reservations");
}
Snapshot* callback_owner{}; Index* callback_result{};
void reenter_preparation() { *callback_result = Index::prepare(*callback_owner, callback_owner->root().cells); }
void new_handler_reentry() {
    auto owner = make(); Index nested;
    callback_owner = &owner; callback_result = &nested;
    const auto before = si::process_requested_bytes();
    const auto old = std::set_new_handler(reenter_preparation);
    {
        Index outer;
        try { index_host::Arm arm(0); outer = Index::prepare(owner, owner.root().cells); }
        catch (...) { std::set_new_handler(old); throw; }
        std::set_new_handler(old);
        need(bool(outer) && bool(nested) && index_host::handler_calls == 1,
             "new_handler reentry deadlocked or produced a partial index");
    }
    nested = Index{};
    need(si::process_requested_bytes() == before, "reentrant indexes retained budget");
}
}
int main() { try {
    supported_domain_and_fallback(); owner_and_index_moves(); actual_oom_and_budget_arithmetic(); new_handler_reentry();
    std::cout << "immutable snapshot array index: typed witness/fallback/lifetime/OOM/caps/reentry passed\n";
    return 0;
} catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; } }
