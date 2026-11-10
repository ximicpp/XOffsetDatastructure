#ifndef X_OFFSET_RECEIVER_HPP
#define X_OFFSET_RECEIVER_HPP

// Trust tiers by link type (3.4 trust tier design, phases P1 and P2).
//
// A receiver is built for one kind of link, and the link decides how much of
// an incoming wire is checked:
//
//   XLink::server_peer   server <-> server        default XCheck::admit
//   XLink::from_server   a client receiving        default XCheck::admit
//   XLink::from_client   a server receiving        XCheck::full only (admit does
//                        from a client             not compile)
//
//   XCheck::admit  the wire framing only: magic, flags, root type id and
//                  schema hash, used_bytes within the maximum capacity, the
//                  extent and hole tables' lengths, the root object inside the
//                  payload, every hole an aligned 8-byte multiple inside the
//                  payload. No walk of the object graph: the payload is used
//                  as the sender wrote it. For links whose sender is our own
//                  code over a protected transport (TLS or equivalent), with
//                  the same schema on both sides. The result is mutable.
//                  For a delta (apply_delta): its framing, stream and
//                  version, every block and hole inside the used payload and
//                  the root object inside it; no map of checked bytes is kept.
//   XCheck::bounds read-only: the framing as admit, then one walk from the
//                  root over every container head it reaches: each block a
//                  head names lies inside the used payload (past the payload
//                  header), aligned; size <= capacity; a heap string ends in
//                  NUL; an inline string is at most 10 bytes; a bool is 0 or
//                  1; the element bytes walked add up to at most the payload
//                  (blocks may overlap, so this caps the work). Not checked:
//                  overlap and coverage, set and map key order, padding. The
//                  result is an XView (const access only), read in place.
//                  Default for view() on from_client; on trusted links view()
//                  defaults to admit (framing and the root only).
//   XCheck::full   load_verified / reload_verified: every check.
//
// Shared deltas (XS 1.0, xoffset_shared_delta.hpp, docs/SHARED_DELTA.md) go
// through apply_sync on server_peer and from_server receivers only: a server
// does not read a client's uplink as one, so apply_sync does not compile on
// from_client. admit there reads the header, the stream and version, every
// op's table, kind and id against the receiver's records, and trusts the
// bodies; full also checks every body against its record and applies nothing
// of a message it rejects.
//
// The input is a V2 wire (save_verified, save_compact, any emission), not an
// arena image: admit runs the same parse and copy as the full check and only
// skips the graph walk, so a reload into a buffer whose pages are already
// touched costs about one copy of the payload plus a fixed amount.
//
// There is no global switch and no trusted member on XBuffer: only a receiver
// built for a trusted link can admit. Building a client connection's receiver
// as server_peer is not caught by the types; that is left to review.
//
// Safety net: with XOFFSET_FORCE_FULL_CHECK=1 every admit runs the full check
// instead (a sender bug is then rejected rather than used). It defaults to 1
// in builds without NDEBUG and under AddressSanitizer or ThreadSanitizer, else
// 0; define it to 0 or 1 to choose.

#include "xoffsetdatastructure.hpp"
#include "xoffset_shared_delta.hpp"

// XReceiver::view and XView (XCheck::bounds for client uplinks, 3.5).
#define XOFFSET_RECEIVER_VIEW_API 1

#ifndef XOFFSET_FORCE_FULL_CHECK
#  if !defined(NDEBUG) || defined(__SANITIZE_ADDRESS__) || defined(__SANITIZE_THREAD__)
#    define XOFFSET_FORCE_FULL_CHECK 1
#  elif defined(__has_feature)
#    if __has_feature(address_sanitizer) || __has_feature(thread_sanitizer)
#      define XOFFSET_FORCE_FULL_CHECK 1
#    endif
#  endif
#  ifndef XOFFSET_FORCE_FULL_CHECK
#    define XOFFSET_FORCE_FULL_CHECK 0
#  endif
#endif

namespace XOffsetDatastructure {

enum class XLink : std::uint8_t { server_peer, from_server, from_client };
enum class XCheck : std::uint8_t { admit, bounds, full };

/// The check a receiver for link L applies to a mutable result when none is named.
template <XLink L>
inline constexpr XCheck default_mutable_check = L == XLink::from_client ? XCheck::full : XCheck::admit;

/// The check a receiver for link L applies to a view when none is named.
template <XLink L>
inline constexpr XCheck default_view_check = L == XLink::from_client ? XCheck::bounds : XCheck::admit;

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
    /// The check it passed (XCheck::admit or XCheck::bounds).
    XCheck check() const noexcept { return check_; }

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
    // wire's bytes. Addresses are payload offsets: `base` is the address of
    // payload offset 0 (the wire's payload bytes start at offset 48), held as
    // an integer because it lies before the wire. Every block a head names
    // must lie in [48, used) and be aligned; nothing else in the payload is
    // read before it is checked. Blocks may overlap (a view only reads), so
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
        bool block(const std::int32_t& delta, std::uint64_t bytes, const char*& out) noexcept {
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
    bool bounds_value(const T& value, BoundsWalk& walk) noexcept;

    inline bool bounds_string(const XFixedString& value, BoundsWalk& walk) noexcept {
        if (value.is_inline()) [[likely]] {
            // Bytes past the length are not read; the length must stay in the head.
            if (((value.capacity_ >> 24) & 0x7fu) > XFixedString::inline_max) [[unlikely]]
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
    bool bounds_vector(const XFixedVector<E>& value, BoundsWalk& walk) noexcept {
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
    bool bounds_record(const T& value, BoundsWalk& walk, std::index_sequence<Bs...>,
                       std::index_sequence<Ms...>) noexcept {
        auto member = [&]<std::size_t N>(std::integral_constant<std::size_t, N>) {
            using M = schema::member_t<T, N>;
            if constexpr (wire_value_needs_validation_v<typename M::type>) return bounds_value(M::get(value), walk);
            else return true;
        };
        return (bounds_value(static_cast<const schema::base_t<T, Bs>&>(value), walk) && ...) &&
               (member(std::integral_constant<std::size_t, Ms>{}) && ...);
    }

    template <typename T>
    bool bounds_value(const T& value, BoundsWalk& walk) noexcept {
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
            return bounds_vector(static_cast<const XFixedVector<typename CleanT::value_type>&>(value), walk);
        } else if constexpr (std::is_class_v<CleanT> && wire_value_needs_validation_v<CleanT>) {
            return bounds_record(value, walk, std::make_index_sequence<schema::base_count_v<CleanT>>{},
                                 std::make_index_sequence<schema::member_count_v<CleanT>>{});
        } else {
            return true;
        }
    }

    // The header fields of a dense wire for T that do not depend on the
    // payload, as words (used_bytes, root_offset, hole_count and extent_count 0).
    template <typename T>
    constexpr std::array<std::uint64_t, 5> view_wire_image() noexcept {
        XWireHeaderV2 h{};
        h.magic = XWIRE_MAGIC;
        h.flags = XWIRE_FLAGS;
        h.schema_hash = wire_schema_hash<T>();
        h.root_type_id = wire_root_type_id<T>();
        return std::bit_cast<std::array<std::uint64_t, 5>>(h);
    }

    // The framing a view needs, as a status with the error the full check
    // would give for the same header (only reached when the fast test fails).
    template <typename T>
    vstatus view_framing_error(std::span<const std::byte> wire, std::size_t cap, const char* who) {
        XWireHeaderV2 header;
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
#if XOFFSET_NO_EXCEPTIONS
            if (!detail::ReceiverAccess::reload_admit<T>(dst, wire)) return detail::take_pending_error();
            return {};
#else
            try {
                detail::ReceiverAccess::reload_admit<T>(dst, wire);
                return {};
            } catch (...) {
                return detail::current_exception_error();
            }
#endif
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
#if XOFFSET_NO_EXCEPTIONS
            XBuffer out;
            if (!detail::ReceiverAccess::load_admit<T>(out, wire, cap_)) return detail::take_pending_error();
            return XResult<XBuffer>(std::move(out));
#else
            try {
                XBuffer out;
                detail::ReceiverAccess::load_admit<T>(out, wire, cap_);
                return XResult<XBuffer>(std::move(out));
            } catch (...) {
                return detail::current_exception_error();
            }
#endif
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
    /// and schema fingerprint, every block and hole inside the used payload)
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
#if XOFFSET_NO_EXCEPTIONS
            if (!detail::DeltaAccess::apply<T, true>(dst, delta, cursor)) return detail::take_pending_error();
            return {};
#else
            try {
                detail::DeltaAccess::apply<T, true>(dst, delta, cursor);
                return {};
            } catch (...) {
                return detail::current_exception_error();
            }
#endif
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

    /// Applies the shared delta `msg` (an XS 1.0 message) to the records
    /// `target` keeps for the stream `cursor` follows (docs/SHARED_DELTA.md).
    /// The target names its tables and finds, adds and drops their entries:
    ///
    ///   using tables = std::tuple<R0, R1, ...>;      // the record type of table K (1 to 64 tables)
    ///   template <unsigned K> static constexpr bool keyed = ...;
    ///                                                 // entries enter and leave
    ///   template <unsigned K> std::int64_t find(std::uint32_t id);
    ///                                                 // the entry's handle, < 0 if not held
    ///   template <unsigned K> R& record(std::int64_t handle);
    ///   keyed tables:
    ///   template <unsigned K> std::int64_t enter(std::uint32_t id);
    ///                                                 // adds the entry: its handle, < 0 if it is
    ///                                                 // held already or cannot be added
    ///   template <unsigned K> void leave(std::uint32_t id, std::int64_t handle);
    ///                                                 // drops it; its record is left as it is
    ///   optional:
    ///   template <unsigned K> void applied(std::uint32_t id, std::int64_t handle);
    ///                                                 // after an enter or change wrote the record
    ///
    /// An enter clears the record (XRecordDelta<R>::clear) before its body
    /// applies: on a keyed table the one enter() returns, on a fixed table
    /// the one find() returns. A leave does not touch the record; a target
    /// whose free records must read as empty clears it in leave(). record()
    /// is called for each op after that op's enter, and the reference is
    /// used for that op only (adding an entry or a container operation may
    /// move the records' storage). A handle stays valid until its own entry
    /// leaves: leave() must not move other entries. A key message
    /// (xsync::is_key) enters every entry: empty the keyed tables before
    /// applying one. Other messages need the next version of the stream the
    /// cursor holds, else delta_base_mismatch (ask the sender for a key
    /// message).
    ///
    /// admit (the default) checks the header, the stream and version, and
    /// every op's table, kind and id against the records held; bodies are
    /// the sender's. A message it rejects after the header may be partly
    /// applied: the cursor is then empty. full checks every body against its
    /// record (an enter against a cleared one) and that no entry appears
    /// twice before any op applies: a message it rejects changes nothing.
    /// What full accepts writes only plain bytes of the record type (checked
    /// against its byte classes) and changes containers through library
    /// operations in the receiver's own arena, so a buffer holding the
    /// records stays one that load_verified<T> accepts. admit leaves that to
    /// the sender: bodies that are not the sender's own may be read past
    /// their end and may write any byte of a record.
    template <XCheck C = default_mutable_check<L>, class Target>
    [[nodiscard]] XStatus apply_sync(std::span<const std::byte> msg, xsync::XSyncCursor& cursor, Target& target) {
        static_assert(L != XLink::from_client,
                      "XReceiver<XLink::from_client>::apply_sync: shared deltas go from a server to its clients "
                      "and between servers; a client's uplink is not read as one");
        check_mutable<C>();
        const char* const p = reinterpret_cast<const char*>(msg.data());
        const char* const end = p + msg.size();
#if XOFFSET_NO_EXCEPTIONS
        bool ok;
        if constexpr (C == XCheck::full || detail::force_full_check) ok = bool(xsync::sd_detail::SyncAccess::full(p, end, cursor, target));
        else ok = bool(xsync::sd_detail::SyncAccess::admit(p, end, cursor, target));
        if (!ok) return detail::take_pending_error();
        return {};
#else
        try {
            if constexpr (C == XCheck::full || detail::force_full_check) xsync::sd_detail::SyncAccess::full(p, end, cursor, target);
            else xsync::sd_detail::SyncAccess::admit(p, end, cursor, target);
            return {};
        } catch (...) {
            return detail::current_exception_error();
        }
#endif
    }

    template <XCheck C = default_mutable_check<L>, class Target>
    [[nodiscard]] XStatus apply_sync(std::string_view msg, xsync::XSyncCursor& cursor, Target& target) {
        return apply_sync<C>(std::span<const std::byte>(reinterpret_cast<const std::byte*>(msg.data()), msg.size()), cursor, target);
    }

    /// A read-only view of `wire`'s root in the wire's own bytes, without a
    /// copy (XView<T>). XCheck::bounds (the default on from_client) walks the
    /// containers once so that no const access reads outside the payload;
    /// XCheck::admit (the default on server_peer and from_server) checks the
    /// framing and that the root lies in the payload. Either way the framing
    /// is checked as for reload (magic, flags, root type and schema
    /// fingerprint, used_bytes within maximum_capacity(), root offset, section
    /// lengths). The wire must be dense (no extent table: wires below 4 KiB
    /// always are, larger ones when sent with XWireEmission::dense), start on
    /// an 8-byte boundary, and outlive the view. With XOFFSET_FORCE_FULL_CHECK
    /// the full check also runs on a copy, and what it rejects is rejected.
    template <typename T, XCheck C = default_view_check<L>>
    [[nodiscard]] XResult<XView<T>> view(std::span<const std::byte> wire) const {
        check_view<C>();
        static_assert(is_v1_wire_admitted_v<T>, "XReceiver::view<T>() requires an admitted fixed-schema wire type.");
        constexpr const char* who = C == XCheck::bounds ? "XReceiver::view<T, bounds>()" : "XReceiver::view<T, admit>()";
        constexpr auto image = detail::view_wire_image<T>();
        constexpr std::size_t head = sizeof(XWireHeaderV2);
        std::array<std::uint64_t, 5> words;
        if (wire.size_bytes() < head) [[unlikely]] return framing_error<T>(wire, who);
        std::memcpy(words.data(), wire.data(), head);
        const auto used = std::uint64_t(std::uint32_t(words[1]));
        const auto root = std::uint64_t(std::uint32_t(words[1] >> 32));
        const auto holes = std::uint64_t(std::uint32_t(words[2]));
        // Word 0: magic, flags; word 2 high: extent_count (0: dense); 3, 4: fingerprint.
        const std::uint64_t diff = (words[0] ^ image[0]) | (words[2] >> 32) | (words[3] ^ image[3]) | (words[4] ^ image[4]);
        const std::uint64_t body = wire.size_bytes() - head;
        if (diff != 0 || used < XWIRE_PAYLOAD_BEGIN || used > cap_.value || (used & 7u) != 0 ||
            root < XWIRE_PAYLOAD_BEGIN || root >= used || used - XWIRE_PAYLOAD_BEGIN > body ||
            body - (used - XWIRE_PAYLOAD_BEGIN) != holes * 8u) [[unlikely]]
            return framing_error<T>(wire, who);
        if ((reinterpret_cast<std::uintptr_t>(wire.data()) & 7u) != 0) [[unlikely]]
            return XError{XErrc::invalid_argument, std::string(who) + ": the wire must start on an 8-byte boundary"};
        // Payload offset 0 lies 8 bytes before the wire (the payload header is not sent).
        const auto base = reinterpret_cast<std::uintptr_t>(wire.data()) + head - XWIRE_PAYLOAD_BEGIN;
        if (sizeof(T) > used - root || (root & (alignof(T) - 1u)) != 0 ||
            ((base + root) & (alignof(T) - 1u)) != 0) [[unlikely]]
            return XError{XErrc::root_mismatch, std::string(who) + ": root not found"};
        const auto* object = reinterpret_cast<const T*>(base + root);
        if constexpr (C == XCheck::bounds) {
            detail::BoundsWalk walk{base, used, used};
            if (!detail::bounds_value(*object, walk)) [[unlikely]]
                return XError{XErrc::invalid_payload, std::string(who) + ": " + walk.why};
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
    template <typename T>
    XError framing_error(std::span<const std::byte> wire, const char* who) const {
#if XOFFSET_NO_EXCEPTIONS
        (void)detail::view_framing_error<T>(wire, cap_.value, who);
        return detail::take_pending_error();
#else
        try {
            detail::view_framing_error<T>(wire, cap_.value, who);
        } catch (...) {
            return detail::current_exception_error();
        }
        return XError{XErrc::unspecified, std::string(who) + ": framing"};
#endif
    }

    template <XCheck C>
    static constexpr void check_view() noexcept {
        static_assert(C != XCheck::full,
                      "XReceiver::view: a view is read-only (XCheck::bounds or XCheck::admit); "
                      "the full check gives a mutable XBuffer through reload or load");
        static_assert(!(L == XLink::from_client && C == XCheck::admit),
                      "XReceiver<XLink::from_client>: data from a client is never admitted unchecked; "
                      "view it with XCheck::bounds");
    }

    template <XCheck C>
    static constexpr void check_mutable() noexcept {
        static_assert(C != XCheck::bounds,
                      "XReceiver: XCheck::bounds gives a read-only result (view()); "
                      "a mutable result takes XCheck::admit or XCheck::full");
        static_assert(!(L == XLink::from_client && C == XCheck::admit),
                      "XReceiver<XLink::from_client>: data from a client is never admitted unchecked; "
                      "use XCheck::full");
    }

    XBuffer::MaximumCapacity cap_;
};

} // namespace XOffsetDatastructure

#endif
