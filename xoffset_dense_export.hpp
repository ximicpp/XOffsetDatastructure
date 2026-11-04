#pragma once
#include "xoffsetdatastructure.hpp"
#include <limits>

namespace XOffsetDatastructure {
namespace image { template<class T> class ImageBuilder; }
namespace dense_export_detail {
template<class T> auto segments(const VerifiedSnapshot<T>& snapshot) {
    if (!snapshot) throw XException("dense export: empty snapshot");
    return snapshot.segments();
}
inline std::size_t size(const std::array<std::span<const std::byte>, 3>& parts) {
    std::size_t n = 0;
    for (auto part : parts) {
        if (part.size() > (std::numeric_limits<std::size_t>::max)() - n)
            throw XException("dense export: size overflow");
        n += part.size();
    }
    return n;
}
inline void check_destination(std::span<std::byte> destination, std::size_t n,
                              std::span<const std::byte> source) {
    if (destination.size() < n) throw XException("dense export: insufficient destination capacity");
    if (detail::verified_paths_intersect(destination.data(), n, source.data(), source.size()))
        throw XException("dense export: destination aliases immutable source");
}
}

template<class T>
std::size_t dense_wire_size(const VerifiedSnapshot<T>& snapshot) {
    return dense_export_detail::size(dense_export_detail::segments(snapshot));
}

// Caller retains snapshot and excludes mutable aliases throughout this call.
// All checks precede writing. Exactly one payload copy, plus header/hole copies.
template<class T>
void emit_dense_into(const VerifiedSnapshot<T>& snapshot, std::span<std::byte> destination) {
    const auto parts = dense_export_detail::segments(snapshot);
    const auto n = dense_export_detail::size(parts);
    // Protect the unsent arena header too: a minimal wire could otherwise fit
    // exactly in that header without intersecting any of the three wire spans.
    dense_export_detail::check_destination(destination, n,
        {parts[1].data() - XWIRE_PAYLOAD_BEGIN, parts[1].size() + XWIRE_PAYLOAD_BEGIN});
    for (auto part : parts) dense_export_detail::check_destination(destination, n, part);
    auto* out = destination.data();
    for (auto part : parts) {
        if (!part.empty()) std::memcpy(out, part.data(), part.size());
        out += part.size();
    }
}

template<class T> class PackedWire;
template<class T> PackedWire<T> pack_dense(const VerifiedSnapshot<T>&);

// Immutable, process-local provenance: no constructor from untrusted bytes.
template<class T>
class PackedWire {
    struct State { std::string wire; std::uint32_t root_offset{}; };
    std::shared_ptr<const State> state_;
    explicit PackedWire(std::shared_ptr<const State> state) noexcept : state_(std::move(state)) {}
    friend PackedWire<T> pack_dense<T>(const VerifiedSnapshot<T>&);
    friend class image::ImageBuilder<T>;
public:
    PackedWire() noexcept = default;
    explicit operator bool() const noexcept { return bool(state_); }
    std::span<const std::byte> bytes() const noexcept {
        if (!state_) return {};
        return {reinterpret_cast<const std::byte*>(state_->wire.data()), state_->wire.size()};
    }
    std::uint32_t root_offset() const {
        if (!state_) throw XException("packed wire: empty source");
        return state_->root_offset;
    }
};

template<class T>
PackedWire<T> pack_dense(const VerifiedSnapshot<T>& snapshot) {
    // Pin before allocation/new_handler, which may replace the caller's handle.
    const auto pin = snapshot;
    const auto n = dense_wire_size(pin);
    auto state = std::make_shared<typename PackedWire<T>::State>();
    XWireHeaderV2 header;
    std::memcpy(&header, pin.segments()[0].data(), sizeof(header));
    state->root_offset = header.root_offset;
    state->wire.resize_and_overwrite(n, [&](char* out, std::size_t) noexcept {
        auto* at = out;
        for (auto part : pin.segments()) {
            if (!part.empty()) std::memcpy(at, part.data(), part.size());
            at += part.size();
        }
        return n;
    });
    return PackedWire<T>(std::move(state));
}

template<class T>
void copy_packed_into(const PackedWire<T>& packed, std::span<std::byte> destination) {
    if (!packed) throw XException("packed wire: empty source");
    const auto bytes = packed.bytes();
    dense_export_detail::check_destination(destination, bytes.size(), bytes);
    std::memmove(destination.data(), bytes.data(), bytes.size());
}
}
