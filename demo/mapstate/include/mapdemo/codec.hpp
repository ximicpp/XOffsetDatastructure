// Map state demo: ViewSnapshot payload encodings and the ViewReq /
// ViewSnapshot frame flags that carry them (INTERFACES.md section 3).
// Bodies in src/codec.cpp.
//
// The client names an encoding in its ViewReq flags; A writes the snapshot
// that way and echoes the flags. Every encoding ends in the same check on
// the client: the XOffset wire goes through load_verified / reload_verified
// in full. zstd only carries the bytes; it never replaces validation.
// Nothing here throws (result.hpp).
#pragma once
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include "mapdemo/net.hpp"
#include "mapdemo/result.hpp"

namespace mapdemo::codec {

enum class Encoding : std::uint8_t {
    Dense = 0,     // save_verified: the arena as it is, free blocks included
    Compact = 1,   // save_compact, slack_div 0: live bytes only, capacity == size
    Zstd = 2,      // the Compact wire as one zstd frame, level 3
    // 3 is reserved for lz4 (M2)
};
inline constexpr int kEncodingCount = 3;

// Frame flags: bits 0-7 ViewCategory, bits 8-11 Encoding, bits 12-15 zero.
constexpr std::uint16_t view_flags(net::ViewCategory c, Encoding e) noexcept {
    return static_cast<std::uint16_t>(static_cast<std::uint16_t>(c) | (static_cast<std::uint16_t>(e) << 8));
}
struct ViewFlags {
    net::ViewCategory category;
    Encoding encoding;
};
// nullopt for an unknown category or encoding, or a reserved bit set.
std::optional<ViewFlags> parse_view_flags(std::uint16_t flags) noexcept;

const char* encoding_name(Encoding e) noexcept;   // "dense", "compact", "zstd"
std::optional<Encoding> parse_encoding(std::string_view text) noexcept;

// True when built with zstd (MAPDEMO_WITH_ZSTD).
bool zstd_available() noexcept;

// One zstd frame of `wire` into `out` (previous content replaced, capacity
// kept). The frame header records the content size. One context, reused.
class Compressor {
public:
    Compressor();
    ~Compressor();
    Compressor(const Compressor&) = delete;
    Compressor& operator=(const Compressor&) = delete;
    // An error when built without zstd or on a zstd error.
    Status compress(std::string_view wire, std::string& out);

private:
    void* ctx_ = nullptr;
};

// Receiver side of Encoding::Zstd. Before anything is allocated: the payload
// is exactly one zstd frame (zstd magic, no skippable frame, no trailing
// bytes) and declares a content size of at most `cap`. Afterwards the
// produced length equals that size. `out` then holds the XOffset wire, still
// unverified. An error otherwise (also when built without zstd).
class Decompressor {
public:
    Decompressor();
    ~Decompressor();
    Decompressor(const Decompressor&) = delete;
    Decompressor& operator=(const Decompressor&) = delete;
    Status decompress(std::string_view frame, std::size_t cap, std::string& out);

private:
    void* ctx_ = nullptr;
};

// Malformed-input builders for mapdemo_client --self-check.
// A zstd frame of `bytes` whose header leaves out the content size.
Result<std::string> zstd_frame_without_size(std::string_view bytes);
// A skippable frame (magic 0x184D2A50) carrying `bytes`; needs no zstd.
std::string skippable_frame(std::string_view bytes);

} // namespace mapdemo::codec
