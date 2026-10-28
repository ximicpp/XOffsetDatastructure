// xoffset_zstd_dict: optional zstd dictionary compression for small messages.
//
// Not part of the XOffset library: the library stays header-only with no
// dependencies, and this component is compiled only by projects that link
// zstd (1.5.x). It works on any bytes; for XOffset wires it binds each
// dictionary to one schema through the wire header fingerprint. README.md
// in this directory explains when to use it; docs/DEPLOYMENT.md has the
// choice between zstd-3, a dictionary, batching and lz4.
//
// Three pieces:
//
// * Dictionary file (.xozd): a 48-byte header, then a zstd dictionary.
//     0  u32 magic 'X' 'O' 'Z' 'D'
//     4  u16 format (1)
//     6  u16 header bytes (48)
//     8  u32 dictionary id: the version both ends compare, 1 .. 2^31-1;
//            equal to the zstd dictionary's own id
//    12  u32 dictionary bytes
//    16  16  binding: bytes 24..39 of the XOffset wire header of every
//            message the dictionary is for (schema hash, root type id), or
//            all zero for a dictionary bound to nothing
//    32  u64 FNV-1a 64 of bytes 0..31 followed by the dictionary bytes
//    40  u64 0
//   All integers little endian. parse() checks every field.
//
// * Frame: one zstd frame in the magicless format, with the dictionary id
//   and the content size in its header and no checksum. Against a standard
//   frame it drops the 4-byte magic, so a 200-byte message pays a 3-byte
//   header (1 byte each for flags, id and size while the id is below 256
//   and the content below 256 bytes). The decoder refuses a frame whose
//   dictionary id is not its own before decoding anything, a frame without
//   an id or a content size, a declared size over the caller's cap (before
//   allocating), and anything but exactly one frame (truncated, trailing
//   bytes). It does not check integrity: the transport does (TCP, TLS), and
//   the receiver verifies every decoded XOffset wire in full anyway.
//
// * Header elision (bound dictionaries only): every message must be an
//   XOffset 2.1 wire of the bound schema, and its 24 bytes that the binding
//   fixes ('X' 'O' 2 1, flags 0, the 16 fingerprint bytes 24..39) are left
//   out of the frame and put back by the decoder. The encoder refuses any
//   other message (binding_mismatch), so the decoder returns exactly the
//   bytes the sender passed in; what it returns is still an unverified
//   wire. Unbound dictionaries carry messages unchanged.
//
// * Batch: several messages in one frame. Content = varint count, then
//   per message varint length and the (elided) message (LEB128, minimal
//   encodings only). pack_batch / unpack_batch give the same layout
//   without compression or elision.
//
// Nothing here throws. Out of memory inside zstd is an Errc::zstd_error;
// out of memory in std::string follows the build's usual rule.
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace xoffset_zstd_dict {

enum class Errc : std::uint8_t {
    ok = 0,
    invalid_argument,      // caller error: bad id, empty sample set, ...
    bad_dict_file,         // .xozd header, size or checksum wrong; not a zstd dictionary
    binding_mismatch,      // the dictionary is bound to another schema
    dict_id_mismatch,      // the frame was made with another dictionary version
    bad_frame,             // not exactly one lean frame with id and size
    too_large,             // declared content over the caller's cap
    bad_batch,             // batch count, lengths or layout out of range
    zstd_error,            // zstd failed: corrupt block, out of memory, training failed
};

struct [[nodiscard]] Status {
    Errc code = Errc::ok;
    std::string message;
    bool ok() const noexcept { return code == Errc::ok; }
    explicit operator bool() const noexcept { return ok(); }
};

const char* errc_name(Errc e) noexcept;   // "ok", "dict_id_mismatch", ...

// Bytes 24..39 of an XOffset wire header (XWireHeaderV2: schema_hash, then
// root_type_id, little endian), or all zero for "bound to nothing".
using Binding = std::array<std::uint8_t, 16>;
inline constexpr Binding kUnbound{};

// The binding of an XOffset 2.1 wire ('X' 'O' 2 1 and at least 40 bytes),
// false otherwise.
bool wire_binding(std::string_view wire, Binding& out) noexcept;
// From the two fingerprint words (XWireFingerprint: root_type_id, schema_hash).
Binding make_binding(std::uint64_t root_type_id, std::uint64_t schema_hash) noexcept;
std::string binding_hex(const Binding& b);   // 32 lowercase hex digits

inline constexpr std::uint32_t kMaxDictId = 0x7fffffffu;
inline constexpr std::size_t kFileHeaderBytes = 48;
inline constexpr std::size_t kMaxDictBytes = std::size_t(1) << 20;

class Dictionary {
public:
    // A .xozd file. Checks magic, format, header size, id range, length,
    // checksum, and that the content is a zstd dictionary with the same id.
    static Status parse(std::string_view file, Dictionary& out);
    // From a zstd dictionary whose own id is `id` (ZDICT output).
    static Status from_zstd(std::uint32_t id, const Binding& binding, std::string_view zstd_dict, Dictionary& out);

    std::uint32_t id() const noexcept { return id_; }
    const Binding& binding() const noexcept { return binding_; }
    std::string_view content() const noexcept { return content_; }
    bool empty() const noexcept { return id_ == 0; }
    std::string file_bytes() const;   // the .xozd file

    // ok when the dictionary is bound to `b`; binding_mismatch otherwise.
    // An unbound dictionary matches nothing but kUnbound.
    Status check_binding(const Binding& b) const;

private:
    std::uint32_t id_ = 0;
    Binding binding_{};
    std::string content_;
};

// Trains a dictionary from samples (zstd fastCover, single-threaded,
// deterministic for the same samples). Needs zstd's dictBuilder
// (lib/dictBuilder); defined in xoffset_zstd_dict_train.cpp so receivers
// need not link it. `level` is the compression level the statistics are
// tuned for.
struct TrainOptions {
    std::uint32_t id = 1;
    Binding binding{};
    std::size_t dict_bytes = 16 * 1024;
    int level = 3;
};
Status train(const std::vector<std::string_view>& samples, const TrainOptions& options, Dictionary& out);

struct FrameInfo {
    std::uint32_t dict_id = 0;
    std::uint64_t content_size = 0;
    std::size_t header_bytes = 0;
    std::size_t frame_bytes = 0;   // header + blocks: equals the input size for a valid frame
};
// Reads a lean frame's header and walks its block headers; no dictionary
// needed. bad_frame for a short header, a missing id or size, a checksum
// flag, a reserved block type, or input that is not exactly one frame.
Status peek_frame(std::string_view frame, FrameInfo& out);

class Encoder {
public:
    Encoder() = default;
    ~Encoder();
    Encoder(const Encoder&) = delete;
    Encoder& operator=(const Encoder&) = delete;

    Status set_dictionary(const Dictionary& dict, int level = 3);
    std::uint32_t dict_id() const noexcept { return id_; }

    // One lean frame of `message` into `out` (replaced; capacity kept). A
    // bound dictionary takes only wires of its schema (header elision).
    Status encode(std::string_view message, std::string& out);
    // One lean frame of the batch content of `messages` (at least one).
    Status encode_batch(const std::vector<std::string_view>& messages, std::string& out);

private:
    Status elide(std::string_view message, std::string& out) const;
    Status compress(std::string_view in, std::string& out);
    void* cctx_ = nullptr;
    void* cdict_ = nullptr;
    std::uint32_t id_ = 0;
    bool bound_ = false;
    Binding binding_{};
    std::string scratch_, one_;
};

class Decoder {
public:
    Decoder() = default;
    ~Decoder();
    Decoder(const Decoder&) = delete;
    Decoder& operator=(const Decoder&) = delete;

    Status set_dictionary(const Dictionary& dict);
    std::uint32_t dict_id() const noexcept { return id_; }

    // `frame` must be exactly one lean frame made with this dictionary for
    // a message of at most `cap` bytes; `out` then holds exactly the
    // message the sender encoded (still unverified).
    Status decode(std::string_view frame, std::size_t cap, std::string& out);
    // A batch frame whose content (as declared in the frame) is at most
    // `cap_total` bytes, holding 1..max_count messages of at most cap_each
    // bytes each. `messages` point into `storage`.
    Status decode_batch(std::string_view frame, std::size_t cap_total, std::size_t max_count, std::size_t cap_each,
                        std::string& storage, std::vector<std::string_view>& messages);

private:
    Status inflate(std::string_view frame, std::size_t cap, std::string& out);
    void* dctx_ = nullptr;
    void* ddict_ = nullptr;
    std::uint32_t id_ = 0;
    bool bound_ = false;
    Binding binding_{};
    std::string scratch_;
};

// Bytes a bound dictionary leaves out of every message.
inline constexpr std::size_t kElidedBytes = 24;
// `wire` without the 24 header bytes `binding` fixes (what a bound
// dictionary compresses, and what it is trained on); binding_mismatch for
// anything but an XOffset 2.1 wire of that schema.
Status elide_wire(std::string_view wire, const Binding& binding, std::string& out);

// Batch content without compression or elision (the layout inside a batch frame).
void pack_batch(const std::vector<std::string_view>& messages, std::string& out);
Status unpack_batch(std::string_view content, std::size_t max_count, std::size_t cap_each,
                    std::vector<std::string_view>& messages);

// LEB128 helpers (exposed for transports that frame messages the same way).
void put_varint(std::uint64_t v, std::string& out);
// Reads a minimal LEB128 value of at most 32 bits at `pos`; false when
// truncated, overlong, non-minimal or above 2^32-1.
bool get_varint32(std::string_view in, std::size_t& pos, std::uint32_t& v) noexcept;

} // namespace xoffset_zstd_dict
