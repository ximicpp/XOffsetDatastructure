// Map state demo: failures reported without exceptions.
//
// The client builds with and without exceptions (mapdemo_client and
// mapdemo_client_noexc; -fno-exceptions, MSVC /EHs-c-), as engine clients
// do, so the client side never throws: the network calls it uses (net.hpp
// try_*), the snapshot codec (codec.hpp) and the XOffset receive path
// (try_load_verified / try_reload_verified) return an Error instead. The
// servers keep exceptions and use the throwing network calls.
#pragma once
#include <optional>
#include <string>
#include <utility>

#if defined(__cpp_exceptions) || defined(__EXCEPTIONS) || defined(_CPPUNWIND)
#define MAPDEMO_EXCEPTIONS 1
#else
#define MAPDEMO_EXCEPTIONS 0
#endif

namespace mapdemo {

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
#define MAPDEMO_TRY(...)                                    \
    do {                                                    \
        if (auto mapdemo_try_ = (__VA_ARGS__); !mapdemo_try_) \
            return std::move(mapdemo_try_).error();         \
    } while (0)

} // namespace mapdemo
