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
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
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

// ---- event stream (10-28, INTERFACES.md section 4) ---------------------------
//
// A client that sends Subscribe gets one Events frame per event tick (more
// when a tick has over kMaxEventsPerFrame events) holding the UnitEvent
// wires of that tick, in id order. The small-message options of
// contrib/zstd_dict are demonstrated here: one zstd dictionary trained
// offline on A's events (dict/unit_event_v1.xozd, built into every binary),
// lean frames that carry the dictionary id, and batching. Every encoding
// ends in the same check on the client: each wire goes through
// try_load_verified<UnitEvent> in full.
enum class EventEncoding : std::uint8_t {
    Raw = 0,     // varint count, then per event varint length + the wire
    Zstd = 1,    // varint count, then per event varint length + a zstd-3 frame (no dictionary)
    Dict = 2,    // varint count, then per event varint length + a lean dictionary frame
    Batch = 3,   // one lean dictionary frame holding the whole batch
};
inline constexpr int kEventEncodingCount = 4;
// Subscribe flags: an EventEncoding, or kEventCycle (A takes the event
// number, tick / kEventPeriodTicks, modulo 4). Events frames carry the
// encoding actually used.
inline constexpr std::uint16_t kEventCycle = 15;

const char* event_encoding_name(EventEncoding e) noexcept;   // "raw", "zstd", "zdict", "batch"
// The names above, or "cycle" (kEventCycle); nullopt otherwise.
std::optional<std::uint16_t> parse_event_mode(std::string_view text) noexcept;
const char* event_mode_name(std::uint16_t mode) noexcept;    // an encoding's name, "cycle", or "?"
// nullopt for anything but 0..3.
std::optional<EventEncoding> parse_event_flags(std::uint16_t flags) noexcept;
// Dict, Batch and cycle use the dictionary.
bool event_mode_uses_dictionary(std::uint16_t mode) noexcept;

// Subscribe payload, 24 bytes: UnitEvent's wire fingerprint (root type id,
// schema hash; u64 each), the dictionary id the client holds (0: none),
// u32 0. parse_subscribe checks length, fingerprint and the zero field and
// returns the dictionary id.
std::string subscribe_payload(std::uint32_t dict_id);
Result<std::uint32_t> parse_subscribe(std::string_view payload);

// The event dictionary built into this binary: the bytes of a .xozd file,
// empty when the build has none (no zstd, or no dict/unit_event_v1.xozd).
std::string_view builtin_event_dictionary() noexcept;
// Reads a .xozd file (an empty path: the built-in one), checks it and that
// it is bound to UnitEvent; returns its bytes. *id gets its id.
Result<std::string> load_event_dictionary(const std::string& path, std::uint32_t* id);

// Sender side. A dictionary is needed for Dict and Batch only.
class EventEncoder {
public:
    EventEncoder();
    ~EventEncoder();
    EventEncoder(const EventEncoder&) = delete;
    EventEncoder& operator=(const EventEncoder&) = delete;
    Status set_dictionary(std::string_view file);
    std::uint32_t dictionary_id() const noexcept;
    // 1..kMaxEventsPerFrame wires into one Events payload (`out` replaced).
    Status encode(EventEncoding e, const std::vector<std::string_view>& wires, std::string& out);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

// Receiver side. Before anything is allocated: count 1..kMaxEventsPerFrame,
// each frame or wire within kCapEvent, declared sizes within their caps,
// the dictionary id equal to this end's, no trailing bytes. `wires` then
// point into the decoder (valid until the next decode) and are still
// unverified.
class EventDecoder {
public:
    EventDecoder();
    ~EventDecoder();
    EventDecoder(const EventDecoder&) = delete;
    EventDecoder& operator=(const EventDecoder&) = delete;
    Status set_dictionary(std::string_view file);
    std::uint32_t dictionary_id() const noexcept;
    Status decode(EventEncoding e, std::string_view payload, std::vector<std::string_view>& wires);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

// Self-check builders: a copy of the dictionary file `file` with another id
// (the zstd dictionary relabelled too), or bound to another fingerprint.
Result<std::string> relabel_dictionary(std::string_view file, std::uint32_t id);
Result<std::string> rebind_dictionary(std::string_view file, std::uint64_t root_type_id, std::uint64_t schema_hash);
// The same dictionary content, unbound: frames it writes keep every byte
// (the self-check forges batch contents with it).
Result<std::string> unbind_dictionary(std::string_view file);

} // namespace mapdemo::codec
