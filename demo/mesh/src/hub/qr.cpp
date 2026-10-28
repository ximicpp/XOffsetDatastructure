// Device mesh demo: QR encoder (qr.hpp). OWNER: network thread.
#include "hub/qr.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdlib>

namespace xmesh::qr {
namespace {

// Level M, index = version (0 unused).
constexpr int kEccPerBlock[41] = {-1, 10, 16, 26, 18, 24, 16, 18, 22, 22, 26, 30, 22, 22, 24,
                                  24, 28, 28, 26, 26, 26, 26, 28, 28, 28, 28, 28, 28, 28, 28,
                                  28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28};
constexpr int kBlocks[41] = {-1, 1, 1, 1, 2, 2, 4, 4, 4, 5, 5, 5, 8, 9, 9, 10, 10, 11, 13, 14, 16,
                             17, 17, 18, 20, 21, 23, 25, 26, 28, 29, 31, 33, 35, 37, 38, 40, 43, 45, 47, 49};
constexpr int kEclFormatBits = 0;   // M

int raw_data_modules(int ver) {
    int result = (16 * ver + 128) * ver + 64;
    if (ver >= 2) {
        const int align = ver / 7 + 2;
        result -= (25 * align - 10) * align - 55;
        if (ver >= 7) result -= 36;
    }
    return result;
}

int data_codewords(int ver) { return raw_data_modules(ver) / 8 - kEccPerBlock[ver] * kBlocks[ver]; }

std::uint8_t gf_mul(std::uint8_t x, std::uint8_t y) {
    int z = 0;
    for (int i = 7; i >= 0; --i) {
        z = (z << 1) ^ ((z >> 7) * 0x11D);
        z ^= ((y >> i) & 1) * x;
    }
    return std::uint8_t(z);
}

std::vector<std::uint8_t> rs_divisor(int degree) {
    std::vector<std::uint8_t> result(std::size_t(degree), 0);
    result.back() = 1;
    std::uint8_t root = 1;
    for (int i = 0; i < degree; ++i) {
        for (std::size_t j = 0; j < result.size(); ++j) {
            result[j] = gf_mul(result[j], root);
            if (j + 1 < result.size()) result[j] ^= result[j + 1];
        }
        root = gf_mul(root, 0x02);
    }
    return result;
}

std::vector<std::uint8_t> rs_remainder(const std::vector<std::uint8_t>& data, const std::vector<std::uint8_t>& div) {
    std::vector<std::uint8_t> result(div.size(), 0);
    for (std::uint8_t b : data) {
        const std::uint8_t factor = b ^ result[0];
        result.erase(result.begin());
        result.push_back(0);
        for (std::size_t i = 0; i < result.size(); ++i) result[i] ^= gf_mul(div[i], factor);
    }
    return result;
}

class Builder {
public:
    explicit Builder(int ver) : ver_(ver), size_(ver * 4 + 17), mod_(std::size_t(size_ * size_)), fn_(mod_.size()) {}

    Code build(const std::vector<std::uint8_t>& data) {
        draw_function_patterns();
        draw_codewords(add_ecc_and_interleave(data));
        int best_mask = 0;
        long best = -1;
        for (int m = 0; m < 8; ++m) {
            apply_mask(m);
            draw_format_bits(m);
            const long p = penalty();
            if (best < 0 || p < best) {
                best = p;
                best_mask = m;
            }
            apply_mask(m);   // undo
        }
        apply_mask(best_mask);
        draw_format_bits(best_mask);
        Code c;
        c.size = size_;
        c.modules.assign(mod_.begin(), mod_.end());
        return c;
    }

private:
    bool get(int x, int y) const { return mod_[std::size_t(y * size_ + x)] != 0; }
    void set_fn(int x, int y, bool dark) {
        mod_[std::size_t(y * size_ + x)] = dark;
        fn_[std::size_t(y * size_ + x)] = 1;
    }

    std::vector<int> alignment_positions() const {
        if (ver_ == 1) return {};
        const int n = ver_ / 7 + 2;
        const int step = (ver_ * 8 + n * 3 + 5) / (n * 4 - 4) * 2;
        std::vector<int> result;
        for (int i = 0, pos = size_ - 7; i < n - 1; ++i, pos -= step) result.insert(result.begin(), pos);
        result.insert(result.begin(), 6);
        return result;
    }

    void draw_finder(int x, int y) {
        for (int dy = -4; dy <= 4; ++dy)
            for (int dx = -4; dx <= 4; ++dx) {
                const int dist = std::max(std::abs(dx), std::abs(dy));
                const int xx = x + dx, yy = y + dy;
                if (xx >= 0 && xx < size_ && yy >= 0 && yy < size_) set_fn(xx, yy, dist != 2 && dist != 4);
            }
    }

    void draw_alignment(int x, int y) {
        for (int dy = -2; dy <= 2; ++dy)
            for (int dx = -2; dx <= 2; ++dx) set_fn(x + dx, y + dy, std::max(std::abs(dx), std::abs(dy)) != 1);
    }

    void draw_function_patterns() {
        for (int i = 0; i < size_; ++i) {
            set_fn(6, i, i % 2 == 0);
            set_fn(i, 6, i % 2 == 0);
        }
        draw_finder(3, 3);
        draw_finder(size_ - 4, 3);
        draw_finder(3, size_ - 4);
        const std::vector<int> pos = alignment_positions();
        const int n = int(pos.size());
        for (int i = 0; i < n; ++i)
            for (int j = 0; j < n; ++j) {
                if ((i == 0 && j == 0) || (i == 0 && j == n - 1) || (i == n - 1 && j == 0)) continue;
                draw_alignment(pos[std::size_t(i)], pos[std::size_t(j)]);
            }
        draw_format_bits(0);
        draw_version();
    }

    void draw_format_bits(int mask) {
        const int data = kEclFormatBits << 3 | mask;
        int rem = data;
        for (int i = 0; i < 10; ++i) rem = (rem << 1) ^ ((rem >> 9) * 0x537);
        const int bits = (data << 10 | rem) ^ 0x5412;
        auto bit = [&](int i) { return ((bits >> i) & 1) != 0; };
        for (int i = 0; i <= 5; ++i) set_fn(8, i, bit(i));
        set_fn(8, 7, bit(6));
        set_fn(8, 8, bit(7));
        set_fn(7, 8, bit(8));
        for (int i = 9; i < 15; ++i) set_fn(14 - i, 8, bit(i));
        for (int i = 0; i < 8; ++i) set_fn(size_ - 1 - i, 8, bit(i));
        for (int i = 8; i < 15; ++i) set_fn(8, size_ - 15 + i, bit(i));
        set_fn(8, size_ - 8, true);
    }

    void draw_version() {
        if (ver_ < 7) return;
        int rem = ver_;
        for (int i = 0; i < 12; ++i) rem = (rem << 1) ^ ((rem >> 11) * 0x1F25);
        const long bits = long(ver_) << 12 | rem;
        for (int i = 0; i < 18; ++i) {
            const bool b = ((bits >> i) & 1) != 0;
            const int a = size_ - 11 + i % 3, c = i / 3;
            set_fn(a, c, b);
            set_fn(c, a, b);
        }
    }

    std::vector<std::uint8_t> add_ecc_and_interleave(const std::vector<std::uint8_t>& data) const {
        const int blocks = kBlocks[ver_];
        const int ecc_len = kEccPerBlock[ver_];
        const int raw = raw_data_modules(ver_) / 8;
        const int short_blocks = blocks - raw % blocks;
        const int short_len = raw / blocks;
        const std::vector<std::uint8_t> div = rs_divisor(ecc_len);
        std::vector<std::vector<std::uint8_t>> all;
        std::size_t k = 0;
        for (int i = 0; i < blocks; ++i) {
            const std::size_t n = std::size_t(short_len - ecc_len + (i < short_blocks ? 0 : 1));
            std::vector<std::uint8_t> dat(data.begin() + std::ptrdiff_t(k), data.begin() + std::ptrdiff_t(k + n));
            k += n;
            const std::vector<std::uint8_t> ecc = rs_remainder(dat, div);
            if (i < short_blocks) dat.push_back(0);
            dat.insert(dat.end(), ecc.begin(), ecc.end());
            all.push_back(std::move(dat));
        }
        std::vector<std::uint8_t> result;
        for (std::size_t i = 0; i < all[0].size(); ++i)
            for (std::size_t j = 0; j < all.size(); ++j)
                if (i != std::size_t(short_len - ecc_len) || int(j) >= short_blocks) result.push_back(all[j][i]);
        return result;
    }

    void draw_codewords(const std::vector<std::uint8_t>& data) {
        std::size_t i = 0;
        for (int right = size_ - 1; right >= 1; right -= 2) {
            if (right == 6) right = 5;
            for (int vert = 0; vert < size_; ++vert)
                for (int j = 0; j < 2; ++j) {
                    const int x = right - j;
                    const bool upward = ((right + 1) & 2) == 0;
                    const int y = upward ? size_ - 1 - vert : vert;
                    const std::size_t at = std::size_t(y * size_ + x);
                    if (!fn_[at] && i < data.size() * 8) {
                        mod_[at] = ((data[i >> 3] >> (7 - (i & 7))) & 1) != 0;
                        ++i;
                    }
                }
        }
    }

    void apply_mask(int mask) {
        for (int y = 0; y < size_; ++y)
            for (int x = 0; x < size_; ++x) {
                bool invert = false;
                switch (mask) {
                    case 0: invert = (x + y) % 2 == 0; break;
                    case 1: invert = y % 2 == 0; break;
                    case 2: invert = x % 3 == 0; break;
                    case 3: invert = (x + y) % 3 == 0; break;
                    case 4: invert = (x / 3 + y / 2) % 2 == 0; break;
                    case 5: invert = x * y % 2 + x * y % 3 == 0; break;
                    case 6: invert = (x * y % 2 + x * y % 3) % 2 == 0; break;
                    case 7: invert = ((x + y) % 2 + x * y % 3) % 2 == 0; break;
                }
                const std::size_t at = std::size_t(y * size_ + x);
                if (invert && !fn_[at]) mod_[at] ^= 1;
            }
    }

    // Rules 1 (runs), 2 (2x2 blocks), 3 (finder-like 1:1:3:1:1 runs with
    // four light modules on one side) and 4 (dark/light balance).
    long penalty() const {
        long result = 0;
        auto line = [&](bool rows) {
            for (int a = 0; a < size_; ++a) {
                int run = 0;
                bool color = false;
                std::vector<bool> l(static_cast<std::size_t>(size_));
                for (int b = 0; b < size_; ++b) l[std::size_t(b)] = rows ? get(b, a) : get(a, b);
                for (int b = 0; b < size_; ++b) {
                    if (b == 0 || l[std::size_t(b)] != color) {
                        if (run >= 5) result += 3 + (run - 5);
                        color = l[std::size_t(b)];
                        run = 1;
                    } else {
                        ++run;
                    }
                }
                if (run >= 5) result += 3 + (run - 5);
                for (int b = 0; b + 7 <= size_; ++b) {
                    auto at = [&](int k) { return k >= 0 && k < size_ && l[std::size_t(k)]; };
                    const bool core = at(b) && !at(b + 1) && at(b + 2) && at(b + 3) && at(b + 4) && !at(b + 5) &&
                                      at(b + 6);
                    if (!core) continue;
                    bool before = true, after = true;
                    for (int k = 1; k <= 4; ++k) {
                        if (at(b - k)) before = false;
                        if (at(b + 6 + k)) after = false;
                    }
                    if (before || after) result += 40;
                }
            }
        };
        line(true);
        line(false);
        for (int y = 0; y + 1 < size_; ++y)
            for (int x = 0; x + 1 < size_; ++x) {
                const bool c = get(x, y);
                if (c == get(x + 1, y) && c == get(x, y + 1) && c == get(x + 1, y + 1)) result += 3;
            }
        long dark = 0;
        for (std::uint8_t m : mod_) dark += m;
        const long total = long(size_) * size_;
        const long k = (std::labs(dark * 20 - total * 10) + total - 1) / total - 1;
        result += std::max(0L, k) * 10;
        return result;
    }

    int ver_;
    int size_;
    std::vector<std::uint8_t> mod_;
    std::vector<std::uint8_t> fn_;
};

} // namespace

Code encode(std::string_view text) {
    for (int ver = 1; ver <= 40; ++ver) {
        const int count_bits = ver <= 9 ? 8 : 16;
        const std::size_t capacity_bits = std::size_t(data_codewords(ver)) * 8;
        const std::size_t need = 4 + std::size_t(count_bits) + text.size() * 8;
        if (need > capacity_bits || text.size() >= (std::size_t(1) << count_bits)) continue;
        std::vector<bool> bits;
        auto put = [&](std::uint32_t v, int n) {
            for (int i = n - 1; i >= 0; --i) bits.push_back(((v >> i) & 1) != 0);
        };
        put(0x4, 4);
        put(std::uint32_t(text.size()), count_bits);
        for (unsigned char c : text) put(c, 8);
        put(0, int(std::min<std::size_t>(4, capacity_bits - bits.size())));
        put(0, int((8 - bits.size() % 8) % 8));
        for (std::uint8_t pad = 0xEC; bits.size() < capacity_bits; pad ^= 0xEC ^ 0x11) put(pad, 8);
        std::vector<std::uint8_t> data(bits.size() / 8, 0);
        for (std::size_t i = 0; i < bits.size(); ++i)
            if (bits[i]) data[i >> 3] |= std::uint8_t(1u << (7 - (i & 7)));
        return Builder(ver).build(data);
    }
    return {};
}

std::string to_svg(const Code& code, int scale) {
    const int border = 4;
    const int dim = (code.size + border * 2) * scale;
    std::string s = "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"" + std::to_string(dim) + "\" height=\"" +
                    std::to_string(dim) + "\" viewBox=\"0 0 " + std::to_string(code.size + border * 2) + " " +
                    std::to_string(code.size + border * 2) +
                    "\" shape-rendering=\"crispEdges\"><rect width=\"100%\" height=\"100%\" fill=\"#fff\"/><path d=\"";
    for (int y = 0; y < code.size; ++y)
        for (int x = 0; x < code.size; ++x)
            if (code.dark(x, y))
                s += "M" + std::to_string(x + border) + "," + std::to_string(y + border) + "h1v1h-1z";
    s += "\" fill=\"#000\"/></svg>";
    return s;
}

} // namespace xmesh::qr
