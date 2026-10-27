// save_compact<T>(): canonical wire written by direct traversal of the source
// graph (no temporary buffer). For random fuzz models (holes, slack, inline
// and heap strings, nested vectors, set, map) and fixed cases:
//   - the wire equals XCompactor::compact<T>(source).save_verified<T>() byte
//     for byte, has no holes and no extent table, and passes the independent
//     oracle and load_verified with the source's contents;
//   - save_compact of the loaded wire reproduces it (canonical is a fixpoint)
//     and save_compact_into into a reused string matches;
//   - the source buffer is not changed;
//   - with the default slack (XCompactOptions{}), the same holds except that
//     vectors of at least 8 elements keep size / 8 + 1 zeroed spare elements
//     (checked against the tight wire's sizes), which is the only difference.
#include "fuzz/v2_fuzz_core.hpp"

namespace {
void need(bool ok, const std::string& what) { if (!ok) fail(what); }

// Spare elements of a vector in the slack wire match size / 8 + 1 for sizes
// of at least 8; adds their bytes (plus the most the block padding can
// change) to `extra`.
template <class V>
bool slack_ok(const V& tight, const V& slack, std::size_t& extra) {
    const std::size_t n = tight.size();
    if (slack.size() != n || tight.capacity() != n) return false;
    const std::size_t want = n < 8 ? n : n + n / 8 + 1;
    if (slack.capacity() != want) return false;
    extra += (want - n) * sizeof(typename V::value_type) + (want != n ? 7 : 0); // + padding change
    return true;
}

bool slack_extra(const fz::Root& t, const fz::Root& s, std::size_t& extra) {
    if (!slack_ok(t.items, s.items, extra) || !slack_ok(t.tags, s.tags, extra)) return false;
    if constexpr (requires { t.keys.capacity(); t.attrs.capacity(); })
        if (!slack_ok(t.keys, s.keys, extra) || !slack_ok(t.attrs, s.attrs, extra)) return false;
    for (std::size_t i = 0; i < t.items.size(); ++i)
        if (!slack_ok(t.items[i].vals, s.items[i].vals, extra)) return false;
    return true;
}

// XCompactor on a clone (compact() may take over the source's storage).
std::string reference(Buffer& source) {
    auto clone = typed_load(source.save_verified());
    if (!clone) fail("clone for XCompactor rejected");
    auto compacted = xo::XCompactor::compact<fz::Root>(*clone);
    return compacted.save_verified<fz::Root>();
}

constexpr xo::XCompactOptions kTight{.slack_div = 0};
std::size_t slack_cases = 0;

// Properties shared by the tight and the default (slack) wire.
void check_common(Buffer& b, const std::string& wire, xo::XCompactOptions options, const std::string& semantics,
                  const std::string& where) {
    xo::XWireHeaderV2 h; std::memcpy(&h, wire.data(), kHeader);
    need(h.hole_count == 0 && h.extent_count == 0 && h.root_offset == 48 &&
         wire.size() == kHeader + h.used_bytes - xo::XWIRE_PAYLOAD_BEGIN, where + ": not a compact dense wire");
    const auto o = oracle(wire);
    need(o.ok, where + ": oracle rejects the compact wire: " + o.why);
    auto loaded = typed_load(wire);
    need(bool(loaded), where + ": load_verified rejects the compact wire");
    need(dump(loaded->root()) == semantics, where + ": compact wire changed the contents");
    need(loaded->save_compact(options) == wire, where + ": compact wire is not a fixpoint");
    need(loaded->save_verified() == wire, where + ": compact wire does not re-save identically");
    std::string into = "previous contents that are longer than nothing";
    b.save_compact_into(into, options);
    need(into == wire, where + ": save_compact_into differs");
    b.save_compact_into(into, options); // reused capacity
    need(into == wire, where + ": save_compact_into (reused) differs");
    std::string tiny;
    tiny.reserve(1);
    b.save_compact_into(tiny, options);
    need(tiny == wire, where + ": save_compact_into (growing) differs");
    // Room for all but the last bytes: a write into the existing capacity
    // runs out at the end and the save starts over.
    std::string near_miss(std::size_t(64), char(0x5A));
    near_miss.reserve(wire.size() - 1);
    near_miss.assign(near_miss.capacity(), char(0x5A));
    b.save_compact_into(near_miss, options);
    need(near_miss == wire, where + ": save_compact_into (near miss) differs");
}

void check(Buffer& b, const std::string& where) {
    const auto before = b.copy_bytes();
    const auto size_before = b.get_size();
    const auto semantics = dump(b.root());
    const auto wire = b.save_compact(kTight);
    need(b.copy_bytes() == before && b.get_size() == size_before, where + ": save_compact changed the source");
    need(wire == reference(b), where + ": differs from XCompactor + save_verified");
    check_common(b, wire, kTight, semantics, where + " (tight)");
    // The canonical wire is never larger than the dense one.
    need(wire.size() <= b.save_verified().size(), where + ": compact wire larger than dense");

    // save_verified above may shrink the logical length (normalization).
    const auto before_slack = b.copy_bytes();
    const auto size_slack = b.get_size();
    const auto slack = b.save_compact();
    need(b.copy_bytes() == before_slack && b.get_size() == size_slack, where + ": save_compact (slack) changed the source");
    check_common(b, slack, {}, semantics, where + " (slack)");
    // Slack only adds spare elements to vectors of at least 8 elements.
    auto tight_loaded = typed_load(wire);
    auto slack_loaded = typed_load(slack);
    std::size_t extra = 0;
    need(slack_extra(tight_loaded->root(), slack_loaded->root(), extra), where + ": slack layout differs");
    // Spare elements may fill block padding: the wire grows by at most
    // `extra` and never shrinks.
    need(slack.size() >= wire.size() && slack.size() - wire.size() <= extra,
         where + ": slack wire size does not match the spare elements");
    slack_cases += extra != 0;
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
        smaller += b->save_compact(kTight).size() < b->save_verified().size();
    }
    need(smaller > cases / 2, "compaction rarely shrank the wire");
    need(slack_cases > cases / 4, "slack rarely applied");
    std::cout << "test_save_compact: " << cases << " random models + fixed cases, " << smaller
              << " tight smaller than dense, all tight byte-identical to XCompactor, " << slack_cases
              << " with slack\n";
    return 0;
}
