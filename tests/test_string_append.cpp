// XString::append(): matches std::string::append over random runs of
// appends, reassignments, clears and self-views; keeps the I1 inline bytes
// and the M4 zero spare capacity exact; grows geometrically; and the result
// survives a verified wire round trip.
#include <cassert>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <random>
#include <string>
#include <string_view>
#include <vector>

#include "../xoffsetdatastructure.hpp"

using namespace XOffsetDatastructure;

struct AppendRoot {
    XVector<XString> strings;
};
XOFFSET_REGISTER_SCHEMA_NAME(AppendRoot, "test.StringAppend")

namespace {

// Value, terminator and every byte the head or block owns past it.
void check_exact(const XString& s, const std::string& want) {
    assert(s.view() == want);
    if (s.is_inline()) {
        const auto* head = reinterpret_cast<const unsigned char*>(&s);
        for (std::size_t i = 0; i < 11; ++i) {
            const unsigned char expect = i < want.size() ? static_cast<unsigned char>(want[i]) : 0;
            assert(head[4 + i] == expect);
        }
        assert(head[15] == (0x80u | want.size()));
        return;
    }
    if (s.capacity() == 0) { assert(want.empty()); return; }
    const char* chars = s.data();
    for (std::size_t i = want.size(); i <= s.capacity(); ++i) assert(chars[i] == '\0');
}

std::string piece(std::mt19937& rng, std::size_t length) {
    std::string out;
    for (std::size_t i = 0; i < length; ++i) out.push_back(static_cast<char>(0x21 + rng() % 90));
    return out;
}

} // namespace

int main() {
    // The short move behaves as memmove for every length and overlap.
    for (std::size_t count = 1; count <= 16; ++count) {
        for (std::size_t from = 0; from <= 16; ++from) {
            for (std::size_t to = 0; to <= 16; ++to) {
                char expect[40], got[40];
                for (std::size_t i = 0; i < sizeof expect; ++i) expect[i] = got[i] = static_cast<char>('A' + i);
                std::memmove(expect + to, expect + from, count);
                fixed_detail::move_short_bytes(got + to, got + from, count);
                assert(std::memcmp(expect, got, sizeof expect) == 0);
            }
        }
    }

    auto buf = XBuffer::create<AppendRoot>(64 * 1024,
        XBuffer::maximum_capacity(std::size_t(64) << 20).with_growth(XGrowPolicy::Automatic));
    auto& root = buf.root<AppendRoot>();
    constexpr std::size_t S = 24;
    for (std::size_t i = 0; i < S; ++i) root.strings.emplace_back();
    std::vector<std::string> model(S);
    std::mt19937 rng(20261022);
    std::size_t appends = 0, self_views = 0;

    for (int step = 0; step < 200000; ++step) {
        const auto i = static_cast<std::uint32_t>(rng() % S);
        auto& s = root.strings[i];
        auto& m = model[i];
        const auto op = rng() % 16;
        if (op < 9) {
            // Short pieces cross the inline limit; long ones force growth.
            const std::size_t length = rng() % 4 == 0 ? rng() % 200 : rng() % 12;
            const std::string p = piece(rng, length);
            s.append(p);
            m.append(p);
            ++appends;
        } else if (op < 12 && !m.empty()) {
            // A view of the string's own characters.
            const std::size_t from = rng() % (m.size() + 1);
            const std::size_t count = rng() % (m.size() + 1 - from);
            const std::string expect = m.substr(from, count);
            s.append(std::string_view(s.data() + from, count));
            m.append(expect);
            ++self_views;
        } else if (op < 13) {
            s.clear();
            m.clear();
        } else if (op < 14) {
            const std::string p = piece(rng, rng() % 60);
            s = std::string_view(p);
            m = p;
        } else if (op < 15) {
            s += std::string_view("+=");
            m += "+=";
        } else if (m.size() > 4000) {
            s = std::string_view();
            m.clear();
        }
        check_exact(s, m);
    }

    // Growth at least doubles: n one-character appends take O(log n) blocks.
    {
        root.strings.emplace_back();
        auto& s = root.strings.back();
        std::string m;
        std::size_t blocks = 0;
        const char* last = nullptr;
        for (int k = 0; k < 10000; ++k) {
            s.append("x");
            m.push_back('x');
            if (!s.is_inline() && s.data() != last) { ++blocks; last = s.data(); }
        }
        check_exact(s, m);
        assert(s.capacity() >= 10000 && blocks <= 12);
    }

    // Empty appends change nothing, including on a never-assigned head.
    {
        root.strings.emplace_back();
        auto& s = root.strings.back();
        s.append(std::string_view());
        check_exact(s, "");
        s.append("abc");
        check_exact(s, "abc");
        s.append(std::string_view());
        check_exact(s, "abc");
    }

    auto copy = XBuffer::load_verified<AppendRoot>(buf.save_verified<AppendRoot>());
    const auto& back = copy.root<AppendRoot>();
    assert(back.strings.size() == root.strings.size());
    for (std::uint32_t i = 0; i < back.strings.size(); ++i) assert(back.strings[i].view() == root.strings[i].view());

    std::cout << "test_string_append: " << appends << " appends, " << self_views << " self-views passed\n";
    return 0;
}
