#ifndef X_OFFSET_FIXED_MESSAGE_HPP
#define X_OFFSET_FIXED_MESSAGE_HPP

// Fixed messages: records with no arena container, sent as their own bytes.
//
// A fixed message is an admitted wire type with no XString, XVector, XSet,
// XMap or XBlob anywhere inside it and no implicit padding: scalars, enums,
// arrays of them and records of them (a normalized market-data update, an
// order, an ack). Its object representation holds no offset and no
// uninitialized byte, so it needs no arena, no root and no wire header:
//
//   - The connection binds the type once: list it in the schema handshake
//     (make_schema_hello<Quote>(...), accept_schema_hello<Quote>(...),
//     xoffset_handshake.hpp) like any root type; it needs a schema name
//     (XOFFSET_REGISTER_SCHEMA_NAME). After that a message is sizeof(T) bytes.
//   - The sender copies the record into the transport buffer
//     (write_fixed_message) or builds a batch in place (place_fixed_batch).
//   - The receiver reads it where it lies (view_fixed_message,
//     view_fixed_batch): no copy, no arena, no XBuffer.
//
// A batch is an 8-byte header {count, tag} followed by count records. The
// tag is 32 bits of the type's wire fingerprint: it catches a batch handed to
// the reader of another type, and lets one connection carry several fixed
// types (switch on peek_fixed_batch_tag(bytes), case fixed_message_tag_v<T>).
// A single message carries no tag: send single messages only on a
// connection that carries one type.
//
// What a view checks: the size (exactly sizeof(T), or the batch's count), the
// alignment of T, the batch tag, and the byte of every bool (0 or 1), as
// load_verified does. That is the whole structural validation of a fixed
// message: it holds no offset that could point outside the buffer. Enum
// values are not range-checked, as in the wire. For a type without bool the
// value check compiles away.
//
// The same type can also be a member or element of an ordinary XOffset root
// (XVector<Quote> in a snapshot); the bytes of a record are the same in both.
//
// Lifetime: a view reads the record in place, as XBuffer reads its arena
// (std::launder). The bytes must have been written by an operation that
// implicitly creates objects in the buffer: memcpy, a socket read into a byte
// buffer, or the write functions here. A view is valid while those bytes are
// unchanged.
//
// Byte order: like the wire, a fixed message moves only between 64-bit
// little-endian peers (the library's platform guard).
//
// Nothing here throws or allocates.

#include "xoffsetdatastructure.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <new>
#include <span>
#include <type_traits>
#include <utility>

namespace XOffsetDatastructure {

    namespace detail::fixed_msg {
        template <class T>
        consteval bool no_arena_container();

        template <class C, std::size_t... I>
        consteval bool members_no_arena_container(std::index_sequence<I...>) {
            return (no_arena_container<typename schema::member_t<C, I>::type>() && ...);
        }

        // Only asked of admitted types: every class reached is described.
        template <class T>
        consteval bool no_arena_container() {
            using C = std::remove_cv_t<T>;
            if constexpr (fixed_size_scalar<C> || std::is_enum_v<C>) {
                return true;
            } else if constexpr (std::is_array_v<C>) {
                return no_arena_container<std::remove_extent_t<C>>();
            } else if constexpr (is_fixed_string_v<C> || is_fixed_vector_v<C> ||
                                 is_fixed_flat_set_v<C> || is_fixed_flat_map_v<C>) {
                return false;
            } else if constexpr (std::is_class_v<C>) {
                return members_no_arena_container<C>(std::make_index_sequence<schema::member_count_v<C>>{});
            } else {
                return false;
            }
        }

        template <class T>
        consteval bool is_fixed_message() {
            using C = std::remove_cv_t<T>;
            if constexpr (!std::is_class_v<C> || std::is_union_v<C>) return false;
            else if constexpr (!v1_wire_admitted<C>()) return false;
            else if constexpr (!no_arena_container<C>()) return false;
            else return !has_implicit_padding_v<C>;
        }

        // The values whose byte patterns can be invalid: bool, as the wire
        // validator checks it. bool_bits ORs the byte of every bool in v,
        // without a branch; the values are valid when it is 0 or 1.
        template <class T>
        inline unsigned bool_bits(const T& v) noexcept {
            using C = std::remove_cv_t<T>;
            if constexpr (!wire_value_needs_validation_v<C>) {
                return 0;
            } else if constexpr (std::is_same_v<C, bool>) {
                unsigned char b;
                std::memcpy(&b, &v, 1);
                return b;
            } else if constexpr (std::is_array_v<C>) {
                unsigned bits = 0;
                for (const auto& e : v) bits |= bool_bits(e);
                return bits;
            } else {
                return [&]<std::size_t... I>(std::index_sequence<I...>) {
                    return (bool_bits(schema::member_t<C, I>::get(v)) | ... | 0u);
                }(std::make_index_sequence<schema::member_count_v<C>>{});
            }
        }

        inline bool aligned_for(const void* p, std::size_t alignment) noexcept {
            return (reinterpret_cast<std::uintptr_t>(p) & (alignment - 1)) == 0;
        }

        // Never 0: peek_fixed_batch_tag answers 0 for "no batch header".
        template <class T>
        consteval std::uint32_t tag() {
            const std::uint64_t h = wire_schema_hash_v<T>() ^ wire_root_type_id_v<T>();
            const auto t = static_cast<std::uint32_t>(h ^ (h >> 32));
            return t != 0 ? t : 1u;
        }
    } // namespace detail::fixed_msg

    template <class T>
    inline constexpr bool is_fixed_message_v = detail::fixed_msg::is_fixed_message<T>();

    template <class T>
    concept fixed_message = is_fixed_message_v<T>;

    // The tag a batch of T carries (never 0).
    template <fixed_message T>
    inline constexpr std::uint32_t fixed_message_tag_v = detail::fixed_msg::tag<std::remove_cv_t<T>>();

    struct XFixedBatchHeader {
        std::uint32_t count;
        std::uint32_t tag;
    };
    static_assert(sizeof(XFixedBatchHeader) == 8);

    enum class XFixedMessageStatus : std::uint8_t { ok, wrong_size, misaligned, wrong_tag, invalid_value };

    template <class T>
    struct XFixedMessageView {
        const T* record = nullptr;
        XFixedMessageStatus status = XFixedMessageStatus::wrong_size;
        explicit operator bool() const noexcept { return status == XFixedMessageStatus::ok; }
        const T& operator*() const noexcept { return *record; }
        const T* operator->() const noexcept { return record; }
    };

    template <class T>
    struct XFixedBatchView {
        std::span<const T> records;
        XFixedMessageStatus status = XFixedMessageStatus::wrong_size;
        explicit operator bool() const noexcept { return status == XFixedMessageStatus::ok; }
    };

    // A batch built in place: the records to fill and the bytes to send
    // (0 when the output was too small or misaligned).
    template <class T>
    struct XFixedBatchSlot {
        std::span<T> records;
        std::size_t bytes = 0;
        explicit operator bool() const noexcept { return bytes != 0; }
    };

    // ---- send -----------------------------------------------------------------

    template <fixed_message T>
    constexpr std::size_t fixed_batch_bytes(std::uint32_t count) noexcept {
        return sizeof(XFixedBatchHeader) + std::size_t(count) * sizeof(T);
    }

    // Copies `value` to the start of `out`; returns sizeof(T), or 0 when `out`
    // is smaller.
    template <fixed_message T>
    inline std::size_t write_fixed_message(std::span<std::byte> out, const T& value) noexcept {
        if (out.size() < sizeof(T)) return 0;
        std::memcpy(out.data(), &value, sizeof(T));
        return sizeof(T);
    }

    // Writes the header of a batch of `count` records at the start of `out`
    // and returns the records to fill where they will be sent. `out` must be
    // aligned for T and hold fixed_batch_bytes<T>(count). The records' bytes
    // are whatever `out` held: fill every one before sending.
    template <fixed_message T>
    inline XFixedBatchSlot<T> place_fixed_batch(std::span<std::byte> out, std::uint32_t count) noexcept {
        const std::size_t bytes = fixed_batch_bytes<T>(count);
        if (out.size() < bytes || !detail::fixed_msg::aligned_for(out.data(), alignof(T))) return {};
        const XFixedBatchHeader h{count, fixed_message_tag_v<T>};
        std::memcpy(out.data(), &h, sizeof h);
        std::byte* first = out.data() + sizeof(XFixedBatchHeader);
        if (count == 0) return {std::span<T>(), bytes};
        // The nonallocating allocation function implicitly creates the array
        // (as XBuffer starts container storage).
        T* records = *static_cast<T(*)[]>(::operator new(std::size_t(count) * sizeof(T), first));
        return {std::span<T>(records, count), bytes};
    }

    // Writes a batch of `values`; returns the bytes written, or 0 when `out`
    // is too small or misaligned or `values` holds more than 2^32 - 1 records.
    template <fixed_message T>
    inline std::size_t write_fixed_batch(std::span<std::byte> out, std::span<const T> values) noexcept {
        if (values.size() > (std::numeric_limits<std::uint32_t>::max)()) return 0;
        const auto slot = place_fixed_batch<T>(out, static_cast<std::uint32_t>(values.size()));
        if (!slot) return 0;
        if (!values.empty()) std::memcpy(slot.records.data(), values.data(), values.size_bytes());
        return slot.bytes;
    }

    // ---- receive (read in place) ----------------------------------------------

    // The record `in` holds: exactly sizeof(T) bytes, aligned for T.
    template <fixed_message T>
    inline XFixedMessageView<T> view_fixed_message(std::span<const std::byte> in) noexcept {
        if (in.size() != sizeof(T)) return {nullptr, XFixedMessageStatus::wrong_size};
        if (!detail::fixed_msg::aligned_for(in.data(), alignof(T))) return {nullptr, XFixedMessageStatus::misaligned};
        const T* p = std::launder(reinterpret_cast<const T*>(in.data()));
        if constexpr (detail::wire_value_needs_validation_v<T>)
            if (detail::fixed_msg::bool_bits(*p) > 1) return {nullptr, XFixedMessageStatus::invalid_value};
        return {p, XFixedMessageStatus::ok};
    }

    // The tag of the batch `in` holds, or 0 when it is shorter than a batch
    // header.
    inline std::uint32_t peek_fixed_batch_tag(std::span<const std::byte> in) noexcept {
        if (in.size() < sizeof(XFixedBatchHeader)) return 0;
        XFixedBatchHeader h;
        std::memcpy(&h, in.data(), sizeof h);
        return h.tag;
    }

    // The records of the batch `in` holds: aligned for T, T's tag, exactly
    // fixed_batch_bytes<T>(count) bytes.
    template <fixed_message T>
    inline XFixedBatchView<T> view_fixed_batch(std::span<const std::byte> in) noexcept {
        if (in.size() < sizeof(XFixedBatchHeader)) return {{}, XFixedMessageStatus::wrong_size};
        if (!detail::fixed_msg::aligned_for(in.data(), alignof(T))) return {{}, XFixedMessageStatus::misaligned};
        XFixedBatchHeader h;
        std::memcpy(&h, in.data(), sizeof h);
        if (h.tag != fixed_message_tag_v<T>) return {{}, XFixedMessageStatus::wrong_tag};
        if (in.size() != fixed_batch_bytes<T>(h.count)) return {{}, XFixedMessageStatus::wrong_size};
        if (h.count == 0) return {{}, XFixedMessageStatus::ok};
        const std::span<const T> records(
            std::launder(reinterpret_cast<const T*>(in.data() + sizeof(XFixedBatchHeader))), h.count);
        if constexpr (detail::wire_value_needs_validation_v<T>) {
            unsigned bits = 0;
            for (const T& v : records) bits |= detail::fixed_msg::bool_bits(v);
            if (bits > 1) return {{}, XFixedMessageStatus::invalid_value};
        }
        return {records, XFixedMessageStatus::ok};
    }

} // namespace XOffsetDatastructure

#endif // X_OFFSET_FIXED_MESSAGE_HPP
