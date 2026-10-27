// Hot-path constants: the inline string store (every length, source offset,
// previous content and self-aliasing view) writes exactly the I1 head bytes,
// and push_back into spare capacity matches the general path.
#include <cassert>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <string>
#include <string_view>

#include "../xoffsetdatastructure.hpp"

using namespace XOffsetDatastructure;

struct HotRoot {
    XVector<XString> strings;
    XVector<std::int32_t> ints;
    XVector<std::uint64_t> wides;
};
XOFFSET_REGISTER_SCHEMA_NAME(HotRoot, "test.HotPathConstants")

namespace {

// I1 head bytes 0..11: characters, zero padding, then 0x80 | length.
void check_inline(const XFixedString& s, std::string_view expected) {
    assert(s.is_inline());
    assert(s.view() == expected);
    const auto* head = reinterpret_cast<const unsigned char*>(&s);
    for (std::size_t i = 0; i < 11; ++i) {
        const unsigned char want = i < expected.size() ? static_cast<unsigned char>(expected[i]) : 0;
        assert(head[i] == want);
    }
    assert(head[11] == (0x80u | expected.size()));
}

std::string pattern(std::size_t length, unsigned seed) {
    std::string out;
    for (std::size_t i = 0; i < length; ++i) out.push_back(static_cast<char>(0x21 + (seed * 7 + i * 13) % 90));
    return out;
}

} // namespace

int main() {
    auto buf = XBuffer::create<HotRoot>(64 * 1024, XBuffer::maximum_capacity(std::size_t(16) << 20).with_growth(XGrowPolicy::Automatic));
    auto& root = buf.root<HotRoot>();
    root.strings.emplace_back();
    auto& s = root.strings[0];
    std::size_t cases = 0;

    // Every new length, every source offset, every previous inline length, and
    // a previous heap value.
    alignas(16) char source[32];
    for (std::size_t before = 0; before <= 12; ++before) {
        for (std::size_t length = 0; length <= XFixedString::inline_max; ++length) {
            for (std::size_t offset = 0; offset < 8; ++offset) {
                s = std::string_view(pattern(before, unsigned(before)));
                const std::string want = pattern(length, unsigned(length + offset));
                std::memset(source, 0x7f, sizeof source);
                std::memcpy(source + offset, want.data(), want.size());
                s = std::string_view(source + offset, length);
                if (length == 0) {
                    assert(s.view().empty());
                } else if (before > XFixedString::inline_max) {
                    // The heap block is kept and reused; contents still match.
                    assert(s.view() == want);
                } else {
                    check_inline(s, want);
                }
                ++cases;
            }
        }
    }

    // Self-aliasing views: assigning part of the current inline value.
    for (std::size_t length = 1; length <= XFixedString::inline_max; ++length) {
        for (std::size_t from = 0; from < length; ++from) {
            for (std::size_t count = 0; from + count <= length; ++count) {
                root.strings.emplace_back();
                auto& t = root.strings.back();
                const std::string full = pattern(length, 3);
                t = std::string_view(full);
                check_inline(t, full);
                t = t.view().substr(from, count);
                const std::string want = full.substr(from, count);
                if (count == 0) assert(t.view().empty());
                else check_inline(t, want);
                ++cases;
            }
        }
    }

    // push_back into spare capacity after clear, and across growth.
    for (int round = 0; round < 3; ++round) {
        root.ints.clear();
        root.wides.clear();
        for (int i = 0; i < 5000; ++i) {
            root.ints.push_back(i * 3 - round);
            root.wides.push_back(std::uint64_t(i) << 33 | unsigned(round));
        }
        assert(root.ints.size() == 5000 && root.wides.size() == 5000);
        for (int i = 0; i < 5000; ++i) {
            assert(root.ints[std::uint32_t(i)] == i * 3 - round);
            assert(root.wides[std::uint32_t(i)] == (std::uint64_t(i) << 33 | unsigned(round)));
        }
    }

    // push_back of one of the vector's own elements, in spare capacity and
    // when it must grow (the element may move: the general path copies it
    // first).
    for (int round = 0; round < 2; ++round) {
        root.wides.clear();
        root.wides.push_back(7);
        root.strings.clear();
        root.strings.push_back(XString());
        root.strings[0] = std::string_view(pattern(30, 5));
        const std::string first = pattern(30, 5);
        for (std::uint32_t i = 1; i < 300; ++i) {
            root.wides.push_back(root.wides[i - 1]);
            root.wides.back() += 1;
            root.strings.push_back(root.strings[i - 1]);
        }
        for (std::uint32_t i = 0; i < 300; ++i) {
            assert(root.wides[i] == 7u + i);
            assert(root.strings[i].view() == first);
        }
    }

    // The wire round trip keeps every value.
    auto copy = XBuffer::load_verified<HotRoot>(buf.save_verified<HotRoot>());
    const auto& back = copy.root<HotRoot>();
    assert(back.strings.size() == root.strings.size());
    for (std::uint32_t i = 0; i < back.strings.size(); ++i) assert(back.strings[i].view() == root.strings[i].view());
    assert(back.ints.size() == 5000 && back.ints[4999] == root.ints[4999]);

    std::cout << "test_hot_path_constants: " << cases << " string cases passed\n";
    return 0;
}
