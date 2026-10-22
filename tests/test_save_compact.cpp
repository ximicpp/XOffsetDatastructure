// save_compact<T>(): canonical wire written by direct traversal of the source
// graph (no temporary buffer). For random fuzz models (holes, slack, inline
// and heap strings, nested vectors, set, map) and fixed cases:
//   - the wire equals XCompactor::compact<T>(source).save_verified<T>() byte
//     for byte, has no holes and no extent table, and passes the independent
//     oracle and load_verified with the source's contents;
//   - save_compact of the loaded wire reproduces it (canonical is a fixpoint)
//     and save_compact_into into a reused string matches;
//   - the source buffer is not changed.
#include "fuzz/v2_fuzz_core.hpp"

namespace {
void need(bool ok, const std::string& what) { if (!ok) fail(what); }

std::string reference(xo::XBufferCore& source) {
    auto compacted = xo::XCompactor::compact<fz::Root>(source);
    return compacted.save_verified<fz::Root>();
}

void check(Buffer& b, const std::string& where) {
    const auto before = b.copy_bytes();
    const auto size_before = b.get_size();
    const auto semantics = dump(b.root());
    const auto wire = b.save_compact();
    need(b.copy_bytes() == before && b.get_size() == size_before, where + ": save_compact changed the source");
    xo::XWireHeaderV2 h; std::memcpy(&h, wire.data(), kHeader);
    need(h.hole_count == 0 && h.hole_bytes == 0 && h.extent_count == 0 && h.root_offset == 48 &&
         wire.size() == kHeader + h.used_bytes, where + ": not a canonical dense wire");
    need(wire == reference(b), where + ": differs from XCompactor + save_verified");
    const auto o = oracle(wire);
    need(o.ok, where + ": oracle rejects the compact wire: " + o.why);
    auto loaded = typed_load(wire);
    need(bool(loaded), where + ": load_verified rejects the compact wire");
    need(dump(loaded->root()) == semantics, where + ": compact wire changed the contents");
    need(loaded->save_compact() == wire, where + ": canonical wire is not a fixpoint");
    need(loaded->save_verified() == wire, where + ": canonical wire does not re-save identically");
    std::string into = "previous contents that are longer than nothing";
    b.save_compact_into(into);
    need(into == wire, where + ": save_compact_into differs");
    b.save_compact_into(into); // reused capacity
    need(into == wire, where + ": save_compact_into (reused) differs");
    std::string tiny;
    tiny.reserve(1);
    b.save_compact_into(tiny);
    need(tiny == wire, where + ": save_compact_into (growing) differs");
    // The canonical wire is never larger than the dense one.
    need(wire.size() <= b.save_verified().size(), where + ": compact wire larger than dense");
}

void fixed_cases() {
    {   // Empty root: no blocks at all.
        auto b = Buffer::create(4096);
        check(b, "empty root");
    }
    {   // Strings at the inline boundary, heap strings shrunk below it, empty
        // containers with retained capacity.
        auto b = Buffer::create(4096, xo::XBuffer::maximum_capacity(1u << 20).with_growth(xo::XGrowPolicy::Automatic));
        auto& r = b.root();
        r.title = std::string(10, 't');
        auto& a = r.items.emplace_back();
        a.name = std::string(11, 'n');
        auto& c = r.items.emplace_back();
        c.name = std::string(40, 'h');
        c.name = "short";        // heap block keeps capacity 40, size 5 -> inline when compacted
        c.vals.reserve(64);      // empty with capacity -> no storage when compacted
        r.tags.reserve(8);
        r.tags.emplace_back(std::string(0, 'x'), r.tags.get_stored_allocator());
        r.keys.insert(3);
        r.keys.insert(1);
        r.attrs.insert_or_assign(std::string_view("k"), std::uint64_t(9));
        check(b, "boundaries");
    }
}
} // namespace

int main(int argc, char** argv) {
    const std::size_t cases = argc > 1 ? std::stoul(argv[1]) : 600;
    std::mt19937_64 rng(argc > 2 ? std::stoull(argv[2]) : 20261022);
    fixed_cases();
    std::size_t smaller = 0;
    for (std::size_t i = 0; i < cases; ++i) {
        auto wire = build_wire(rng);
        auto b = typed_load(wire);
        need(bool(b), "case " + std::to_string(i) + ": generated wire rejected");
        if (rng() % 2) mutate_valid(b->root(), rng); // fresh holes and slack in the live buffer
        check(*b, "case " + std::to_string(i));
        smaller += b->save_compact().size() < b->save_verified().size();
    }
    need(smaller > cases / 2, "compaction rarely shrank the wire");
    std::cout << "test_save_compact: " << cases << " random models + fixed cases, " << smaller
              << " smaller than dense, all byte-identical to XCompactor\n";
    return 0;
}
