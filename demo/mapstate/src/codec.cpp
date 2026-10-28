// Map state demo: snapshot encodings (codec.hpp). OWNER: common-library thread.
#include "mapdemo/codec.hpp"

#include <cstdio>
#include <cstring>

#include "mapdemo/schema.hpp"

#if defined(MAPDEMO_WITH_ZSTD)
#include <zstd.h>
#include "xoffset_zstd_dict.hpp"
#endif

namespace mapdemo::codec {

std::optional<ViewFlags> parse_view_flags(std::uint16_t flags) noexcept {
    const unsigned category = flags & 0xffu;
    const unsigned encoding = (flags >> 8) & 0xfu;
    if ((flags >> 12) != 0) return std::nullopt;
    if (category > static_cast<unsigned>(net::ViewCategory::Empty)) return std::nullopt;
    if (encoding >= unsigned(kEncodingCount)) return std::nullopt;
    return ViewFlags{static_cast<net::ViewCategory>(category), static_cast<Encoding>(encoding)};
}

const char* encoding_name(Encoding e) noexcept {
    switch (e) {
    case Encoding::Dense: return "dense";
    case Encoding::Compact: return "compact";
    case Encoding::Zstd: return "zstd";
    }
    return "?";
}

std::optional<Encoding> parse_encoding(std::string_view text) noexcept {
    if (text == "dense") return Encoding::Dense;
    if (text == "compact") return Encoding::Compact;
    if (text == "zstd") return Encoding::Zstd;
    return std::nullopt;
}

#if defined(MAPDEMO_WITH_ZSTD)

namespace {
constexpr int kLevel = 3;
// Largest window a valid snapshot frame needs: the 16 MiB snapshot cap.
constexpr int kWindowLogMax = 24;
}   // namespace

bool zstd_available() noexcept { return true; }

// A context that could not be created (out of memory) fails its first use.
Compressor::Compressor() : ctx_(ZSTD_createCCtx()) {}

Compressor::~Compressor() { ZSTD_freeCCtx(static_cast<ZSTD_CCtx*>(ctx_)); }

Status Compressor::compress(std::string_view wire, std::string& out) {
    if (ctx_ == nullptr) return Error{"ZSTD_createCCtx failed"};
    out.resize(ZSTD_compressBound(wire.size()));
    const std::size_t n = ZSTD_compressCCtx(static_cast<ZSTD_CCtx*>(ctx_), out.data(), out.size(), wire.data(),
                                            wire.size(), kLevel);
    if (ZSTD_isError(n)) return Error{std::string("zstd compress: ") + ZSTD_getErrorName(n)};
    out.resize(n);
    return {};
}

Decompressor::Decompressor() : ctx_(ZSTD_createDCtx()) {
    if (ctx_ != nullptr) ZSTD_DCtx_setParameter(static_cast<ZSTD_DCtx*>(ctx_), ZSTD_d_windowLogMax, kWindowLogMax);
}

Decompressor::~Decompressor() { ZSTD_freeDCtx(static_cast<ZSTD_DCtx*>(ctx_)); }

Status Decompressor::decompress(std::string_view frame, std::size_t cap, std::string& out) {
    if (ctx_ == nullptr) return Error{"ZSTD_createDCtx failed"};
    // 1. A zstd frame (not a skippable one), and only that frame.
    std::uint32_t magic = 0;
    if (frame.size() < sizeof magic) return Error{"zstd: payload of " + std::to_string(frame.size()) + " bytes"};
    std::memcpy(&magic, frame.data(), sizeof magic);
    if (magic != ZSTD_MAGICNUMBER) return Error{"zstd: not a zstd frame"};
    const std::size_t frame_bytes = ZSTD_findFrameCompressedSize(frame.data(), frame.size());
    if (ZSTD_isError(frame_bytes)) return Error{std::string("zstd: bad frame: ") + ZSTD_getErrorName(frame_bytes)};
    if (frame_bytes != frame.size())
        return Error{"zstd: " + std::to_string(frame.size() - frame_bytes) + " bytes after the frame"};
    // 2. Declared content size, known and within the cap, before allocating.
    const unsigned long long declared = ZSTD_getFrameContentSize(frame.data(), frame.size());
    if (declared == ZSTD_CONTENTSIZE_ERROR) return Error{"zstd: bad frame header"};
    if (declared == ZSTD_CONTENTSIZE_UNKNOWN) return Error{"zstd: frame does not declare its content size"};
    if (declared > cap)
        return Error{"zstd: declared content of " + std::to_string(declared) + " bytes exceeds the cap of " +
                     std::to_string(cap)};
    // 3. Exactly the declared bytes.
    out.resize(static_cast<std::size_t>(declared));
    const std::size_t n =
        ZSTD_decompressDCtx(static_cast<ZSTD_DCtx*>(ctx_), out.data(), out.size(), frame.data(), frame.size());
    if (ZSTD_isError(n)) {
        out.clear();
        return Error{std::string("zstd: ") + ZSTD_getErrorName(n)};
    }
    if (n != declared) {
        out.clear();
        return Error{"zstd: produced " + std::to_string(n) + " bytes, declared " + std::to_string(declared)};
    }
    return {};
}

Result<std::string> zstd_frame_without_size(std::string_view bytes) {
    ZSTD_CCtx* c = ZSTD_createCCtx();
    if (c == nullptr) return Error{"ZSTD_createCCtx failed"};
    std::string out(ZSTD_compressBound(bytes.size()), '\0');
    ZSTD_CCtx_setParameter(c, ZSTD_c_compressionLevel, kLevel);
    ZSTD_CCtx_setParameter(c, ZSTD_c_contentSizeFlag, 0);
    // A streamed frame with no pledged size: the header cannot carry one.
    ZSTD_outBuffer o{out.data(), out.size(), 0};
    ZSTD_inBuffer i{bytes.data(), bytes.size(), 0};
    const std::size_t rc = ZSTD_compressStream2(c, &o, &i, ZSTD_e_end);
    ZSTD_freeCCtx(c);
    if (ZSTD_isError(rc) || rc != 0) return Error{"zstd stream compress failed"};
    out.resize(o.pos);
    return out;
}

#else   // built without zstd

namespace {
Error no_zstd() { return Error{"built without zstd (MAPDEMO_WITH_ZSTD=OFF)"}; }
}   // namespace

Result<std::string> zstd_frame_without_size(std::string_view) { return no_zstd(); }

bool zstd_available() noexcept { return false; }

Compressor::Compressor() = default;
Compressor::~Compressor() = default;
Status Compressor::compress(std::string_view, std::string&) { return no_zstd(); }

Decompressor::Decompressor() = default;
Decompressor::~Decompressor() = default;
Status Decompressor::decompress(std::string_view, std::size_t, std::string&) { return no_zstd(); }

#endif

std::string skippable_frame(std::string_view bytes) {
    std::string out(8, '\0');
    const std::uint32_t magic = 0x184D2A50u;
    const auto size = static_cast<std::uint32_t>(bytes.size());
    std::memcpy(out.data(), &magic, 4);
    std::memcpy(out.data() + 4, &size, 4);
    out.append(bytes.data(), bytes.size());
    return out;
}


// ---- event stream --------------------------------------------------------------

namespace {

void put_varint(std::uint64_t v, std::string& out) {
    while (v >= 0x80) {
        out += static_cast<char>((v & 0x7f) | 0x80);
        v >>= 7;
    }
    out += static_cast<char>(v);
}

// Minimal LEB128, at most 32 bits.
bool get_varint(std::string_view in, std::size_t& pos, std::uint32_t& v) noexcept {
    std::uint64_t r = 0;
    for (int i = 0; i < 5; ++i) {
        if (pos >= in.size()) return false;
        const unsigned c = static_cast<unsigned char>(in[pos++]);
        r |= std::uint64_t(c & 0x7f) << (7 * i);
        if ((c & 0x80) == 0) {
            if ((i > 0 && c == 0) || r > 0xffffffffull) return false;
            v = static_cast<std::uint32_t>(r);
            return true;
        }
    }
    return false;
}

// The parts of a Raw / Zstd / Dict payload: count, then (length, bytes).
Status split_parts(std::string_view payload, std::size_t cap_each, std::vector<std::string_view>& parts) {
    parts.clear();
    std::size_t pos = 0;
    std::uint32_t count = 0;
    if (!get_varint(payload, pos, count)) return Error{"events: bad event count"};
    if (count == 0 || count > kMaxEventsPerFrame)
        return Error{"events: " + std::to_string(count) + " events, allowed 1.." + std::to_string(kMaxEventsPerFrame)};
    for (std::uint32_t i = 0; i < count; ++i) {
        std::uint32_t n = 0;
        if (!get_varint(payload, pos, n)) return Error{"events: bad length of event " + std::to_string(i)};
        if (n == 0 || n > cap_each)
            return Error{"events: event " + std::to_string(i) + " of " + std::to_string(n) + " bytes, allowed 1.." +
                         std::to_string(cap_each)};
        if (payload.size() - pos < n) return Error{"events: event " + std::to_string(i) + " runs past the payload"};
        parts.push_back(payload.substr(pos, n));
        pos += n;
    }
    if (pos != payload.size()) return Error{"events: " + std::to_string(payload.size() - pos) + " bytes after the last event"};
    return {};
}

void put_u64(std::string& out, std::uint64_t v) {
    for (int i = 0; i < 8; ++i) out += static_cast<char>(v >> (8 * i));
}
std::uint64_t get_u64(const char* p) {
    std::uint64_t v = 0;
    for (int i = 0; i < 8; ++i) v |= std::uint64_t(static_cast<unsigned char>(p[i])) << (8 * i);
    return v;
}

constexpr std::uint64_t kEventRootTypeId = wire_root_type_id_v<UnitEvent>();
constexpr std::uint64_t kEventSchemaHash = wire_schema_hash_v<UnitEvent>();

}   // namespace

const char* event_encoding_name(EventEncoding e) noexcept {
    switch (e) {
    case EventEncoding::Raw: return "raw";
    case EventEncoding::Zstd: return "zstd";
    case EventEncoding::Dict: return "zdict";
    case EventEncoding::Batch: return "batch";
    }
    return "?";
}

std::optional<std::uint16_t> parse_event_mode(std::string_view text) noexcept {
    for (int e = 0; e < kEventEncodingCount; ++e)
        if (text == event_encoding_name(static_cast<EventEncoding>(e))) return static_cast<std::uint16_t>(e);
    if (text == "cycle") return kEventCycle;
    return std::nullopt;
}

const char* event_mode_name(std::uint16_t mode) noexcept {
    if (mode == kEventCycle) return "cycle";
    if (mode < kEventEncodingCount) return event_encoding_name(static_cast<EventEncoding>(mode));
    return "?";
}

std::optional<EventEncoding> parse_event_flags(std::uint16_t flags) noexcept {
    if (flags >= kEventEncodingCount) return std::nullopt;
    return static_cast<EventEncoding>(flags);
}

bool event_mode_uses_dictionary(std::uint16_t mode) noexcept {
    return mode == kEventCycle || mode == static_cast<std::uint16_t>(EventEncoding::Dict) ||
           mode == static_cast<std::uint16_t>(EventEncoding::Batch);
}

std::string subscribe_payload(std::uint32_t dict_id) {
    std::string p;
    p.reserve(24);
    put_u64(p, kEventRootTypeId);
    put_u64(p, kEventSchemaHash);
    put_u64(p, dict_id);   // u32 id, then u32 0
    return p;
}

Result<std::uint32_t> parse_subscribe(std::string_view payload) {
    if (payload.size() != 24) return Error{"subscribe: payload of " + std::to_string(payload.size()) + " bytes, expected 24"};
    if (get_u64(payload.data()) != kEventRootTypeId || get_u64(payload.data() + 8) != kEventSchemaHash)
        return Error{"subscribe: UnitEvent schema hash mismatch"};
    const std::uint64_t tail = get_u64(payload.data() + 16);
    if ((tail >> 32) != 0) return Error{"subscribe: reserved field is not zero"};
    return static_cast<std::uint32_t>(tail);
}

#if defined(MAPDEMO_WITH_ZSTD)

namespace zd = xoffset_zstd_dict;

namespace detail {
// mapdemo_event_dict.cpp, generated by CMake from dict/unit_event_v1.xozd.
extern const unsigned char kEventDictBytes[];
extern const std::size_t kEventDictSize;
}   // namespace detail

std::string_view builtin_event_dictionary() noexcept {
    return std::string_view(reinterpret_cast<const char*>(detail::kEventDictBytes), detail::kEventDictSize);
}

namespace {
Error zd_error(const zd::Status& s) { return Error{s.message}; }

Result<zd::Dictionary> parse_event_dictionary(std::string_view file) {
    if (file.empty()) return Error{"events: this build has no event dictionary"};
    zd::Dictionary d;
    if (zd::Status s = zd::Dictionary::parse(file, d); !s) return zd_error(s);
    if (zd::Status s = d.check_binding(zd::make_binding(kEventRootTypeId, kEventSchemaHash)); !s)
        return Error{"events: " + s.message + " (not a UnitEvent dictionary)"};
    return d;
}
}   // namespace

Result<std::string> load_event_dictionary(const std::string& path, std::uint32_t* id) {
    std::string file;
    if (path.empty()) {
        file.assign(builtin_event_dictionary());
    } else {
        std::FILE* f = std::fopen(path.c_str(), "rb");
        if (f == nullptr) return Error{"events: cannot read " + path};
        char buf[16384];
        std::size_t n;
        while ((n = std::fread(buf, 1, sizeof buf, f)) > 0) file.append(buf, n);
        const bool bad = std::ferror(f) != 0;
        std::fclose(f);
        if (bad) return Error{"events: cannot read " + path};
    }
    Result<zd::Dictionary> d = parse_event_dictionary(file);
    if (!d) return std::move(d).error();
    if (id != nullptr) *id = d->id();
    return file;
}

struct EventEncoder::Impl {
    Compressor zstd;
    zd::Encoder dict;
    std::string part;
};

EventEncoder::EventEncoder() : impl_(new Impl) {}
EventEncoder::~EventEncoder() = default;

Status EventEncoder::set_dictionary(std::string_view file) {
    Result<zd::Dictionary> d = parse_event_dictionary(file);
    if (!d) return std::move(d).error();
    if (zd::Status s = impl_->dict.set_dictionary(*d, 3); !s) return zd_error(s);
    return {};
}

std::uint32_t EventEncoder::dictionary_id() const noexcept { return impl_->dict.dict_id(); }

Status EventEncoder::encode(EventEncoding e, const std::vector<std::string_view>& wires, std::string& out) {
    out.clear();
    if (wires.empty() || wires.size() > kMaxEventsPerFrame)
        return Error{"events: " + std::to_string(wires.size()) + " events in one frame"};
    if ((e == EventEncoding::Dict || e == EventEncoding::Batch) && impl_->dict.dict_id() == 0)
        return Error{"events: no event dictionary"};
    if (e == EventEncoding::Batch) {
        if (zd::Status s = impl_->dict.encode_batch(wires, out); !s) return zd_error(s);
        return {};
    }
    put_varint(wires.size(), out);
    for (std::string_view w : wires) {
        std::string_view part = w;
        if (e == EventEncoding::Zstd) {
            MAPDEMO_TRY(impl_->zstd.compress(w, impl_->part));
            part = impl_->part;
        } else if (e == EventEncoding::Dict) {
            if (zd::Status s = impl_->dict.encode(w, impl_->part); !s) return zd_error(s);
            part = impl_->part;
        }
        put_varint(part.size(), out);
        out.append(part.data(), part.size());
    }
    return {};
}

struct EventDecoder::Impl {
    Decompressor zstd;
    zd::Decoder dict;
    std::vector<std::string_view> parts;
    std::vector<std::size_t> ends;
    std::string storage, one;
};

EventDecoder::EventDecoder() : impl_(new Impl) {}
EventDecoder::~EventDecoder() = default;

Status EventDecoder::set_dictionary(std::string_view file) {
    Result<zd::Dictionary> d = parse_event_dictionary(file);
    if (!d) return std::move(d).error();
    if (zd::Status s = impl_->dict.set_dictionary(*d); !s) return zd_error(s);
    return {};
}

std::uint32_t EventDecoder::dictionary_id() const noexcept { return impl_->dict.dict_id(); }

Status EventDecoder::decode(EventEncoding e, std::string_view payload, std::vector<std::string_view>& wires) {
    wires.clear();
    Impl& m = *impl_;
    if ((e == EventEncoding::Dict || e == EventEncoding::Batch) && m.dict.dict_id() == 0)
        return Error{"events: no event dictionary"};
    if (e == EventEncoding::Batch) {
        // Batch content: count + (length, elided wire) per event.
        const std::size_t cap_total = kMaxEventsPerFrame * (kCapEvent + 5) + 5;
        if (zd::Status s = m.dict.decode_batch(payload, cap_total, kMaxEventsPerFrame, kCapEvent, m.storage, wires); !s) {
            wires.clear();
            return Error{"events: " + s.message};
        }
        return {};
    }
    if (e == EventEncoding::Raw) return split_parts(payload, kCapEvent, wires);
    // Zstd / Dict: every part decodes to one wire of at most kCapEvent.
    MAPDEMO_TRY(split_parts(payload, kCapEvent + 64, m.parts));
    m.storage.clear();
    m.ends.clear();
    for (std::string_view part : m.parts) {
        if (e == EventEncoding::Zstd) {
            MAPDEMO_TRY(m.zstd.decompress(part, kCapEvent, m.one));
        } else if (zd::Status s = m.dict.decode(part, kCapEvent, m.one); !s) {
            return Error{"events: " + s.message};
        }
        m.storage += m.one;
        m.ends.push_back(m.storage.size());
    }
    std::size_t at = 0;
    for (std::size_t end : m.ends) {
        wires.push_back(std::string_view(m.storage).substr(at, end - at));
        at = end;
    }
    return {};
}

namespace {
Result<std::string> rebuild(std::string_view file, std::uint32_t id, const zd::Binding& binding) {
    zd::Dictionary d;
    if (zd::Status s = zd::Dictionary::parse(file, d); !s) return zd_error(s);
    std::string content(d.content());
    // A zstd dictionary: magic, then its u32 id (little endian).
    for (int i = 0; i < 4; ++i) content[4 + i] = static_cast<char>(id >> (8 * i));
    zd::Dictionary out;
    if (zd::Status s = zd::Dictionary::from_zstd(id, binding, content, out); !s) return zd_error(s);
    return out.file_bytes();
}
}   // namespace

Result<std::string> relabel_dictionary(std::string_view file, std::uint32_t id) {
    return rebuild(file, id, zd::make_binding(kEventRootTypeId, kEventSchemaHash));
}

Result<std::string> rebind_dictionary(std::string_view file, std::uint64_t root_type_id, std::uint64_t schema_hash) {
    zd::Dictionary d;
    if (zd::Status s = zd::Dictionary::parse(file, d); !s) return zd_error(s);
    return rebuild(file, d.id(), zd::make_binding(root_type_id, schema_hash));
}

Result<std::string> unbind_dictionary(std::string_view file) {
    zd::Dictionary d;
    if (zd::Status s = zd::Dictionary::parse(file, d); !s) return zd_error(s);
    return rebuild(file, d.id(), zd::kUnbound);
}

#else   // built without zstd: raw events only

std::string_view builtin_event_dictionary() noexcept { return {}; }
Result<std::string> load_event_dictionary(const std::string&, std::uint32_t*) {
    return Error{"events: built without zstd (MAPDEMO_WITH_ZSTD=OFF)"};
}

struct EventEncoder::Impl {};
EventEncoder::EventEncoder() : impl_(new Impl) {}
EventEncoder::~EventEncoder() = default;
Status EventEncoder::set_dictionary(std::string_view) { return Error{"events: built without zstd (MAPDEMO_WITH_ZSTD=OFF)"}; }
std::uint32_t EventEncoder::dictionary_id() const noexcept { return 0; }
Status EventEncoder::encode(EventEncoding e, const std::vector<std::string_view>& wires, std::string& out) {
    out.clear();
    if (e != EventEncoding::Raw) return Error{"events: built without zstd (MAPDEMO_WITH_ZSTD=OFF)"};
    if (wires.empty() || wires.size() > kMaxEventsPerFrame)
        return Error{"events: " + std::to_string(wires.size()) + " events in one frame"};
    put_varint(wires.size(), out);
    for (std::string_view w : wires) {
        put_varint(w.size(), out);
        out.append(w.data(), w.size());
    }
    return {};
}

struct EventDecoder::Impl {};
EventDecoder::EventDecoder() : impl_(new Impl) {}
EventDecoder::~EventDecoder() = default;
Status EventDecoder::set_dictionary(std::string_view) { return Error{"events: built without zstd (MAPDEMO_WITH_ZSTD=OFF)"}; }
std::uint32_t EventDecoder::dictionary_id() const noexcept { return 0; }
Status EventDecoder::decode(EventEncoding e, std::string_view payload, std::vector<std::string_view>& wires) {
    wires.clear();
    if (e != EventEncoding::Raw) return Error{"events: built without zstd (MAPDEMO_WITH_ZSTD=OFF)"};
    return split_parts(payload, kCapEvent, wires);
}

Result<std::string> relabel_dictionary(std::string_view, std::uint32_t) { return Error{"built without zstd"}; }
Result<std::string> rebind_dictionary(std::string_view, std::uint64_t, std::uint64_t) { return Error{"built without zstd"}; }
Result<std::string> unbind_dictionary(std::string_view) { return Error{"built without zstd"}; }

#endif

} // namespace mapdemo::codec
