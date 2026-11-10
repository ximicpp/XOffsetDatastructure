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
//   XCheck::bounds read-only bounds check for client uplinks (phase P3, with
//                  XView in P4); not available yet.
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

private:
    template <XCheck C>
    static constexpr void check_mutable() noexcept {
        static_assert(C != XCheck::bounds,
                      "XReceiver: XCheck::bounds gives a read-only result (XView, not available yet); "
                      "a mutable result takes XCheck::admit or XCheck::full");
        static_assert(!(L == XLink::from_client && C == XCheck::admit),
                      "XReceiver<XLink::from_client>: data from a client is never admitted unchecked; "
                      "use XCheck::full");
    }

    XBuffer::MaximumCapacity cap_;
};

} // namespace XOffsetDatastructure

#endif
