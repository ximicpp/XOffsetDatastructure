// Purpose: Regressions for allocation bounds, move ownership, and container aliasing.
#include <cassert>
#include <iostream>
#include <limits>
#include "../xoffsetdatastructure.hpp"
using namespace XOffsetDatastructure;

struct RuntimeRoot {
    int32_t id{};
    XString name;
    XVector<int32_t> first, guard, second;
    XVector<XString> strings;
    XMap<XString, int32_t> map;
};
struct OtherRoot { int32_t id{}; };
XOFFSET_RECORD_DERIVED(RuntimeRoot, (), id, name, first, guard, second, strings, map)
XOFFSET_RECORD_DERIVED(OtherRoot, (), id)
XOFFSET_REGISTER_SCHEMA_NAME(RuntimeRoot, "test.RuntimeRoot")
XOFFSET_REGISTER_SCHEMA_NAME(OtherRoot, "test.OtherRoot")

template <typename F> void must_reject(F&& f) {
    bool rejected = false;
    try { f(); } catch (const XException&) { rejected = true; }
    assert(rejected);
}

void free_block_sizes() {
    auto buf = XBuffer::create<RuntimeRoot>(8192);
    auto& r = buf.root<RuntimeRoot>();
    r.first.reserve(18); // 72 bytes in the 128-byte class.
    r.guard.reserve(4);
    for (int i = 0; i < 4; ++i) r.guard.push_back(42);
    r.first.reserve(64);
    r.second.reserve(32);
    for (int i = 0; i < 32; ++i) r.second.push_back(99);
    for (auto value : r.guard) assert(value == 42);
    auto loaded = XBuffer::load_verified<RuntimeRoot>(buf.save_verified<RuntimeRoot>());
    assert(loaded.root<RuntimeRoot>().guard[0] == 42);
}

void move_ownership() {
    auto source = XBuffer::create<RuntimeRoot>(4096);
    source.root<RuntimeRoot>().id = 7;
    auto source_handle = source.handle<RuntimeRoot>();
    XBuffer moved(std::move(source));
    assert(!source_handle);
    moved.root<RuntimeRoot>().name = "moved";
    auto target = XBuffer::create<RuntimeRoot>(4096);
    auto handle = target.handle<RuntimeRoot>();
    target = std::move(moved);
    assert(handle->id == 7);
    handle->first.push_back(123);
    assert(target.root<RuntimeRoot>().first[0] == 123);
    const auto bytes = target.save_verified<RuntimeRoot>();
    auto loaded = TypedXBuffer<RuntimeRoot>::load(bytes);
    bool grew = loaded.grow(4096);
    assert(grew);
    loaded.root().name = std::string(300, 'x');
    assert(loaded.root().name.size() == 300);
}

void strings_and_aliases() {
    auto buf = XBuffer::create<RuntimeRoot>(65536);
    auto& r = buf.root<RuntimeRoot>();
    r.name = "";
    assert(r.name.empty() && r.name.c_str()[0] == '\0');
    r.name = "abcdef";
    r.name.assign(r.name.view().substr(2));
    assert(r.name == "cdef");
    r.first.reserve(4);
    for (int i = 0; i < 4; ++i) r.first.push_back(i + 11);
    r.first.push_back(r.first[0]);
    assert(r.first.back() == 11);
    r.first.insert(r.first.begin(), r.first.back());
    assert(r.first.front() == 11);
    r.first.assign(9, r.first[1]);
    for (auto x : r.first) assert(x == 11);
    r.strings.reserve(1);
    r.strings.push_back("hello");
    r.strings.push_back(r.strings[0]);
    assert(r.strings[0] == "hello" && r.strings[1] == "hello");
    r.strings.emplace_back(r.strings.front());
    r.strings.push_back("");
    const char* payload_before = r.strings.front().data();
    r.strings.reserve(64);
    assert(r.strings.front().data() == payload_before);
    r.strings.insert(r.strings.begin() + 1, r.strings.back());
    r.strings.erase(r.strings.begin() + 1);
    r.map.try_emplace("key", 1);
    auto compacted = XCompactor::compact<RuntimeRoot>(buf);
    assert(compacted.root<RuntimeRoot>().strings[2] == "hello");
    assert(compacted.root<RuntimeRoot>().map.find("key")->second == 1);
}

// Inserts and erases in the middle of a vector of trivially copyable
// elements with spare capacity (inline under Clang): a value that aliases a
// moved element, order, the zeroed vacated slots (M4), and the general path
// once the vector is full.
void mid_insert_erase() {
    auto buf = XBuffer::create<RuntimeRoot>(65536);
    auto& v = buf.root<RuntimeRoot>().first;
    v.reserve(16);
    for (int i = 0; i < 8; ++i) v.push_back(i * 10);
    v.insert(v.begin() + 3, v[5]);
    auto it = v.insert(v.begin() + 8, -1);
    assert(*it == -1 && v.capacity() == 16);
    const int32_t inserted[] = {0, 10, 20, 50, 30, 40, 50, 60, -1, 70};
    assert(v.size() == 10);
    for (std::uint32_t i = 0; i < 10; ++i) assert(v[i] == inserted[i]);
    v.erase(v.begin() + 1);
    v.erase(v.begin() + 4);
    const int32_t erased[] = {0, 20, 50, 30, 50, 60, -1, 70};
    assert(v.size() == 8 && v.capacity() == 16);
    for (std::uint32_t i = 0; i < 8; ++i) assert(v[i] == erased[i]);
    assert(v.data()[8] == 0 && v.data()[9] == 0);
    while (v.size() < v.capacity()) v.push_back(7);
    v.insert(v.begin() + 1, v[2]);
    assert(v.size() == 17 && v[0] == 0 && v[1] == 50 && v[2] == 20 && v[16] == 7);
    auto loaded = XBuffer::load_verified<RuntimeRoot>(buf.save_verified<RuntimeRoot>());
    const auto& w = loaded.root<RuntimeRoot>().first;
    assert(w.size() == 17);
    for (std::uint32_t i = 0; i < 17; ++i) assert(w[i] == v[i]);
}

void limits_and_schema() {
    auto buf = XBuffer::create<RuntimeRoot>(4096);
    auto& r = buf.root<RuntimeRoot>();
    must_reject([&] { r.first.reserve(1u << 30); });
    must_reject([&] { r.first.reserve(1ull << 32); });
    must_reject([&] { r.first.resize(1ull << 32); });
    must_reject([&] { r.first.assign(1ull << 32, 42); });
    must_reject([&] { r.name.reserve(UINT32_MAX); });
    must_reject([&] { r.name.reserve(1ull << 32); });
    must_reject([&] { (void)buf.arena_allocator<int64_t>().allocate(SIZE_MAX); });
    assert(r.first.capacity() == 0 && r.name.capacity() == 0);
    const auto size = buf.get_size();
    bool grew = buf.grow(SIZE_MAX);
    assert(!grew && buf.get_size() == size);
    must_reject([&] { (void)buf.handle<OtherRoot>(); });
    must_reject([&] { (void)buf.save_verified<OtherRoot>(); });
    must_reject([&] { (void)buf.make<OtherRoot>(); });
    assert(buf.matches_schema<RuntimeRoot>());
    auto raw = XBuffer::load_unverified(buf.save());
    must_reject([&] { (void)raw.save_verified<RuntimeRoot>(); });
}

int main() {
    free_block_sizes();
    move_ownership();
    strings_and_aliases();
    mid_insert_erase();
    limits_and_schema();
    std::cout << "Runtime regressions passed\n";
}
