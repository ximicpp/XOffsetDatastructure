# Diagnostic only: inserts allocator logging hooks into the B5 copy of the
# W9a header (/Volumes/XOffsetCS/diag/b5/src/xoffsetdatastructure.hpp).
# Every replacement must match exactly once; behaviour is unchanged except for
# appending events to a host-side vector while b5diag::enabled is true.
import sys

path = sys.argv[1]
src = open(path).read()

def rep(old, new, count=1):
    global src
    n = src.count(old)
    if n != count:
        sys.exit(f"pattern found {n} times (want {count}):\n{old}")
    src = src.replace(old, new)

DIAG = r'''
    namespace b5diag {
        struct Ev {
            char op;            // 'A' alloc_dynamic, 'R' retire_dynamic, 'E' expand attempt, 'G' no-growth probe failed, 'K' release_cached_blocks
            std::uint8_t src;   // A: 1 free list, 2 core ; E: 1 ok 0 fail
            std::uint32_t off;  // payload offset of user pointer
            std::uint32_t bytes;// aligned bytes as seen by the free-list layer
            std::uint32_t align;
            std::uint32_t aux;  // A: fallback old offset (0 none); G: buffer size; K: free_bytes
            std::uint32_t aux2; // buffer logical size at event
        };
        inline bool enabled = false;
        inline std::vector<Ev>* log = nullptr;
        inline const void* pending_fallback = nullptr;
        inline int dyn_depth = 0;
        inline std::uint64_t stray_core_alloc = 0, core_dealloc = 0, core_free_reuse = 0, rcb = 0, growths = 0;
        inline std::uint32_t off_of(const void* base, const void* p) {
            return static_cast<std::uint32_t>(static_cast<const char*>(p) - static_cast<const char*>(base));
        }
        inline void push(Ev e) { if (enabled && log) log->push_back(e); }
        struct DepthGuard { DepthGuard() { ++dyn_depth; } ~DepthGuard() { --dyn_depth; } };
    }

    class XBufferCore {
    public:
        using size_type = std::size_t;
'''
rep('''
    class XBufferCore {
    public:
        using size_type = std::size_t;
''', DIAG)

# Core bump allocation: count calls not issued by allocate_dynamic_bytes.
rep('''            sm->used_end = std::max<std::uint64_t>(sm->used_end, block_end);
            return payload_byte_address(user_offset);''',
'''            sm->used_end = std::max<std::uint64_t>(sm->used_end, block_end);
            if (b5diag::enabled && b5diag::dyn_depth == 0) ++b5diag::stray_core_alloc;
            return payload_byte_address(user_offset);''')
rep('''                    free_blocks_.erase(free_blocks_.begin() + static_cast<std::ptrdiff_t>(i));
                    return ptr;''',
'''                    free_blocks_.erase(free_blocks_.begin() + static_cast<std::ptrdiff_t>(i));
                    if (b5diag::enabled) ++b5diag::core_free_reuse;
                    return ptr;''')
rep('''        void deallocate_impl(void* ptr) noexcept {
            if (!ptr || buffer_.empty()) return;''',
'''        void deallocate_impl(void* ptr) noexcept {
            if (b5diag::enabled) ++b5diag::core_dealloc;
            if (!ptr || buffer_.empty()) return;''')
rep('''            if (!buffer_.grow_to(std::max(required, geometric))) throw XBadAlloc();
            return true;''',
'''            if (!buffer_.grow_to(std::max(required, geometric))) throw XBadAlloc();
            if (b5diag::enabled) ++b5diag::growths;
            return true;''')

# release_cached_blocks
rep('''            auto* arena = allocator_arena(state);
            if (!state || !arena) return;''',
'''            auto* arena = allocator_arena(state);
            if (!state || !arena) return;
            ++b5diag::rcb;
            b5diag::push({'K', 0, 0, 0, 0, state->free_bytes, 0});''')

# allocate_dynamic_bytes
rep('''                                            std::size_t alignment) {
            auto* arena = alloc_ref.segment;
            if (!arena) {''',
'''                                            std::size_t alignment) {
            b5diag::DepthGuard b5guard;
            auto* arena = alloc_ref.segment;
            if (!arena) {''')
rep('''                                std::destroy_at(block); // last header read precedes reuse
                                return static_cast<void*>(block);''',
'''                                std::destroy_at(block); // last header read precedes reuse
                                b5diag::push({'A', 1, b5diag::off_of(arena, block), aligned_bytes,
                                    static_cast<std::uint32_t>(alignment),
                                    b5diag::pending_fallback ? b5diag::off_of(arena, b5diag::pending_fallback) : 0u,
                                    static_cast<std::uint32_t>(owner->get_size())});
                                b5diag::pending_fallback = nullptr;
                                return static_cast<void*>(block);''')
rep('''            void* raw = owner->try_allocate_aligned_without_growth(aligned_bytes, alignment);
            if (!raw) {''',
'''            void* raw = owner->try_allocate_aligned_without_growth(aligned_bytes, alignment);
            if (!raw) b5diag::push({'G', 0, 0, aligned_bytes, static_cast<std::uint32_t>(alignment),
                alloc_ref.state ? alloc_ref.state->free_bytes : 0u, static_cast<std::uint32_t>(owner->get_size())});
            if (!raw) {''')
rep('''            note_allocator_block(alloc_ref.state, raw, aligned_bytes);
            return raw;
        }''',
'''            note_allocator_block(alloc_ref.state, raw, aligned_bytes);
            b5diag::push({'A', 2, b5diag::off_of(arena, raw), aligned_bytes,
                static_cast<std::uint32_t>(alignment),
                b5diag::pending_fallback ? b5diag::off_of(arena, b5diag::pending_fallback) : 0u,
                static_cast<std::uint32_t>(owner->get_size())});
            b5diag::pending_fallback = nullptr;
            return raw;
        }''')

# try_expand_dynamic_bytes: rename and wrap.
rep('''        inline bool try_expand_dynamic_bytes(const allocator_ref& alloc_ref,
                                             void* ptr, std::uint32_t bytes,
                                             std::size_t alignment) {''',
'''        inline bool try_expand_dynamic_bytes_b5impl(const allocator_ref& alloc_ref,
                                             void* ptr, std::uint32_t bytes,
                                             std::size_t alignment) {''')
rep('''        inline void retire_dynamic_bytes(const allocator_ref& alloc_ref,
                                         void* ptr, std::uint32_t bytes) noexcept {
            if (!ptr || bytes == 0) return;''',
'''        inline bool try_expand_dynamic_bytes(const allocator_ref& alloc_ref,
                                             void* ptr, std::uint32_t bytes,
                                             std::size_t alignment) {
            const bool ok = try_expand_dynamic_bytes_b5impl(alloc_ref, ptr, bytes, alignment);
            if (ptr && alloc_ref.segment && bytes && alignment && (alignment & (alignment - 1)) == 0) {
                const auto ab = round_up_to_alignment(bytes,
                    static_cast<std::uint32_t>(std::max<std::size_t>(alignment, 8u)));
                b5diag::push({'E', static_cast<std::uint8_t>(ok ? 1 : 0), b5diag::off_of(alloc_ref.segment, ptr), ab,
                    static_cast<std::uint32_t>(alignment), 0u, 0u});
                if (!ok && b5diag::enabled) b5diag::pending_fallback = ptr;
            }
            return ok;
        }

        inline void retire_dynamic_bytes(const allocator_ref& alloc_ref,
                                         void* ptr, std::uint32_t bytes) noexcept {
            if (!ptr || bytes == 0) return;
            if (alloc_ref.segment)
                b5diag::push({'R', static_cast<std::uint8_t>(alloc_ref.state ? 1 : 0),
                    b5diag::off_of(alloc_ref.segment, ptr), round_up_to_alignment(bytes, 8u), 8u, 0u, 0u});''')

open(path, 'w').write(src)
print("patched", path)
