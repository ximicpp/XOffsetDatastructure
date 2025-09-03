#pragma once
#include "../xtypesignature/xtypesignature.hpp"
#include "xtypes2.hpp"

namespace XTypeSignature {
    // 为 XOffsetDatastructure2::XVector 添加特化
    template <typename T>
    struct TypeSignature<XOffsetDatastructure2::XVector<T>> {
        static constexpr auto calculate() noexcept {
            return CompileString{"vector[s:"} +
                   CompileString<32>::from_number(sizeof(XOffsetDatastructure2::XVector<T>)) +
                   CompileString{",a:"} +
                   CompileString<32>::from_number(alignof(XOffsetDatastructure2::XVector<T>)) +
                   CompileString{"]<"} +
                   TypeSignature<T>::calculate() +
                   CompileString{">"};
        }
    };

    // 为 XOffsetDatastructure2::XString 添加特化
    template <>
    struct TypeSignature<XOffsetDatastructure2::XString> {
        static constexpr auto calculate() noexcept {
            return CompileString{"string[s:"} +
                   CompileString<32>::from_number(sizeof(XOffsetDatastructure2::XString)) +
                   CompileString{",a:"} +
                   CompileString<32>::from_number(alignof(XOffsetDatastructure2::XString)) +
                   CompileString{"]"};
        }
    };

    // 为 XOffsetDatastructure2::XMap 添加特化
    template <typename K, typename V>
    struct TypeSignature<XOffsetDatastructure2::XMap<K, V>> {
        static constexpr auto calculate() noexcept {
            return CompileString{"map[s:"} +
                   CompileString<32>::from_number(sizeof(XOffsetDatastructure2::XMap<K, V>)) +
                   CompileString{",a:"} +
                   CompileString<32>::from_number(alignof(XOffsetDatastructure2::XMap<K, V>)) +
                   CompileString{"]<"} +
                   TypeSignature<K>::calculate() +
                   CompileString{","} +
                   TypeSignature<V>::calculate() +
                   CompileString{">"};
        }
    };

    // 为 XOffsetDatastructure2::XSet 添加特化
    template <typename T>
    struct TypeSignature<XOffsetDatastructure2::XSet<T>> {
        static constexpr auto calculate() noexcept {
            return CompileString{"set[s:"} +
                   CompileString<32>::from_number(sizeof(XOffsetDatastructure2::XSet<T>)) +
                   CompileString{",a:"} +
                   CompileString<32>::from_number(alignof(XOffsetDatastructure2::XSet<T>)) +
                   CompileString{"]<"} +
                   TypeSignature<T>::calculate() +
                   CompileString{">"};
        }
    };
} // namespace XTypeSignature