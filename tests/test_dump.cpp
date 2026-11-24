#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <limits>
#include <string>
#include <string_view>

#include "../xoffset_dump.hpp"
#include "../xoffset_receiver.hpp"

using namespace XOffsetDatastructure;

namespace dump_test {

enum class Kind : std::uint8_t { none = 0, hero = 2 };

struct CompositeKey {
    std::uint32_t zone{};
    std::uint32_t local_id{};
    bool operator<(const CompositeKey& o) const noexcept {
        return zone != o.zone ? zone < o.zone : local_id < o.local_id;
    }
    bool operator==(const CompositeKey& o) const noexcept = default;
};

struct BaseEntity {
    std::uint64_t id{};
};

struct DerivedNote : BaseEntity {
    std::uint32_t code{};
};

struct Pos {
    float x{};
    float y{};
};

struct Record {
    std::uint64_t id{};
    bool active{};
    std::byte tag{};
    Kind kind{};
    double ratio{};
    XString name;
    std::array<std::uint16_t, 2> pair{};
    XVector<Pos> path;
    XSet<std::int32_t> flags;
    XMap<XString, std::int32_t> stats;
    XMap<std::uint32_t, XString> labels;
    XMap<CompositeKey, std::uint32_t> cells;
};

} // namespace dump_test

XOFFSET_RECORD(dump_test::CompositeKey, zone, local_id)
XOFFSET_RECORD(dump_test::BaseEntity, id)
XOFFSET_RECORD_DERIVED(dump_test::DerivedNote, (dump_test::BaseEntity), code)
XOFFSET_RECORD(dump_test::Pos, x, y)
XOFFSET_RECORD(dump_test::Record,
               id, active, tag, kind, ratio, name, pair, path, flags, stats, labels, cells)
XOFFSET_REGISTER_SCHEMA_NAME(dump_test::Record, "test.dump.Record")

namespace {

int failures = 0;

void need(bool ok, const std::string& what) {
    if (!ok) {
        std::cerr << "FAIL: " << what << "\n";
        ++failures;
    }
}

bool has_sub(std::string_view text, std::string_view sub) {
    return text.find(sub) != std::string_view::npos;
}

} // namespace

int main() {
    auto cap = XBuffer::maximum_capacity(1u << 20).with_growth(XGrowPolicy::Automatic);
    auto created = TypedXBuffer<dump_test::Record>::try_create(4096, cap);
    need(bool(created), "try_create dump record");
    if (!created) return 1;
    auto& buf = *created;

    const bool ok = buf.edit([](dump_test::Record& r) {
        r.id = 1001;
        r.active = true;
        r.tag = std::byte{7};
        r.kind = dump_test::Kind::hero;
        r.ratio = 1.5;
        r.name = "A\"B\\C\n\t\x01\xE4\xB8\xAD";
        r.pair = {10, 20};
        r.path.push_back({1.0f, -2.5f});
        r.path.push_back({std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity()});
        r.flags.insert(3);
        r.flags.insert(1);
        r.flags.insert(9);
        r.stats["hp"] = 100;
        r.stats["mp"] = 50;
        r.labels[7u] = "seven";
        r.labels[2u] = "two";
        r.cells[dump_test::CompositeKey{1, 2}] = 42;
        r.cells[dump_test::CompositeKey{1, 5}] = 88;
    });
    need(ok && buf.ok(), "populate dump record");

    // Compact JSON dump.
    const std::string compact = buf.read([](const dump_test::Record& r) { return to_json(r); });
    need(has_sub(compact, "\"id\":1001"), "field id: " + compact);
    // Verify derived record flattening too.
    {
        dump_test::DerivedNote dn{};
        dn.id = 77;
        dn.code = 5;
        need(to_json(dn) == "{\"id\":77,\"code\":5}", "derived record dump: " + to_json(dn));
    }
    need(has_sub(compact, "\"active\":true"), "bool field");
    need(has_sub(compact, "\"tag\":7"), "std::byte field");
    need(has_sub(compact, "\"kind\":2"), "enum field");
    need(has_sub(compact, "\"ratio\":1.5"), "double field");
    need(has_sub(compact, "\"name\":\"A\\\"B\\\\C\\n\\t\\u0001\xE4\xB8\xAD\""), "escaped string: " + compact);
    need(has_sub(compact, "\"pair\":[10,20]"), "std::array field");
    need(has_sub(compact, "\"path\":[{\"x\":1,\"y\":-2.5},{\"x\":null,\"y\":null}]"), "vector + non-finite float: " + compact);
    need(has_sub(compact, "\"flags\":[1,3,9]"), "set field");
    need(has_sub(compact, "\"stats\":{\"hp\":100,\"mp\":50}"), "string-keyed map: " + compact);
    need(has_sub(compact, "\"labels\":{\"2\":\"two\",\"7\":\"seven\"}"), "int-keyed map: " + compact);
    need(has_sub(compact, "\"cells\":[[{\"zone\":1,\"local_id\":2},42],[{\"zone\":1,\"local_id\":5},88]]"),
         "composite-keyed map: " + compact);

    // Pretty dump + truncation limits.
    XDumpOptions trunc_opts;
    trunc_opts.indent = 2;
    trunc_opts.max_elements = 1;
    trunc_opts.max_string_bytes = 3;
    const std::string pretty = buf.read([&](const dump_test::Record& r) { return to_json(r, trunc_opts); });
    need(has_sub(pretty, "\n  \"id\": 1001"), "pretty indentation: " + pretty);
    need(has_sub(pretty, "\"name\": \"A\\\"B...\""), "string truncation: " + pretty);
    // Verify truncation mid-UTF-8 codepoint backs up cleanly to a valid boundary.
    {
        XDumpOptions utf8_opts;
        utf8_opts.max_string_bytes = 9; // lands inside the 3-byte UTF-8 char at bytes 8..10
        const std::string utf8_json = buf.read([&](const dump_test::Record& r) { return to_json(r.name, utf8_opts); });
        need(utf8_json == "\"A\\\"B\\\\C\\n\\t\\u0001...\"", "utf8-safe truncation: " + utf8_json);
    }
    need(has_sub(pretty, "\"... (2 more)\""), "sequence truncation: " + pretty);
    need(has_sub(pretty, "\"__truncated__\": 1"), "map truncation: " + pretty);
    need(has_sub(pretty, "\"... (1 more)\""), "composite map truncation: " + pretty);

    // Dump through a read-only XReceiver::view (bounds tier, no copy).
    const std::string wire = buf.save_verified();
    XReceiver<XLink::from_client> rx(cap);
    auto viewed = rx.view<dump_test::Record>(wire);
    need(bool(viewed), "view for dump");
    if (viewed) {
        const std::string from_view = to_json(*viewed);
        need(from_view == compact, "view dump matches buffer dump");
    }

    // Wire header summary JSON (valid, bad magic, truncated).
    const std::string hdr = dump_wire_header_json(wire);
    need(has_sub(hdr, "\"valid_magic\":true"), "wire header valid_magic: " + hdr);
    need(has_sub(hdr, "\"format_major\":3"), "wire header format_major: " + hdr);
    need(has_sub(hdr, "\"format_minor\":0"), "wire header format_minor: " + hdr);
    need(has_sub(hdr, "\"header_bytes\":40"), "wire header header_bytes: " + hdr);
    need(has_sub(hdr, "\"schema_hash\":\"0x"), "wire header schema_hash hex: " + hdr);
    need(has_sub(hdr, "\"wire_bytes\":" + std::to_string(wire.size())), "wire header wire_bytes: " + hdr);

    const std::string short_hdr = dump_wire_header_json(std::string_view(wire).substr(0, 12));
    need(has_sub(short_hdr, "\"wire_bytes\":12") && has_sub(short_hdr, "\"truncated\":true"),
         "truncated wire header: " + short_hdr);

    std::string bad_wire = wire;
    bad_wire[0] = 'Z';
    const std::string bad_hdr = dump_wire_header_json(bad_wire);
    need(has_sub(bad_hdr, "\"valid_magic\":false"), "bad magic wire header: " + bad_hdr);

    if (failures) {
        std::cerr << "test_dump: " << failures << " failure(s)\n";
        return 1;
    }
    std::cout << "test_dump: ok\n";
    return 0;
}
