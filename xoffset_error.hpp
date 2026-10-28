#pragma once
// Error codes, the exception switch and the non-throwing result types shared
// by every XOffset header. C++17, no other XOffset header.
//
// Exceptions on (the default when the compiler has them): every failure
// throws, exactly as before; the try_* receive entries catch and return the
// same code and message. Exceptions off (-fno-exceptions, MSVC /EHs-c-, or
// -DXOFFSET_NO_EXCEPTIONS=1): the receive path returns its first failure up
// to the try_* entry, and any other failure calls the fatal handler.
#include <atomic>
#include <cstdint>
#include <cstdlib>
#include <new>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>

#if !defined(XOFFSET_NO_EXCEPTIONS)
#  if defined(__cpp_exceptions) || defined(__EXCEPTIONS) || defined(_CPPUNWIND)
#    define XOFFSET_NO_EXCEPTIONS 0
#  else
#    define XOFFSET_NO_EXCEPTIONS 1
#  endif
#endif
#if !XOFFSET_NO_EXCEPTIONS && !(defined(__cpp_exceptions) || defined(__EXCEPTIONS) || defined(_CPPUNWIND))
#  error "XOFFSET_NO_EXCEPTIONS=0 requires a build with exceptions enabled"
#endif

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
    // Payload verification.
    invalid_payload = 32,          // the structural graph check rejected the payload
    root_mismatch = 33,            // no root at the declared offset, or a non-normalized extent
    invalid_payload_header = 34,   // the 48-byte payload header is malformed
    // Limits and resources.
    capacity_exceeded = 64,        // more than the maximum capacity (or the format limit)
    out_of_storage = 65,           // the arena or its OS storage could not provide memory
    host_allocation_failed = 66,   // the host allocator failed (std::bad_alloc)
    // Usage.
    invalid_argument = 96,
    invalid_state = 97,            // not valid in the object's current state
    length_error = 98,
    no_root = 99,                  // typed access to a buffer without a verified root
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
        case XErrc::invalid_payload: return "invalid_payload";
        case XErrc::root_mismatch: return "root_mismatch";
        case XErrc::invalid_payload_header: return "invalid_payload_header";
        case XErrc::capacity_exceeded: return "capacity_exceeded";
        case XErrc::out_of_storage: return "out_of_storage";
        case XErrc::host_allocation_failed: return "host_allocation_failed";
        case XErrc::invalid_argument: return "invalid_argument";
        case XErrc::invalid_state: return "invalid_state";
        case XErrc::length_error: return "length_error";
        case XErrc::no_root: return "no_root";
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

// expected-style result of a try_* entry: a value or an XError.
template <class T>
class XResult {
public:
    XResult(T value) : value_(std::move(value)) {}
    XResult(XError error) : error_(std::move(error)) {}

    bool has_value() const noexcept { return value_.has_value(); }
    explicit operator bool() const noexcept { return has_value(); }
    T& value() & { require(); return *value_; }
    const T& value() const& { require(); return *value_; }
    T&& value() && { require(); return std::move(*value_); }
    T& operator*() & noexcept { return *value_; }
    const T& operator*() const& noexcept { return *value_; }
    T&& operator*() && noexcept { return std::move(*value_); }
    T* operator->() noexcept { return &*value_; }
    const T* operator->() const noexcept { return &*value_; }
    // Valid only without a value.
    const XError& error() const& noexcept { return error_; }
    XError&& error() && noexcept { return std::move(error_); }
    XErrc code() const noexcept { return has_value() ? XErrc::ok : error_.code; }

private:
    void require() const {
        if (value_) return;
#if XOFFSET_NO_EXCEPTIONS
        detail::fatal(error_.code, error_.message.c_str());
#else
        throw XException(error_.code, error_.message);
#endif
    }
    std::optional<T> value_;
    XError error_;
};

template <>
class XResult<void> {
public:
    XResult() = default;
    XResult(XError error) : error_(std::move(error)) {}

    bool has_value() const noexcept { return error_.code == XErrc::ok; }
    explicit operator bool() const noexcept { return has_value(); }
    const XError& error() const& noexcept { return error_; }
    XError&& error() && noexcept { return std::move(error_); }
    XErrc code() const noexcept { return error_.code; }

private:
    XError error_;
};
using XStatus = XResult<void>;

namespace detail {
// Receive-path status. With exceptions every failure throws and a function
// returning vstatus returns void (the code is what it was without this
// type). Without them it returns false after recording the failure in
// pending_error, and XOFFSET_VTRY passes a false up.
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

// The first failure of the current receive call (no-exceptions builds).
inline thread_local XError pending_error;

[[noreturn]] inline void unreachable_rethrow() noexcept { std::abort(); }

#if defined(__GNUC__) || defined(__clang__)
[[gnu::cold, gnu::noinline]]
#endif
inline bool record_failure(XErrc code, std::string message) {
    pending_error.code = code;
    pending_error.message = std::move(message);
    return false;
}

[[noreturn]] inline void fatal_pending() noexcept {
    fatal(pending_error.code, pending_error.message.c_str());
}

#if !XOFFSET_NO_EXCEPTIONS
// Inside a catch block: the error a try_* entry reports for the exception
// in flight. Exceptions that are not std::exception propagate.
inline XError current_exception_error() {
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
inline XError take_pending_error() noexcept {
    XError error = std::move(pending_error);
    pending_error.code = XErrc::ok;
    pending_error.message.clear();
    return error;
}
} // namespace detail

#if defined(__cplusplus) && __cplusplus >= 202002L
#  define XOFFSET_DETAIL_UNLIKELY [[unlikely]]
#else
#  define XOFFSET_DETAIL_UNLIKELY
#endif

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

} // namespace XOffsetDatastructure
