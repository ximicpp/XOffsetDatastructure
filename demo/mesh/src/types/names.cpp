// Device mesh demo: names of the types.hpp enums and their wire values.
// OWNER: types thread.
#include "xmesh/types.hpp"

namespace xmesh {
namespace {

bool same_letters(std::string_view a, std::string_view b) noexcept {
    if (a.size() != b.size()) return false;
    for (std::size_t i = 0; i < a.size(); ++i) {
        char x = a[i], y = b[i];
        if (x >= 'A' && x <= 'Z') x = char(x - 'A' + 'a');
        if (y >= 'A' && y <= 'Z') y = char(y - 'A' + 'a');
        if (x != y) return false;
    }
    return true;
}

template <class E, std::size_t N>
std::optional<E> parse_name(std::string_view name, const std::array<E, N>& all, const char* (*to_name)(E) noexcept) {
    for (E e : all)
        if (same_letters(name, to_name(e))) return e;
    return std::nullopt;
}

constexpr std::array<Scale, 3> kAllScales{Scale::Small, Scale::Medium, Scale::Large};
constexpr std::array<SaveMode, 2> kAllSaveModes{SaveMode::Verified, SaveMode::Compact};
constexpr std::array<EvilCase, 8> kAllEvilValues{EvilCase::None, EvilCase::Truncated, EvilCase::BadOffset,
                                                 EvilCase::SchemaHash, EvilCase::WrongType, EvilCase::TooLarge,
                                                 EvilCase::StaleDigest, EvilCase::ModelDrift};

} // namespace

const char* type_name(TypeId type) noexcept {
    switch (type) {
        case TypeId::Document: return "Document";
        case TypeId::Order: return "Order";
        case TypeId::SensorSeries: return "SensorSeries";
        case TypeId::KvConfig: return "KvConfig";
        case TypeId::SceneTree: return "SceneTree";
        case TypeId::ChatRoom: return "ChatRoom";
    }
    return "unknown";
}

const char* scale_name(Scale scale) noexcept {
    switch (scale) {
        case Scale::Small: return "small";
        case Scale::Medium: return "medium";
        case Scale::Large: return "large";
    }
    return "unknown";
}

const char* save_mode_name(SaveMode mode) noexcept {
    switch (mode) {
        case SaveMode::Verified: return "verified";
        case SaveMode::Compact: return "compact";
    }
    return "unknown";
}

const char* evil_name(EvilCase evil) noexcept {
    switch (evil) {
        case EvilCase::None: return "none";
        case EvilCase::Truncated: return "truncated";
        case EvilCase::BadOffset: return "bad_offset";
        case EvilCase::SchemaHash: return "schema_hash";
        case EvilCase::WrongType: return "wrong_type";
        case EvilCase::TooLarge: return "too_large";
        case EvilCase::StaleDigest: return "stale_digest";
        case EvilCase::ModelDrift: return "model_drift";
    }
    return "unknown";
}

std::optional<TypeId> parse_type(std::string_view name) noexcept { return parse_name(name, kAllTypes, type_name); }
std::optional<Scale> parse_scale(std::string_view name) noexcept { return parse_name(name, kAllScales, scale_name); }
std::optional<SaveMode> parse_save_mode(std::string_view name) noexcept {
    return parse_name(name, kAllSaveModes, save_mode_name);
}
std::optional<EvilCase> parse_evil(std::string_view name) noexcept {
    return parse_name(name, kAllEvilValues, evil_name);
}

std::optional<TypeId> to_type(std::uint16_t raw) noexcept {
    if (raw >= 1 && raw <= 6) return TypeId(raw);
    return std::nullopt;
}
std::optional<Scale> to_scale(std::uint8_t raw) noexcept {
    if (raw <= 2) return Scale(raw);
    return std::nullopt;
}
std::optional<SaveMode> to_save_mode(std::uint8_t raw) noexcept {
    if (raw <= 1) return SaveMode(raw);
    return std::nullopt;
}
std::optional<EvilCase> to_evil(std::uint8_t raw) noexcept {
    if (raw <= 7) return EvilCase(raw);
    return std::nullopt;
}

} // namespace xmesh
