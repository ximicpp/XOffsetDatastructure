// Purpose: Sweep allocation failures; verify atomic individual mutations and explicit retry.
#include <cassert>
#include <iostream>
#include <string>
#include "../xoffsetdatastructure.hpp"
using namespace XOffsetDatastructure;

struct RetryRecord { XString first, second; };
struct RetryRoot {
    RetryRecord source;
    XVector<RetryRecord> rows;
    XVector<RetryRecord> other_rows;
    XMap<int32_t, RetryRecord> map;
    XSet<XString> set;
};
XOFFSET_REGISTER_SCHEMA_NAME(RetryRoot, "test.RetryRoot")

std::string snapshot(const RetryRoot& r) {
    std::string out;
    for (const auto& v : r.rows) out += std::string(v.first.view()) + ":" + std::string(v.second.view()) + ";";
    out += "|";
    for (const auto& kv : r.map) out += std::to_string(kv.first) + ":" + std::string(kv.second.first.view()) + ":" + std::string(kv.second.second.view()) + ";";
    out += "|";
    for (const auto& v : r.set) out += std::string(v.view()) + ";";
    return out;
}

void mutate(RetryRoot& r, int operation) {
    switch (operation) {
        case 0: r.rows.push_back(r.source); break;
        case 1: r.rows.insert(r.rows.begin(), r.source); break;
        case 2: r.rows.assign(5, r.source); break;
        case 3: r.rows.resize(7, r.source); break;
        case 4: r.map.try_emplace(20, r.source); break;
        case 5: r.map.insert_or_assign(10, r.source); break;
        case 6: r.set.insert(r.source.first.view()); break;
        case 7: r.rows.reserve(100); break;
        case 8: r.map.erase(10); break;
        case 9: r.rows.erase(r.rows.begin()); break;
        case 10: r.rows = r.other_rows; break;
    }
}

int main() {
    int failed = 0, succeeded = 0;
    for (int operation = 0; operation < 11; ++operation) {
        for (std::size_t slack = 0; slack <= 4096; slack += 64) {
            auto buf = XBuffer::create<RetryRoot>(16384);
            auto& r = buf.root<RetryRoot>();
            r.source.first = std::string(180, 'm');
            r.source.second = std::string(220, 'n');
            r.other_rows.push_back(r.source);
            r.rows.reserve(4);
            for (int i = 0; i < 2; ++i) {
                auto& row = r.rows.emplace_back();
                row.first = i == 0 ? "aaa" : "zzz";
                row.second = "value";
            }
            r.map.reserve(4);
            r.map.try_emplace(10, r.rows[0]);
            r.map.try_emplace(30, r.rows[1]);
            r.set.insert("aaa");
            r.set.insert("zzz");
            auto before = snapshot(r);
            auto free = buf.get_free_memory();
            // V2: an unreachable filler block must be released before a
            // verified save (live blocks + holes must tile the payload).
            void* filler = nullptr;
            std::uint32_t filler_bytes = 0;
            if (free > slack + 32) {
                filler_bytes = static_cast<uint32_t>(free - slack - 32);
                filler = buf.arena()->allocate_aligned(filler_bytes, 8);
            }
            auto release_filler = [&] {
                if (filler) buf.arena()->deallocate(filler, filler_bytes);
                filler = nullptr;
            };
            try {
                mutate(r, operation);
                release_filler();
                auto expected = snapshot(r);
                auto loaded = TypedXBuffer<RetryRoot>::load(buf.save_verified<RetryRoot>());
                assert(snapshot(loaded.root()) == expected);
                ++succeeded;
            } catch (const XBadAlloc&) {
                ++failed;
                release_filler();
                assert(snapshot(r) == before);
                auto loaded = TypedXBuffer<RetryRoot>::load(buf.save_verified<RetryRoot>());
                assert(snapshot(loaded.root()) == before);
                bool grew = loaded.grow(65536);
                assert(grew);
                mutate(loaded.root(), operation);
                auto expected = snapshot(loaded.root());
                auto again = TypedXBuffer<RetryRoot>::load(loaded.save_verified());
                assert(snapshot(again.root()) == expected);
            }
        }
    }
    assert(failed > 0 && succeeded > 0);
    std::cout << "Mutation sweep: " << failed << " failures recovered, " << succeeded << " successes\n";
}
