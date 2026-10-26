#pragma once

// Explicit immutable-owner index. Never consulted by mutable move assignment.
#include "xoffset_verified_paths.hpp"
#include "xoffset_snapshot_index_budget.hpp"
#include <array>
#include <atomic>
#include <cstring>
#include <limits>
#include <new>
#include <type_traits>

namespace XOffsetDatastructure {
namespace snapshot_index_detail {
    template<class T> inline const unsigned char type_tag = 0;

    // Enumerate only declared inline owning heads. A byte payload is not a
    // declaration of every head that a caller could place into its storage.
    template<class T, class Visitor>
    bool inline_heads(const T& value, Visitor& visit) {
        using U = std::remove_cv_t<T>;
        if constexpr (std::is_trivially_copyable_v<U>) return false;
        else if constexpr (std::is_array_v<U>) {
            for (const auto& child : value) if (inline_heads(child, visit)) return true;
        } else if constexpr (fixed_detail::is_owning_head<U>::value) {
            return visit(value);
        } else if constexpr (std::is_class_v<U> && boost::typelayout::is_byte_copy_safe_v<U>) {
            bool stop = false;
            detail::for_each_reflected_subobject<U>([&](auto index) {
                using Base = schema::base_t<U, decltype(index)::value>;
                if (!stop) stop = inline_heads(static_cast<const Base&>(value), visit);
            }, [&](auto index) {
                using member = schema::member_t<U, decltype(index)::value>;
                if constexpr (!member::is_bit_field)
                    if (!stop) stop = inline_heads(member::get(value), visit);
            });
            return stop;
        }
        return false;
    }

    // Find the complete typed parent in the validated root. Unlike inline_heads
    // this follows live dynamic children. It is a preparation cost, not hidden
    // in a measured query. No arbitrary reinterpret_cast creates membership.
    template<class T, class Parent>
    bool find_parent(const T& value, const Parent* parent) {
        using U = std::remove_cv_t<T>;
        if constexpr (std::is_same_v<U, Parent>) if (&value == parent) return true;
        if constexpr (std::is_trivially_copyable_v<U>) return false;
        else if constexpr (std::is_array_v<U>) {
            for (const auto& child : value) if (find_parent(child, parent)) return true;
        } else if constexpr (fixed_detail::is_owning_head<U>::value) {
            if constexpr (!detail::is_fixed_string_v<U>) {
                if constexpr (!std::is_trivially_copyable_v<typename U::value_type>)
                    for (const auto& child : value) if (find_parent(child, parent)) return true;
            }
        } else if constexpr (std::is_class_v<U> && boost::typelayout::is_byte_copy_safe_v<U>) {
            bool found = false;
            detail::for_each_reflected_subobject<U>([&](auto index) {
                using Base = schema::base_t<U, decltype(index)::value>;
                if (!found) found = find_parent(static_cast<const Base&>(value), parent);
            }, [&](auto index) {
                using member = schema::member_t<U, decltype(index)::value>;
                if constexpr (!member::is_bit_field)
                    if (!found) found = find_parent(member::get(value), parent);
            });
            return found;
        }
        return false;
    }
} // namespace snapshot_index_detail

enum class SnapshotIndexEvidence { unknown, strict_negative };

// An optional index for DECLARED heads in DISTINCT LIVE ELEMENTS of one
// verified immutable parent array. It is deliberately not a mutable cache or a
// universal address locator. Same-element, out-of-line, foreign, byte-placement
// and spare-capacity addresses return unknown and use the original predicate.
template<class Root, class Parent>
class VerifiedSnapshotArrayIndex {
    using P = std::remove_cv_t<Parent>;
    static_assert(fixed_detail::is_owning_head<P>::value && !detail::is_fixed_string_v<P>);
    static_assert(sizeof(P) == 16);
    using Element = typename P::value_type;
    struct Entry { std::uint32_t offset{}; const void* type{}; };
    struct Control {
        std::atomic<std::size_t> references{1};
        const void* owner_key{};
        std::size_t charge{}, count{};
        const P* parent{};
        std::array<std::byte, 16> descriptor{};
        const Element* data{};
        std::size_t live_count{};
    };
    static constexpr std::size_t entries_offset =
        (sizeof(Control) + alignof(Entry) - 1u) / alignof(Entry) * alignof(Entry);
    static_assert(alignof(Control) <= alignof(std::max_align_t) && alignof(Entry) <= alignof(std::max_align_t));
    VerifiedSnapshot<Root> owner_;
    Control* control_{};
    static Entry* entries(Control* c) noexcept {
        return reinterpret_cast<Entry*>(reinterpret_cast<std::byte*>(c) + entries_offset);
    }
    void drop() noexcept {
        auto* c = control_;
        if (!c || c->references.fetch_sub(1, std::memory_order_acq_rel) != 1) return;
        const auto key = c->owner_key; const auto charge = c->charge;
        // No mutex is held across user allocator/delete callbacks. The owner_
        // member remains alive until this destructor body completes.
        std::destroy_at(c); ::operator delete(c);
        snapshot_index_detail::release(key, charge);
    }
    std::size_t element_at(const void* address, const void* type) const noexcept {
        const auto* c = control_;
        const auto base = reinterpret_cast<std::uintptr_t>(c->data);
        const auto at = reinterpret_cast<std::uintptr_t>(address);
        if (at < base) return c->live_count;
        const auto delta = at - base;
        const auto element = delta / sizeof(Element);
        if (element >= c->live_count) return c->live_count;
        const auto within = delta % sizeof(Element);
        for (std::size_t i = 0; i < c->count; ++i) {
            const auto& entry = entries(control_)[i];
            if (entry.offset == within && (!type || entry.type == type)) return element;
        }
        return c->live_count;
    }
public:
    VerifiedSnapshotArrayIndex() noexcept = default;
    VerifiedSnapshotArrayIndex(const VerifiedSnapshotArrayIndex& other) noexcept
        : owner_(other.owner_), control_(other.control_) {
        if (control_) control_->references.fetch_add(1, std::memory_order_relaxed);
    }
    VerifiedSnapshotArrayIndex(VerifiedSnapshotArrayIndex&& other) noexcept
        : owner_(std::move(other.owner_)), control_(std::exchange(other.control_, nullptr)) {}
    VerifiedSnapshotArrayIndex& operator=(VerifiedSnapshotArrayIndex other) noexcept {
        swap(other); return *this;
    }
    ~VerifiedSnapshotArrayIndex() { drop(); }
    void swap(VerifiedSnapshotArrayIndex& other) noexcept {
        std::swap(owner_, other.owner_); std::swap(control_, other.control_);
    }
    explicit operator bool() const noexcept { return control_ != nullptr; }
    std::size_t requested_bytes() const noexcept { return control_ ? control_->charge : 0; }
    const VerifiedSnapshot<Root>& snapshot() const noexcept { return owner_; }

    // Preparation is allowed to fail closed. A disabled index retains the
    // immutable owner but every query falls back; no valid input is rejected.
    static VerifiedSnapshotArrayIndex prepare(VerifiedSnapshot<Root> owner, const P& parent) noexcept {
        VerifiedSnapshotArrayIndex result;
        result.owner_ = std::move(owner);
        if (!result.owner_ || !snapshot_index_detail::registry() || !parent.size()) return result;
        bool reserved = false;
        std::size_t charge = 0;
        const auto key = result.owner_.segments()[0].data();
        void* block = nullptr;
        Control* constructed = nullptr;
        try {
            if (!snapshot_index_detail::find_parent(result.owner_.root(), &parent)) return result;
            std::size_t count = 0;
            auto count_head = [&](const auto&) {
                ++count;
                return count > (snapshot_index_detail::owner_limit - entries_offset) / sizeof(Entry);
            };
            if (snapshot_index_detail::inline_heads(parent[0], count_head) || !count) return result;
            charge = entries_offset + count * sizeof(Entry);
            reserved = snapshot_index_detail::reserve(key, charge);
            if (!reserved) return result;
            block = ::operator new(charge);
            auto* c = std::construct_at(static_cast<Control*>(block));
            constructed = c;
            c->owner_key = key; c->charge = charge; c->count = count; c->parent = &parent;
            std::memcpy(c->descriptor.data(), &parent, 16);
            c->data = parent.data(); c->live_count = parent.size();
            const auto first = reinterpret_cast<std::uintptr_t>(c->data);
            std::size_t position = 0;
            auto fill_head = [&](const auto& head) {
                using H = std::remove_cvref_t<decltype(head)>;
                const auto offset = reinterpret_cast<std::uintptr_t>(&head) - first;
                if (offset > sizeof(Element) || sizeof(H) > sizeof(Element) - offset ||
                    offset > (std::numeric_limits<std::uint32_t>::max)())
                    throw XException("snapshot index: invalid declared subobject offset");
                std::construct_at(entries(c) + position++, Entry{
                    static_cast<std::uint32_t>(offset), &snapshot_index_detail::type_tag<H>});
                return false;
            };
            snapshot_index_detail::inline_heads(parent[0], fill_head);
            if (position != count) throw XException("snapshot index: changed declaration traversal");
            result.control_ = c; // publish only a complete allocation
        } catch (...) {
            if (constructed) std::destroy_at(constructed);
            if (block) ::operator delete(block);
            if (reserved) snapshot_index_detail::release(key, charge);
        }
        return result;
    }

    template<class Head>
    SnapshotIndexEvidence evidence(const Head& value, const void* address) const noexcept {
        using H = std::remove_cv_t<Head>;
        if constexpr (!fixed_detail::is_owning_head<H>::value) return SnapshotIndexEvidence::unknown;
        else {
            const auto* c = control_;
            if (!c || !address ||
                std::memcmp(c->descriptor.data(), c->parent, 16) != 0)
                return SnapshotIndexEvidence::unknown;
            const auto from = element_at(&value, &snapshot_index_detail::type_tag<H>);
            const auto to = element_at(address, nullptr);
            if (from == c->live_count || to == c->live_count || from == to)
                return SnapshotIndexEvidence::unknown;
            return SnapshotIndexEvidence::strict_negative;
        }
    }
    template<class Head>
    bool owns_address(const Head& value, const void* address) const noexcept {
        if (evidence(value, address) == SnapshotIndexEvidence::strict_negative) return false;
        return fixed_detail::move_assignment_owns_address(value, address);
    }
};

template<class Root, class Parent>
auto make_verified_snapshot_array_index(VerifiedSnapshot<Root> snapshot, const Parent& parent) noexcept {
    return VerifiedSnapshotArrayIndex<Root, Parent>::prepare(std::move(snapshot), parent);
}
} // namespace XOffsetDatastructure
