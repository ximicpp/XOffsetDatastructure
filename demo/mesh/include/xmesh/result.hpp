// Device mesh demo: failures reported without exceptions (the same shape as
// demo/mapstate/include/mapdemo/result.hpp).
//
// FROZEN INTERFACE (demo/mesh/INTERFACES.md). The device program builds
// with and without exceptions (xo_peer and xo_peer_noexc; -fno-exceptions,
// MSVC /EHs-c-), as engine clients do, so everything a device runs returns
// an Error instead of throwing: types.hpp, hop.hpp, the network calls
// (try_*) and the XOffset receive path (try_load_verified). The hub keeps
// exceptions.
#pragma once
#include <optional>
#include <string>
#include <utility>

#if defined(__cpp_exceptions) || defined(__EXCEPTIONS) || defined(_CPPUNWIND)
#define XMESH_EXCEPTIONS 1
#else
#define XMESH_EXCEPTIONS 0
#endif

namespace xmesh {

// The text a throwing call puts in its exception.
struct Error {
    std::string message;
    bool peer_closed = false;   // net: the peer closed or reset the connection between frames
};

// A value or an Error.
template <class T>
class [[nodiscard]] Result {
public:
    Result(T value) : value_(std::move(value)) {}
    Result(Error error) : error_(std::move(error)) {}

    explicit operator bool() const noexcept { return value_.has_value(); }
    T& operator*() & noexcept { return *value_; }
    T&& operator*() && noexcept { return std::move(*value_); }
    T* operator->() noexcept { return &*value_; }
    // Valid only without a value.
    const Error& error() const& noexcept { return error_; }
    Error&& error() && noexcept { return std::move(error_); }

private:
    std::optional<T> value_;
    Error error_;
};

template <>
class [[nodiscard]] Result<void> {
public:
    Result() = default;
    Result(Error error) : error_(std::move(error)), failed_(true) {}

    explicit operator bool() const noexcept { return !failed_; }
    const Error& error() const& noexcept { return error_; }
    Error&& error() && noexcept { return std::move(error_); }

private:
    Error error_;
    bool failed_ = false;
};
using Status = Result<void>;

// Returns the Error of a failed Status or Result from the enclosing function.
#define XMESH_TRY(...)                                    \
    do {                                                    \
        if (auto xmesh_try_ = (__VA_ARGS__); !xmesh_try_) \
            return std::move(xmesh_try_).error();         \
    } while (0)

} // namespace xmesh
