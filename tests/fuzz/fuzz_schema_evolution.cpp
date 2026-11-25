// libFuzzer target for schema evolution (xoffset_evolution.hpp, E5): a program
// working in generation v3 of a message that still loads stored data of the
// older generations v1 and v2 (its lineage). The first input byte picks the
// mode, the rest is its input:
//
//   0  load: the rest is a stored image. evolve_load_verified<v3> must accept
//      it exactly when load_verified of the generation its header names
//      accepts it and the upgrade succeeds; an accepted result must verify as
//      v3 and re-save byte-identically, and converting it back down to the
//      image's generation must give the image's object (compared in a
//      canonical layout: both sides rebuilt by the conversion).
//   1  convert: the rest seeds a random v3 object. Converted down to every
//      generation with evolve (what a caller does that still writes an older
//      image), it must load_verified as that generation, or the conversion
//      must fail with an XException for a value that generation cannot hold;
//      down, up, down again must give the same object.
//   2  handshake: the rest is a hello (or an ack). accept_schema_hello never
//      throws, answers every working entry, writes no retired verdict and
//      never accepts another generation; read_schema_ack never throws.
//
// Built like fuzz_wire_schemas.cpp: as the fuzzer, with XOFFSET_FUZZ_SEED_MAIN
// as fuzz_schema_evolution_seeds, and linked with replay_main.cpp as
// fuzz_schema_evolution_replay (any compiler with reflection).
#include "v2_fuzz_core.hpp"
#include "../../xoffset_evolution.hpp"

#include <filesystem>

namespace ev {
namespace v1 {
enum class Rarity : std::uint8_t { common, rare };
struct Item { std::int32_t id; Rarity rarity; std::uint8_t pad[3]; };
struct Player {
    xo::XString name;
    std::int32_t hp;
    xo::XVector<Item> items;
    xo::XSet<std::int32_t> tags;
};
} // namespace v1
namespace v2 {
enum class Rarity : std::uint8_t { common, rare, epic };
struct Item { std::int32_t id; Rarity rarity; std::uint8_t pad[3]; std::int32_t n; };
struct Player {
    xo::XString name;
    std::int32_t hp;
    std::int32_t mp;
    xo::XVector<Item> items;
    xo::XSet<std::int32_t> tags;
    xo::XMap<xo::XString, std::int32_t> stats;
};
} // namespace v2
namespace v3 {
using Rarity = v2::Rarity;
struct Item { std::int32_t n; Rarity rarity; std::uint8_t pad[3]; std::int32_t id; std::int32_t quality; };
struct Player {
    xo::XString nick;
    std::int64_t hp;
    std::int32_t mp;
    std::int32_t level;
    xo::XVector<Item> items;
    xo::XSet<std::int32_t> tags;
    xo::XMap<xo::XString, std::int32_t> stats;
    xo::XMap<std::int32_t, xo::XVector<Rarity>> drops;
};
} // namespace v3
} // namespace ev

XOFFSET_REGISTER_SCHEMA_NAME(ev::v1::Player, "fuzz.evolution.Player")
XOFFSET_REGISTER_SCHEMA_NAME(ev::v2::Player, "fuzz.evolution.Player")
XOFFSET_REGISTER_SCHEMA_NAME(ev::v3::Player, "fuzz.evolution.Player")

// v2 -> v3: hp widens, name becomes nick.
template <> struct XOffsetDatastructure::xevolve<ev::v2::Player, ev::v3::Player> {
    static constexpr std::string_view handles[] = {"hp", "name", "nick"};
    static void upgrade(const ev::v2::Player& from, ev::v3::Player& to, XEvolveContext& ctx) {
        to.hp = ctx.widen<std::int64_t>(from.hp);
        to.nick.assign(from.name.view());
    }
    static void downgrade(const ev::v3::Player& from, ev::v2::Player& to, XEvolveContext& ctx) {
        to.hp = ctx.checked_narrow<std::int32_t>(from.hp, "hp");
        to.name.assign(from.nick.view());
    }
};

XOFFSET_SCHEMA_LINEAGE(ev::v3::Player, ev::v1::Player, ev::v2::Player)

namespace {
namespace xe = xo::detail::evolve;
using W = ev::v3::Player;
constexpr std::size_t kVersions = xe::version_count<W>;
static_assert(kVersions == 3 && xe::working_index<W> == 2);

template <std::size_t I>
using V = xe::version_t<W, I>;

xo::TypedXBuffer<W> fresh() {
    return xo::TypedXBuffer<W>::create(4096,
        xo::XBuffer::maximum_capacity(kLimit).with_growth(xo::XGrowPolicy::Automatic));
}

template <typename F>
bool throws_xexception(F&& f, std::string* why = nullptr) {
    try {
        f();
    } catch (const xo::XException& e) {
        if (why) *why = e.what();
        return true;
    }
    return false;
}

// A canonical layout of a version-I object: rebuilt by the conversion into a
// fresh buffer, so two objects with equal contents give equal bytes.
template <typename T>
std::string canonical(xo::TypedXBuffer<T>& b) {
    auto copy = xo::evolve<T>(b);
    return copy.save_compact();
}

template <std::size_t I, typename F>
void for_versions(F&& f) {
    if constexpr (I < kVersions) {
        f(std::integral_constant<std::size_t, I>{});
        for_versions<I + 1>(f);
    }
}

// ---- mode 0: receive ----

void check_receive(const std::string& wire) {
    const auto peek = xo::peek_wire_fingerprint(wire);
    const std::size_t index = peek ? xe::find_version<W>(peek.fingerprint.schema_hash) : xe::npos;
    const bool ours = index != xe::npos && peek.fingerprint.root_type_id == xo::wire_root_type_id_v<W>();
    const auto cap = xo::XBuffer::maximum_capacity(kLimit);

    std::optional<xo::TypedXBuffer<W>> got;
    std::string why;
    if (!throws_xexception([&] { got.emplace(xo::evolve_load_verified<W>(wire, cap)); }, &why) == !got)
        fail("evolve_load_verified neither returned nor threw XException");

    if (!ours) {
        // Not a version of the lineage: the same answer as load_verified<W>.
        if (bool(got) != bool(typed_load<W>(wire)))
            fail("evolve_load_verified and load_verified<W> disagree on a wire of no lineage version");
        return;
    }
    xe::with_index<kVersions>(index, [&](auto I) {
        using Sent = V<I()>;
        auto sent = typed_load<Sent>(wire);
        if (!sent) {
            if (got) fail("evolve_load_verified accepted a wire its own version rejects");
            return 0;
        }
        std::optional<xo::TypedXBuffer<W>> up;
        throws_xexception([&] {
            if constexpr (I() == xe::working_index<W>) up.emplace(std::move(*sent));
            else up.emplace(xe::chain<W, I(), xe::working_index<W>>(*sent, {}));
        });
        if (bool(up) != bool(got))
            fail("evolve_load_verified disagrees with load_verified + upgrade (" + why + ")");
        if (!got) return 0;
        const std::string saved = got->save_verified();
        auto again = typed_load<W>(saved);
        if (!again) fail("upgraded object does not verify as the working version");
        if (again->save_verified() != saved) fail("upgraded object does not re-save byte-identically");
        if (canonical(*got) != canonical(*up)) fail("evolve_load_verified and the step chain built different objects");
        if constexpr (I() != xe::working_index<W>) {
            // Everything an old version can hold survives the round trip.
            auto sent_again = typed_load<Sent>(wire);
            auto down = xe::chain<W, xe::working_index<W>, I()>(*got, {});
            if (canonical(down) != canonical(*sent_again))
                fail("downgrading an upgraded object did not give the sender's object back");
        }
        return 0;
    });
}

// ---- mode 1: send ----

std::string text(std::mt19937_64& rng) { return random_text(rng, 24); }

void edit(W& p, std::mt19937_64& rng) {
    switch (rng() % 8) {
    case 0: p.nick = text(rng); break;
    case 1: p.hp = rng() % 4 == 0 ? std::int64_t(rng()) : std::int64_t(rng() % 1000); break;
    case 2: p.mp = std::int32_t(rng()); p.level = std::int32_t(rng() % 100); break;
    case 3: {
        auto& it = p.items.emplace_back();
        it.id = std::int32_t(rng() % 1000);
        it.n = std::int32_t(rng() % 50);
        it.quality = std::int32_t(rng() % 5);
        it.rarity = ev::v3::Rarity(rng() % 8 == 0 ? 2 : rng() % 2);
        break;
    }
    case 4: if (!p.items.empty()) p.items.pop_back(); break;
    case 5: p.tags.insert(std::int32_t(rng() % 64)); break;
    case 6: p.stats[text(rng)] = std::int32_t(rng()); break;
    default: p.drops[std::int32_t(rng() % 8)].push_back(ev::v3::Rarity(rng() % 3)); break;
    }
}

xo::TypedXBuffer<W> random_player(std::mt19937_64& rng) {
    auto b = fresh();
    for (int i = int(rng() % 24); i > 0; --i) edit(b.plain_root(), rng);
    return b;
}

// The image of `object` in generation I: its own bytes for the working one,
// else converted down and written with save_compact.
template <std::size_t I>
std::string image_in(xo::TypedXBuffer<W>& object) {
    if constexpr (I == xe::working_index<W>) {
        return object.save_verified();
    } else {
        auto down = xe::chain<W, xe::working_index<W>, I>(object, {});
        return down.save_compact();
    }
}

void check_convert(std::mt19937_64& rng) {
    auto object = random_player(rng);
    for_versions<0>([&](auto I) {
        using Peer = V<I()>;
        std::string wire, why;
        const bool failed = throws_xexception([&] { wire = image_in<I()>(object); }, &why);
        if (failed) {
            // Only a value an old version cannot hold fails: hp out of int32 or
            // an epic item for v1.
            const auto& p = object.plain_root();
            bool lossy = p.hp < INT32_MIN || p.hp > INT32_MAX;
            if constexpr (I() == 0) {
                for (const auto& it : p.items) lossy |= it.rarity == ev::v3::Rarity::epic;
            }
            if (!lossy) fail("downgrade failed for a value the old version holds: " + why);
            return;
        }
        auto received = typed_load<Peer>(wire);
        if (!received) fail("an image converted down does not verify as its generation");
        if (xo::peek_wire_fingerprint(wire).fingerprint != xo::wire_fingerprint_v<Peer>)
            fail("an image converted down carries another generation's fingerprint");
        // The old generation's object goes up and down again unchanged.
        auto back = xo::evolve_load_verified<W>(wire, xo::XBuffer::maximum_capacity(kLimit));
        auto twice = typed_load<Peer>(image_in<I()>(back));
        if (!twice || canonical(*twice) != canonical(*received))
            fail("down, up, down changed the old generation's object");
    });
}

// ---- mode 2: handshake ----

void check_handshake(const std::string& bytes) {
    std::string ack;
    const xo::XSchemaHelloResult r = xo::accept_schema_hello<W>(bytes, ack, "fuzz-server");
    if (!r.valid) {
        if (!ack.empty()) fail("an invalid hello got an ack");
    } else {
        if (ack.empty()) fail("a valid hello without an ack");
        for (const auto& e : r.entries) {
            if (e.verdict == xo::XSchemaVerdict::convert) fail("the retired verdict convert was written");
            if (e.verdict == xo::XSchemaVerdict::same &&
                (e.root_type_id != xo::wire_root_type_id_v<W>() || e.schema_hash != xo::wire_schema_hash_v<W>()))
                fail("a peer of another root type or generation was accepted");
        }
        for_versions<0>([&](auto I) { (void)xo::read_schema_ack<V<I()>>(ack); });
    }
    for_versions<0>([&](auto I) { (void)xo::read_schema_ack<V<I()>>(bytes); });
}

// ---- seeds ----

template <std::size_t I>
std::string old_wire(std::mt19937_64& rng) {
    auto object = random_player(rng);
    object.plain_root().hp = std::int64_t(rng() % 1000);
    for (auto& it : object.plain_root().items) it.rarity = ev::v3::Rarity(rng() % 2);
    return image_in<I>(object);
}

int write_evolution_seeds(const std::string& dir) {
    std::filesystem::create_directories(dir);
    std::mt19937_64 rng(20261027);
    int written = 0;
    auto put = [&](char mode, const std::string& body) {
        const auto path = dir + "/seed-" + std::to_string(written) + ".bin";
        std::FILE* f = std::fopen(path.c_str(), "wb");
        if (!f || std::fwrite(&mode, 1, 1, f) != 1 || std::fwrite(body.data(), 1, body.size(), f) != body.size() ||
            std::fclose(f) != 0)
            fail("cannot write " + path);
        ++written;
    };
    for (int i = 0; i < 12; ++i) {
        put(0, old_wire<0>(rng));
        put(0, old_wire<1>(rng));
        put(0, old_wire<2>(rng));
        put(1, std::to_string(rng()));
    }
    put(2, xo::make_schema_hello<V<0>>("seed-v1"));
    put(2, xo::make_schema_hello<V<1>>("seed-v2"));
    put(2, xo::make_schema_hello<W>("seed-server", xo::XSchemaRole::server));
    return written;
}

void run_one(const std::uint8_t* data, std::size_t size) {
    if (size == 0) return;
    const std::string rest(reinterpret_cast<const char*>(data) + 1, size - 1);
    switch (data[0] % 3) {
    case 0: check_receive(rest); break;
    case 1: {
        std::uint64_t h = 0xcbf29ce484222325ull;
        for (char c : rest) h = (h ^ std::uint8_t(c)) * 0x100000001b3ull;
        std::mt19937_64 rng(h);
        check_convert(rng);
        break;
    }
    default: check_handshake(rest); break;
    }
}
} // namespace

extern "C" int XOffsetFuzzWriteSeeds(const char* dir) {
    const int n = write_evolution_seeds(dir);
    // Every seed must pass the harness itself.
    for (const auto& entry : std::filesystem::directory_iterator(dir)) {
        std::FILE* f = std::fopen(entry.path().c_str(), "rb");
        std::string bytes;
        for (int c; f && (c = std::fgetc(f)) != EOF;) bytes.push_back(char(c));
        if (f) std::fclose(f);
        run_one(reinterpret_cast<const std::uint8_t*>(bytes.data()), bytes.size());
    }
    std::cout << "fuzz_schema_evolution: wrote " << n << " seeds to " << dir << "\n";
    return 0;
}

#ifdef XOFFSET_FUZZ_SEED_MAIN
int main(int argc, char** argv) {
    if (argc != 2) { std::cerr << "usage: fuzz_schema_evolution_seeds <dir>\n"; return 2; }
    return XOffsetFuzzWriteSeeds(argv[1]);
}
#else
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    inject_crash_drill(data, size);
    run_one(data, size);
    return 0;
}
#endif
