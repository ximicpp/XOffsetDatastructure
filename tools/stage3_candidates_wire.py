#!/usr/bin/env python3
"""Generate isolated A-wire/B-wire candidates from the frozen F header.

Only wire allocation changes. Core remains F. Both candidates retain F's
size-class/list donor order and its advertised-size/actual-address predicate.
A keeps physical tail donors whole; B carves their high end. Residuals are
inserted at their size-class head. Every split produces two valid Core headers.
The residual advertisement never includes hidden physical capacity.
"""

from __future__ import annotations

import argparse
from pathlib import Path


_FUNCTION_ANCHOR = """        inline void* allocate_dynamic_bytes(const allocator_ref& alloc_ref,
                                            std::uint32_t bytes,
                                            std::size_t alignment) {"""

_REUSE_ANCHOR = """                            if (block->block_bytes >= aligned_bytes &&
                                reinterpret_cast<std::uintptr_t>(block) % alignment == 0) {
                                *link = block->next_offset;
                                state->free_bytes -= block->block_bytes;
                                ++state->epoch;
                                return block;
                            }"""

_HELPER = r"""        // Stage3 isolated __VARIANT__: preserve F donor order and predicates.
        // No host allocation is needed: compute and validate every boundary
        // before publishing either header or touching the wire list.
        inline void* stage3_try_split_wire(XAllocatorStateV1* state,
                                          XArena* arena,
                                          std::uint32_t* link,
                                          FreeBlockHeader* block,
                                          std::uint32_t request,
                                          std::size_t alignment) noexcept {
            using Header = XBufferCore::AllocationHeader;
            constexpr bool high_tail = __HIGH_TAIL__;
            constexpr auto hsize = sizeof(Header);
            const auto address = reinterpret_cast<std::uintptr_t>(block);
            const auto base = reinterpret_cast<std::uintptr_t>(arena);
            const auto origin = reinterpret_cast<std::uintptr_t>(state);
            const auto limit = (std::numeric_limits<std::uintptr_t>::max)();
            const auto advertised = block->block_bytes;
            if (request == 0 || request > advertised || state->free_bytes < advertised ||
                address < base || address - base < hsize || address <= origin ||
                address - base > XBufferCore::MAX_PAYLOAD ||
                (address - hsize) % alignof(Header) != 0 ||
                address - origin > (std::numeric_limits<std::uint32_t>::max)()) return nullptr;
            Header original;
            std::memcpy(&original, reinterpret_cast<const void*>(address - hsize), hsize);
            const auto offset = address - base;
            if (original.user_offset != offset || original.user_bytes < advertised ||
                original.total_bytes != std::uint64_t(original.user_bytes) + hsize ||
                original.alignment < alignof(Header) ||
                original.alignment > XBufferCore::MAX_PAYLOAD ||
                (original.alignment & (original.alignment - 1)) != 0 ||
                address % original.alignment != 0 ||
                original.user_bytes > XBufferCore::MAX_PAYLOAD - offset ||
                std::uint64_t(offset) + original.user_bytes > arena->used_end ||
                original.user_bytes > limit - address) return nullptr;
            const auto physical_end = address + original.user_bytes;
            const bool tail = std::uint64_t(offset) + original.user_bytes == arena->used_end;
            if (tail && !high_tail) return nullptr;

            const auto split_alignment = std::max<std::size_t>(alignment, alignof(std::max_align_t));
            std::uintptr_t result_address = address;
            std::uintptr_t residual_address = 0;
            std::uint32_t front_physical = 0;
            std::uint32_t residual_physical = 0;
            std::uint32_t residual_advertised = 0;
            Header result_header = original;
            Header residual_header = original;
            if (!tail) {
                // Round the new residual user address, assigning any padding
                // before its header to the returned front allocation.
                constexpr auto residual_alignment = alignof(std::max_align_t);
                if (request > limit - address || hsize > limit - address - request)
                    return nullptr;
                const auto first = address + request + hsize;
                if (residual_alignment - 1 > limit - first) return nullptr;
                residual_address = (first + residual_alignment - 1) & ~(residual_alignment - 1);
                if (residual_address > physical_end || residual_address - address > advertised)
                    return nullptr;
                residual_physical = static_cast<std::uint32_t>(physical_end - residual_address);
                residual_advertised = advertised - static_cast<std::uint32_t>(residual_address - address);
                if (residual_advertised < sizeof(FreeBlockHeader)) return nullptr;
                front_physical = static_cast<std::uint32_t>(residual_address - hsize - address);
                result_header.user_bytes = front_physical;
                result_header.total_bytes = static_cast<std::uint32_t>(hsize + front_physical);
                // Wire's F predicate uses actual alignment, not stored alignment.
                // A split's live header must record the alignment it now serves.
                result_header.alignment = static_cast<std::uint32_t>(
                    std::max<std::size_t>(original.alignment, alignment));
                residual_header.user_offset = static_cast<std::uint32_t>(residual_address - base);
                residual_header.user_bytes = residual_physical;
                residual_header.total_bytes = static_cast<std::uint32_t>(hsize + residual_physical);
                residual_header.alignment = residual_alignment;
            } else {
                // The new allocation owns the physical tail, including P bytes
                // of alignment slack. Charge R + 16 + P to the old W budget.
                if (request > original.user_bytes) return nullptr;
                result_address = (physical_end - request) & ~(split_alignment - 1);
                if (result_address < address || result_address - address < hsize)
                    return nullptr;
                const auto padding = physical_end - result_address - request;
                if (request > advertised || hsize > advertised - request ||
                    padding > advertised - request - hsize) return nullptr;
                residual_address = address;
                residual_physical = static_cast<std::uint32_t>(result_address - hsize - address);
                const auto budget = advertised - request - static_cast<std::uint32_t>(hsize) -
                                    static_cast<std::uint32_t>(padding);
                residual_advertised = std::min(residual_physical, budget) & ~std::uint32_t(7);
                if (residual_advertised < sizeof(FreeBlockHeader)) return nullptr;
                residual_header.user_bytes = residual_physical;
                residual_header.total_bytes = static_cast<std::uint32_t>(hsize + residual_physical);
                result_header.user_offset = static_cast<std::uint32_t>(result_address - base);
                result_header.user_bytes = static_cast<std::uint32_t>(physical_end - result_address);
                result_header.total_bytes = static_cast<std::uint32_t>(hsize + result_header.user_bytes);
                result_header.alignment = static_cast<std::uint32_t>(split_alignment);
            }

            const auto residual_class = freelist_size_class(residual_advertised);
            if (residual_class >= freelist_class_count || residual_advertised % 8 != 0 ||
                residual_address <= origin ||
                residual_address - origin > (std::numeric_limits<std::uint32_t>::max)() ||
                result_address <= origin ||
                result_address - origin > (std::numeric_limits<std::uint32_t>::max)() - request ||
                residual_address - address > advertised ||
                residual_advertised > advertised - (residual_address - address)) return nullptr;
            const auto residual_offset = static_cast<std::uint32_t>(residual_address - origin);
            const auto result_end = static_cast<std::uint32_t>(result_address - origin + request);
            const auto next = block->next_offset;

            // Publish only after all checks. The previous link cannot refer to
            // either new allocation in a valid acyclic free list.
            *link = next;
            std::memcpy(reinterpret_cast<void*>(result_address - hsize), &result_header, hsize);
            std::memcpy(reinterpret_cast<void*>(residual_address - hsize), &residual_header, hsize);
            auto* residual = reinterpret_cast<FreeBlockHeader*>(residual_address);
            residual->block_bytes = residual_advertised;
            residual->next_offset = state->freelist_head[residual_class];
            state->freelist_head[residual_class] = residual_offset;
            state->free_bytes = state->free_bytes - advertised + residual_advertised;
            if (result_end > state->arena_end) state->arena_end = result_end;
            ++state->epoch;
            return reinterpret_cast<void*>(result_address);
        }

"""


def _replace_once(source: str, anchor: str, replacement: str) -> str:
    count = source.count(anchor)
    if count != 1:
        raise ValueError(f"wire candidate anchor expected exactly once, found {count}: {anchor[:100]!r}")
    return source.replace(anchor, replacement, 1)


def transform(source: str, variant: str) -> str:
    if variant not in ("A-wire", "B-wire"):
        raise ValueError(f"unsupported wire candidate: {variant}")
    if "stage3_try_split_wire" in source:
        raise ValueError("wire candidate input is already transformed")
    helper = _HELPER.replace("__VARIANT__", variant).replace("__HIGH_TAIL__", str(variant == "B-wire").lower())
    source = _replace_once(source, _FUNCTION_ANCHOR, helper + _FUNCTION_ANCHOR)
    replacement = _REUSE_ANCHOR.replace(
        "                                *link = block->next_offset;",
        "                                if (void* split = stage3_try_split_wire(\n"
        "                                        state, arena, link, block, aligned_bytes, alignment))\n"
        "                                    return split;\n"
        "                                *link = block->next_offset;",
    )
    return _replace_once(source, _REUSE_ANCHOR, replacement)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("variant", choices=("A-wire", "B-wire"))
    parser.add_argument("input", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    args.output.write_text(transform(args.input.read_text(), args.variant))


if __name__ == "__main__":
    main()
