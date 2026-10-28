// Map state demo: snapshot encodings (codec.hpp). OWNER: common-library thread.
#include "mapdemo/codec.hpp"

#include <cstring>

#if defined(MAPDEMO_WITH_ZSTD)
#include <zstd.h>
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

} // namespace mapdemo::codec
