#pragma once
#include "../xtypesignature/xtypesignature.hpp"
#include "xtypes.hpp"

namespace XTypeSignature {
    // 为 XOffsetDatastructure::XVector 添加特化
    template <typename T>
    struct TypeSignature<XOffsetDatastructure::XVector<T>> {
        static constexpr auto calculate() noexcept {
            return CompileString{"vector[s:"} +
                   CompileString<32>::from_number(sizeof(XOffsetDatastructure::XVector<T>)) +
                   CompileString{",a:"} +
                   CompileString<32>::from_number(alignof(XOffsetDatastructure::XVector<T>)) +
                   CompileString{"]<"} +
                   TypeSignature<T>::calculate() +
                   CompileString{">"};
        }
    };

    // 为 XOffsetDatastructure::XString 添加特化
    template <>
    struct TypeSignature<XOffsetDatastructure::XString> {
        static constexpr auto calculate() noexcept {
            return CompileString{"string[s:"} +
                   CompileString<32>::from_number(sizeof(XOffsetDatastructure::XString)) +
                   CompileString{",a:"} +
                   CompileString<32>::from_number(alignof(XOffsetDatastructure::XString)) +
                   CompileString{"]"};
        }
    };

    // 为 XOffsetDatastructure::XMap 添加特化
    template <typename K, typename V>
    struct TypeSignature<XOffsetDatastructure::XMap<K, V>> {
        static constexpr auto calculate() noexcept {
            return CompileString{"map[s:"} +
                   CompileString<32>::from_number(sizeof(XOffsetDatastructure::XMap<K, V>)) +
                   CompileString{",a:"} +
                   CompileString<32>::from_number(alignof(XOffsetDatastructure::XMap<K, V>)) +
                   CompileString{"]<"} +
                   TypeSignature<K>::calculate() +
                   CompileString{","} +
                   TypeSignature<V>::calculate() +
                   CompileString{">"};
        }
    };

    // 为 XOffsetDatastructure::XSet 添加特化
    template <typename T>
    struct TypeSignature<XOffsetDatastructure::XSet<T>> {
        static constexpr auto calculate() noexcept {
            return CompileString{"set[s:"} +
                   CompileString<32>::from_number(sizeof(XOffsetDatastructure::XSet<T>)) +
                   CompileString{",a:"} +
                   CompileString<32>::from_number(alignof(XOffsetDatastructure::XSet<T>)) +
                   CompileString{"]<"} +
                   TypeSignature<T>::calculate() +
                   CompileString{">"};
        }
    };

    // 为 boost::container::basic_string 添加特化
    template <>
    struct TypeSignature<boost::container::basic_string<char>> {
        static constexpr auto calculate() noexcept {
            return CompileString{"string[s:"} +
                   CompileString<32>::from_number(sizeof(boost::container::basic_string<char>)) +
                   CompileString{",a:"} +
                   CompileString<32>::from_number(alignof(boost::container::basic_string<char>)) +
                   CompileString{"]"};
        }
    };
} // namespace XTypeSignature