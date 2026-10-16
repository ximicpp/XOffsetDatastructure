// Candidate policy checks are separate from F's exact whole-donor assertions.
// This executable has no diagnostic dependency and remains active with NDEBUG.
#include <array>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#ifndef _WIN32
#include <unistd.h>
#endif
#include "../xoffsetdatastructure.hpp"

using namespace XOffsetDatastructure;
namespace {
using H = XBufferCore::AllocationHeader;
enum class Variant { F, A, B };
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
H header(const void* p) {
    H result;
    std::memcpy(&result, static_cast<const char*>(p) - sizeof(H), sizeof(H));
    return result;
}
std::size_t offset(XBufferCore& b, const void* p) {
    return reinterpret_cast<std::uintptr_t>(p) - reinterpret_cast<std::uintptr_t>(b.data());
}
void valid_header(XBufferCore& b, const void* p, std::size_t required) {
    const auto h = header(p);
    require(h.user_offset == offset(b, p), "header/user offset differs");
    require(h.user_bytes >= required && h.total_bytes == std::uint64_t(h.user_bytes) + sizeof(H),
            "header physical capacity differs");
    require(h.alignment && !(h.alignment & (h.alignment - 1)) &&
                reinterpret_cast<std::uintptr_t>(p) % h.alignment == 0,
            "stored alignment differs from actual address");
    require((reinterpret_cast<std::uintptr_t>(p) - sizeof(H)) % alignof(H) == 0,
            "header address is misaligned");
    require(h.user_offset + std::uint64_t(h.user_bytes) <= b.arena()->used_end,
            "header reaches beyond used end");
}
void bytes_equal(const void* p, std::size_t n, unsigned char value, const char* message) {
    for (std::size_t i = 0; i < n; ++i)
        require(static_cast<const unsigned char*>(p)[i] == value, message);
}

// With an aligned request and donor, D = R + H leaves no residual user byte;
// D = R + H + 1 permits exactly one. The unaligned request also checks padding.
void non_tail_boundary(Variant variant, std::uint32_t request, std::uint32_t donor_size) {
    XBufferCore b(16384, XBufferCore::maximum_capacity(16384));
    auto* donor = b.arena()->allocate_aligned(donor_size, alignof(std::max_align_t));
    auto* guard = b.arena()->allocate_aligned(96, 64);
    std::memset(guard, 0x3a, 96);
    const auto original = header(donor);
    const auto original_end = original.user_offset + std::uint64_t(original.user_bytes);
    const auto used = b.arena()->used_end;
    const auto* base = b.get_address();
    b.arena()->deallocate(donor);
    auto* front = b.arena()->allocate_aligned(request, alignof(std::max_align_t));
    require(front == donor, "non-tail front moved from original donor address");
    valid_header(b, front, request);
    const auto effective = alignof(std::max_align_t);
    const auto raw_next = reinterpret_cast<std::uintptr_t>(front) + request + sizeof(H);
    const auto next_address = (raw_next + effective - 1) & ~(std::uintptr_t(effective) - 1);
    const auto next_offset = next_address - reinterpret_cast<std::uintptr_t>(b.data());
    const bool split = variant != Variant::F && next_offset < original_end;
    if (split) {
        const auto f = header(front);
        require(f.user_bytes == next_offset - sizeof(H) - original.user_offset,
                "front failed to own alignment padding");
        require(f.alignment == original.alignment, "front recorded alignment changed");
        const auto r = header(reinterpret_cast<const void*>(next_address));
        require(r.user_offset == next_offset && r.user_bytes == original_end - next_offset &&
                    r.total_bytes == sizeof(H) + r.user_bytes && r.alignment == effective,
                "residual physical header differs");
        require(std::uint64_t(f.total_bytes) + r.total_bytes == original.total_bytes,
                "split lost or invented physical bytes");
        auto* residual = b.arena()->allocate_aligned(1, effective);
        require(reinterpret_cast<std::uintptr_t>(residual) == next_address,
                "residual was not reusable from donor slot");
        valid_header(b, residual, 1);
        std::memset(front, 0x51, request);
        std::memset(residual, 0x62, 1);
        bytes_equal(front, request, 0x51, "residual overlaps front user bytes");
        b.arena()->deallocate(residual);
    } else {
        require(header(front).user_bytes == original.user_bytes,
                "whole-donor fallback shortened physical capacity");
    }
    require(b.arena()->used_end == used && b.get_address() == base,
            "non-tail reuse grew arena or changed owner base");
    bytes_equal(guard, 96, 0x3a, "split changed following live allocation");
    b.arena()->deallocate(front);
}

void stored_alignment_remains_a_donor_predicate() {
    constexpr std::size_t minimum = alignof(std::max_align_t);
    constexpr std::size_t stronger = 4 * minimum;
    XBufferCore b(16384, XBufferCore::maximum_capacity(16384));
    // Force the next donor user address to satisfy stronger while recording only minimum.
    b.arena()->allocate_aligned(static_cast<std::uint32_t>(stronger - sizeof(H)), stronger);
    auto* donor = b.arena()->allocate_aligned(256, minimum);
    require(reinterpret_cast<std::uintptr_t>(donor) % stronger == 0,
            "alignment counterexample setup failed");
    require(header(donor).alignment == minimum, "donor stored-alignment setup failed");
    auto* guard = b.arena()->allocate_aligned(64, minimum);
    std::memset(guard, 0x73, 64);
    b.arena()->deallocate(donor);
    auto* reused = b.arena()->allocate_aligned(32, stronger);
    require(reused != donor, "stored-alignment eligibility predicate was broadened");
    valid_header(b, reused, 32);
    bytes_equal(guard, 64, 0x73, "alignment mismatch path changed live bytes");
}

void tail_contract(Variant variant, bool consume_prefix) {
    XBufferCore b(16384, XBufferCore::maximum_capacity(16384));
    auto* donor = b.arena()->allocate_aligned(256, 64);
    std::memset(donor, 0x6d, 256);
    const auto original = header(donor);
    const auto end = b.arena()->used_end;
    const auto* base = b.get_address();
    b.arena()->deallocate(donor);
    auto* tail = b.arena()->allocate_aligned(32, 64);
    valid_header(b, tail, 32);
    bytes_equal(tail, 32, 0x6d, "tail reuse changed returned user bytes");
    if (variant == Variant::B) {
        const auto expected = (reinterpret_cast<std::uintptr_t>(b.data()) + end - 32) & ~std::uintptr_t(63);
        require(reinterpret_cast<std::uintptr_t>(tail) == expected && tail != donor,
                "B did not allocate the high-address tail");
        const auto prefix = header(donor);
        require(prefix.user_offset == original.user_offset && prefix.user_bytes >= 1 &&
                    prefix.alignment == original.alignment,
                "B prefix is not a legal independent Core block");
        require(std::uint64_t(prefix.total_bytes) + header(tail).total_bytes == original.total_bytes,
                "B tail split lost or invented physical bytes");
    } else {
        require(tail == donor && header(tail).user_bytes == original.user_bytes,
                "F/A failed to retain a whole tail donor");
    }
    require(header(tail).user_offset + std::uint64_t(header(tail).user_bytes) == end,
            "reused allocation lost physical-tail identity");
    std::memset(tail, 0x84, 32);
    void* prefix_user = nullptr;
    if (consume_prefix) {
        prefix_user = b.arena()->allocate_aligned(64, alignof(std::max_align_t));
        valid_header(b, prefix_user, 64);
        if (variant == Variant::B) {
            require(prefix_user == donor && b.arena()->used_end == end,
                    "B prefix consumption appended beyond live tail");
            require(offset(b, prefix_user) + std::uint64_t(header(prefix_user).user_bytes) <=
                        offset(b, tail) - sizeof(H), "consumed prefix overlaps tail header");
        }
        std::memset(prefix_user, 0x95, 64);
    }
    const auto physical = header(tail).user_bytes;
    const bool expanded = b.arena()->try_expand(tail, physical + 128, 64);
    require(expanded == (!consume_prefix || variant == Variant::B),
            "tail growth result differs after prefix consumption");
    if (expanded)
        require(header(tail).user_bytes == physical + 128 && b.arena()->used_end == end + 128,
                "tail growth did not preserve physical end");
    bytes_equal(tail, 32, 0x84, "tail growth changed existing data");
    if (prefix_user) bytes_equal(prefix_user, 64, 0x95, "tail growth changed live prefix data");
    require(b.get_address() == base, "tail growth changed owner base");
}

void superpage_gap(Variant variant) {
#ifdef _WIN32
    SYSTEM_INFO info;
    GetSystemInfo(&info);
    const auto page = static_cast<std::size_t>(info.dwPageSize);
#else
    const auto page = static_cast<std::size_t>(sysconf(_SC_PAGESIZE));
#endif
    require(page && !(page & (page - 1)), "unexpected page size");
    const auto alignment = 2 * page;
    const auto extent = 8 * alignment + 65536;
    XBufferCore b(extent, XBufferCore::maximum_capacity(extent));
    auto* lead = b.arena()->allocate_aligned(19, 16);
    std::memset(lead, 0xa6, 19);
    const auto gap_begin = b.arena()->used_end;
    auto* donor = b.arena()->allocate_aligned(static_cast<std::uint32_t>(2 * alignment + 256), alignment);
    const auto original = header(donor);
    const auto gap_end = original.user_offset - sizeof(H);
    require(gap_begin <= gap_end, "negative unknown-gap setup");
    std::memset(b.data() + gap_begin, 0xb7, gap_end - gap_begin);
    auto* guard = b.arena()->allocate_aligned(67, 16);
    std::memset(guard, 0xc8, 67);
    b.arena()->deallocate(donor);
    auto* front = b.arena()->allocate_aligned(37, alignment);
    require(front == donor, "non-tail superpage donor address changed");
    valid_header(b, front, 37);
    if (variant != Variant::F) {
        require(header(front).user_bytes < original.user_bytes,
                "superpage non-tail split unexpectedly fell back");
    }
    bytes_equal(b.data() + gap_begin, gap_end - gap_begin, 0xb7,
                "split swallowed or changed unknown pre-header alignment gap");
    bytes_equal(lead, 19, 0xa6, "superpage split changed leading live block");
    bytes_equal(guard, 67, 0xc8, "superpage split changed following live block");
}

template <class Operation>
void rejection_preserves_state(XBufferCore& b, Operation operation) {
    const auto before = b.copy_bytes();
    const auto free = b.get_free_memory();
    const auto used = b.arena()->used_end;
    const auto* base = b.get_address();
    operation();
    require(b.copy_bytes() == before && b.get_free_memory() == free &&
                b.arena()->used_end == used && b.get_address() == base,
            "rejected near-integer-limit request changed allocator state");
}
void near_integer_limits() {
    XBufferCore b(4096, XBufferCore::maximum_capacity(4096));
    auto* live = b.arena()->allocate_aligned(64, 16);
    std::memset(live, 0xd9, 64);
    auto rejected_allocate = [&](std::uint32_t size, std::size_t alignment) {
        rejection_preserves_state(b, [&] {
            bool rejected = false;
            try { b.arena()->allocate_aligned(size, alignment); }
            catch (const XBadAlloc&) { rejected = true; }
            require(rejected, "invalid or oversized allocate unexpectedly succeeded");
        });
    };
    rejected_allocate((std::numeric_limits<std::uint32_t>::max)(), 16);
    rejected_allocate(static_cast<std::uint32_t>(XBufferCore::MAX_PAYLOAD), 16);
    for (auto alignment : {std::size_t(0), std::size_t(3),
                           XBufferCore::MAX_PAYLOAD + 1,
                           (std::numeric_limits<std::size_t>::max)()})
        rejected_allocate(32, alignment);
    rejection_preserves_state(b, [&] {
        require(!b.arena()->try_expand(live, (std::numeric_limits<std::uint32_t>::max)(), 16),
                "integer-limit expand unexpectedly succeeded");
        require(!b.grow((std::numeric_limits<std::size_t>::max)()),
                "integer-limit grow unexpectedly succeeded");
    });
    bytes_equal(live, 64, 0xd9, "rejected request changed live allocation");
}
} // namespace

int main(int argc, char** argv) {
    try {
        require(argc == 2, "usage: test_vm_allocator_core F|A-core|B-core");
        const std::string_view name = argv[1];
        require(name == "F" || name == "A-core" || name == "B-core", "unknown Core test variant");
        const auto variant = name == "F" ? Variant::F : (name == "A-core" ? Variant::A : Variant::B);
        const auto request = static_cast<std::uint32_t>(2 * alignof(std::max_align_t));
        for (auto delta : {15u, 16u, 17u, 32u, 33u}) non_tail_boundary(variant, request, request + delta);
        non_tail_boundary(variant, 1, 33);
        stored_alignment_remains_a_donor_predicate();
        tail_contract(variant, false);
        tail_contract(variant, true);
        superpage_gap(variant);
        near_integer_limits();
        std::cout << "{\"status\":\"passed\",\"variant\":\"" << name
                  << "\",\"boundary_cases\":6,\"tail_cases\":2,\"near_integer_limits\":true}\n";
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
