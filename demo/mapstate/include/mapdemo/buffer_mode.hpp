// Map state demo: the --buffers option shared by A, B and the client
// (INTERFACES.md section 3).
#pragma once
#include <optional>
#include <string_view>

namespace mapdemo {

// Fresh: every received batch, request and snapshot is a new XBuffer
//        (load_verified / create + save_verified), destroyed after use.
// Reuse: each process keeps its buffers and refills them in place
//        (reload_verified / reset + save_verified_into) and hands network
//        payload strings back for the next frame. Same validator, same
//        rejection set, byte-identical wires (BUFFER_REUSE.md).
enum class BufferMode { Fresh, Reuse };

inline constexpr BufferMode kDefaultBufferMode = BufferMode::Reuse;

inline std::optional<BufferMode> parse_buffer_mode(std::string_view text) noexcept {
    if (text == "fresh") return BufferMode::Fresh;
    if (text == "reuse") return BufferMode::Reuse;
    return std::nullopt;
}

inline const char* buffer_mode_name(BufferMode mode) noexcept {
    return mode == BufferMode::Fresh ? "fresh" : "reuse";
}

} // namespace mapdemo
