// libFuzzer target over three more schemas (G5 multi-schema): fixed arrays,
// enums and std::byte blobs; nested maps with string keys and record values;
// three-level nested vectors with bool/float. The first input byte picks the
// schema, the rest is the wire. There is no spec oracle for these schemas, so
// the check is differential: if the wire framing is valid, typed load, raw
// validation and verified receive must agree; if it is not, typed load must
// reject. An accepted wire must re-save byte-identically, and after an edit
// must re-save to a wire that loads again and re-saves identically.
//
// Built three times: as the fuzzer; with XOFFSET_FUZZ_SEED_MAIN as
// fuzz_wire_schemas_seeds, which writes a seed corpus; and without libFuzzer,
// linked with replay_main.cpp, as fuzz_wire_schemas_replay (any compiler).
#include "v2_fuzz_core.hpp"

#include <filesystem>

namespace ms {
enum class Color : std::uint8_t { red, green, blue };
struct Cell {
    Color color{};
    std::uint8_t reserved[3]{};
    std::int32_t value{};
};
struct Fixed {
    std::uint64_t arr[3]{};
    Cell cells[2]{};
    xo::XVector<Cell> grid;
    xo::XSet<std::int16_t> marks;
    xo::XVector<std::byte> blob;
};

struct Inner {
    xo::XString name;
    xo::XSet<std::uint32_t> tags;
    double score{};
};
struct Maps {
    xo::XMap<xo::XString, xo::XVector<xo::XString>> index;
    xo::XMap<std::uint64_t, Inner> by_id;
};

struct Deep {
    xo::XVector<xo::XVector<xo::XVector<std::uint16_t>>> cube;
    xo::XString text;
    bool flag{};
    float ratio{};
};
} // namespace ms

XOFFSET_REGISTER_SCHEMA_NAME(ms::Fixed, "fuzz.schemas.fixed")
XOFFSET_REGISTER_SCHEMA_NAME(ms::Maps, "fuzz.schemas.maps")
XOFFSET_REGISTER_SCHEMA_NAME(ms::Deep, "fuzz.schemas.deep")

namespace XOffsetDatastructure {
template<> struct wire_record_layout<ms::Cell> { static constexpr bool ordinary_members = true; };
template<> struct wire_record_layout<ms::Fixed> { static constexpr bool ordinary_members = true; };
template<> struct wire_record_layout<ms::Inner> { static constexpr bool ordinary_members = true; };
template<> struct wire_record_layout<ms::Maps> { static constexpr bool ordinary_members = true; };
template<> struct wire_record_layout<ms::Deep> { static constexpr bool ordinary_members = true; };
}

namespace {
// Header and framing rules of the V2 format, independent of the schema: the
// same checks as the oracle's header stage in v2_fuzz_core.hpp.
template <class Root>
bool framing_valid(const std::string& wire) {
    if (wire.size() < kHeader) return false;
    xo::XWireHeaderV2 h;
    std::memcpy(&h, wire.data(), kHeader);
    if (std::memcmp(h.magic, xo::XWIRE_MAGIC_V2, 8) != 0) return false;
    if (h.format_major != 2 || h.format_minor != 0 || h.header_size != kHeader ||
        h.endian_tag != 0x01020304u || h.flags != 0 || h.crc32c != 0 || h.reserved0 != 0)
        return false;
    if (h.root_type_id != xo::detail::wire_root_type_id<Root>() ||
        h.schema_hash_lo != xo::detail::wire_schema_hash<Root>() || h.schema_hash_hi != 0)
        return false;
    const std::uint64_t body = wire.size() - kHeader;
    if (h.used_bytes > kMaxPayload || h.extent_count > body / 8) return false;
    if (h.used_bytes > kLimit) return false;
    const auto* table = reinterpret_cast<const unsigned char*>(wire.data()) + kHeader;
    std::uint64_t stored = h.used_bytes, end = 0;
    if (h.extent_count) {
        stored = 0;
        bool gap = false;
        for (std::uint64_t i = 0; i < h.extent_count; ++i) {
            std::uint32_t off, b; std::memcpy(&off, table + i * 8, 4); std::memcpy(&b, table + i * 8 + 4, 4);
            if (b == 0 || off % 8 || b % 8 || (i == 0 ? off != 0 : off <= end) || std::uint64_t(off) + b > h.used_bytes)
                return false;
            gap |= i != 0;
            end = std::uint64_t(off) + b; stored += b;
        }
        if (end < h.used_bytes) gap = true;
        if (!gap) return false;
    }
    const auto tbytes = h.extent_count * 8;
    if (stored > body - tbytes) return false;
    const auto rest = body - tbytes - stored;
    if (h.hole_count > rest / 8 || h.hole_count * 8 != rest) return false;
    if (h.root_offset >= h.used_bytes) return false;
    std::uint64_t hole_sum = 0;
    const auto* holes = table + tbytes + stored;
    for (std::uint64_t i = 0; i < h.hole_count; ++i) {
        std::uint32_t b; std::memcpy(&b, holes + i * 8 + 4, 4); hole_sum += b;
    }
    return hole_sum == h.hole_bytes;
}

std::string text(std::mt19937_64& rng) { return random_text(rng, 40); }

void edit(ms::Fixed& r, std::mt19937_64& rng) {
    r.arr[rng() % 3] = rng();
    r.cells[rng() % 2].value = std::int32_t(rng());
    if (!r.grid.empty() && rng() % 2) r.grid.pop_back();
    auto& c = r.grid.emplace_back();
    c.color = ms::Color(rng() % 3);
    c.value = std::int32_t(rng());
    r.marks.insert(std::int16_t(rng()));
    r.blob.resize(rng() % 64);
}
void edit(ms::Maps& r, std::mt19937_64& rng) {
    auto& list = r.index[text(rng)];
    list.emplace_back(text(rng));
    auto& inner = r.by_id[rng() % 16];
    inner.name = text(rng);
    inner.tags.insert(std::uint32_t(rng() % 100));
    inner.score = double(rng() % 1000) / 7.0;
    if (rng() % 3 == 0 && !r.by_id.empty()) r.by_id.erase(r.by_id.begin()->first);
}
void edit(ms::Deep& r, std::mt19937_64& rng) {
    if (r.cube.empty() || rng() % 3 == 0) r.cube.emplace_back();
    auto& plane = r.cube[rng() % r.cube.size()];
    if (plane.empty() || rng() % 2) plane.emplace_back();
    auto& row = plane[rng() % plane.size()];
    for (int i = int(rng() % 8); i > 0; --i) row.push_back(std::uint16_t(rng()));
    if (rng() % 4 == 0) row.clear();
    r.text = text(rng);
    r.flag = rng() % 2;
    r.ratio = float(rng() % 100) / 3.0f;
}

template <class Root>
void check(const std::string& w, std::mt19937_64& rng) {
    std::string terr, rerr, verr;
    auto typed = typed_load<Root>(w, &terr);
    if (!framing_valid<Root>(w)) {
        if (typed) fail("typed load accepted a wire with invalid framing");
        return;
    }
    const bool raw = raw_accepts<Root>(w, &rerr);
    const bool recv = receive_accepts<Root>(w, &verr);
    if (raw != bool(typed) || recv != bool(typed))
        fail(std::string("acceptors disagree: typed=") + (typed ? "accept" : "reject(" + terr + ")") +
             " raw=" + (raw ? "accept" : "reject(" + rerr + ")") +
             " receive=" + (recv ? "accept" : "reject(" + verr + ")"));
    if (!typed) return;
    if (typed->save_verified() != w) fail("accepted wire does not re-save byte-identically");
    edit(typed->root(), rng);
    const auto saved = typed->save_verified();
    auto again = typed_load<Root>(saved);
    if (!again) fail("re-saved wire after editing an accepted input was rejected");
    if (again->save_verified() != saved) fail("write-read-write not byte stable after edit");
}

template <class Root>
std::string seed(std::mt19937_64& rng) {
    auto b = xo::TypedXBuffer<Root>::create(4096,
        xo::XBuffer::maximum_capacity(kLimit).with_growth(xo::XGrowPolicy::Automatic));
    for (int i = int(1 + rng() % 12); i > 0; --i) edit(b.root(), rng);
    return b.save_verified();
}
// Seed corpus: 48 wires over the three schemas, each prefixed by its tag byte.
// Returns the number written.
int write_schema_seeds(const std::string& dir) {
    std::filesystem::create_directories(dir);
    std::mt19937_64 rng(20261021);
    int written = 0;
    for (int i = 0; i < 48; ++i) {
        const char tag = char(i % 3);
        const std::string w = tag == 0 ? seed<ms::Fixed>(rng) : tag == 1 ? seed<ms::Maps>(rng) : seed<ms::Deep>(rng);
        // Every seed must pass the check itself.
        std::mt19937_64 local(i);
        if (tag == 0) check<ms::Fixed>(w, local); else if (tag == 1) check<ms::Maps>(w, local); else check<ms::Deep>(w, local);
        const auto path = dir + "/seed-" + std::to_string(i) + ".bin";
        std::FILE* f = std::fopen(path.c_str(), "wb");
        if (!f || std::fwrite(&tag, 1, 1, f) != 1 || std::fwrite(w.data(), 1, w.size(), f) != w.size() ||
            std::fclose(f) != 0)
            fail("cannot write " + path);
        ++written;
    }
    return written;
}
} // namespace

extern "C" int XOffsetFuzzWriteSeeds(const char* dir) {
    std::cout << "fuzz_wire_schemas: wrote " << write_schema_seeds(dir) << " seeds to " << dir << "\n";
    return 0;
}

#ifdef XOFFSET_FUZZ_SEED_MAIN
int main(int argc, char** argv) {
    if (argc != 2) { std::cerr << "usage: fuzz_wire_schemas_seeds <dir>\n"; return 2; }
    return XOffsetFuzzWriteSeeds(argv[1]);
}
#else
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    inject_crash_drill(data, size);
    if (size == 0) return 0;
    const std::string wire(reinterpret_cast<const char*>(data) + 1, size - 1);
    std::uint64_t h = 0xcbf29ce484222325ull;
    for (std::size_t i = 0; i < size; ++i) h = (h ^ data[i]) * 0x100000001b3ull;
    std::mt19937_64 rng(h);
    switch (data[0] % 3) {
    case 0: check<ms::Fixed>(wire, rng); break;
    case 1: check<ms::Maps>(wire, rng); break;
    default: check<ms::Deep>(wire, rng); break;
    }
    return 0;
}
#endif
