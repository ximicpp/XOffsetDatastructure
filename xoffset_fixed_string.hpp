#ifndef X_OFFSET_FIXED_STRING_HPP
#define X_OFFSET_FIXED_STRING_HPP

// Compile-time strings for the wire ABI signature (wire_abi_signature<T>(),
// schema hashes). Taken from TypeLayout's FixedString / to_fixed_string
// (Boost Software License 1.0, TypeLayout Development Team) so the core
// library no longer includes TypeLayout; the text it produces is unchanged.

#include <compare>
#include <cstddef>
#include <ostream>
#include <string_view>

namespace XOffsetDatastructure::fixed_text {
    // FixedString<N>: N characters plus a terminating null.
    template <std::size_t N>
    struct FixedString {
        char value[N + 1];
        static constexpr std::size_t size = N;

        constexpr FixedString() : value{} {}

        constexpr FixedString(const char (&str)[N + 1]) {
            for (std::size_t i = 0; i <= N; ++i)
                value[i] = str[i];
        }

        constexpr FixedString(std::string_view sv) : value{} {
            for (std::size_t i = 0; i < N && i < sv.size(); ++i)
                value[i] = sv[i];
        }

        template <std::size_t M>
        constexpr auto operator+(const FixedString<M>& other) const noexcept {
            constexpr std::size_t new_size = N + M;
            char result[new_size + 1] = {};
            for (std::size_t i = 0; i < N; ++i)
                result[i] = value[i];
            for (std::size_t i = 0; i <= M; ++i)
                result[N + i] = other.value[i];
            return FixedString<new_size>(result);
        }

        template <std::size_t M>
        constexpr bool operator==(const FixedString<M>& other) const noexcept {
            std::size_t i = 0;
            while (i <= N && i <= M) {
                if (value[i] != other.value[i]) return false;
                if (value[i] == '\0') return true;
                ++i;
            }
            if (i <= N) return value[i] == '\0';
            if (i <= M) return other.value[i] == '\0';
            return true;
        }

        constexpr bool operator==(const char* other) const noexcept {
            for (std::size_t i = 0; i <= N; ++i) {
                if (value[i] != other[i]) return false;
                if (value[i] == '\0') return true;
            }
            return true;
        }

        template <std::size_t M>
        constexpr std::strong_ordering operator<=>(const FixedString<M>& other) const noexcept {
            std::size_t i = 0;
            while (i < N && i < M && value[i] != '\0' && other.value[i] != '\0') {
                if (value[i] < other.value[i]) return std::strong_ordering::less;
                if (value[i] > other.value[i]) return std::strong_ordering::greater;
                ++i;
            }
            return length() <=> other.length();
        }

        constexpr std::size_t length() const noexcept {
            std::size_t len = 0;
            while (len < N && value[len] != '\0') ++len;
            return len;
        }

        constexpr const char* c_str() const noexcept { return value; }

        constexpr operator std::string_view() const noexcept {
            return {value, length()};
        }
    };

    template <std::size_t N>
    FixedString(const char (&)[N]) -> FixedString<N - 1>;

    template <std::size_t N>
    std::ostream& operator<<(std::ostream& os, const FixedString<N>& str) {
        return os << str.value;
    }

    consteval std::size_t count_digits(std::size_t v) noexcept {
        if (v == 0) return 1;
        std::size_t n = 0;
        while (v > 0) { v /= 10; ++n; }
        return n;
    }

    // Decimal text of V, exactly sized.
    template <std::size_t V>
    consteval auto to_fixed_string() noexcept {
        constexpr std::size_t digits = count_digits(V);
        char result[digits + 1] = {};
        if constexpr (V == 0) {
            result[0] = '0';
        } else {
            std::size_t pos = digits;
            std::size_t v = V;
            while (v > 0) { result[--pos] = '0' + char(v % 10); v /= 10; }
        }
        return FixedString<digits>(result);
    }
} // namespace XOffsetDatastructure::fixed_text

#endif // X_OFFSET_FIXED_STRING_HPP
