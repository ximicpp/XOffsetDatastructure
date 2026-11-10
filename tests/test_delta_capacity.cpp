// A delta that announces a payload larger than the replica's maximum
// capacity is refused with capacity_exceeded before anything is sized for
// it (pre-release fuzz 3.5.0, input tests/fuzz/regressions/delta_sync/
// 509a8c24e3): the map of checked bytes is about a third of the announced
// payload, and was allocated before the arena's growth was refused. Key and
// non-key deltas, the full check and the trust tier admit; the replica and
// the cursor are left as they were, and no single allocation during the
// apply is larger than 1 MiB. Not in NO_EXCEPTIONS_TESTS: the hook on
// operator new throws.
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <new>
#include <string>
#include <vector>

#include "xoffsetdatastructure.hpp"
#include "xoffset_receiver.hpp"

namespace xo = XOffsetDatastructure;

namespace host {
bool track = false;
std::size_t largest = 0;
}
void* operator new(std::size_t n) {
    if (host::track && n > host::largest) host::largest = n;
    if (void* p = std::malloc(n ? n : 1)) return p;
    throw std::bad_alloc();
}
void operator delete(void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }
void* operator new[](std::size_t n) { return ::operator new(n); }
void operator delete[](void* p) noexcept { ::operator delete(p); }
void operator delete[](void* p, std::size_t) noexcept { ::operator delete(p); }

namespace dc {
struct Item {
    std::uint32_t id{};
    xo::XString name;
    xo::XVector<std::uint32_t> vals;
};
struct Root {
    std::uint32_t epoch{};
    xo::XVector<Item> items;
};
} // namespace dc
XOFFSET_RECORD(dc::Item, id, name, vals)
XOFFSET_RECORD(dc::Root, epoch, items)
XOFFSET_REGISTER_SCHEMA_NAME(dc::Root, "test.delta.capacity")

namespace {
using Buffer = xo::TypedXBuffer<dc::Root>;
constexpr std::size_t kHeaderFixed = 12;    // 'X' 'D', format, flags, stream
constexpr std::uint8_t kFlagExtent = 8;
constexpr std::size_t kHuge = std::size_t(1) << 30;  // 1 GiB announced
constexpr std::size_t kReplicaCap = 64 * 1024;

int failures = 0;
void need(bool ok, const std::string& what) {
    if (!ok) {
        std::cerr << "FAIL: " << what << '\n';
        ++failures;
    }
}

std::string varint(std::uint64_t v) {
    std::string s;
    while (v >= 0x80) { s += char(std::uint8_t(v) | 0x80); v >>= 7; }
    s += char(v);
    return s;
}
std::uint64_t get_varint(const std::string& d, std::size_t& at) {
    std::uint64_t v = 0;
    for (int shift = 0;; shift += 7) {
        const auto b = std::uint8_t(d.at(at++));
        v |= std::uint64_t(b & 0x7F) << shift;
        if (!(b & 0x80)) return v;
    }
}

// The same delta announcing a payload of `used` bytes (root unchanged).
std::string announce(const std::string& d, std::uint64_t root_from_begin, std::size_t used) {
    const auto flags = std::uint8_t(d[3]);
    std::size_t at = kHeaderFixed;
    const std::uint64_t version = get_varint(d, at);
    if (flags & kFlagExtent) { (void)get_varint(d, at); (void)get_varint(d, at); }
    std::string out = d.substr(0, kHeaderFixed);
    out[3] = char(flags | kFlagExtent);
    out += varint(version) + varint((used - 48) / 8) + varint(root_from_begin) + d.substr(at);
    return out;
}
std::uint64_t root_of_key(const std::string& key) {
    std::size_t at = kHeaderFixed;
    (void)get_varint(key, at);  // version
    (void)get_varint(key, at);  // used words
    return get_varint(key, at);
}

void fill(dc::Root& r, int n) {
    for (int i = 0; i < n; ++i) {
        auto& it = r.items.emplace_back();
        it.id = std::uint32_t(i);
        it.name = "item name long enough for the heap " + std::to_string(i);
        for (int k = 0; k < 8; ++k) it.vals.push_back(std::uint32_t(i * k));
    }
}

template <class Apply>
void refused(const char* where, Buffer& rep, xo::XDeltaCursor& cur, const std::string& d, Apply&& apply) {
    const std::string before(reinterpret_cast<const char*>(rep.get_buffer().data()), rep.get_buffer().size());
    const auto version = cur.version();
    const auto stream = cur.stream();
    host::largest = 0;
    host::track = true;
    const xo::XStatus r = apply(rep, d, cur);
    host::track = false;
    need(!r && r.code() == xo::XErrc::capacity_exceeded,
         std::string(where) + ": not refused with capacity_exceeded (" + (r ? std::string("accepted") : r.error().message) + ")");
    need(host::largest <= (std::size_t(1) << 20),
         std::string(where) + ": allocated " + std::to_string(host::largest) + " bytes before refusing");
    const std::string after(reinterpret_cast<const char*>(rep.get_buffer().data()), rep.get_buffer().size());
    need(after == before && cur.version() == version && cur.stream() == stream,
         std::string(where) + ": the refusal changed the replica or the cursor");
}

xo::XStatus full(Buffer& rep, const std::string& d, xo::XDeltaCursor& cur) { return rep.try_apply_delta_verified(d, cur); }
xo::XStatus admit(Buffer& rep, const std::string& d, xo::XDeltaCursor& cur) {
    static xo::XReceiver<xo::XLink::from_server> rx(xo::XBuffer::maximum_capacity(kReplicaCap));
    return rx.apply_delta<xo::XCheck::admit>(rep, d, cur);
}

void run(const char* name, xo::XStatus (*apply)(Buffer&, const std::string&, xo::XDeltaCursor&)) {
    auto src = Buffer::create(4096, xo::XBuffer::maximum_capacity(kReplicaCap));
    fill(src.root(), 20);
    xo::XDeltaBaseline base;
    std::string key;
    (void)src.save_delta_into(key, base);
    const std::uint64_t root = root_of_key(key);

    // A key delta: refused before the buffer is reinitialised.
    {
        Buffer rep(4096, xo::XBuffer::maximum_capacity(kReplicaCap));
        xo::XDeltaCursor cur;
        refused((std::string(name) + " key").c_str(), rep, cur, announce(key, root, kHuge), apply);
        need(bool(apply(rep, key, cur)), std::string(name) + ": the good key delta after the refusal");
    }
    // A delta after the key delta, as a payload growing past the capacity.
    {
        Buffer rep(4096, xo::XBuffer::maximum_capacity(kReplicaCap));
        xo::XDeltaCursor cur;
        need(bool(apply(rep, key, cur)), std::string(name) + ": key delta");
        src.root().epoch += 1;
        src.root().items[3].vals.push_back(7);
        std::string d;
        (void)src.save_delta_into(d, base);
        for (std::size_t used : {kHuge, kReplicaCap + 4096, std::size_t(0x7ffffff8)})
            refused((std::string(name) + " delta used " + std::to_string(used)).c_str(), rep, cur,
                    announce(d, root, used), apply);
        need(bool(apply(rep, d, cur)), std::string(name) + ": the good delta after the refusals");
        need(rep.root().items[3].vals.size() == src.root().items[3].vals.size(), std::string(name) + ": replica differs");
        base.reset();
        key.clear();
        (void)src.save_delta_into(key, base);
    }
}
} // namespace

int main() {
    run("full", &full);
    run("admit", &admit);
    if (failures) return 1;
    std::cout << "test_delta_capacity: all checks passed\n";
    return 0;
}
