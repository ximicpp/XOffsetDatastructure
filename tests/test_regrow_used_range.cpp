// D6: after an in-place reload the arena regrows inside its previously used
// range in 4 KiB steps (each step memsets only what it exposes) instead of
// doubling, and grows geometrically again once it passes that range. The
// logical length never reaches the wire: every save equals the save of the
// same operations on a freshly loaded buffer.
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <string>

#include "../xoffsetdatastructure.hpp"

using namespace XOffsetDatastructure;

struct RegrowRoot {
    XVector<std::uint64_t> values;
    XVector<std::uint64_t> other;
};
XOFFSET_REGISTER_SCHEMA_NAME(RegrowRoot, "test.RegrowRoot")

namespace {

using Buffer = TypedXBuffer<RegrowRoot>;

XBuffer::MaximumCapacity growable() {
    return XBuffer::maximum_capacity(std::size_t(32) << 20).with_growth(XGrowPolicy::Automatic);
}

std::size_t used_end(Buffer& buffer) { return buffer.arena()->used_end; }

bool tail_zero(Buffer& buffer) {
    const auto* bytes = static_cast<const unsigned char*>(buffer.get_address());
    for (std::size_t i = used_end(buffer); i < buffer.get_size(); ++i)
        if (bytes[i] != 0) return false;
    return true;
}

} // namespace

int main() {
    // High-water mark: 2 MiB of non-zero values, then reload a small wire in place.
    auto a = Buffer::create(4096, growable());
    for (std::uint64_t i = 0; i < (std::uint64_t(2) << 20) / 8; ++i) a.root().values.push_back(~i);
    const std::size_t high = a.get_size();
    assert(high >= (std::size_t(2) << 20));

    std::string small;
    {
        auto s = Buffer::create(4096, growable());
        s.root().values.push_back(7);
        small = s.save_verified();
    }
    a.reload_verified(small);
    auto b = Buffer::load_verified(small, growable());
    assert(a.get_size() < high);
    assert(a.save_verified() == b.save_verified());

    // Inside the used range: logical length stays within 4 KiB of the tail.
    std::size_t growths = 0, last = a.get_size();
    std::uint64_t pushed = 0;
    while (used_end(a) + 64 * 1024 < high) {
        a.root().other.push_back(pushed);
        b.root().other.push_back(pushed);
        ++pushed;
        if (a.get_size() != last) {
            ++growths;
            last = a.get_size();
            const std::size_t needed = used_end(a);
            assert(last >= needed && last - needed <= 4096);
            assert(tail_zero(a));
        }
    }
    assert(growths > 4);
    assert(b.get_size() >= a.get_size());
    assert(a.save_verified() == b.save_verified());

    // Past the used range: geometric growth resumes.
    bool doubled = false;
    while (used_end(a) < 4 * high) {
        const std::size_t before = a.get_size();
        a.root().other.push_back(pushed);
        b.root().other.push_back(pushed);
        ++pushed;
        if (a.get_size() != before) {
            assert(tail_zero(a));
            if (before >= high && a.get_size() >= 2 * before) doubled = true;
        }
    }
    assert(doubled);
    assert(a.save_verified() == b.save_verified());
    assert(a.root().other.size() == pushed && a.root().values.size() == 1);

    std::cout << "test_regrow_used_range: growths_in_used_range=" << growths
              << " high=" << high << " final_size=" << a.get_size() << " passed\n";
    return 0;
}
