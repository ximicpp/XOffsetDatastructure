// Device mesh demo: the six types (types.hpp). OWNER: types thread.
//
// One template per operation, instantiated for each root type through
// with_type(); the per-type content is in gen.hpp (state 0, mutations),
// model.hpp (reference model, field lists) and walk.hpp (digest, compare).
#include <chrono>
#include <cstring>
#include <type_traits>
#include "gen.hpp"
#include "internal.hpp"
#include "walk.hpp"
#include "xmesh/protocol.hpp"

namespace xmesh {
namespace {

using ops::Rng;

// ---- per type ------------------------------------------------------------------------

template <class X> struct Traits;
#define XMESH_TRAITS(X, BUILD, MUTATE)                                                     \
    template <> struct Traits<X> {                                                          \
        static constexpr TypeId id = TypeId::X;                                             \
        using Model = model::X;                                                             \
        template <class Fam, class R> static void build(R& r, Rng& g, Scale sc) { gen::BUILD<Fam>(r, g, sc); } \
        template <class Fam, class R> static void change(R& r, Rng& g) { gen::MUTATE<Fam>(r, g); }          \
    };
XMESH_TRAITS(Document, build_document, mutate_document)
XMESH_TRAITS(Order, build_order, mutate_order)
XMESH_TRAITS(SensorSeries, build_sensor, mutate_sensor)
XMESH_TRAITS(KvConfig, build_kv, mutate_kv)
XMESH_TRAITS(SceneTree, build_scene, mutate_scene)
XMESH_TRAITS(ChatRoom, build_chat, mutate_chat)
#undef XMESH_TRAITS

template <class F>
decltype(auto) with_type(TypeId type, F&& f) {
    switch (type) {
        case TypeId::Order: return f(std::type_identity<Order>{});
        case TypeId::SensorSeries: return f(std::type_identity<SensorSeries>{});
        case TypeId::KvConfig: return f(std::type_identity<KvConfig>{});
        case TypeId::SceneTree: return f(std::type_identity<SceneTree>{});
        case TypeId::ChatRoom: return f(std::type_identity<ChatRoom>{});
        case TypeId::Document: break;
    }
    return f(std::type_identity<Document>{});
}

// Stream purposes.
constexpr std::uint64_t kMakeStream = 0x6d616b65;     // "make"
constexpr std::uint64_t kMutateStream = 0x6d757461;   // "muta"

template <class R>
void set_edit_count(R& r) {
    if constexpr (requires { r.edit_count; }) r.edit_count = std::uint32_t(r.trail.size());
}

// State 0, on either model.
template <class Fam, class X, class R>
void state0(R& r, std::uint64_t seed, Scale scale, std::string_view creator) {
    Rng g = ops::stream(kMakeStream + std::uint64_t(scale), std::uint16_t(Traits<X>::id), seed, 0, {});
    Traits<X>::template build<Fam>(r, g, scale);
    gen::mark<Fam>(r, 0, creator);
    set_edit_count(r);
}

// Mutation step.hop, on either model.
template <class Fam, class X, class R>
void change(R& r, std::uint64_t seed, const Step& step) {
    Rng g = ops::stream(kMutateStream, std::uint16_t(Traits<X>::id), seed, step.hop, step.device);
    Traits<X>::template change<Fam>(r, g);
    gen::mark<Fam>(r, step.hop, step.device);
    set_edit_count(r);
}

// ---- limits ---------------------------------------------------------------------------

struct Limits {
    std::uint32_t data;      // data_limit: longest data part accepted
    std::uint32_t used;      // the largest payload (used_end) a received object may have
    std::uint32_t headroom;  // the largest arena growth of one mutation
};

// Set from the self-test (`xmesh_selftest --limits`: the largest wire,
// payload and growth of one mutation over routes of kMaxRoute stops, both
// save modes) with a margin of 2x or more: e.g. Order large reaches a 1.6 MB
// wire, a 2.1 MB payload and grows 1.0 MB in one mutation (its 13000-line
// vector doubling). receive_cap = used + headroom.
constexpr Limits kLimits[6][3] = {
    // small: data, used, headroom      medium                                   large
    {{64u << 10, 128u << 10, 64u << 10}, {512u << 10, 1u << 20, 256u << 10}, {4u << 20, 6u << 20, 2u << 20}},    // Document
    {{64u << 10, 128u << 10, 64u << 10}, {512u << 10, 1u << 20, 256u << 10}, {4u << 20, 6u << 20, 2u << 20}},    // Order
    {{64u << 10, 128u << 10, 64u << 10}, {512u << 10, 1u << 20, 256u << 10}, {8u << 20, 10u << 20, 2u << 20}},   // SensorSeries
    {{64u << 10, 128u << 10, 64u << 10}, {512u << 10, 1u << 20, 256u << 10}, {4u << 20, 6u << 20, 2u << 20}},    // KvConfig
    {{64u << 10, 128u << 10, 64u << 10}, {512u << 10, 1u << 20, 256u << 10}, {4u << 20, 6u << 20, 2u << 20}},    // SceneTree
    {{64u << 10, 128u << 10, 64u << 10}, {512u << 10, 1u << 20, 256u << 10}, {4u << 20, 6u << 20, 2u << 20}},    // ChatRoom
};

const Limits& limits(TypeId type, Scale scale) noexcept {
    const auto t = std::size_t(type) - 1;
    const auto s = std::size_t(scale);
    return kLimits[t < 6 ? t : 0][s < 3 ? s : 0];
}

XBuffer::MaximumCapacity cap_of(std::uint32_t bytes) {
    return XBuffer::maximum_capacity(bytes).with_growth(XGrowPolicy::Automatic);
}

std::uint64_t used_end(const XBuffer& b) noexcept {
    const auto* a = b.arena();
    return a ? std::uint64_t(a->used_end) : 0;
}

template <class X>
Result<Object> make_with_cap(std::uint64_t seed, Scale scale, std::string_view creator, std::uint32_t cap) {
#if XMESH_EXCEPTIONS
    try {
#endif
        XBuffer buf = XBuffer::create<X>(4096, cap_of(cap));
        state0<XFamily, X>(buf.root<X>(), seed, scale, creator);
        return Object(Traits<X>::id, std::move(buf));
#if XMESH_EXCEPTIONS
    } catch (const std::exception& e) {
        return Error{std::string("make_object: ") + e.what()};
    }
#endif
}

template <class X>
Result<std::string> save_as(XBuffer& buf, SaveMode mode, XWireEmission emission = XWireEmission::free_runs) {
#if XMESH_EXCEPTIONS
    try {
#endif
        if (mode == SaveMode::Compact) return buf.save_compact<X>();
        return buf.save_verified<X>(XSaveOptions{emission});
#if XMESH_EXCEPTIONS
    } catch (const std::exception& e) {
        return Error{std::string("save: ") + e.what()};
    }
#endif
}

std::string trail_text(std::span<const std::string> names) {
    std::string out;
    const std::size_t n = names.size();
    const std::size_t from = n > 6 ? n - 5 : 0;
    if (from) out = names[0] + " > ... > ";
    for (std::size_t i = from; i < n; ++i) {
        if (i > from) out += " > ";
        out += names[i];
    }
    return out;
}

template <class R>
std::string trail_of(const R& r) {
    std::vector<std::string> names;
    for (const auto& m : r.trail) names.emplace_back(m.device.view());
    return trail_text(names);
}

} // namespace

// ---- types.hpp ----------------------------------------------------------------------------

std::uint32_t data_limit(TypeId type, Scale scale) noexcept { return limits(type, scale).data; }
std::uint32_t receive_cap(TypeId type, Scale scale) noexcept {
    const Limits& l = limits(type, scale);
    return l.used + l.headroom;
}

namespace internal {
std::uint64_t used_bytes(const Object& object) noexcept { return used_end(object.buffer()); }
std::uint32_t mutation_headroom(TypeId type, Scale scale) noexcept { return limits(type, scale).headroom; }
Scale scale_of(const Object& object) noexcept {
    const auto cap = object.buffer().maximum_capacity();
    for (Scale s : {Scale::Small, Scale::Medium, Scale::Large})
        if (cap == receive_cap(object.type(), s)) return s;
    return Scale::Large;
}
} // namespace internal

Result<Object> make_object(TypeId type, std::uint64_t seed, Scale scale, std::string_view creator) {
    if (!to_type(std::uint16_t(type)) || !to_scale(std::uint8_t(scale))) return Error{"make_object: bad type or scale"};
    return with_type(type, [&](auto tag) {
        using X = typename decltype(tag)::type;
        return make_with_cap<X>(seed, scale, creator, receive_cap(type, scale));
    });
}

Result<Object> receive_object(TypeId type, Scale scale, std::string_view wire) {
    if (!to_type(std::uint16_t(type)) || !to_scale(std::uint8_t(scale))) return Error{"receive: bad type or scale"};
    if (wire.size() > data_limit(type, scale))
        return Error{"receive: " + std::to_string(wire.size()) + " bytes over the limit of " +
                     std::to_string(data_limit(type, scale))};
    return with_type(type, [&](auto tag) -> Result<Object> {
        using X = typename decltype(tag)::type;
        const std::span<const std::byte> bytes(reinterpret_cast<const std::byte*>(wire.data()), wire.size());
        XResult<XBuffer> in = XBuffer::try_load_verified<X>(bytes, cap_of(receive_cap(type, scale)));
        if (!in) return Error{"load_verified: " + in.error().message};
        return Object(type, std::move(*in));
    });
}

std::uint64_t content_digest(Object& object) {
    return with_type(object.type(), [&](auto tag) {
        using X = typename decltype(tag)::type;
        return walk::digest_of(std::uint16_t(object.type()), object.root<X>());
    });
}

Status mutate(Object& object, std::uint64_t seed, const Step& step) {
    if (step.hop == 0) return Error{"mutate: hop 0 is state 0, not a mutation"};
    const Scale scale = internal::scale_of(object);
    const std::uint64_t cap = object.buffer().maximum_capacity();
    const std::uint64_t used = used_end(object.buffer());
    const std::uint64_t need = internal::mutation_headroom(object.type(), scale);
    if (used > cap || cap - used < need)
        return Error{"mutate: no room for a mutation (" + std::to_string(used) + " of " + std::to_string(cap) +
                     " bytes used, " + std::to_string(need) + " needed)"};
    return with_type(object.type(), [&](auto tag) -> Status {
        using X = typename decltype(tag)::type;
#if XMESH_EXCEPTIONS
        try {
#endif
            change<XFamily, X>(object.root<X>(), seed, step);
            return {};
#if XMESH_EXCEPTIONS
        } catch (const std::exception& e) {
            return Error{std::string("mutate: ") + e.what()};
        }
#endif
    });
}

Status check_against_model(Object& object, std::uint64_t seed, Scale scale, std::span<const Step> route,
                           std::uint32_t upto) {
    if (route.empty() || upto >= route.size())
        return Error{"model: hop " + std::to_string(upto) + " outside a route of " + std::to_string(route.size())};
    return with_type(object.type(), [&](auto tag) -> Status {
        using X = typename decltype(tag)::type;
        typename Traits<X>::Model m;
        state0<ModelFamily, X>(m, seed, scale, route[0].device);
        for (std::uint32_t k = 1; k <= upto; ++k) {
            const Step& st = route[k];
            if (st.hop != k) return Error{"model: route[" + std::to_string(k) + "] has hop " + std::to_string(st.hop)};
            change<ModelFamily, X>(m, seed, st);
        }
        std::string diff = walk::compare(type_name(object.type()), object.root<X>(), m);
        if (!diff.empty()) return Error{std::move(diff)};
        return {};
    });
}

Result<std::string> save(Object& object, SaveMode mode) {
    return with_type(object.type(), [&](auto tag) {
        using X = typename decltype(tag)::type;
        return save_as<X>(object.buffer(), mode);
    });
}

std::string summary(Object& object) {
    char head[160];
    std::string trail;
    with_type(object.type(), [&](auto tag) {
        using X = typename decltype(tag)::type;
        const X& r = object.root<X>();
        trail = trail_of(r);
        if constexpr (std::is_same_v<X, Document>)
            std::snprintf(head, sizeof head, "Document rev %u, %zu paragraphs, %zu tags, %zu comments",
                          unsigned(r.revision), std::size_t(r.paragraphs.size()), std::size_t(r.tags.size()),
                          std::size_t(r.comments.size()));
        else if constexpr (std::is_same_v<X, Order>)
            std::snprintf(head, sizeof head, "Order %zu lines, %s, total %lld", std::size_t(r.lines.size()),
                          r.coupon.size() ? "coupon" : "no coupon", static_cast<long long>(r.total_cents));
        else if constexpr (std::is_same_v<X, SensorSeries>)
            std::snprintf(head, sizeof head, "SensorSeries %zu samples, min %g max %g", std::size_t(r.samples.size()),
                          r.stats[0], r.stats[1]);
        else if constexpr (std::is_same_v<X, KvConfig>)
            std::snprintf(head, sizeof head, "KvConfig gen %u, %zu entries, %zu flags", unsigned(r.generation),
                          std::size_t(r.entries.size()), std::size_t(r.feature_flags.size()));
        else if constexpr (std::is_same_v<X, SceneTree>)
            std::snprintf(head, sizeof head, "SceneTree %zu nodes, next id %u", std::size_t(r.nodes.size()),
                          unsigned(r.next_id));
        else
            std::snprintf(head, sizeof head, "ChatRoom %zu members, %zu messages, %zu pinned",
                          std::size_t(r.members.size()), std::size_t(r.messages.size()), std::size_t(r.pinned.size()));
    });
    return std::string(head) + ", trail " + trail;
}

Result<std::vector<HopExpect>> replay(TypeId type, std::uint64_t seed, Scale scale, SaveMode mode,
                                      std::span<const Step> route) {
    if (route.size() < 2 || route.size() > kMaxRoute)
        return Error{"replay: a route has 2.." + std::to_string(kMaxRoute) + " stops"};
    std::vector<HopExpect> out;
    out.reserve(route.size() - 1);
    Result<Object> obj = make_object(type, seed, scale, route[0].device);
    if (!obj) return std::move(obj).error();
    for (std::uint32_t k = 0;; ++k) {
        if (k > 0) {
            const Step& st = route[k];
            if (st.hop != k) return Error{"replay: route[" + std::to_string(k) + "] has hop " + std::to_string(st.hop)};
            XMESH_TRY(mutate(*obj, seed, st));
        }
        Result<std::string> wire = save(*obj, mode);
        if (!wire) return std::move(wire).error();
        out.push_back(HopExpect{content_digest(*obj), protocol::wire_digest(*wire), std::uint32_t(wire->size())});
        if (k + 2 == route.size()) break;
        obj = receive_object(type, scale, *wire);
        if (!obj) return Error{"replay hop " + std::to_string(k + 1) + ": " + obj.error().message};
    }
    return out;
}

Result<EvilPart> make_evil(TypeId type, std::uint64_t seed, Scale scale, SaveMode mode, std::string_view creator,
                           EvilCase evil) {
    if (!to_type(std::uint16_t(type)) || !to_scale(std::uint8_t(scale)) || !to_evil(std::uint8_t(evil)))
        return Error{"make_evil: bad type, scale or case"};
    if (evil == EvilCase::WrongType) {
        // State 0 of another (small) type, sent as this one: the schema
        // fingerprint in the wire header names the other type.
        const TypeId other = type == TypeId::Order ? TypeId::KvConfig : TypeId::Order;
        Result<Object> o = make_object(other, seed, Scale::Small, creator);
        if (!o) return std::move(o).error();
        Result<std::string> w = save(*o, mode);
        if (!w) return std::move(w).error();
        return EvilPart{std::move(*w), content_digest(*o)};
    }
    return with_type(type, [&](auto tag) -> Result<EvilPart> {
        using X = typename decltype(tag)::type;
        const std::uint32_t big = evil == EvilCase::TooLarge ? 2 * data_limit(type, scale) + (1u << 20)
                                                              : receive_cap(type, scale);
        Result<Object> o = make_with_cap<X>(seed, scale, creator, big);
        if (!o) return std::move(o).error();
        X& root = o->template root<X>();
        EvilPart part;
        part.claimed_content_digest = content_digest(*o);
        switch (evil) {
            case EvilCase::None:
            case EvilCase::WrongType:
            case EvilCase::Truncated:
            case EvilCase::SchemaHash:
                break;
            case EvilCase::BadOffset: {
                // A dense wire (payload right after the 40-byte header), then
                // the trail's data offset pointed far outside the arena.
                Result<std::string> w = save_as<X>(o->buffer(), mode, XWireEmission::dense);
                if (!w) return std::move(w).error();
                XWireHeaderV2 h{};
                std::memcpy(&h, w->data(), sizeof h);
                if (h.extent_count != 0) return Error{"make_evil: the dense wire has extents"};
                const std::size_t field = std::size_t(reinterpret_cast<const char*>(&root.trail) -
                                                      reinterpret_cast<const char*>(&root));
                const std::size_t at = sizeof h + (h.root_offset - XWIRE_PAYLOAD_BEGIN) + field;
                const std::int32_t far = 0x3ffffff0;
                if (at + sizeof far > w->size()) return Error{"make_evil: trail head outside the wire"};
                std::memcpy(w->data() + at, &far, sizeof far);
                part.wire = std::move(*w);
                return part;
            }
            case EvilCase::TooLarge: {
                // Still a valid wire: the creator's trail name made long enough.
                Result<std::string> w = save(*o, mode);
                if (!w) return std::move(w).error();
                const std::size_t pad = data_limit(type, scale) - std::min<std::size_t>(w->size(), data_limit(type, scale)) + 4096;
                std::string name(creator);
                name.append(pad, '~');
                ops::set(root.trail[0].device, name);
                part.claimed_content_digest = content_digest(*o);
                break;
            }
            case EvilCase::StaleDigest:
                root.trail[0].hop = 7;   // after the digest was taken
                break;
            case EvilCase::ModelDrift:
                root.trail[0].hop = 7;
                part.claimed_content_digest = content_digest(*o);
                break;
        }
        Result<std::string> w = save(*o, mode);
        if (!w) return std::move(w).error();
        part.wire = std::move(*w);
        if (evil == EvilCase::Truncated) part.wire.resize(part.wire.size() / 2);
        if (evil == EvilCase::SchemaHash) part.wire[offsetof(XWireHeaderV2, schema_hash)] ^= 0x01;
        if (part.wire.size() > protocol::kMaxDataBytes) return Error{"make_evil: the part is over 16 MiB"};
        return part;
    });
}

} // namespace xmesh
