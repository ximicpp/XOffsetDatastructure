// xoffset_zstd_dict: dictionary file, lean frames and batches (see the header).
#define ZSTD_STATIC_LINKING_ONLY   // magicless format, ZSTD_getFrameHeader_advanced
#include "xoffset_zstd_dict.hpp"

#include <zstd.h>

#include <cstring>

namespace xoffset_zstd_dict {

namespace {

constexpr std::uint32_t kFileMagic = 0x445a4f58u;   // "XOZD"
constexpr std::uint16_t kFileFormat = 1;
// Largest window a lean frame may ask for. Frames carry their content size,
// so zstd decodes them in one shot into the output and never allocates a
// window; this only bounds what a forged header can claim.
constexpr int kWindowLogMax = 24;
// zstd's own dictionary magic (the first 4 bytes of a ZDICT dictionary).
constexpr std::uint32_t kZstdDictMagic = 0xEC30A437u;

Status fail(Errc code, std::string message) { return Status{code, std::move(message)}; }

std::uint32_t load32(const unsigned char* p) noexcept {
    return std::uint32_t(p[0]) | std::uint32_t(p[1]) << 8 | std::uint32_t(p[2]) << 16 | std::uint32_t(p[3]) << 24;
}
std::uint64_t load64(const unsigned char* p) noexcept { return load32(p) | std::uint64_t(load32(p + 4)) << 32; }
void store16(unsigned char* p, std::uint16_t v) noexcept {
    p[0] = static_cast<unsigned char>(v);
    p[1] = static_cast<unsigned char>(v >> 8);
}
void store32(unsigned char* p, std::uint32_t v) noexcept {
    for (int i = 0; i < 4; ++i) p[i] = static_cast<unsigned char>(v >> (8 * i));
}
void store64(unsigned char* p, std::uint64_t v) noexcept {
    for (int i = 0; i < 8; ++i) p[i] = static_cast<unsigned char>(v >> (8 * i));
}

std::uint64_t fnv1a64(std::uint64_t h, const unsigned char* p, std::size_t n) noexcept {
    for (std::size_t i = 0; i < n; ++i) {
        h ^= p[i];
        h *= 0x100000001b3ull;
    }
    return h;
}
constexpr std::uint64_t kFnvBasis = 0xcbf29ce484222325ull;

const unsigned char* bytes(std::string_view s) noexcept { return reinterpret_cast<const unsigned char*>(s.data()); }

std::string zstd_text(const char* what, std::size_t rc) { return std::string(what) + ": " + ZSTD_getErrorName(rc); }

}   // namespace

const char* errc_name(Errc e) noexcept {
    switch (e) {
    case Errc::ok: return "ok";
    case Errc::invalid_argument: return "invalid_argument";
    case Errc::bad_dict_file: return "bad_dict_file";
    case Errc::binding_mismatch: return "binding_mismatch";
    case Errc::dict_id_mismatch: return "dict_id_mismatch";
    case Errc::bad_frame: return "bad_frame";
    case Errc::too_large: return "too_large";
    case Errc::bad_batch: return "bad_batch";
    case Errc::zstd_error: return "zstd_error";
    }
    return "?";
}

// ---- binding ----------------------------------------------------------------

bool wire_binding(std::string_view wire, Binding& out) noexcept {
    const unsigned char* p = bytes(wire);
    if (wire.size() < 40 || p[0] != 'X' || p[1] != 'O' || p[2] != 2 || p[3] != 1) return false;
    std::memcpy(out.data(), p + 24, out.size());
    return true;
}

Binding make_binding(std::uint64_t root_type_id, std::uint64_t schema_hash) noexcept {
    Binding b{};
    store64(b.data(), schema_hash);
    store64(b.data() + 8, root_type_id);
    return b;
}

std::string binding_hex(const Binding& b) {
    static const char* digits = "0123456789abcdef";
    std::string s;
    s.reserve(32);
    for (unsigned char c : b) {
        s += digits[c >> 4];
        s += digits[c & 15];
    }
    return s;
}

// ---- dictionary file --------------------------------------------------------

namespace {
// The checksum covers header bytes 0..31 and the dictionary.
std::uint64_t file_checksum(const unsigned char* header32, std::string_view content) noexcept {
    return fnv1a64(fnv1a64(kFnvBasis, header32, 32), bytes(content), content.size());
}

Status check_zstd_dict(std::uint32_t id, std::string_view content) {
    if (content.size() < 8 || load32(bytes(content)) != kZstdDictMagic)
        return fail(Errc::bad_dict_file, "dictionary: not a zstd dictionary");
    const unsigned own = ZSTD_getDictID_fromDict(content.data(), content.size());
    if (own != id)
        return fail(Errc::bad_dict_file, "dictionary: zstd dictionary id " + std::to_string(own) +
                                             " differs from the file's id " + std::to_string(id));
    return {};
}
}   // namespace

Status Dictionary::from_zstd(std::uint32_t id, const Binding& binding, std::string_view zstd_dict, Dictionary& out) {
    if (id == 0 || id > kMaxDictId)
        return fail(Errc::invalid_argument, "dictionary: id " + std::to_string(id) + " outside 1..2^31-1");
    if (zstd_dict.size() > kMaxDictBytes)
        return fail(Errc::invalid_argument, "dictionary: " + std::to_string(zstd_dict.size()) + " bytes, at most 1 MiB");
    if (Status s = check_zstd_dict(id, zstd_dict); !s) return s;
    out.id_ = id;
    out.binding_ = binding;
    out.content_.assign(zstd_dict.data(), zstd_dict.size());
    return {};
}

Status Dictionary::parse(std::string_view file, Dictionary& out) {
    const unsigned char* p = bytes(file);
    if (file.size() < kFileHeaderBytes)
        return fail(Errc::bad_dict_file, "dictionary file: " + std::to_string(file.size()) + " bytes, shorter than the header");
    if (load32(p) != kFileMagic) return fail(Errc::bad_dict_file, "dictionary file: bad magic");
    const unsigned format = unsigned(p[4]) | unsigned(p[5]) << 8;
    const unsigned header = unsigned(p[6]) | unsigned(p[7]) << 8;
    if (format != kFileFormat) return fail(Errc::bad_dict_file, "dictionary file: format " + std::to_string(format));
    if (header != kFileHeaderBytes)
        return fail(Errc::bad_dict_file, "dictionary file: header of " + std::to_string(header) + " bytes");
    const std::uint32_t id = load32(p + 8);
    const std::uint32_t n = load32(p + 12);
    if (id == 0 || id > kMaxDictId) return fail(Errc::bad_dict_file, "dictionary file: id " + std::to_string(id));
    if (n > kMaxDictBytes || n != file.size() - kFileHeaderBytes)
        return fail(Errc::bad_dict_file, "dictionary file: declares " + std::to_string(n) + " dictionary bytes, has " +
                                             std::to_string(file.size() - kFileHeaderBytes));
    if (load64(p + 40) != 0) return fail(Errc::bad_dict_file, "dictionary file: reserved bytes not zero");
    const std::string_view content = file.substr(kFileHeaderBytes);
    if (load64(p + 32) != file_checksum(p, content)) return fail(Errc::bad_dict_file, "dictionary file: checksum mismatch");
    Binding b{};
    std::memcpy(b.data(), p + 16, b.size());
    return from_zstd(id, b, content, out);
}

std::string Dictionary::file_bytes() const {
    std::string f(kFileHeaderBytes, '\0');
    auto* p = reinterpret_cast<unsigned char*>(f.data());
    store32(p, kFileMagic);
    store16(p + 4, kFileFormat);
    store16(p + 6, std::uint16_t(kFileHeaderBytes));
    store32(p + 8, id_);
    store32(p + 12, std::uint32_t(content_.size()));
    std::memcpy(p + 16, binding_.data(), binding_.size());
    store64(p + 32, file_checksum(p, content_));
    f += content_;
    return f;
}

Status Dictionary::check_binding(const Binding& b) const {
    if (b == binding_) return {};
    return fail(Errc::binding_mismatch, "dictionary " + std::to_string(id_) + " is bound to " + binding_hex(binding_) +
                                            ", the messages are " + binding_hex(b));
}

// ---- frame header -----------------------------------------------------------

Status peek_frame(std::string_view frame, FrameInfo& out) {
    ZSTD_FrameHeader h{};
    const std::size_t rc = ZSTD_getFrameHeader_advanced(&h, frame.data(), frame.size(), ZSTD_f_zstd1_magicless);
    if (ZSTD_isError(rc)) return fail(Errc::bad_frame, zstd_text("frame header", rc));
    if (rc != 0) return fail(Errc::bad_frame, "frame header truncated (" + std::to_string(frame.size()) + " bytes)");
    if (h.frameType != ZSTD_frame) return fail(Errc::bad_frame, "not a data frame");
    if (h.dictID == 0) return fail(Errc::bad_frame, "frame carries no dictionary id");
    if (h.frameContentSize == ZSTD_CONTENTSIZE_UNKNOWN) return fail(Errc::bad_frame, "frame does not declare its content size");
    if (h.checksumFlag != 0) return fail(Errc::bad_frame, "frame has a checksum (not a lean frame)");
    // Walk the block headers: 3 bytes each, then the block's bytes (1 for RLE).
    std::size_t pos = h.headerSize;
    for (;;) {
        if (frame.size() - pos < 3) return fail(Errc::bad_frame, "frame truncated in a block header");
        const unsigned char* b = bytes(frame) + pos;
        const std::uint32_t bh = std::uint32_t(b[0]) | std::uint32_t(b[1]) << 8 | std::uint32_t(b[2]) << 16;
        const bool last = (bh & 1u) != 0;
        const unsigned type = (bh >> 1) & 3u;
        const std::size_t size = bh >> 3;
        if (type == 3) return fail(Errc::bad_frame, "reserved block type");
        const std::size_t body = type == 1 ? 1 : size;
        pos += 3;
        if (frame.size() - pos < body) return fail(Errc::bad_frame, "frame truncated in a block");
        pos += body;
        if (last) break;
    }
    if (pos != frame.size())
        return fail(Errc::bad_frame, std::to_string(frame.size() - pos) + " bytes after the frame");
    out.dict_id = h.dictID;
    out.content_size = h.frameContentSize;
    out.header_bytes = h.headerSize;
    out.frame_bytes = pos;
    return {};
}

// ---- varint and batch layout ------------------------------------------------

void put_varint(std::uint64_t v, std::string& out) {
    while (v >= 0x80) {
        out += static_cast<char>((v & 0x7f) | 0x80);
        v >>= 7;
    }
    out += static_cast<char>(v);
}

bool get_varint32(std::string_view in, std::size_t& pos, std::uint32_t& v) noexcept {
    std::uint64_t r = 0;
    for (int i = 0; i < 5; ++i) {
        if (pos >= in.size()) return false;
        const unsigned c = static_cast<unsigned char>(in[pos++]);
        r |= std::uint64_t(c & 0x7f) << (7 * i);
        if ((c & 0x80) == 0) {
            if (i > 0 && c == 0) return false;   // non-minimal: a zero last byte
            if (r > 0xffffffffull) return false;
            v = static_cast<std::uint32_t>(r);
            return true;
        }
    }
    return false;
}

void pack_batch(const std::vector<std::string_view>& messages, std::string& out) {
    out.clear();
    std::size_t total = 0;
    for (std::string_view m : messages) total += m.size();
    out.reserve(5 * (messages.size() + 1) + total);
    put_varint(messages.size(), out);
    for (std::string_view m : messages) {
        put_varint(m.size(), out);
        out.append(m.data(), m.size());
    }
}

Status unpack_batch(std::string_view content, std::size_t max_count, std::size_t cap_each,
                    std::vector<std::string_view>& messages) {
    messages.clear();
    std::size_t pos = 0;
    std::uint32_t count = 0;
    if (!get_varint32(content, pos, count)) return fail(Errc::bad_batch, "batch: bad message count");
    if (count == 0) return fail(Errc::bad_batch, "batch: no messages");
    if (count > max_count)
        return fail(Errc::bad_batch, "batch: " + std::to_string(count) + " messages, at most " + std::to_string(max_count));
    messages.reserve(count);
    for (std::uint32_t i = 0; i < count; ++i) {
        std::uint32_t n = 0;
        if (!get_varint32(content, pos, n)) {
            messages.clear();
            return fail(Errc::bad_batch, "batch: bad length of message " + std::to_string(i));
        }
        if (n == 0 || n > cap_each) {
            messages.clear();
            return fail(Errc::bad_batch, "batch: message " + std::to_string(i) + " of " + std::to_string(n) +
                                             " bytes, allowed 1.." + std::to_string(cap_each));
        }
        if (content.size() - pos < n) {
            messages.clear();
            return fail(Errc::bad_batch, "batch: message " + std::to_string(i) + " runs past the content");
        }
        messages.push_back(content.substr(pos, n));
        pos += n;
    }
    if (pos != content.size()) {
        messages.clear();
        return fail(Errc::bad_batch, "batch: " + std::to_string(content.size() - pos) + " bytes after the last message");
    }
    return {};
}

// ---- encoder ----------------------------------------------------------------

Encoder::~Encoder() {
    ZSTD_freeCDict(static_cast<ZSTD_CDict*>(cdict_));
    ZSTD_freeCCtx(static_cast<ZSTD_CCtx*>(cctx_));
}

Status Encoder::set_dictionary(const Dictionary& dict, int level) {
    if (dict.empty()) return fail(Errc::invalid_argument, "encoder: empty dictionary");
    if (cctx_ == nullptr) cctx_ = ZSTD_createCCtx();
    if (cctx_ == nullptr) return fail(Errc::zstd_error, "ZSTD_createCCtx failed");
    auto* c = static_cast<ZSTD_CCtx*>(cctx_);
    ZSTD_CCtx_reset(c, ZSTD_reset_session_and_parameters);
    ZSTD_freeCDict(static_cast<ZSTD_CDict*>(cdict_));
    cdict_ = ZSTD_createCDict(dict.content().data(), dict.content().size(), level);
    id_ = 0;
    if (cdict_ == nullptr) return fail(Errc::zstd_error, "ZSTD_createCDict failed");
    std::size_t rc = ZSTD_CCtx_setParameter(c, ZSTD_c_format, ZSTD_f_zstd1_magicless);
    if (!ZSTD_isError(rc)) rc = ZSTD_CCtx_setParameter(c, ZSTD_c_dictIDFlag, 1);
    if (!ZSTD_isError(rc)) rc = ZSTD_CCtx_setParameter(c, ZSTD_c_contentSizeFlag, 1);
    if (!ZSTD_isError(rc)) rc = ZSTD_CCtx_setParameter(c, ZSTD_c_checksumFlag, 0);
    if (!ZSTD_isError(rc)) rc = ZSTD_CCtx_refCDict(c, static_cast<ZSTD_CDict*>(cdict_));
    if (ZSTD_isError(rc)) return fail(Errc::zstd_error, zstd_text("encoder setup", rc));
    id_ = dict.id();
    binding_ = dict.binding();
    bound_ = binding_ != kUnbound;
    return {};
}

namespace {
// The fixed start of every XOffset 2.1 wire header: 'X' 'O' 2 1, flags 0.
constexpr unsigned char kWirePrefix[8] = {'X', 'O', 2, 1, 0, 0, 0, 0};
}   // namespace

Status elide_wire(std::string_view wire, const Binding& binding, std::string& out) {
    out.clear();
    if (wire.size() < 40 || std::memcmp(wire.data(), kWirePrefix, sizeof kWirePrefix) != 0)
        return fail(Errc::binding_mismatch, "the dictionary is bound to a schema; the message is not an XOffset 2.1 wire");
    if (std::memcmp(wire.data() + 24, binding.data(), binding.size()) != 0) {
        Binding b{};
        std::memcpy(b.data(), wire.data() + 24, b.size());
        return fail(Errc::binding_mismatch, "the dictionary is bound to " + binding_hex(binding) + ", the message is " +
                                                binding_hex(b));
    }
    out.assign(wire.data() + 8, 16);
    out.append(wire.data() + 40, wire.size() - 40);
    return {};
}

Status Encoder::elide(std::string_view message, std::string& out) const { return elide_wire(message, binding_, out); }

Status Encoder::compress(std::string_view in, std::string& out) {
    out.resize(ZSTD_compressBound(in.size()));
    const std::size_t n = ZSTD_compress2(static_cast<ZSTD_CCtx*>(cctx_), out.data(), out.size(), in.data(), in.size());
    if (ZSTD_isError(n)) {
        out.clear();
        return fail(Errc::zstd_error, zstd_text("compress", n));
    }
    out.resize(n);
    return {};
}

Status Encoder::encode(std::string_view message, std::string& out) {
    out.clear();
    if (id_ == 0) return fail(Errc::invalid_argument, "encoder: no dictionary");
    if (!bound_) return compress(message, out);
    if (Status s = elide(message, one_); !s) return s;
    return compress(one_, out);
}

Status Encoder::encode_batch(const std::vector<std::string_view>& messages, std::string& out) {
    out.clear();
    if (id_ == 0) return fail(Errc::invalid_argument, "encoder: no dictionary");
    if (messages.empty()) return fail(Errc::invalid_argument, "encoder: empty batch");
    if (!bound_) {
        pack_batch(messages, scratch_);
        return compress(scratch_, out);
    }
    scratch_.clear();
    put_varint(messages.size(), scratch_);
    for (std::string_view m : messages) {
        if (Status s = elide(m, one_); !s) return s;
        put_varint(one_.size(), scratch_);
        scratch_ += one_;
    }
    return compress(scratch_, out);
}

// ---- decoder ----------------------------------------------------------------

Decoder::~Decoder() {
    ZSTD_freeDDict(static_cast<ZSTD_DDict*>(ddict_));
    ZSTD_freeDCtx(static_cast<ZSTD_DCtx*>(dctx_));
}

Status Decoder::set_dictionary(const Dictionary& dict) {
    if (dict.empty()) return fail(Errc::invalid_argument, "decoder: empty dictionary");
    if (dctx_ == nullptr) dctx_ = ZSTD_createDCtx();
    if (dctx_ == nullptr) return fail(Errc::zstd_error, "ZSTD_createDCtx failed");
    auto* d = static_cast<ZSTD_DCtx*>(dctx_);
    ZSTD_DCtx_reset(d, ZSTD_reset_session_and_parameters);
    ZSTD_freeDDict(static_cast<ZSTD_DDict*>(ddict_));
    ddict_ = ZSTD_createDDict(dict.content().data(), dict.content().size());
    id_ = 0;
    if (ddict_ == nullptr) return fail(Errc::zstd_error, "ZSTD_createDDict failed");
    std::size_t rc = ZSTD_DCtx_setParameter(d, ZSTD_d_format, ZSTD_f_zstd1_magicless);
    if (!ZSTD_isError(rc)) rc = ZSTD_DCtx_setParameter(d, ZSTD_d_windowLogMax, kWindowLogMax);
    if (!ZSTD_isError(rc)) rc = ZSTD_DCtx_refDDict(d, static_cast<ZSTD_DDict*>(ddict_));
    if (ZSTD_isError(rc)) return fail(Errc::zstd_error, zstd_text("decoder setup", rc));
    id_ = dict.id();
    binding_ = dict.binding();
    bound_ = binding_ != kUnbound;
    return {};
}

Status Decoder::inflate(std::string_view frame, std::size_t cap, std::string& out) {
    out.clear();
    if (id_ == 0) return fail(Errc::invalid_argument, "decoder: no dictionary");
    // 1. Exactly one lean frame with an id and a size (no dictionary needed).
    FrameInfo info;
    if (Status s = peek_frame(frame, info); !s) return s;
    // 2. Made with this dictionary version.
    if (info.dict_id != id_)
        return fail(Errc::dict_id_mismatch, "frame uses dictionary " + std::to_string(info.dict_id) + ", this end has " +
                                                std::to_string(id_));
    // 3. Declared size within the cap, before allocating.
    if (info.content_size > cap)
        return fail(Errc::too_large, "frame declares " + std::to_string(info.content_size) + " bytes, cap " +
                                         std::to_string(cap));
    // 4. Exactly the declared bytes.
    out.resize(static_cast<std::size_t>(info.content_size));
    const std::size_t n =
        ZSTD_decompressDCtx(static_cast<ZSTD_DCtx*>(dctx_), out.data(), out.size(), frame.data(), frame.size());
    if (ZSTD_isError(n)) {
        out.clear();
        return fail(Errc::zstd_error, zstd_text("decompress", n));
    }
    if (n != info.content_size) {
        out.clear();
        return fail(Errc::bad_frame, "frame produced " + std::to_string(n) + " bytes, declared " +
                                         std::to_string(info.content_size));
    }
    return {};
}

namespace {
// Puts the elided header bytes back: prefix, 16 kept bytes, binding, rest.
void restore(std::string_view elided, const Binding& binding, std::string& out) {
    out.append(reinterpret_cast<const char*>(kWirePrefix), sizeof kWirePrefix);
    out.append(elided.data(), 16);
    out.append(reinterpret_cast<const char*>(binding.data()), binding.size());
    out.append(elided.data() + 16, elided.size() - 16);
}
constexpr std::size_t kKeptHeaderBytes = 16;
}   // namespace

Status Decoder::decode(std::string_view frame, std::size_t cap, std::string& out) {
    if (!bound_) return inflate(frame, cap, out);
    out.clear();
    if (cap < kElidedBytes + kKeptHeaderBytes)
        return fail(Errc::too_large, "cap " + std::to_string(cap) + " is below the smallest XOffset wire");
    if (Status s = inflate(frame, cap - kElidedBytes, scratch_); !s) return s;
    if (scratch_.size() < kKeptHeaderBytes)
        return fail(Errc::bad_frame, "frame holds " + std::to_string(scratch_.size()) + " bytes, less than a wire header");
    out.reserve(scratch_.size() + kElidedBytes);
    restore(scratch_, binding_, out);
    return {};
}

Status Decoder::decode_batch(std::string_view frame, std::size_t cap_total, std::size_t max_count, std::size_t cap_each,
                             std::string& storage, std::vector<std::string_view>& messages) {
    messages.clear();
    if (!bound_) {
        if (Status s = inflate(frame, cap_total, storage); !s) return s;
        return unpack_batch(storage, max_count, cap_each, messages);
    }
    storage.clear();
    if (cap_each < kElidedBytes + kKeptHeaderBytes)
        return fail(Errc::too_large, "cap " + std::to_string(cap_each) + " is below the smallest XOffset wire");
    if (Status s = inflate(frame, cap_total, scratch_); !s) return s;
    std::vector<std::string_view> elided;
    if (Status s = unpack_batch(scratch_, max_count, cap_each - kElidedBytes, elided); !s) return s;
    std::size_t total = 0;
    for (std::string_view e : elided) {
        if (e.size() < kKeptHeaderBytes)
            return fail(Errc::bad_batch, "batch: a message of " + std::to_string(e.size()) +
                                             " bytes, less than a wire header");
        total += e.size() + kElidedBytes;
    }
    storage.reserve(total);
    for (std::string_view e : elided) restore(e, binding_, storage);
    messages.reserve(elided.size());
    std::size_t at = 0;
    for (std::string_view e : elided) {
        messages.push_back(std::string_view(storage).substr(at, e.size() + kElidedBytes));
        at += e.size() + kElidedBytes;
    }
    return {};
}

} // namespace xoffset_zstd_dict
