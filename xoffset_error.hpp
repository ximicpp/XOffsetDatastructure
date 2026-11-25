#pragma once
// Error codes and the non-throwing result types shared by every XOffset
// header (the exception switch is in xoffset_config.hpp). C++17.
#include "xoffset_config.hpp"

#include <atomic>
#include <cstdint>
#include <cstdlib>
#include <new>
#include <optional>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>

namespace XOffsetDatastructure {

// What went wrong, independent of how it is reported. Receive entries report
// the same code (and message) whether they throw or return it.
enum class XErrc : std::uint16_t {
    ok = 0,
    // Wire framing: header, tables and section lengths.
    bad_magic = 1,                 // not an XOffset wire
    unsupported_version = 2,       // another wire format version
    truncated = 3,                 // shorter than the wire header
    unsupported_flags = 4,
    root_type_mismatch = 5,
    schema_mismatch = 6,
    size_mismatch = 7,             // used size or a section length disagrees with the header
    root_offset_out_of_bounds = 8,
    invalid_extent_table = 9,
    incomplete_input = 10,         // a direct receive did not deliver what it announced
    delta_base_mismatch = 11,      // a delta for another state than the replica holds (ask for a key delta)
    // Payload verification.
    invalid_payload = 32,          // the structural graph check rejected the payload
    root_mismatch = 33,            // no root at the declared offset, or a non-normalized extent
    invalid_payload_header = 34,   // the 40-byte image header is malformed
    digest_mismatch = 35,          // a delta's result differs from the sender's digest
    // Limits and resources.
    capacity_exceeded = 64,        // more than the maximum capacity (or the format limit)
    out_of_storage = 65,           // the arena or its OS storage could not provide memory
    host_allocation_failed = 66,   // the host allocator failed (std::bad_alloc)
    // Usage.
    invalid_argument = 96,
    invalid_state = 97,            // not valid in the object's current state
    length_error = 98,
    no_root = 99,                  // typed access to a buffer without a verified root
    borrowed = 100,                // the buffer would move while a pin scope (read/edit) is open
    stale = 101,                   // a proxy of a buffer that is gone or whose content was replaced
    out_of_range = 102,            // a write through a proxy that names no element (index or key)
    unspecified = 255,
};

inline const char* to_string(XErrc code) noexcept {
    switch (code) {
        case XErrc::ok: return "ok";
        case XErrc::bad_magic: return "bad_magic";
        case XErrc::unsupported_version: return "unsupported_version";
        case XErrc::truncated: return "truncated";
        case XErrc::unsupported_flags: return "unsupported_flags";
        case XErrc::root_type_mismatch: return "root_type_mismatch";
        case XErrc::schema_mismatch: return "schema_mismatch";
        case XErrc::size_mismatch: return "size_mismatch";
        case XErrc::root_offset_out_of_bounds: return "root_offset_out_of_bounds";
        case XErrc::invalid_extent_table: return "invalid_extent_table";
        case XErrc::incomplete_input: return "incomplete_input";
        case XErrc::delta_base_mismatch: return "delta_base_mismatch";
        case XErrc::invalid_payload: return "invalid_payload";
        case XErrc::root_mismatch: return "root_mismatch";
        case XErrc::invalid_payload_header: return "invalid_payload_header";
        case XErrc::digest_mismatch: return "digest_mismatch";
        case XErrc::capacity_exceeded: return "capacity_exceeded";
        case XErrc::out_of_storage: return "out_of_storage";
        case XErrc::host_allocation_failed: return "host_allocation_failed";
        case XErrc::invalid_argument: return "invalid_argument";
        case XErrc::invalid_state: return "invalid_state";
        case XErrc::length_error: return "length_error";
        case XErrc::no_root: return "no_root";
        case XErrc::borrowed: return "borrowed";
        case XErrc::stale: return "stale";
        case XErrc::out_of_range: return "out_of_range";
        case XErrc::unspecified: return "unspecified";
    }
    return "unknown";
}

class XException : public std::runtime_error {
public:
    XException(XErrc code, const std::string& what) : std::runtime_error(what), code_(code) {}
    XException(XErrc code, const char* what) : std::runtime_error(what), code_(code) {}
    explicit XException(const std::string& what) : std::runtime_error(what) {}
    explicit XException(const char* what) : std::runtime_error(what) {}
    XErrc code() const noexcept { return code_; }

private:
    XErrc code_ = XErrc::unspecified;
};

namespace detail {
inline constexpr const char* arena_failure_message = "XOffset arena allocation failed";
// An allocation past the buffer's maximum_capacity, or a container, string
// or block past the payload limit (XErrc::capacity_exceeded).
inline constexpr const char* capacity_failure_message =
    "XOffset allocation exceeds maximum_capacity or the payload limit of 2 GiB - 8 bytes";
// A maximum capacity (or a payload) above XBufferCore::MAX_PAYLOAD.
inline constexpr const char* payload_limit_message =
    "XOffset maximum capacity exceeds the payload limit of 2 GiB - 8 bytes "
    "(XBufferCore::MAX_PAYLOAD; offsets inside a payload are 32-bit)";
} // namespace detail

class XBadAlloc : public XException {
public:
    XBadAlloc() : XException(XErrc::out_of_storage, detail::arena_failure_message) {}
    explicit XBadAlloc(const std::string& what) : XException(XErrc::out_of_storage, what) {}
    XBadAlloc(XErrc code, const std::string& what) : XException(code, what) {}
};

// A failure reported without an exception: the code and the message the
// throwing entry would have thrown.
struct XError {
    XErrc code = XErrc::ok;
    std::string message;
};

// The code of a library exception (what a try_* entry reports for it).
inline XErrc error_code_of(const XException& e) noexcept { return e.code(); }
inline XErrc error_code_of(const std::bad_alloc&) noexcept { return XErrc::host_allocation_failed; }
inline XErrc error_code_of(const std::invalid_argument&) noexcept { return XErrc::invalid_argument; }
inline XErrc error_code_of(const std::length_error&) noexcept { return XErrc::length_error; }
inline XErrc error_code_of(const std::exception&) noexcept { return XErrc::unspecified; }

// Called for a failure that cannot be returned in a build without
// exceptions; it must not return (the library aborts if it does). Engines
// install their crash reporter here. Unused when exceptions are on.
using XFatalHandler = void (*)(XErrc code, const char* message);

namespace detail {
inline std::atomic<XFatalHandler>& fatal_handler_slot() noexcept {
    static std::atomic<XFatalHandler> handler{nullptr};
    return handler;
}
} // namespace detail

inline XFatalHandler set_fatal_handler(XFatalHandler handler) noexcept {
    return detail::fatal_handler_slot().exchange(handler);
}

namespace detail {
[[noreturn]] inline void fatal(XErrc code, const char* message) noexcept {
    if (auto* handler = fatal_handler_slot().load()) handler(code, message);
    std::abort();
}
template <class E>
[[noreturn]] void fatal_from(const E& e) noexcept {
    fatal(error_code_of(e), e.what());
}
} // namespace detail

namespace detail {
// Fail-soft region (proxies and pin scopes in builds
// without exceptions). While one is open on this thread, an arena
// allocation that fails records its code here and returns null instead of
// calling the fatal handler; the container operation that asked for it then
// returns without effect.
struct SoftFailState {
    int depth = 0;
    XErrc code = XErrc::ok;
};
inline SoftFailState& soft_fail_state() noexcept {
    static thread_local SoftFailState state;
    return state;
}
// True (and `code` recorded) inside a fail-soft region.
inline bool soft_fail(XErrc code) noexcept {
    auto& state = soft_fail_state();
    if (state.depth == 0) return false;
    if (state.code == XErrc::ok) state.code = code;
    return true;
}
inline bool soft_fail_active() noexcept { return soft_fail_state().depth != 0; }
inline bool soft_fail_failed() noexcept {
    const auto& state = soft_fail_state();
    return state.depth != 0 && state.code != XErrc::ok;
}
class SoftFailScope {
public:
    SoftFailScope() noexcept : saved_(soft_fail_state().code) {
        auto& state = soft_fail_state();
        ++state.depth;
        state.code = XErrc::ok;
    }
    // The region's failure is its owner's to handle (failed(), code());
    // the enclosing region keeps its own.
    ~SoftFailScope() {
        auto& state = soft_fail_state();
        --state.depth;
        state.code = saved_;
    }
    SoftFailScope(const SoftFailScope&) = delete;
    SoftFailScope& operator=(const SoftFailScope&) = delete;
    bool failed() const noexcept { return soft_fail_state().code != XErrc::ok; }
    XErrc code() const noexcept { return soft_fail_state().code; }

private:
    XErrc saved_;
};
} // namespace detail

// expected-style result of a try_* entry: a value or an XError.
// Discriminated union storage avoids constructing/destroying XError::message
// (std::string) on the hot success path (notably XReceiver::view on MSVC).
// Value constructors take references (T&& / const T&) so heavy non-trivial
// values like XBuffer incur only a single move into the union, and forced
// destructor inlining is restricted to trivially destructible T (e.g. XView).
namespace detail {
struct xresult_value_tag {};
struct xresult_error_tag {};

template <class T, bool TrivialDtor = std::is_trivially_destructible_v<T>>
struct XResultStorage {
    union {
        T value_;
        XError error_;
    };
    bool has_value_;

    XResultStorage(xresult_value_tag, T&& value) noexcept(std::is_nothrow_move_constructible_v<T>)
        : has_value_(true) {
        ::new (static_cast<void*>(&value_)) T(std::move(value));
    }
    template <class U = T, std::enable_if_t<std::is_same_v<U, T> && std::is_copy_constructible_v<U>, int> = 0>
    XResultStorage(xresult_value_tag, const U& value) noexcept(std::is_nothrow_copy_constructible_v<T>)
        : has_value_(true) {
        ::new (static_cast<void*>(&value_)) T(value);
    }
    XOFFSET_DETAIL_COLD XResultStorage(xresult_error_tag, XError&& error) noexcept : has_value_(false) {
        ::new (static_cast<void*>(&error_)) XError(std::move(error));
    }
    XOFFSET_DETAIL_COLD XResultStorage(xresult_error_tag, const XError& error) : has_value_(false) {
        ::new (static_cast<void*>(&error_)) XError(error);
    }
    XResultStorage(XResultStorage&& other) noexcept(std::is_nothrow_move_constructible_v<T>)
        : has_value_(other.has_value_) {
        if (!has_value_) XOFFSET_DETAIL_UNLIKELY {
            construct_error_move(std::move(other.error_));
        } else {
            ::new (static_cast<void*>(&value_)) T(std::move(other.value_));
        }
    }
    XResultStorage(const XResultStorage&) = delete;
    XResultStorage& operator=(const XResultStorage&) = delete;

    XResultStorage& operator=(XResultStorage&& other) noexcept(std::is_nothrow_move_constructible_v<T>) {
        if (this == &other) return *this;
        destroy();
        has_value_ = other.has_value_;
        if (!has_value_) XOFFSET_DETAIL_UNLIKELY {
            construct_error_move(std::move(other.error_));
        } else {
            ::new (static_cast<void*>(&value_)) T(std::move(other.value_));
        }
        return *this;
    }

    XOFFSET_DETAIL_COLD void construct_error_move(XError&& error) noexcept {
        ::new (static_cast<void*>(&error_)) XError(std::move(error));
    }
    XOFFSET_DETAIL_COLD void destroy_error() noexcept { error_.~XError(); }

    void destroy() noexcept {
        if (!has_value_) XOFFSET_DETAIL_UNLIKELY {
            destroy_error();
        } else {
            value_.~T();
        }
    }

    ~XResultStorage() noexcept { destroy(); }
};

template <class T>
struct XResultStorage<T, true> {
    union {
        T value_;
        XError error_;
    };
    bool has_value_;

    XOFFSET_DETAIL_INLINE XResultStorage(xresult_value_tag, T&& value) noexcept(std::is_nothrow_move_constructible_v<T>)
        : has_value_(true) {
        ::new (static_cast<void*>(&value_)) T(std::move(value));
    }
    template <class U = T, std::enable_if_t<std::is_same_v<U, T> && std::is_copy_constructible_v<U>, int> = 0>
    XOFFSET_DETAIL_INLINE XResultStorage(xresult_value_tag, const U& value) noexcept(std::is_nothrow_copy_constructible_v<T>)
        : has_value_(true) {
        ::new (static_cast<void*>(&value_)) T(value);
    }
    XOFFSET_DETAIL_COLD XResultStorage(xresult_error_tag, XError&& error) noexcept : has_value_(false) {
        ::new (static_cast<void*>(&error_)) XError(std::move(error));
    }
    XOFFSET_DETAIL_COLD XResultStorage(xresult_error_tag, const XError& error) : has_value_(false) {
        ::new (static_cast<void*>(&error_)) XError(error);
    }
    XOFFSET_DETAIL_INLINE XResultStorage(XResultStorage&& other) noexcept(std::is_nothrow_move_constructible_v<T>)
        : has_value_(other.has_value_) {
        if (!has_value_) XOFFSET_DETAIL_UNLIKELY {
            construct_error_move(std::move(other.error_));
        } else {
            ::new (static_cast<void*>(&value_)) T(std::move(other.value_));
        }
    }
    XResultStorage(const XResultStorage&) = delete;
    XResultStorage& operator=(const XResultStorage&) = delete;

    XResultStorage& operator=(XResultStorage&& other) noexcept(std::is_nothrow_move_constructible_v<T>) {
        if (this == &other) return *this;
        destroy();
        has_value_ = other.has_value_;
        if (!has_value_) XOFFSET_DETAIL_UNLIKELY {
            construct_error_move(std::move(other.error_));
        } else {
            ::new (static_cast<void*>(&value_)) T(std::move(other.value_));
        }
        return *this;
    }

    XOFFSET_DETAIL_COLD void construct_error_move(XError&& error) noexcept {
        ::new (static_cast<void*>(&error_)) XError(std::move(error));
    }
    XOFFSET_DETAIL_COLD void destroy_error() noexcept { error_.~XError(); }

    XOFFSET_DETAIL_INLINE void destroy() noexcept {
        if (!has_value_) XOFFSET_DETAIL_UNLIKELY destroy_error();
    }

    XOFFSET_DETAIL_INLINE ~XResultStorage() noexcept { destroy(); }
};

template <class T, bool Copyable = std::is_copy_constructible_v<T>>
struct XResultCopyStorage : XResultStorage<T> {
    using XResultStorage<T>::XResultStorage;
    XResultCopyStorage(const XResultCopyStorage&) = delete;
    XResultCopyStorage& operator=(const XResultCopyStorage&) = delete;
    XResultCopyStorage(XResultCopyStorage&&) = default;
    XResultCopyStorage& operator=(XResultCopyStorage&&) = default;
};

template <class T>
struct XResultCopyStorage<T, true> : XResultStorage<T> {
    using XResultStorage<T>::XResultStorage;
    XResultCopyStorage(const XResultCopyStorage& other)
        : XResultStorage<T>(other.has_value_ ? XResultStorage<T>(xresult_value_tag{}, other.value_)
                                             : XResultStorage<T>(xresult_error_tag{}, other.error_)) {}
    XResultCopyStorage(XResultCopyStorage&&) = default;
    XResultCopyStorage& operator=(const XResultCopyStorage& other) {
        if (this == &other) return *this;
        XResultCopyStorage tmp(other);
        XResultStorage<T>::operator=(std::move(tmp));
        return *this;
    }
    XResultCopyStorage& operator=(XResultCopyStorage&&) = default;
};
} // namespace detail

template <class T>
class XResult : private detail::XResultCopyStorage<T> {
    using Base = detail::XResultCopyStorage<T>;
public:
    XOFFSET_DETAIL_INLINE XResult(T&& value) noexcept(std::is_nothrow_move_constructible_v<T>)
        : Base(detail::xresult_value_tag{}, std::move(value)) {}
    template <class U = T, std::enable_if_t<std::is_same_v<U, T> && std::is_copy_constructible_v<U>, int> = 0>
    XOFFSET_DETAIL_INLINE XResult(const U& value) noexcept(std::is_nothrow_copy_constructible_v<T>)
        : Base(detail::xresult_value_tag{}, value) {}
    XOFFSET_DETAIL_COLD XResult(XError&& error) noexcept
        : Base(detail::xresult_error_tag{}, std::move(error)) {}
    XOFFSET_DETAIL_COLD XResult(const XError& error)
        : Base(detail::xresult_error_tag{}, error) {}

    XOFFSET_DETAIL_INLINE bool has_value() const noexcept { return this->has_value_; }
    XOFFSET_DETAIL_INLINE explicit operator bool() const noexcept { return this->has_value_; }
    T& value() & { require_value(); return this->value_; }
    const T& value() const& { require_value(); return this->value_; }
    T&& value() && { require_value(); return std::move(this->value_); }
    XOFFSET_DETAIL_INLINE T& operator*() & noexcept { return this->value_; }
    XOFFSET_DETAIL_INLINE const T& operator*() const& noexcept { return this->value_; }
    XOFFSET_DETAIL_INLINE T&& operator*() && noexcept { return std::move(this->value_); }
    XOFFSET_DETAIL_INLINE T* operator->() noexcept { return &this->value_; }
    XOFFSET_DETAIL_INLINE const T* operator->() const noexcept { return &this->value_; }
    // Valid only without a value.
    const XError& error() const& noexcept { return this->error_; }
    XError&& error() && noexcept { return std::move(this->error_); }
    XErrc code() const noexcept { return this->has_value_ ? XErrc::ok : this->error_.code; }

private:
    XOFFSET_DETAIL_COLD void fail_require() const {
#if XOFFSET_NO_EXCEPTIONS
        detail::fatal(this->error_.code, this->error_.message.c_str());
#else
        throw XException(this->error_.code, this->error_.message);
#endif
    }
    XOFFSET_DETAIL_INLINE void require_value() const {
        if (!this->has_value_) XOFFSET_DETAIL_UNLIKELY fail_require();
    }
};

template <>
class XResult<void> {
public:
    XOFFSET_DETAIL_INLINE XResult() noexcept : has_error_(false) {}
    XOFFSET_DETAIL_COLD XResult(XError error) noexcept : has_error_(error.code != XErrc::ok) {
        if (has_error_) ::new (static_cast<void*>(&error_)) XError(std::move(error));
    }
    XOFFSET_DETAIL_COLD XResult(const XResult& other) : has_error_(other.has_error_) {
        if (has_error_) ::new (static_cast<void*>(&error_)) XError(other.error_);
    }
    XOFFSET_DETAIL_INLINE XResult(XResult&& other) noexcept : has_error_(other.has_error_) {
        if (has_error_) XOFFSET_DETAIL_UNLIKELY construct_error_move(std::move(other.error_));
    }
    XOFFSET_DETAIL_COLD XResult& operator=(const XResult& other) {
        if (this == &other) return *this;
        XResult tmp(other);
        *this = std::move(tmp);
        return *this;
    }
    XOFFSET_DETAIL_INLINE XResult& operator=(XResult&& other) noexcept {
        if (this == &other) return *this;
        if (has_error_) XOFFSET_DETAIL_UNLIKELY destroy_error();
        has_error_ = other.has_error_;
        if (has_error_) XOFFSET_DETAIL_UNLIKELY construct_error_move(std::move(other.error_));
        return *this;
    }
    XOFFSET_DETAIL_INLINE ~XResult() noexcept {
        if (has_error_) XOFFSET_DETAIL_UNLIKELY destroy_error();
    }

    XOFFSET_DETAIL_INLINE bool has_value() const noexcept { return !has_error_; }
    XOFFSET_DETAIL_INLINE explicit operator bool() const noexcept { return !has_error_; }
    const XError& error() const& noexcept { return has_error_ ? error_ : ok_error(); }
    // Valid only without a value.
    XError&& error() && noexcept { return std::move(error_); }
    XOFFSET_DETAIL_INLINE XErrc code() const noexcept { return has_error_ ? error_.code : XErrc::ok; }

private:
    XOFFSET_DETAIL_COLD void construct_error_move(XError&& error) noexcept {
        ::new (static_cast<void*>(&error_)) XError(std::move(error));
    }
    XOFFSET_DETAIL_COLD void destroy_error() noexcept { error_.~XError(); }
    XOFFSET_DETAIL_COLD static const XError& ok_error() noexcept {
        static const XError empty{};
        return empty;
    }

    union {
        char ok_;
        XError error_;
    };
    bool has_error_;
};
using XStatus = XResult<void>;

namespace detail {
// Receive-path status. With exceptions every failure throws and a function
// returning vstatus returns void (the code is what it was without this
// type). Without them it returns false after recording the failure in
// pending_error(), and XOFFSET_VTRY passes a false up.
#if XOFFSET_NO_EXCEPTIONS
struct [[nodiscard]] vstatus {
    bool ok;
    constexpr vstatus(bool value) noexcept : ok(value) {}
    constexpr explicit operator bool() const noexcept { return ok; }
};
constexpr vstatus vok() noexcept { return true; }
#else
using vstatus = void;
constexpr void vok() noexcept {}
#endif

// The first failure of the current receive call (no-exceptions builds). A
// function-local thread_local: for a namespace-scope inline thread_local of a
// type with a destructor, MinGW GCC emits the TLS init function as a strong
// symbol in every translation unit, so a program with two translation units
// that include this header fails to link.
inline XError& pending_error() noexcept {
    static thread_local XError error;
    return error;
}

[[noreturn]] inline void unreachable_rethrow() noexcept { std::abort(); }

#if defined(__GNUC__) || defined(__clang__)
[[gnu::cold, gnu::noinline]]
#endif
inline bool record_failure(XErrc code, std::string message) {
    pending_error().code = code;
    pending_error().message = std::move(message);
    return false;
}

[[noreturn]] inline void fatal_pending() noexcept {
    fatal(pending_error().code, pending_error().message.c_str());
}

#if !XOFFSET_NO_EXCEPTIONS
// Inside a catch block: the error a try_* entry reports for the exception
// in flight. Exceptions that are not std::exception propagate.
XOFFSET_DETAIL_COLD inline XError current_exception_error() {
    try {
        throw;
    } catch (const XException& e) {
        return {e.code(), e.what()};
    } catch (const std::bad_alloc& e) {
        return {XErrc::host_allocation_failed, e.what()};
    } catch (const std::invalid_argument& e) {
        return {XErrc::invalid_argument, e.what()};
    } catch (const std::length_error& e) {
        return {XErrc::length_error, e.what()};
    } catch (const std::exception& e) {
        return {XErrc::unspecified, e.what()};
    }
}
#endif

// The pending failure, moved out (no-exceptions builds).
XOFFSET_DETAIL_COLD inline XError take_pending_error() noexcept {
    XError error = std::move(pending_error());
    pending_error().code = XErrc::ok;
    pending_error().message.clear();
    return error;
}

// A try_* entry around a receive-path call `step` (returning vstatus): its
// failure comes back as an XError instead of an exception or a fatal error.
template <class Step>
XOFFSET_DETAIL_INLINE XStatus try_v(Step&& step) {
#if XOFFSET_NO_EXCEPTIONS
    if (!step()) XOFFSET_DETAIL_UNLIKELY return take_pending_error();
    return {};
#else
    try {
        step();
        return {};
    } catch (...) {
        return current_exception_error();
    }
#endif
}

// try_v for a call that fills `out`: `out` itself on success.
template <class T, class Step>
XOFFSET_DETAIL_INLINE XResult<T> try_v_into(T& out, Step&& step) {
    XStatus status = try_v(step);
    if (!status) XOFFSET_DETAIL_UNLIKELY return std::move(status).error();
    return XResult<T>(std::move(out));
}

// Runs f in a fail-soft region (a pin scope): the code of a failed arena
// allocation, or ok. With exceptions, an XException or std::bad_alloc that f
// lets out is caught and turned into its code; others propagate.
template <class F>
XOFFSET_DETAIL_INLINE XErrc run_fail_soft(F&& f) {
#if XOFFSET_NO_EXCEPTIONS
    SoftFailScope soft;
    f();
    return soft.code();
#else
    try {
        f();
        return XErrc::ok;
    } catch (const XException& e) {
        return e.code() == XErrc::unspecified ? XErrc::out_of_storage : e.code();
    } catch (const std::bad_alloc&) {
        return XErrc::host_allocation_failed;
    }
#endif
}
} // namespace detail


#if XOFFSET_NO_EXCEPTIONS
// A failure outside the receive path: there is nobody to return it to.
#  define XOFFSET_THROW(...) ::XOffsetDatastructure::detail::fatal_from(__VA_ARGS__)
#  define XOFFSET_TRY if (true)
#  define XOFFSET_CATCH(...) else if (false)
#  define XOFFSET_RETHROW ::XOffsetDatastructure::detail::unreachable_rethrow()
// Receive path: record and return false.
#  define XOFFSET_VFAIL(Exception, code, ...) \
    return ::XOffsetDatastructure::detail::record_failure((code), (__VA_ARGS__))
#  define XOFFSET_VFAIL_STD(Exception, code, ...) \
    return ::XOffsetDatastructure::detail::record_failure((code), (__VA_ARGS__))
#  define XOFFSET_VFAIL_BAD_ALLOC() \
    return ::XOffsetDatastructure::detail::record_failure( \
        ::XOffsetDatastructure::XErrc::host_allocation_failed, std::bad_alloc().what())
#  define XOFFSET_VTRY(...) \
    do { if (!(__VA_ARGS__)) XOFFSET_DETAIL_UNLIKELY return false; } while (0)
// XOFFSET_VTRY in a function that returns `failed` instead of a vstatus.
#  define XOFFSET_VTRY_OR(failed, ...) \
    do { if (!(__VA_ARGS__)) XOFFSET_DETAIL_UNLIKELY return failed; } while (0)
// A receive-path call from code that cannot return the failure.
#  define XOFFSET_VREQUIRE(...) \
    do { if (!(__VA_ARGS__)) ::XOffsetDatastructure::detail::fatal_pending(); } while (0)
#  define XOFFSET_VNORETURN
// A receive-path block whose failure runs `undo` before it propagates. With
// exceptions it is try { ... } catch (...) { undo; throw; }; without them
// the block is a lambda whose false runs undo and returns false. The block
// must not return from the enclosing function.
#  define XOFFSET_ROLLBACK_BEGIN \
    if (!([&]() -> ::XOffsetDatastructure::detail::vstatus {
#  define XOFFSET_ROLLBACK_END(...) \
        return ::XOffsetDatastructure::detail::vok(); }())) { __VA_ARGS__; return false; }
#else
#  define XOFFSET_THROW(...) throw __VA_ARGS__
#  define XOFFSET_TRY try
#  define XOFFSET_CATCH(...) catch (__VA_ARGS__)
#  define XOFFSET_RETHROW throw
// Exception is an XOffset exception type taking (code, message).
#  define XOFFSET_VFAIL(Exception, code, ...) throw Exception((code), (__VA_ARGS__))
// A standard exception type taking only the message.
#  define XOFFSET_VFAIL_STD(Exception, code, ...) throw Exception(__VA_ARGS__)
#  define XOFFSET_VFAIL_BAD_ALLOC() throw std::bad_alloc()
#  define XOFFSET_VTRY(...) (__VA_ARGS__)
#  define XOFFSET_VTRY_OR(failed, ...) (__VA_ARGS__)
#  define XOFFSET_VREQUIRE(...) (__VA_ARGS__)
#  define XOFFSET_VNORETURN [[noreturn]]
#  define XOFFSET_ROLLBACK_BEGIN try {
#  define XOFFSET_ROLLBACK_END(...) } catch (...) { __VA_ARGS__; throw; }
#endif

namespace detail {
// The cold throw of a request past maximum_capacity or the payload limit: a
// container, string or block that cannot exist in any payload, or a payload
// that would have to grow past its maximum. XBadAlloc with
// capacity_exceeded, as the proxies and the fail-soft preflight report it;
// out_of_storage stays for storage the policy or the OS cannot provide.
[[noreturn]] XOFFSET_DETAIL_COLD inline void throw_capacity_exceeded() {
    XOFFSET_THROW(XBadAlloc(XErrc::capacity_exceeded, capacity_failure_message));
}
} // namespace detail

} // namespace XOffsetDatastructure
