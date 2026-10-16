"""Generate isolated Stage III Core split candidates from the frozen F header.

This module never edits the production header.  The caller owns snapshot creation
and records input/output identities.  A residual always replaces its donor's slot,
so preparing a split requires no new host allocation.
"""

from __future__ import annotations


_CORE_REUSE_LOOP = """            for (std::size_t i = 0; i < free_blocks_.size(); ++i) {
                const auto& block = free_blocks_[i];
                if (block.user_bytes >= bytes && block.alignment >= alignment) {
                    void* ptr = buffer_.data() + block.user_offset;
                    if (reinterpret_cast<std::uintptr_t>(ptr) % alignment != 0) continue;
                    free_blocks_.erase(free_blocks_.begin() + static_cast<std::ptrdiff_t>(i));
                    return ptr;
                }
            }
"""


_REPLACEMENT = """            // S3: __VARIANT__ preserves F's donor order and eligibility predicates.
            for (std::size_t i = 0; i < free_blocks_.size(); ++i) {
                const auto block = free_blocks_[i];
                if (block.user_bytes >= bytes && block.alignment >= alignment) {
                    void* ptr = buffer_.data() + block.user_offset;
                    if (reinterpret_cast<std::uintptr_t>(ptr) % alignment != 0) continue;

                    const auto donor_base = reinterpret_cast<std::uintptr_t>(buffer_.data());
                    const auto donor_address = reinterpret_cast<std::uintptr_t>(ptr);
                    const auto address_limit = (std::numeric_limits<std::uintptr_t>::max)();
                    const size_type donor_offset = block.user_offset;
                    const size_type donor_bytes = block.user_bytes;
                    // S3: only a fully checked physical interval may be divided.
                    if (donor_offset >= sizeof(AllocationHeader) &&
                        donor_offset <= buffer_.size() &&
                        donor_bytes <= buffer_.size() - donor_offset &&
                        donor_address >= donor_base && donor_address - donor_base == donor_offset &&
                        (donor_address - sizeof(AllocationHeader)) % alignof(AllocationHeader) == 0 &&
                        donor_bytes <= address_limit - donor_address &&
                        block.total_bytes == std::uint64_t(donor_bytes) + sizeof(AllocationHeader)) {
                        auto* donor_header = std::launder(reinterpret_cast<AllocationHeader*>(
                            buffer_.data() + donor_offset - sizeof(AllocationHeader)));
                        if (donor_header->user_offset == block.user_offset &&
                            donor_header->user_bytes == block.user_bytes &&
                            donor_header->total_bytes == block.total_bytes &&
                            donor_header->alignment == block.alignment) {
                            const size_type donor_end = donor_offset + donor_bytes;
                            const bool donor_is_tail = donor_end == arena()->used_end;
__TAIL_SPLIT__
                            if (!donor_is_tail) {
                                // S3: align the residual to the allocator's minimum user alignment.
                                // Any padding before its header stays in the returned front block.
                                constexpr size_type residual_alignment = alignof(std::max_align_t);
                                if (bytes <= address_limit - donor_address &&
                                    sizeof(AllocationHeader) <= address_limit - donor_address - bytes) {
                                    const auto first_residual_address =
                                        donor_address + bytes + sizeof(AllocationHeader);
                                    if (residual_alignment - 1 <= address_limit - first_residual_address) {
                                        const auto residual_address = align_up(
                                            first_residual_address, residual_alignment);
                                        const size_type residual_offset = residual_address - donor_base;
                                        if (residual_address >= first_residual_address &&
                                            residual_address >= donor_base &&
                                            residual_offset < donor_end && residual_offset <= MAX_PAYLOAD &&
                                            (residual_address - sizeof(AllocationHeader)) %
                                                alignof(AllocationHeader) == 0) {
                                            const size_type front_bytes =
                                                residual_offset - sizeof(AllocationHeader) - donor_offset;
                                            const size_type residual_bytes = donor_end - residual_offset;
                                            // The Core free record is hosted outside the arena; one
                                            // payload byte suffices for its next allocation.
                                            if (front_bytes >= bytes && residual_bytes >= 1 &&
                                                front_bytes <= MAX_PAYLOAD && residual_bytes <= MAX_PAYLOAD) {
                                                const AllocationHeader front_header{
                                                    static_cast<std::uint32_t>(sizeof(AllocationHeader) + front_bytes),
                                                    static_cast<std::uint32_t>(front_bytes), block.user_offset,
                                                    block.alignment};
                                                const AllocationHeader residual_header{
                                                    static_cast<std::uint32_t>(sizeof(AllocationHeader) + residual_bytes),
                                                    static_cast<std::uint32_t>(residual_bytes),
                                                    static_cast<std::uint32_t>(residual_offset),
                                                    static_cast<std::uint32_t>(residual_alignment)};
                                                const FreeBlock residual{
                                                    residual_header.user_offset, residual_header.total_bytes,
                                                    residual_header.user_bytes, residual_header.alignment};
                                                auto* residual_header_address = std::launder(
                                                    reinterpret_cast<AllocationHeader*>(buffer_.data() +
                                                        residual_offset - sizeof(AllocationHeader)));
                                                // S3: no-throw publication after all arithmetic and
                                                // metadata preparation; retain the original slot.
                                                *residual_header_address = residual_header;
                                                *donor_header = front_header;
                                                free_blocks_[i] = residual;
                                                return ptr;
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                    // S3: preserve F's whole-donor fallback, including tiny remnants.
                    free_blocks_.erase(free_blocks_.begin() + static_cast<std::ptrdiff_t>(i));
                    return ptr;
                }
            }
"""


_B_TAIL_SPLIT = """                            if (donor_is_tail) {
                                // S3: the tail allocation includes every byte through donor_end.
                                // Its alignment padding belongs to that allocation, never the prefix.
                                const auto donor_end_address = donor_address + donor_bytes;
                                const auto tail_address = (donor_end_address - bytes) &
                                    ~(static_cast<std::uintptr_t>(alignment) - 1u);
                                if (tail_address >= donor_address && tail_address >= donor_base &&
                                    tail_address - donor_address >= sizeof(AllocationHeader) + 1 &&
                                    (tail_address - sizeof(AllocationHeader)) % alignof(AllocationHeader) == 0) {
                                    const size_type tail_offset = tail_address - donor_base;
                                    const size_type prefix_bytes =
                                        tail_offset - sizeof(AllocationHeader) - donor_offset;
                                    const size_type tail_bytes = donor_end - tail_offset;
                                    if (tail_offset <= MAX_PAYLOAD && prefix_bytes >= 1 &&
                                        prefix_bytes <= MAX_PAYLOAD && tail_bytes >= bytes &&
                                        tail_bytes <= MAX_PAYLOAD) {
                                        const AllocationHeader prefix_header{
                                            static_cast<std::uint32_t>(sizeof(AllocationHeader) + prefix_bytes),
                                            static_cast<std::uint32_t>(prefix_bytes), block.user_offset,
                                            block.alignment};
                                        const AllocationHeader tail_header{
                                            static_cast<std::uint32_t>(sizeof(AllocationHeader) + tail_bytes),
                                            static_cast<std::uint32_t>(tail_bytes),
                                            static_cast<std::uint32_t>(tail_offset),
                                            static_cast<std::uint32_t>(alignment)};
                                        const FreeBlock prefix{
                                            prefix_header.user_offset, prefix_header.total_bytes,
                                            prefix_header.user_bytes, prefix_header.alignment};
                                        auto* tail_header_address = std::launder(
                                            reinterpret_cast<AllocationHeader*>(buffer_.data() +
                                                tail_offset - sizeof(AllocationHeader)));
                                        // S3: all preparation precedes the no-throw publication.
                                        *tail_header_address = tail_header;
                                        *donor_header = prefix_header;
                                        free_blocks_[i] = prefix;
                                        return buffer_.data() + tail_offset;
                                    }
                                }
                            }
"""


def transform(source: str, variant: str) -> str:
    """Return A-core or B-core source, rejecting a drifted or reused input.

    A splits eligible non-tail donors and retains tail donors whole.  B uses A's
    non-tail rule and allocates a tail donor from its aligned high-address end.
    Both retain a residual at the donor's original free-list position.
    """
    if variant not in ("A-core", "B-core"):
        raise ValueError(f"unsupported Core split candidate: {variant!r}")
    matches = source.count(_CORE_REUSE_LOOP)
    if matches != 1:
        raise ValueError(f"Core F reuse-loop anchor count is {matches}, expected 1")
    replacement = _REPLACEMENT.replace("__VARIANT__", variant)
    replacement = replacement.replace("__TAIL_SPLIT__", _B_TAIL_SPLIT.rstrip() if variant == "B-core" else "")
    if "__VARIANT__" in replacement or "__TAIL_SPLIT__" in replacement:
        raise RuntimeError("unresolved Core generator placeholder")
    return source.replace(_CORE_REUSE_LOOP, replacement, 1)
