// Probe 3 (P2996 build, real library): a hot per-member walk written with the
// descriptor layer compiles to the same machine code as the library's
// reflection version. Kernel: shift_rebase_inline_value (SHIFT relocation)
// and empty_vacated_slot, instantiated for SLG records.
#include <cstddef>
#include "xoffsetdatastructure.hpp"
#include "slg/model_xoffset.hpp"
#include "xo_describe.hpp"

XO_DESCRIBE(slg::xm::Buff, id, stacks, expire_ms)
XO_DESCRIBE(slg::xm::Skill, id, level)
XO_DESCRIBE(slg::xm::Troop, type, count, wounded)
XO_DESCRIBE(slg::xm::Hero, id, level, star, name, skills, buffs)
XO_DESCRIBE(slg::xm::Army, army_id, owner_id, x, y, heroes, troops, buffs)
XO_DESCRIBE(slg::xm::City, city_id, level, buildings, resources)
XO_DESCRIBE(slg::xm::PlayerState, player_id, name, city, armies, heroes, research, items)

namespace d = XOffsetDatastructure::detail;
namespace fd = XOffsetDatastructure::fixed_detail;

namespace probe {
// Line-for-line copy of detail::shift_rebase_inline_value / empty_vacated_slot;
// only the record branch changes (descriptor instead of std::meta).
template <class T>
void shift_rebase_inline_value(char* address, std::int32_t shift) noexcept {
    if constexpr (fd::is_owning_head<T>::value) {
        d::shift_rebase_inline_value<T>(address, shift);   // head branch has no reflection
    } else if constexpr (std::is_array_v<T>) {
        using E = std::remove_extent_t<T>;
        if constexpr (d::raw_needs_activation<E>())
            for (std::size_t i = 0; i < std::extent_v<T>; ++i)
                shift_rebase_inline_value<E>(address + i * sizeof(E), shift);
    } else if constexpr (std::is_class_v<T> && d::raw_needs_activation<T>()) {
        xo_desc::static_for<xo_desc::member_count<T>>([&](auto index) {
            using F = xo_desc::member_t<T, decltype(index)::value>;
            using M = typename F::type;
            if constexpr (d::raw_needs_activation<M>())
                shift_rebase_inline_value<M>(address + F::offset, shift);
        });
    }
}
template <class T>
void empty_vacated_slot(T* slot) noexcept {
    if constexpr (fd::is_owning_head<T>::value) {
        slot->data_delta = 0; slot->size_ = 0; slot->capacity_ = 0;
    } else if constexpr (std::is_array_v<T>) {
        for (auto& element : *slot) empty_vacated_slot(&element);
    } else if constexpr (std::is_class_v<T> && d::raw_needs_activation<T>()) {
        xo_desc::static_for<xo_desc::member_count<T>>([&](auto index) {
            using F = xo_desc::member_t<T, decltype(index)::value>;
            if constexpr (d::raw_needs_activation<typename F::type>())
                empty_vacated_slot(&F::get(*slot));
        });
    }
}
} // namespace probe

#define PAIR(T, tag)                                                                              \
    extern "C" [[gnu::noinline]] void refl_rebase_##tag(char* a, std::int32_t s) noexcept { d::shift_rebase_inline_value<T>(a, s); } \
    extern "C" [[gnu::noinline]] void desc_rebase_##tag(char* a, std::int32_t s) noexcept { probe::shift_rebase_inline_value<T>(a, s); } \
    extern "C" [[gnu::noinline]] void refl_vacate_##tag(T* p) noexcept { d::empty_vacated_slot<T>(p); } \
    extern "C" [[gnu::noinline]] void desc_vacate_##tag(T* p) noexcept { probe::empty_vacated_slot<T>(p); }
PAIR(slg::xm::Hero, hero)
PAIR(slg::xm::Army, army)
PAIR(slg::xm::City, city)
PAIR(slg::xm::PlayerState, player)
