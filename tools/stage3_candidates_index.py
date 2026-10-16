"""Generate isolated N6 Core lookup candidates from the frozen F header.

``transform(source, variant)`` accepts only C-linear and C-index.  Every source
anchor must occur exactly once; input drift is an error.  No production file is
written by importing or calling this module.

Both candidates retain whole-block reuse and F's earliest eligible insertion
rule, including stored alignment and actual-address alignment.  A live FIFO
list provides insertion order; a separate hole list recycles stable *indices*,
never pointers into the growable host vector.  C-index additionally stores two
links per slot, 32 bucket head/tail pairs and a bitmap.  Size buckets are floor
log2(user_bytes), so the first bucket must still test exact size.  It visits the
oldest eligible item in every eligible bucket and compares FIFO sequence ids.
Alignment rejection can require walking an entire bucket.  Worst-case lookup
is linear, not O(1); malformed index references trigger full FIFO fallback.

The sole throwing metadata operation is growing slots_.  It precedes any list
publication, so failure preserves the old free set.  F's best-effort deallocate
catch still owns OOM handling.  Reusing holes does not allocate.  Sequence ids
are renumbered in FIFO order before uint64 overflow, without allocation.

Host cost is intentionally observable: capacity() is allocated slot capacity,
size() is active count, storage_size() includes holes, metadata_slot_bytes()
reports sizeof(Slot), and metadata_capacity_bytes() includes sizeof(container)
plus vector capacity * sizeof(Slot).  This includes fixed index state, active
and inactive slot storage and the vector object, but excludes allocator overhead
and malloc size-class slack, which require independent host instrumentation.
C-linear omits bucket links/state, so C-index/C-linear includes index storage
and update costs; both use the same FIFO/slot rules.  Capacities remain retained
after erasure, like F's vector.  Move empties the source; swap transfers all
metadata.  Pruning unlinks removed slots through the same mutation path.
"""

from __future__ import annotations

import argparse
from pathlib import Path


VARIANTS = ("C-linear", "C-index")


def _once(source: str, old: str, new: str, name: str) -> str:
    count = source.count(old)
    if count != 1:
        raise ValueError(f"{name}: expected one F source anchor, got {count}")
    return source.replace(old, new, 1)


_FREE_BLOCK = """        struct FreeBlock {
            std::uint32_t user_offset = 0;
            std::uint32_t total_bytes = 0;
            std::uint32_t user_bytes = 0;
            std::uint32_t alignment = 0;
        };
"""

_PRUNE = """            free_blocks_.erase(
                std::remove_if(
                    free_blocks_.begin(), free_blocks_.end(),
                    [&](const FreeBlock& block) {
                        return static_cast<size_type>(block.user_offset + block.user_bytes) > used;
                    }),
                free_blocks_.end());"""

_LOOKUP = """            for (std::size_t i = 0; i < free_blocks_.size(); ++i) {
                const auto& block = free_blocks_[i];
                if (block.user_bytes >= bytes && block.alignment >= alignment) {
                    void* ptr = buffer_.data() + block.user_offset;
                    if (reinterpret_cast<std::uintptr_t>(ptr) % alignment != 0) continue;
                    free_blocks_.erase(free_blocks_.begin() + static_cast<std::ptrdiff_t>(i));
                    return ptr;
                }
            }"""

_HELPER = r"""
        // Stage 3 isolated candidate: host-only state, absent from saved wire.
        struct Stage3NoIndex {};
        struct Stage3IndexLinks {
            std::size_t bucket_previous = (std::numeric_limits<std::size_t>::max)();
            std::size_t bucket_next = (std::numeric_limits<std::size_t>::max)();
        };
        struct Stage3IndexState {
            std::size_t bucket_heads[32];
            std::size_t bucket_tails[32];
            std::uint32_t nonempty = 0;
            Stage3IndexState() noexcept {
                for (unsigned b = 0; b != 32; ++b) {
                    bucket_heads[b] = bucket_tails[b] =
                        (std::numeric_limits<std::size_t>::max)();
                }
            }
        };

        template <bool Indexed>
        class Stage3FreeSlots : private std::conditional_t<Indexed, Stage3IndexState, Stage3NoIndex> {
        public:
            using index_type = std::size_t;
            static constexpr index_type npos = (std::numeric_limits<index_type>::max)();

        private:
            struct Slot : std::conditional_t<Indexed, Stage3IndexLinks, Stage3NoIndex> {
                FreeBlock block{};
                index_type previous = npos;
                index_type next = npos;
                index_type next_free = npos;
                std::uint64_t sequence = 0;
                bool active = false;
            };
            static_assert(sizeof(index_type) <= sizeof(std::uint64_t));
            std::vector<Slot> slots_;
            index_type head_ = npos;
            index_type tail_ = npos;
            index_type free_head_ = npos;
            index_type active_count_ = 0;
            std::uint64_t next_sequence_ = 1;

            static unsigned bucket_for(std::uint32_t bytes) noexcept {
                unsigned result = 0;
                while (bytes >>= 1u) ++result;
                return result;
            }

            bool qualifies(const Slot& slot, std::uint32_t bytes,
                           std::size_t alignment, const char* base) const noexcept {
                return slot.block.user_bytes >= bytes && slot.block.alignment >= alignment &&
                    reinterpret_cast<std::uintptr_t>(base + slot.block.user_offset) % alignment == 0;
            }

            index_type linear_find(std::uint32_t bytes, std::size_t alignment,
                                   const char* base) const noexcept {
                for (auto i = head_; i != npos; i = slots_[i].next) {
                    if (qualifies(slots_[i], bytes, alignment, base)) return i;
                }
                return npos;
            }

            void prepare_sequence() {
                if (next_sequence_ != (std::numeric_limits<std::uint64_t>::max)()) return;
                if (active_count_ >= (std::numeric_limits<std::uint64_t>::max)() - 1u)
                    throw std::bad_alloc();
                std::uint64_t sequence = 1;
                for (auto i = head_; i != npos; i = slots_[i].next)
                    slots_[i].sequence = sequence++;
                next_sequence_ = sequence;
            }

            void link_bucket(index_type index) noexcept {
                if constexpr (Indexed) {
                    auto& slot = slots_[index];
                    const auto bucket = bucket_for(slot.block.user_bytes);
                    slot.bucket_previous = this->bucket_tails[bucket];
                    slot.bucket_next = npos;
                    if (slot.bucket_previous != npos)
                        slots_[slot.bucket_previous].bucket_next = index;
                    else
                        this->bucket_heads[bucket] = index;
                    this->bucket_tails[bucket] = index;
                    this->nonempty |= std::uint32_t(1) << bucket;
                }
            }

            void unlink_bucket(index_type index) noexcept {
                if constexpr (Indexed) {
                    auto& slot = slots_[index];
                    const auto bucket = bucket_for(slot.block.user_bytes);
                    if (slot.bucket_previous != npos)
                        slots_[slot.bucket_previous].bucket_next = slot.bucket_next;
                    else
                        this->bucket_heads[bucket] = slot.bucket_next;
                    if (slot.bucket_next != npos)
                        slots_[slot.bucket_next].bucket_previous = slot.bucket_previous;
                    else
                        this->bucket_tails[bucket] = slot.bucket_previous;
                    if (this->bucket_heads[bucket] == npos)
                        this->nonempty &= ~(std::uint32_t(1) << bucket);
                    slot.bucket_previous = slot.bucket_next = npos;
                }
            }

        public:
            Stage3FreeSlots() = default;
            Stage3FreeSlots(const Stage3FreeSlots&) = delete;
            Stage3FreeSlots& operator=(const Stage3FreeSlots&) = delete;
            Stage3FreeSlots(Stage3FreeSlots&& other) noexcept { swap(other); }
            Stage3FreeSlots& operator=(Stage3FreeSlots&& other) noexcept {
                if (this != &other) {
                    Stage3FreeSlots old;
                    swap(old);
                    swap(other);
                }
                return *this;
            }

            void swap(Stage3FreeSlots& other) noexcept {
                slots_.swap(other.slots_);
                std::swap(head_, other.head_);
                std::swap(tail_, other.tail_);
                std::swap(free_head_, other.free_head_);
                std::swap(active_count_, other.active_count_);
                std::swap(next_sequence_, other.next_sequence_);
                if constexpr (Indexed) {
                    for (unsigned b = 0; b != 32; ++b) {
                        std::swap(this->bucket_heads[b], other.bucket_heads[b]);
                        std::swap(this->bucket_tails[b], other.bucket_tails[b]);
                    }
                    std::swap(this->nonempty, other.nonempty);
                }
            }

            std::size_t size() const noexcept { return active_count_; }
            std::size_t capacity() const noexcept { return slots_.capacity(); }
            std::size_t storage_size() const noexcept { return slots_.size(); }
            static constexpr std::size_t metadata_slot_bytes() noexcept { return sizeof(Slot); }
            std::size_t metadata_capacity_bytes() const noexcept {
                return sizeof(*this) + slots_.capacity() * sizeof(Slot);
            }
            const FreeBlock& get(index_type index) const noexcept { return slots_[index].block; }

            class const_iterator {
                const Stage3FreeSlots* owner_;
                index_type index_;
            public:
                const_iterator(const Stage3FreeSlots* owner, index_type index) noexcept
                    : owner_(owner), index_(index) {}
                const FreeBlock& operator*() const noexcept { return owner_->slots_[index_].block; }
                const_iterator& operator++() noexcept {
                    index_ = owner_->slots_[index_].next;
                    return *this;
                }
                bool operator==(const const_iterator& other) const noexcept {
                    return owner_ == other.owner_ && index_ == other.index_;
                }
            };
            const_iterator begin() const noexcept { return {this, head_}; }
            const_iterator end() const noexcept { return {this, npos}; }

            void push_back(const FreeBlock& block) {
                // Sequence normalization changes no donor order and never allocates.
                prepare_sequence();
                if (active_count_ == (std::numeric_limits<index_type>::max)())
                    throw std::bad_alloc();
                index_type index = free_head_;
                if (index == npos) {
                    if (slots_.size() == slots_.max_size()) throw std::bad_alloc();
                    index = slots_.size();
                    // Publication starts only after this sole throwing operation.
                    slots_.emplace_back();
                } else {
                    free_head_ = slots_[index].next_free;
                }
                auto& slot = slots_[index];
                slot.block = block;
                slot.previous = tail_;
                slot.next = npos;
                slot.next_free = npos;
                slot.sequence = next_sequence_++;
                slot.active = true;
                if (tail_ != npos) slots_[tail_].next = index;
                else head_ = index;
                tail_ = index;
                ++active_count_;
                link_bucket(index);
            }

            void erase_slot(index_type index) noexcept {
                auto& slot = slots_[index];
                unlink_bucket(index);
                if (slot.previous != npos) slots_[slot.previous].next = slot.next;
                else head_ = slot.next;
                if (slot.next != npos) slots_[slot.next].previous = slot.previous;
                else tail_ = slot.previous;
                slot.active = false;
                slot.previous = slot.next = npos;
                slot.next_free = free_head_;
                free_head_ = index;
                --active_count_;
            }

            template <typename Predicate>
            void erase_if(Predicate predicate) noexcept {
                for (auto i = head_; i != npos;) {
                    const auto next = slots_[i].next;
                    if (predicate(slots_[i].block)) erase_slot(i);
                    i = next;
                }
            }

            index_type find_first(std::uint32_t bytes, std::size_t alignment,
                                  const char* base) const noexcept {
                if constexpr (!Indexed) {
                    return linear_find(bytes, alignment, base);
                } else {
                    index_type selected = npos;
                    const auto first_bucket = bucket_for(bytes);
                    const auto eligible = this->nonempty & (~std::uint32_t(0) << first_bucket);
                    for (unsigned b = first_bucket; b != 32; ++b) {
                        if ((eligible & (std::uint32_t(1) << b)) == 0) continue;
                        auto i = this->bucket_heads[b];
                        index_type visited = 0;
                        std::uint64_t previous_sequence = 0;
                        while (i != npos) {
                            // A bad index can never turn into an early search failure.
                            if (i >= slots_.size() || !slots_[i].active ||
                                bucket_for(slots_[i].block.user_bytes) != b ||
                                slots_[i].sequence <= previous_sequence || ++visited > active_count_)
                                return linear_find(bytes, alignment, base);
                            const auto& slot = slots_[i];
                            previous_sequence = slot.sequence;
                            if (selected != npos && slot.sequence >= slots_[selected].sequence) break;
                            if (qualifies(slot, bytes, alignment, base)) {
                                selected = i;
                                break;
                            }
                            i = slot.bucket_next;
                        }
                    }
                    return selected;
                }
            }

            // Diagnostic-only caller use; not part of the allocation hot path.
            bool invariants_hold() const noexcept {
                index_type count = 0;
                index_type previous = npos;
                std::uint64_t previous_sequence = 0;
                for (auto i = head_; i != npos; i = slots_[i].next) {
                    if (i >= slots_.size() || !slots_[i].active || ++count > active_count_ ||
                        slots_[i].previous != previous || slots_[i].sequence <= previous_sequence)
                        return false;
                    previous = i;
                    previous_sequence = slots_[i].sequence;
                }
                if (count != active_count_ || previous != tail_ ||
                    (count != 0 && previous_sequence >= next_sequence_)) return false;
                count = 0;
                for (auto i = free_head_; i != npos; i = slots_[i].next_free) {
                    if (i >= slots_.size() || slots_[i].active || ++count > slots_.size())
                        return false;
                }
                if (count != slots_.size() - active_count_) return false;
                if constexpr (Indexed) {
                    index_type indexed_count = 0;
                    std::uint32_t bitmap = 0;
                    for (unsigned b = 0; b != 32; ++b) {
                        previous = npos;
                        previous_sequence = 0;
                        for (auto i = this->bucket_heads[b]; i != npos; i = slots_[i].bucket_next) {
                            if (i >= slots_.size() || !slots_[i].active ||
                                ++indexed_count > active_count_ || slots_[i].bucket_previous != previous ||
                                slots_[i].sequence <= previous_sequence || bucket_for(slots_[i].block.user_bytes) != b)
                                return false;
                            previous = i;
                            previous_sequence = slots_[i].sequence;
                        }
                        if (previous != this->bucket_tails[b]) return false;
                        if (previous != npos) bitmap |= std::uint32_t(1) << b;
                    }
                    if (indexed_count != active_count_ || bitmap != this->nonempty) return false;
                }
                return true;
            }
        };
"""


def transform(source: str, variant: str) -> str:
    """Return a generated header; reject unknown variants and source drift."""
    if variant not in VARIANTS:
        raise ValueError(f"unknown Stage 3 index candidate: {variant}")
    source = _once(source, _FREE_BLOCK, _FREE_BLOCK + _HELPER, "free-block definition")
    source = _once(source, _PRUNE, """            free_blocks_.erase_if([&](const FreeBlock& block) {
                return static_cast<size_type>(block.user_offset + block.user_bytes) > used;
            });""", "prune")
    source = _once(source, _LOOKUP, """            const auto donor = free_blocks_.find_first(bytes, alignment, buffer_.data());
            if (donor != decltype(free_blocks_)::npos) {
                void* ptr = buffer_.data() + free_blocks_.get(donor).user_offset;
                free_blocks_.erase_slot(donor);
                return ptr;
            }""", "first-fit lookup")
    source = _once(source, "        std::vector<FreeBlock> free_blocks_;",
                   f"        Stage3FreeSlots<{'true' if variant == 'C-index' else 'false'}> free_blocks_;",
                   "free-list storage")
    return source


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("variant", choices=VARIANTS)
    parser.add_argument("input", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    args.output.write_text(transform(args.input.read_text(), args.variant))


if __name__ == "__main__":
    main()
