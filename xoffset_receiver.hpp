#ifndef X_OFFSET_RECEIVER_HPP
#define X_OFFSET_RECEIVER_HPP

// XReceiver: receives wires and deltas with the check its link type calls
// for (docs/USER_GUIDE.md section 8, docs/V2_THREAT_MODEL.md). server_peer
// and from_server default to XCheck::admit; from_client checks what a client
// sends (bounds for a view, full for a mutable result) and admit does not
// compile there, unless the build sets XOFFSET_CHECK_UPLINK=0. Building a
// client connection's receiver as server_peer is not caught by the types.
// XOFFSET_FORCE_FULL_CHECK=1 (the default without NDEBUG and under
// sanitizers) runs the full check wherever admit was chosen. The input is a
// wire 3.0 image: admit runs the same parse and copy as full and skips the
// graph walk and the check of the free blocks.

#include "xoffsetdatastructure.hpp"
#include "xoffset_shared_delta.hpp"

namespace XOffsetDatastructure {

enum class XLink : std::uint8_t { server_peer, from_server, from_client };
enum class XCheck : std::uint8_t {
    // The wire framing only: magic, header size, root type id and schema
    // hash, used_bytes within the maximum capacity, the extent table and the
    // length, the root object inside the image; no walk of the object graph,
    // and the free blocks are taken as they are. A delta: its framing,
    // stream and version, every run or block inside [32, used) of the
    // image, the root inside it. For a sender that is our own code over a protected
    // transport with the same schema. The result is mutable.
    admit,
    // Read-only: admit's framing, then one walk from the root over every
    // container head it reaches: each block inside the used payload past
    // the header and aligned, size <= capacity, a heap string ends in NUL,
    // an inline string is at most 10 bytes, a bool is 0 or 1, and the bytes
    // walked add up to at most the payload (blocks may overlap). Not checked:
    // overlap, coverage, key order, padding. The result is an XView.
    bounds,
    // load_verified / reload_verified: every check.
    full
};

/// The check a receiver for link L applies to a mutable result when none is named.
template <XLink L>
inline constexpr XCheck default_mutable_check =
    XOFFSET_CHECK_UPLINK && L == XLink::from_client ? XCheck::full : XCheck::admit;

/// The check a receiver for link L applies to a view when none is named.
template <XLink L>
inline constexpr XCheck default_view_check =
    XOFFSET_CHECK_UPLINK && L == XLink::from_client ? XCheck::bounds : XCheck::admit;

/// A read-only view of a wire's root, in the wire's own bytes (XReceiver::view):
/// nothing is copied. Only const access is given, so no write compiles; to
/// change the data, receive it into an XBuffer with reload() (on from_client
/// that is the full check). The view refers to the wire: the bytes must stay
/// alive and unchanged while it is used.
template <typename T>
class XView {
public:
    const T& root() const noexcept { return *root_; }
    const T& operator*() const noexcept { return *root_; }
    const T* operator->() const noexcept { return root_; }
    /// The wire the view reads.
    std::span<const std::byte> wire() const noexcept { return wire_; }
    /// The check tier it passed (XCheck::admit or XCheck::bounds).
    XCheck tier() const noexcept { return check_; }

private:
    template <XLink> friend class XReceiver;
    XView(const T* root, std::span<const std::byte> wire, XCheck check) noexcept
        : root_(root), wire_(wire), check_(check) {}

    const T* root_;
    std::span<const std::byte> wire_;
    XCheck check_;
};

namespace detail {
    inline constexpr bool force_full_check = XOFFSET_FORCE_FULL_CHECK != 0;

    struct ReceiverAccess {
        template <typename T>
        static vstatus reload_admit(XBuffer& dst, std::span<const std::byte> wire) {
            return dst.template reload_admit_v<T>(wire);
        }
        template <typename T>
        static vstatus load_admit(XBuffer& dst, std::span<const std::byte> wire, XBuffer::MaximumCapacity cap) {
            return dst.template load_admit_v<T>(wire, cap);
        }
    };

    inline std::span<const std::byte> as_wire(const std::string& data) noexcept {
        return {reinterpret_cast<const std::byte*>(data.data()), data.size()};
    }

    // ---- XCheck::bounds (views of client uplinks) -------------------------
    // One walk from the root over the container heads it reaches, in the
    // wire's bytes. Addresses are image offsets: `base` is the address of
    // the wire's first byte (a dense wire is the image), held as an integer.
    // Every block a head names must lie in [40, used) and be aligned;
    // nothing else in the image is read before it is checked. Blocks may overlap (a view only reads), so
    // the element bytes walked are counted against `budget` (the payload
    // size): one small wire cannot make the walk revisit one block many times.
    struct BoundsWalk {
        std::uintptr_t base;
        std::uint64_t used;
        std::uint64_t budget;
        const char* why = nullptr;

        bool fail(const char* reason) noexcept {
            why = reason;
            return false;
        }

        // The block of `bytes` at head-relative `delta` (head at &delta).
        template <std::size_t Align>
        XOFFSET_DETAIL_INLINE bool block(const std::int32_t& delta, std::uint64_t bytes, const char*& out) noexcept {
            const std::uint64_t head = reinterpret_cast<std::uintptr_t>(&delta) - base;
            const std::int64_t off = std::int64_t(head) + delta;
            if (off < std::int64_t(XWIRE_PAYLOAD_BEGIN) || std::uint64_t(off) > used ||
                bytes > used - std::uint64_t(off) || (std::uint64_t(off) & 7u) != 0) [[unlikely]]
                return fail("block out of bounds");
            const auto address = base + std::uint64_t(off);
            if constexpr (Align > 8) {
                if ((address & (Align - 1u)) != 0) [[unlikely]] return fail("misaligned block");
            }
            out = reinterpret_cast<const char*>(address);
            return true;
        }
    };

    template <typename T>
    XOFFSET_DETAIL_INLINE bool bounds_value(const T& value, BoundsWalk& walk) noexcept;

    XOFFSET_DETAIL_INLINE bool bounds_string(const XString& value, BoundsWalk& walk) noexcept {
        if (value.is_inline()) [[likely]] {
            // Bytes past the length are not read; the length must stay in the head.
            if (((value.capacity_ >> 24) & 0x7fu) > XString::inline_max) [[unlikely]]
                return walk.fail("inline string length exceeds 10");
            return true;
        }
        if (value.size_ > value.capacity_) [[unlikely]] return walk.fail("string size exceeds capacity");
        if (value.data_delta == 0) return value.size_ == 0 || walk.fail("empty string with a size");
        const char* data;
        if (!walk.block<8>(value.data_delta, std::uint64_t(value.capacity_) + 1u, data)) [[unlikely]] return false;
        // c_str() stops at the terminator inside the block.
        return data[value.size_] == '\0' || walk.fail("string missing terminator");
    }

    template <typename E>
    XOFFSET_DETAIL_INLINE bool bounds_vector(const XVectorCore<E>& value, BoundsWalk& walk) noexcept {
        if (value.size_ > value.capacity_) [[unlikely]] return walk.fail("vector size exceeds capacity");
        if (value.data_delta == 0) return value.size_ == 0 || walk.fail("empty vector with a size");
        const char* raw;
        if (!walk.block<(alignof(E) > 8 ? alignof(E) : 8)>(
                value.data_delta, std::uint64_t(value.capacity_) * sizeof(E), raw)) [[unlikely]]
            return false;
        if constexpr (wire_value_needs_validation_v<E>) {
            const std::uint64_t bytes = std::uint64_t(value.size_) * sizeof(E);
            if (bytes > walk.budget) [[unlikely]] return walk.fail("walk exceeds the payload (overlapping blocks)");
            walk.budget -= bytes;
            const auto* data = reinterpret_cast<const E*>(raw);
            for (std::uint32_t i = 0; i < value.size_; ++i)
                if (!bounds_value(data[i], walk)) [[unlikely]] return false;
        }
        return true;
    }

    template <typename T, std::size_t... Bs, std::size_t... Ms>
    XOFFSET_DETAIL_INLINE bool bounds_record(const T& value, BoundsWalk& walk, std::index_sequence<Bs...>,
                                             std::index_sequence<Ms...>) noexcept {
        auto member = [&]<std::size_t N>(std::integral_constant<std::size_t, N>) XOFFSET_DETAIL_LAMBDA_INLINE {
            using M = schema::member_t<T, N>;
            if constexpr (wire_value_needs_validation_v<typename M::type>) return bounds_value(M::get(value), walk);
            else return true;
        };
        return (bounds_value(static_cast<const schema::base_t<T, Bs>&>(value), walk) && ...) &&
               (member(std::integral_constant<std::size_t, Ms>{}) && ...);
    }

    template <typename T>
    XOFFSET_DETAIL_INLINE bool bounds_value(const T& value, BoundsWalk& walk) noexcept {
        using CleanT = std::remove_cv_t<T>;
        if constexpr (std::is_same_v<CleanT, bool>) {
            unsigned char bits;
            std::memcpy(&bits, &value, 1);
            return bits <= 1 || walk.fail("invalid bool");
        } else if constexpr (std::is_array_v<CleanT>) {
            if constexpr (wire_value_needs_validation_v<std::remove_extent_t<CleanT>>)
                for (const auto& element : value)
                    if (!bounds_value(element, walk)) [[unlikely]] return false;
            return true;
        } else if constexpr (is_fixed_string_v<CleanT>) {
            return bounds_string(value, walk);
        } else if constexpr (is_fixed_vector_v<CleanT> || is_fixed_flat_set_v<CleanT> ||
                             is_fixed_flat_map_v<CleanT>) {
            // Sets and maps: their key order is not checked; a lookup in a
            // misordered one stays inside [0, size) and may miss.
            return bounds_vector(static_cast<const XVectorCore<typename CleanT::value_type>&>(value), walk);
        } else if constexpr (std::is_class_v<CleanT> && wire_value_needs_validation_v<CleanT>) {
            return bounds_record(value, walk, std::make_index_sequence<schema::base_count_v<CleanT>>{},
                                 std::make_index_sequence<schema::member_count_v<CleanT>>{});
        } else {
            return true;
        }
    }

    // The header words of a wire for T that do not depend on the image:
    // the magic and header size, then the fingerprint.
    template <typename T>
    constexpr std::array<std::uint64_t, 3> view_wire_image() noexcept {
        return {XBufferCore::image_magic, wire_schema_hash<T>(), wire_root_type_id<T>()};
    }

    // The framing a view needs, as a status with the error the full check
    // would give for the same header (only reached when the fast test fails).
    template <typename T>
    vstatus view_framing_error(std::span<const std::byte> wire, std::size_t cap, const char* who) {
        XWireHeader header;
        XOFFSET_VTRY(read_wire_header(wire.data(), wire.size_bytes(), who, header));
        if (header.root_type_id != wire_root_type_id<T>())
            XOFFSET_VFAIL(XException, XErrc::root_type_mismatch, std::string(who) + ": root type id mismatch");
        if (header.schema_hash != wire_schema_hash<T>())
            XOFFSET_VFAIL(XException, XErrc::schema_mismatch, std::string(who) + ": schema hash mismatch");
        XOFFSET_VTRY(check_wire_extent(header, who));
        XOFFSET_VTRY(require_wire_within_capacity(header.used_bytes, cap, true, who));
        if (header.extent_count != 0)
            XOFFSET_VFAIL(XException, XErrc::invalid_argument,
                          std::string(who) + ": a view needs a dense wire (no extent table); use reload");
        XOFFSET_VFAIL(XException, XErrc::size_mismatch, std::string(who) + ": used size mismatch");
    }
} // namespace detail

template <XLink L>
class XReceiver {
public:
    static constexpr XLink link = L;

    /// `cap` bounds what load() may reserve for one wire. reload() is bounded
    /// by the destination buffer's own maximum capacity.
    explicit XReceiver(XBuffer::MaximumCapacity cap) noexcept : cap_(cap) {}

    XBuffer::MaximumCapacity maximum_capacity() const noexcept { return cap_; }

    /// Loads `wire` into `dst`, reusing its mapping, like reload_verified<T>:
    /// on success dst holds the sender's payload with typed root access; on
    /// failure dst is left empty without typed root access. Every reference
    /// into dst's previous content is invalidated. No host allocation per
    /// message for dense wires once dst's free table has its size.
    template <typename T, XCheck C = default_mutable_check<L>>
    [[nodiscard]] XStatus reload(XBuffer& dst, std::span<const std::byte> wire) {
        check_mutable<C>();
        if constexpr (C == XCheck::full || detail::force_full_check) {
            return dst.template try_reload_verified<T>(wire);
        } else {
            return detail::try_v([&] { return detail::ReceiverAccess::reload_admit<T>(dst, wire); });
        }
    }

    template <typename T, XCheck C = default_mutable_check<L>>
    [[nodiscard]] XStatus reload(XBuffer& dst, const std::string& wire) {
        return reload<T, C>(dst, detail::as_wire(wire));
    }

    /// The same for a typed buffer: rx.reload(buffer, wire), rx.reload<XCheck::full>(buffer, wire).
    template <XCheck C = default_mutable_check<L>, typename T>
    [[nodiscard]] XStatus reload(TypedXBuffer<T>& dst, std::span<const std::byte> wire) {
        return reload<T, C>(static_cast<XBuffer&>(dst), wire);
    }

    template <XCheck C = default_mutable_check<L>, typename T>
    [[nodiscard]] XStatus reload(TypedXBuffer<T>& dst, const std::string& wire) {
        return reload<T, C>(static_cast<XBuffer&>(dst), detail::as_wire(wire));
    }

    /// A new buffer holding `wire` (at most maximum_capacity()), like
    /// load_verified<T>. Maps fresh memory every call: use reload() on a hot path.
    template <typename T, XCheck C = default_mutable_check<L>>
    [[nodiscard]] XResult<XBuffer> load(std::span<const std::byte> wire) {
        check_mutable<C>();
        if constexpr (C == XCheck::full || detail::force_full_check) {
            return XBuffer::try_load_verified<T>(wire, cap_);
        } else {
            XBuffer out;
            return detail::try_v_into(out, [&] { return detail::ReceiverAccess::load_admit<T>(out, wire, cap_); });
        }
    }

    template <typename T, XCheck C = default_mutable_check<L>>
    [[nodiscard]] XResult<XBuffer> load(const std::string& wire) {
        return load<T, C>(detail::as_wire(wire));
    }

    /// load() as a TypedXBuffer<T>.
    template <typename T, XCheck C = default_mutable_check<L>>
    [[nodiscard]] XResult<TypedXBuffer<T>> load_typed(std::span<const std::byte> wire) {
        auto loaded = load<T, C>(wire);
        if (!loaded) return loaded.error();
        return XResult<TypedXBuffer<T>>(TypedXBuffer<T>(std::move(*loaded)));
    }

    template <typename T, XCheck C = default_mutable_check<L>>
    [[nodiscard]] XResult<TypedXBuffer<T>> load_typed(const std::string& wire) {
        return load_typed<T, C>(detail::as_wire(wire));
    }

    /// Applies `delta` to `dst`, the replica `cursor` follows, like
    /// apply_delta_verified<T> (docs/DELTA_SYNC.md): a key delta replaces the
    /// content, the others patch it in place. With admit (the default on
    /// server_peer and from_server) the delta's framing is checked as for the
    /// full check (format, stream and version against the cursor, root type
    /// and schema fingerprint, every run or block inside [32, used) of the image)
    /// plus the root object inside the used payload; what the bytes say is the
    /// sender's. No map of checked bytes is kept, so a full apply that later
    /// follows admits on the same cursor verifies the whole payload once. On
    /// failure dst keeps its content (a failed key delta leaves it empty), as
    /// with apply_delta_verified<T>.
    template <typename T, XCheck C = default_mutable_check<L>>
    [[nodiscard]] XStatus apply_delta(XBuffer& dst, std::span<const std::byte> delta, XDeltaCursor& cursor) {
        check_mutable<C>();
        if constexpr (C == XCheck::full || detail::force_full_check) {
            return dst.template try_apply_delta_verified<T>(delta, cursor);
        } else {
            return detail::try_v([&] { return detail::DeltaAccess::apply<T, true>(dst, delta, cursor); });
        }
    }

    template <typename T, XCheck C = default_mutable_check<L>>
    [[nodiscard]] XStatus apply_delta(XBuffer& dst, const std::string& delta, XDeltaCursor& cursor) {
        return apply_delta<T, C>(dst, detail::as_wire(delta), cursor);
    }

    /// The same for a typed buffer: rx.apply_delta(buffer, delta, cursor).
    template <XCheck C = default_mutable_check<L>, typename T>
    [[nodiscard]] XStatus apply_delta(TypedXBuffer<T>& dst, std::span<const std::byte> delta, XDeltaCursor& cursor) {
        return apply_delta<T, C>(static_cast<XBuffer&>(dst), delta, cursor);
    }

    template <XCheck C = default_mutable_check<L>, typename T>
    [[nodiscard]] XStatus apply_delta(TypedXBuffer<T>& dst, const std::string& delta, XDeltaCursor& cursor) {
        return apply_delta<T, C>(static_cast<XBuffer&>(dst), detail::as_wire(delta), cursor);
    }

    /// Applies the shared delta `msg` (an XS 2.0 message) to the records
    /// `target` keeps for the stream `cursor` follows. The Target contract
    /// (tables, keyed, find, record, enter, leave, applied) and the apply
    /// rules are in docs/SHARED_DELTA.md ("The target", section 3).
    ///
    /// admit (the default) checks the head, the stream and version, every
    /// section's table and kind, and every op's id; bodies are the sender's,
    /// and a message rejected after the head may be partly applied (the
    /// cursor is then empty). full checks every body against its record type
    /// and every id before anything applies, so a rejected message changes
    /// nothing, and what it accepts keeps the records' buffer one that
    /// load_verified<T> accepts.
    template <XCheck C = default_mutable_check<L>, class Target>
    [[nodiscard]] XStatus apply_sync(std::span<const std::byte> msg, xsync::XSyncCursor& cursor, Target& target) {
        static_assert(L != XLink::from_client,
                      "XReceiver<XLink::from_client>::apply_sync: shared deltas go from a server to its clients "
                      "and between servers; a client's uplink is not read as one");
        check_mutable<C>();
        const char* const p = reinterpret_cast<const char*>(msg.data());
        const char* const end = p + msg.size();
        return detail::try_v([&] {
            if constexpr (C == XCheck::full || detail::force_full_check)
                return xsync::sd_detail::SyncAccess::full(p, end, cursor, target);
            else
                return xsync::sd_detail::SyncAccess::admit(p, end, cursor, target);
        });
    }

    template <XCheck C = default_mutable_check<L>, class Target>
    [[nodiscard]] XStatus apply_sync(std::string_view msg, xsync::XSyncCursor& cursor, Target& target) {
        return apply_sync<C>(std::span<const std::byte>(reinterpret_cast<const std::byte*>(msg.data()), msg.size()), cursor, target);
    }

    /// A read-only view of `wire`'s root in the wire's own bytes, without a
    /// copy (XView<T>). XCheck::bounds (the default on from_client, unless the
    /// build sets XOFFSET_CHECK_UPLINK=0) walks the containers once so that no
    /// const access reads outside the payload; XCheck::admit (the default
    /// otherwise) checks the framing and that the root lies in the payload.
    /// Either way the framing is checked as for reload (magic, header size, root type and schema
    /// fingerprint, used_bytes within maximum_capacity() and equal to the wire's
    /// length). The wire must be dense (no extent table: the default emission
    /// sends an image whose free blocks hold less than 4 KiB, or a churned
    /// image's compact wire, dense, and XWireEmission::dense always does),
    /// start on an 8-byte boundary, and
    /// outlive the view. With XOFFSET_FORCE_FULL_CHECK
    /// the full check also runs on a copy, and what it rejects is rejected.
    template <typename T, XCheck C = default_view_check<L>>
    [[nodiscard]] XOFFSET_DETAIL_INLINE XResult<XView<T>> view(std::span<const std::byte> wire) const {
        check_view<C>();
        static_assert(is_wire_admitted_v<T>, "XReceiver::view<T>() requires an admitted fixed-schema wire type.");
        constexpr const char* who = C == XCheck::bounds ? "XReceiver::view<T, bounds>()" : "XReceiver::view<T, admit>()";
        constexpr auto image = detail::view_wire_image<T>();
        constexpr std::size_t head = sizeof(XWireHeader);
        if (wire.size_bytes() < head) [[unlikely]] return framing_error<T>(wire, who);
        const std::byte* const raw = wire.data();
        const std::uint64_t used = wire.size_bytes();
        // A view copies nothing, so the first read of each line of the wire
        // falls in the caller's use of the view, one miss after another
        // inside its loops. A small wire (one message, a few lines) has all
        // its lines requested here, so those misses overlap each other and
        // the checks below. Larger wires are left to the hardware's
        // sequential prefetch.
        if (used <= view_prefetch_bytes) {
            // Every line after the first (the header read below loads it).
            const auto first = reinterpret_cast<std::uintptr_t>(raw) & ~std::uintptr_t(63);
            const auto last = reinterpret_cast<std::uintptr_t>(raw) + used - 1;
            for (auto line = first + 64; line <= last; line += 64)
                XOFFSET_DETAIL_PREFETCH_WITH_MSVC(reinterpret_cast<const void*>(line), 0);
        }
        // Scalar loads avoid a local array on the stack (which triggers MSVC /GS cookies).
        std::uint64_t w0, w1, w2, w3;
        std::memcpy(&w0, raw, 8);
        std::memcpy(&w1, raw + 8, 8);
        std::memcpy(&w2, raw + 16, 8);
        std::memcpy(&w3, raw + 24, 8);
        // Words 0..2: magic and header size, fingerprint; word 3: used_bytes
        // and extent_count (0: dense), the wire's size with nothing else.
        const std::uint64_t diff = (w0 ^ image[0]) | (w1 ^ image[1]) | (w2 ^ image[2]) | (w3 ^ used);
        if (diff != 0 || used <= XWIRE_PAYLOAD_BEGIN || used > cap_.value || (used & 7u) != 0) [[unlikely]]
            return framing_error<T>(wire, who);
        if ((reinterpret_cast<std::uintptr_t>(raw) & 7u) != 0) [[unlikely]]
            return view_error(XErrc::invalid_argument, who, "the wire must start on an 8-byte boundary");
        // The wire is the image: offset 0 is its first byte.
        const auto base = reinterpret_cast<std::uintptr_t>(raw);
        constexpr std::uint64_t root = XWIRE_PAYLOAD_BEGIN;
        if (sizeof(T) > used - root) [[unlikely]]
            return view_error(XErrc::root_mismatch, who, "root not found");
        const auto* object = reinterpret_cast<const T*>(base + root);
        if constexpr (C == XCheck::bounds) {
            detail::BoundsWalk walk{base, used, used};
            if (!detail::bounds_value(*object, walk)) [[unlikely]]
                return view_error(XErrc::invalid_payload, who, walk.why);
        }
        if constexpr (detail::force_full_check) {
            auto full = XBuffer::try_load_verified<T>(wire, cap_);
            if (!full) return std::move(full).error();
        }
        return XView<T>(object, wire, C);
    }

    template <typename T, XCheck C = default_view_check<L>>
    [[nodiscard]] XResult<XView<T>> view(const std::string& wire) const {
        return view<T, C>(detail::as_wire(wire));
    }

private:
    // The largest wire view() prefetches whole (16 lines).
    static constexpr std::size_t view_prefetch_bytes = 1024;

    XOFFSET_DETAIL_COLD static XError view_error(XErrc code, const char* who, const char* detail) {
        return XError{code, std::string(who) + ": " + detail};
    }

    template <typename T>
    XOFFSET_DETAIL_COLD XError framing_error(std::span<const std::byte> wire, const char* who) const {
        XStatus status = detail::try_v([&] { return detail::view_framing_error<T>(wire, cap_.value, who); });
        if (!status) return std::move(status).error();
        return XError{XErrc::unspecified, std::string(who) + ": framing"};
    }

    template <XCheck C>
    static constexpr void check_view() noexcept {
        static_assert(C != XCheck::full,
                      "XReceiver::view: a view is read-only (XCheck::bounds or XCheck::admit); "
                      "the full check gives a mutable XBuffer through reload or load");
        static_assert(!(XOFFSET_CHECK_UPLINK && L == XLink::from_client && C == XCheck::admit),
                      "XReceiver<XLink::from_client>: data from a client is never admitted unchecked; "
                      "view it with XCheck::bounds");
    }

    template <XCheck C>
    static constexpr void check_mutable() noexcept {
        static_assert(C != XCheck::bounds,
                      "XReceiver: XCheck::bounds gives a read-only result (view()); "
                      "a mutable result takes XCheck::admit or XCheck::full");
        static_assert(!(XOFFSET_CHECK_UPLINK && L == XLink::from_client && C == XCheck::admit),
                      "XReceiver<XLink::from_client>: data from a client is never admitted unchecked; "
                      "use XCheck::full");
    }

    XBuffer::MaximumCapacity cap_;
};

} // namespace XOffsetDatastructure

#endif
