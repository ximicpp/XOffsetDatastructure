#ifndef X_OFFSET_RECEIVER_HPP
#define X_OFFSET_RECEIVER_HPP

// Trust tiers by link type (3.4 trust tier design, phase P1).
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
//   XCheck::bounds read-only bounds check for client uplinks (phase P3, with
//                  XView in P4); not available yet.
//   XCheck::full   load_verified / reload_verified: every check.
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
