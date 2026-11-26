#ifndef X_OFFSET_COMPACTOR_HPP
#define X_OFFSET_COMPACTOR_HPP

// XCompactor: copies a buffer's root object, member by member, into a new
// tightly packed buffer, with a migrate_as strategy per type. Part of
// xoffsetdatastructure.hpp; include that header. Usage: docs/USER_GUIDE.md.

#include "xoffset_buffer.hpp"

namespace XOffsetDatastructure {
    // ================================================================
    // XCompactor -- Memory compaction via C++26 reflection.
    // ================================================================
    class XCompactor {
    public:
        enum class MigrateStrategy {
            Bitwise,          // direct assignment (primitives, enums, POD)
            AllocatorAware,   // reconstruct with new allocator (XString-like)
            Container,        // iterate elements, recurse (XVector/XSet/XMap)
            Composite,        // reflect members, recurse (user structs)
            NotRegistered     // use built-in auto-detection
        };

        template<typename T>
        struct migrate_as { static constexpr MigrateStrategy value = MigrateStrategy::NotRegistered; };

        // Compact root object into a new tightly-packed buffer without throwing.
        // Safe in both exception and -fno-exceptions builds: tries 2x, 2.5x,
        // 3x, 4x multipliers, retrying on out_of_storage without aborting.
        template<typename T>
        static XResult<XBuffer> try_compact(XBufferCore& old_xbuf) {
            static_assert(detail::member_types_explained_v<T> && is_byte_copy_safe_v<T>,
                "Type is not byte-copy safe.");
            const auto stats = memory_stats(old_xbuf);
            auto* old_obj = detail::find_root<T>(old_xbuf);

            // Progressive multipliers: try smaller first, fall back to larger.
            constexpr double multipliers[] = {2.0, 2.5, 3.0, 4.0};
            XError last_error{XErrc::out_of_storage,
                              "XCompactor::compact failed: insufficient memory even at 4x multiplier"};
            for (double mult : multipliers) {
                const double scaled = static_cast<double>(stats.used_size) * mult;
                std::size_t new_size = scaled >= static_cast<double>(XBufferCore::MAX_PAYLOAD)
                    ? XBufferCore::MAX_PAYLOAD
                    : static_cast<std::size_t>(scaled);
                if (new_size < 4096) new_size = 4096;

                XBuffer new_xbuf;
                XStatus init_status = detail::try_v([&]() -> detail::vstatus {
                    XOFFSET_VTRY(new_xbuf.init_fresh_default_v(new_size));
                    if (old_obj) {
                        XOFFSET_VTRY(new_xbuf.template make_v<T>());
                    }
                    return detail::vok();
                });
                if (!init_status) {
                    if (init_status.error().code == XErrc::out_of_storage ||
                        init_status.error().code == XErrc::capacity_exceeded) {
                        last_error = std::move(init_status).error();
                        continue;
                    }
                    return std::move(init_status).error();
                }
                if (!old_obj) {
                    return XResult<XBuffer>(std::move(new_xbuf));
                }
                auto* new_obj = detail::find_root<T>(new_xbuf);
                if (!new_obj) {
                    return XError{XErrc::no_root, "XCompactor::compact failed: newly created root missing"};
                }
                const XErrc rc = detail::run_fail_soft([&] {
                    migrate_member(*old_obj, *new_obj, old_xbuf, new_xbuf);
                });
                if (rc == XErrc::ok) {
                    new_xbuf.shrink_to_fit();
                    return XResult<XBuffer>(std::move(new_xbuf));
                }
                if (rc == XErrc::out_of_storage || rc == XErrc::capacity_exceeded) {
                    continue;
                }
                return XError{rc, "XCompactor::compact failed during migration"};
            }
            return last_error;
        }

        // Compact root object into a new tightly-packed buffer.
        // Progressive allocation: tries 2x, 2.5x, 3x, 4x multipliers.
        template<typename T>
        static XBuffer compact(XBufferCore& old_xbuf) {
            auto result = try_compact<T>(old_xbuf);
            if (!result) {
                XOFFSET_THROW(XException(result.error().code, result.error().message));
            }
            return std::move(*result);
        }

    private:

        // Resolve strategy: user-registered > trivial > allocator gate > composite.
        template<typename T>
        static consteval MigrateStrategy resolve_strategy() {
            using CleanT = std::remove_cv_t<T>;
            static_assert(is_byte_copy_safe_v<CleanT>,
                "XCompactor: type is not byte-copy safe and cannot be migrated.");
            if constexpr (migrate_as<CleanT>::value != MigrateStrategy::NotRegistered) {
                return migrate_as<CleanT>::value;
            } else if constexpr (std::is_trivially_copyable_v<CleanT>) {
                return MigrateStrategy::Bitwise;
            } else {
                return MigrateStrategy::Composite;
            }
        }

        template<typename ElementType>
        static auto migrate_element(const ElementType& old_elem, XBufferCore& old_xbuf, XBufferCore& new_xbuf) {
            constexpr auto strategy = resolve_strategy<ElementType>();
            auto new_alloc = detail::allocator_binding_for(new_xbuf);
            if constexpr (strategy == MigrateStrategy::Bitwise) {
                return old_elem;
            } else if constexpr (strategy == MigrateStrategy::AllocatorAware) {
                return ElementType(old_elem, new_alloc);
            } else if constexpr (detail::has_wire_allocator_ctor<ElementType>) {
                ElementType new_elem(new_alloc);
                migrate_members(old_elem, new_elem, old_xbuf, new_xbuf);
                return std::move(new_elem);
            } else {
                alignas(ElementType) unsigned char buf[sizeof(ElementType)];
                detail::reflect_init_all<ElementType>(buf, new_alloc);
                ElementType& new_elem = *std::launder(reinterpret_cast<ElementType*>(buf));
                migrate_members(old_elem, new_elem, old_xbuf, new_xbuf);
                return std::move(new_elem);
            }
        }
        
        template<typename ContainerType>
        static void migrate_container(const ContainerType& old_container, 
                                      ContainerType& new_container,
                                      XBufferCore& old_xbuf, XBufferCore& new_xbuf) {
            if (detail::soft_fail_failed()) return;
            using ElementType = typename ContainerType::value_type;

            if constexpr (std::is_trivially_copyable_v<ElementType>) {
                new_container = old_container;
                return;
            }

            if constexpr (requires { new_container.reserve(old_container.size()); }) {
                new_container.reserve(old_container.size());
                if (detail::soft_fail_failed()) return;
            }

            if constexpr (detail::is_fixed_flat_map_v<ContainerType>) {
                for (const auto& [key, value] : old_container) {
                    auto [it, inserted] = new_container.try_emplace(key);
                    (void)inserted;
                    if (detail::soft_fail_failed() || it == new_container.end()) return;
                    migrate_member(value, it->second, old_xbuf, new_xbuf);
                    if (detail::soft_fail_failed()) return;
                }
            } else if constexpr (detail::is_fixed_flat_set_v<ContainerType>) {
                for (const auto& elem : old_container) {
                    new_container.insert(elem);
                    if (detail::soft_fail_failed()) return;
                }
            } else if constexpr (detail::is_fixed_vector_v<ContainerType>) {
                for (const auto& elem : old_container) {
                    auto& slot = new_container.emplace_back();
                    if (detail::soft_fail_failed()) return;
                    migrate_member(elem, slot, old_xbuf, new_xbuf);
                    if (detail::soft_fail_failed()) return;
                }
            } else if constexpr (detail::MapLikeContainer<ContainerType>) {
                for (const auto& [key, value] : old_container) {
                    auto new_key = migrate_element(key, old_xbuf, new_xbuf);
                    auto new_value = migrate_element(value, old_xbuf, new_xbuf);
                    if (detail::soft_fail_failed()) return;
                    new_container.emplace(std::move(new_key), std::move(new_value));
                    if (detail::soft_fail_failed()) return;
                }
            } else {
                for (const auto& elem : old_container) {
                    auto migrated_elem = migrate_element(elem, old_xbuf, new_xbuf);
                    if (detail::soft_fail_failed()) return;
                    if constexpr (detail::SetLikeContainer<ContainerType>) {
                        new_container.emplace(std::move(migrated_elem));
                    } else {
                        new_container.emplace_back(std::move(migrated_elem));
                    }
                    if (detail::soft_fail_failed()) return;
                }
            }
        }
        
        template<typename MemberType>
        static void migrate_member(const MemberType& old_member, MemberType& new_member, 
                                  XBufferCore& old_xbuf, XBufferCore& new_xbuf) {
            if (detail::soft_fail_failed()) return;
            constexpr auto strategy = resolve_strategy<MemberType>();
            if constexpr (std::is_array_v<MemberType>) {
                for (std::size_t i = 0; i < std::extent_v<MemberType>; ++i) {
                    migrate_member(old_member[i], new_member[i], old_xbuf, new_xbuf);
                    if (detail::soft_fail_failed()) return;
                }
            } else if constexpr (strategy == MigrateStrategy::Bitwise) {
                new_member = old_member;
            } else if constexpr (strategy == MigrateStrategy::AllocatorAware) {
                new_member = old_member;
            } else if constexpr (strategy == MigrateStrategy::Container) {
                migrate_container(old_member, new_member, old_xbuf, new_xbuf);
            } else {
                migrate_members(old_member, new_member, old_xbuf, new_xbuf);
            }
        }
        
        template<typename T, std::size_t N>
        static void migrate_base_at(const T& old_obj, T& new_obj,
                                    XBufferCore& old_xbuf, XBufferCore& new_xbuf) {
            using BaseType = schema::base_t<T, N>;
            migrate_members(static_cast<const BaseType&>(old_obj),
                            static_cast<BaseType&>(new_obj), old_xbuf, new_xbuf);
        }

        template<typename T, std::size_t N>
        static void migrate_member_at(const T& old_obj, T& new_obj,
                                      XBufferCore& old_xbuf, XBufferCore& new_xbuf) {
            using member = schema::member_t<T, N>;
            using MemberType = typename member::type;
            migrate_member<MemberType>(member::get(old_obj), member::get(new_obj),
                                       old_xbuf, new_xbuf);
        }

        template<typename T>
        static void migrate_members(const T& old_obj, T& new_obj,
                                   XBufferCore& old_xbuf, XBufferCore& new_xbuf) {
            detail::for_each_reflected_subobject<T>(
                [&](auto index) {
                    migrate_base_at<T, decltype(index)::value>(
                        old_obj, new_obj, old_xbuf, new_xbuf);
                },
                [&](auto index) {
                    migrate_member_at<T, decltype(index)::value>(
                        old_obj, new_obj, old_xbuf, new_xbuf);
                });
        }
    };
}

#endif // X_OFFSET_COMPACTOR_HPP
