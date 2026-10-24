// SLG prototype: zpp_bits (v4.7.6, default options: uint32 size prefixes) over the std model.
// Aggregates are serialized member by member through structured bindings, so the
// std model needs no annotations.
#pragma once
#include <cstddef>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <system_error>
#include <vector>
#include <zpp_bits.h>
#include "slg/model_std.hpp"
#include "runner.hpp"

namespace slg {

struct ZppBitsFormat {
    static constexpr const char* name = "zpp_bits";
    using Wire = std::vector<std::byte>;
    static std::span<const uint8_t> bytes(const Wire& w) { return {reinterpret_cast<const uint8_t*>(w.data()), w.size()}; }

    static void check(std::errc e, const char* what) {
        if (e != std::errc{}) throw std::runtime_error(std::string("zpp_bits ") + what + ": " + std::make_error_code(e).message());
    }

    template <class T> static Wire encode(const T& v) {
        Wire w;
        zpp::bits::out out(w);
        check(out(v), "encode");
        return w;
    }
    // Same byte layout as encode(BattleMsg), written straight from the scene server's armies.
    static Wire encode_battle_request(int64_t id, int64_t seed, const stdm::Army& a, const stdm::Army& d) {
        static const stdm::Report empty{};
        Wire w;
        zpp::bits::out out(w);
        check(out(id, seed, int32_t{0}, a, d, empty), "encode");
        return w;
    }
    // Every read is bounds-checked against the input span; a short or overlong
    // size prefix fails with an error instead of reading past the end.
    template <class T> static void decode(std::span<const uint8_t> in, T& v) {
        zpp::bits::in ar(std::span<const std::byte>(reinterpret_cast<const std::byte*>(in.data()), in.size()));
        check(ar(v), "decode");
        if (ar.position() != in.size()) throw std::runtime_error("zpp_bits decode: trailing bytes");
    }

    // Reuse protocol: one output vector per slot. The archive writes from position 0
    // over the bytes left from the previous message and only grows the vector.
    struct Reuse {
        Wire slot[kSlots];
        template <class T> std::span<const uint8_t> encode(int s, const T& v) {
            zpp::bits::out out(slot[s]);
            check(out(v), "encode");
            return {reinterpret_cast<const uint8_t*>(slot[s].data()), out.position()};
        }
        std::span<const uint8_t> encode_battle_request(int s, int64_t id, int64_t seed, const stdm::Army& a, const stdm::Army& d) {
            static const stdm::Report empty{};
            zpp::bits::out out(slot[s]);
            check(out(id, seed, int32_t{0}, a, d, empty), "encode");
            return {reinterpret_cast<const uint8_t*>(slot[s].data()), out.position()};
        }
        template <class T> void decode(std::span<const uint8_t> in, T& v) { ZppBitsFormat::decode(in, v); }
    };
};

} // namespace slg
