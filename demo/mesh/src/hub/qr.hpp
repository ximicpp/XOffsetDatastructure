// Device mesh demo: a QR code of the hub's join link, for the join page.
// OWNER: network thread. Byte mode, error correction level M, versions 1-40,
// the mask with the lowest penalty (ISO/IEC 18004; the construction follows
// Project Nayuki's QR Code generator, MIT licence).
#pragma once
#include <string>
#include <string_view>
#include <vector>

namespace xmesh::qr {

// Modules, row-major, size x size; true = dark. Empty when the text does
// not fit in version 40.
struct Code {
    int size = 0;
    std::vector<bool> modules;
    bool dark(int x, int y) const { return modules[std::size_t(y) * std::size_t(size) + std::size_t(x)]; }
};

Code encode(std::string_view text);

// An SVG image with a 4-module quiet zone, `scale` pixels per module.
std::string to_svg(const Code& code, int scale = 6);

} // namespace xmesh::qr
