// Actual VM base alignment guard for every isolated allocator candidate.
// Retained odd-page mappings prevent repeatedly receiving the same mapping.
#include <array>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>
#include <vector>
#ifndef _WIN32
#include <unistd.h>
#endif
#include "../xoffsetdatastructure.hpp"
#include "../tests/allocator_model_support.hpp"
#ifndef S3_PROBE
#error "test_vm_allocator_base requires the isolated S3_PROBE header"
#endif

using namespace XOffsetDatastructure;
using allocator_model::require;
namespace {
using H = XBufferCore::AllocationHeader;
using I = allocator_model::Interval;
struct Live { void* ptr; std::size_t size, alignment; unsigned char value; };
struct Gap { std::uint64_t begin, end; };
H header(const void* pointer) {
    H h;
    std::memcpy(&h, static_cast<const char*>(pointer) - sizeof(H), sizeof(H));
    return h;
}
std::uint64_t offset(XBufferCore& b, const void* pointer) {
    return reinterpret_cast<std::uintptr_t>(pointer) - reinterpret_cast<std::uintptr_t>(b.data());
}
void valid_header(XBufferCore& b, const void* pointer, std::size_t needed, std::size_t alignment) {
    const auto off = offset(b, pointer);
    require(off >= sizeof(H) && off < b.get_size(), "user offset outside arena");
    const auto h = header(pointer);
    require(h.user_offset == off && h.user_bytes >= needed && h.user_bytes <= b.get_size() - off &&
                h.total_bytes == std::uint64_t(h.user_bytes) + sizeof(H), "invalid physical allocation header");
    require(h.alignment && !(h.alignment & (h.alignment - 1)) &&
                h.alignment >= alignment && reinterpret_cast<std::uintptr_t>(pointer) % h.alignment == 0 &&
                reinterpret_cast<std::uintptr_t>(pointer) % alignment == 0 &&
                (reinterpret_cast<std::uintptr_t>(pointer) - sizeof(H)) % alignof(H) == 0,
            "actual user/header alignment differs");
}
std::vector<I> free_intervals(XBufferCore& b) {
    std::vector<I> result;
    b.s3_visit_free([&](auto off, auto size, auto alignment) {
        require(off >= sizeof(H) && off < b.get_size() && size <= b.get_size() - off,
                "free interval out of bounds before header read");
        valid_header(b, b.data() + off, size, alignment);
        const auto h = header(b.data() + off);
        require(h.user_bytes == size && h.alignment == alignment, "free reference/header mismatch");
        result.push_back({off - sizeof(H), off + std::uint64_t(size), result.size(), 1});
    });
    return result;
}
void observe(XBufferCore& b, const std::vector<Live>& live, const std::vector<Gap>& gaps,
             allocator_model::Coverage& coverage, const void* original_base) {
    require(b.get_address() == original_base && b.s3_validate_index(), "base moved or index invariant failed");
    auto ranges = free_intervals(b);
    std::uint64_t free_bytes = 0;
    for (const auto& range : ranges) free_bytes += range.end - range.begin;
    require(free_bytes + b.get_size() - b.arena()->used_end == b.get_free_memory(),
            "free physical accounting mismatch");
    for (std::size_t id = 0; id < live.size(); ++id) if (const auto& item = live[id]; item.ptr) {
        valid_header(b, item.ptr, item.size, item.alignment);
        const auto h = header(item.ptr);
        ranges.push_back({h.user_offset - sizeof(H), h.user_offset + std::uint64_t(h.user_bytes), id, 0});
        for (std::size_t j = 0; j < item.size; ++j)
            require(static_cast<const unsigned char*>(item.ptr)[j] == item.value, "live bytes changed");
    }
    allocator_model::validate(ranges, b.arena()->used_end);
    for (const auto gap : gaps) {
        for (auto i = gap.begin; i < gap.end; ++i)
            require(static_cast<unsigned char>(b.data()[i]) == 0xe7, "unknown alignment gap bytes changed");
        for (const auto range : ranges)
            require(range.end <= gap.begin || range.begin >= gap.end, "allocator claimed unknown alignment gap");
    }
    coverage.observe(ranges);
}
std::size_t add_live(XBufferCore& b, std::vector<Live>& live, std::vector<Gap>& gaps,
                     std::size_t bytes, std::size_t alignment, unsigned char value) {
    const auto previous_end = b.arena()->used_end;
    auto* pointer = b.arena()->allocate_aligned(static_cast<std::uint32_t>(bytes), alignment);
    valid_header(b, pointer, bytes, alignment);
    const auto begin = offset(b, pointer) - sizeof(H);
    if (begin > previous_end) {
        std::memset(b.data() + previous_end, 0xe7, begin - previous_end);
        gaps.push_back({previous_end, begin});
    }
    std::memset(pointer, value, bytes);
    live.push_back({pointer, bytes, alignment, value});
    return live.size() - 1;
}
void emit(std::string_view variant, std::string_view event, XBufferCore& b, const Live& item,
          std::size_t page) {
    const auto address = reinterpret_cast<std::uintptr_t>(item.ptr);
    const auto h = header(item.ptr);
    std::cout << "{\"event\":\"" << event << "\",\"variant\":\"" << variant
              << "\",\"page_bytes\":" << page << ",\"base_mod_2page\":"
              << reinterpret_cast<std::uintptr_t>(b.data()) % (2 * page)
              << ",\"requested_alignment\":" << item.alignment
              << ",\"actual_alignment\":" << (address & (~address + 1))
              << ",\"stored_alignment\":" << h.alignment
              << ",\"address_mod_requested\":" << address % item.alignment
              << ",\"requested_bytes\":" << item.size << ",\"physical_user_bytes\":" << h.user_bytes
              << ",\"user_offset\":" << h.user_offset << ",\"used_end\":" << b.arena()->used_end << "}\n";
}
} // namespace

int main(int argc, char** argv) {
    try {
        require(argc == 2, "usage: test_vm_allocator_base F|A-core|A-wire|B-core|B-wire|C-linear|C-index");
        const std::string_view variant = argv[1];
        require(variant == "F" || variant == "A-core" || variant == "A-wire" || variant == "B-core" ||
                    variant == "B-wire" || variant == "C-linear" || variant == "C-index", "unknown candidate");
#ifdef _WIN32
        SYSTEM_INFO info;
        GetSystemInfo(&info);
        const auto page = static_cast<std::size_t>(info.dwPageSize);
#else
        const auto page = static_cast<std::size_t>(sysconf(_SC_PAGESIZE));
#endif
        require(page && !(page & (page - 1)) && page <= XBufferCore::MAX_PAYLOAD / 65,
                "invalid runtime page size");
        const auto super = 2 * page;
        const auto extent = 65 * page; // Odd page count is intentional.
        std::vector<std::unique_ptr<XBufferCore>> owners;
        owners.reserve(32);
        XBufferCore* selected = nullptr;
        for (std::size_t attempt = 0; attempt < 32; ++attempt) {
            owners.push_back(std::make_unique<XBufferCore>(extent, XBufferCore::maximum_capacity(extent)));
            auto* candidate = owners.back().get();
            if (reinterpret_cast<std::uintptr_t>(candidate->data()) % super != 0) {
                selected = candidate;
                break;
            }
        }
        if (!selected) {
            std::cout << "{\"status\":\"precondition_unavailable\",\"variant\":\"" << variant
                      << "\",\"page_bytes\":" << page << ",\"base_mod_2page\":0,\"retained_owners\":"
                      << owners.size() << "}\n";
            return 2;
        }
        auto& b = *selected;
        const auto* original_base = b.get_address();
        require(reinterpret_cast<std::uintptr_t>(original_base) % super == page,
                "selected mapping does not have page-only base alignment");
        std::vector<Live> live;
        std::vector<Gap> gaps;
        live.reserve(32);
        gaps.reserve(16);
        allocator_model::Coverage coverage;
        const std::array<std::size_t, 7> alignments = {1, 2, 8, 16, 64, page, super};
        for (const auto alignment : alignments) {
            const auto donor = add_live(b, live, gaps, 2 * alignment + 257, alignment, 0x41);
            observe(b, live, gaps, coverage, original_base);
            emit(variant, "allocate", b, live[donor], page);
            add_live(b, live, gaps, 64, 16, 0x52);
            observe(b, live, gaps, coverage, original_base);
            b.arena()->deallocate(live[donor].ptr);
            live[donor].ptr = nullptr;
            observe(b, live, gaps, coverage, original_base);
            const auto available = free_intervals(b);
            const auto previous_end = b.arena()->used_end;
            const auto reused = add_live(b, live, gaps, 33, alignment, 0x63);
            const auto h = header(live[reused].ptr);
            const auto begin = h.user_offset - sizeof(H);
            const auto end = h.user_offset + std::uint64_t(h.user_bytes);
            bool contained = false;
            for (const auto range : available)
                contained = contained || (range.begin <= begin && range.end >= end);
            require(contained && b.arena()->used_end == previous_end,
                    "eligible reuse was not contained in a known free physical donor");
            observe(b, live, gaps, coverage, original_base);
            emit(variant, "reuse", b, live[reused], page);
        }
        // This request exceeds every earlier residual, forcing a fresh tail.
        // Its subsequent reuse still permits each candidate's own donor policy.
        const auto tail_donor = add_live(b, live, gaps, 8 * page + 4096, super, 0x74);
        observe(b, live, gaps, coverage, original_base);
        b.arena()->deallocate(live[tail_donor].ptr);
        live[tail_donor].ptr = nullptr;
        observe(b, live, gaps, coverage, original_base);
        const auto tail = add_live(b, live, gaps, 4 * page + 2048, super, 0x85);
        observe(b, live, gaps, coverage, original_base);
        emit(variant, "tail_reuse", b, live[tail], page);
        const auto h = header(live[tail].ptr);
        require(h.user_offset + std::uint64_t(h.user_bytes) == b.arena()->used_end,
                "reused superpage allocation lost tail identity");
        require(b.arena()->try_expand(live[tail].ptr, h.user_bytes + static_cast<std::uint32_t>(page), super),
                "superpage-aligned physical tail could not expand");
        const auto expanded = header(live[tail].ptr).user_bytes;
        std::memset(static_cast<char*>(live[tail].ptr) + live[tail].size, live[tail].value,
                    expanded - live[tail].size);
        live[tail].size = expanded;
        observe(b, live, gaps, coverage, original_base);
        emit(variant, "expand", b, live[tail], page);
        std::cout << "{\"status\":\"passed\",\"variant\":\"" << variant
                  << "\",\"page_bytes\":" << page << ",\"base_mod_2page\":"
                  << reinterpret_cast<std::uintptr_t>(b.data()) % super
                  << ",\"retained_owners\":" << owners.size() << ",\"alignment_cases\":7,\"known_gaps\":"
                  << gaps.size() << "}\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
