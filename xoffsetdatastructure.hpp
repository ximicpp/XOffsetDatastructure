#ifndef X_OFFSET_DATA_STRUCTURE_HPP
#define X_OFFSET_DATA_STRUCTURE_HPP

#if defined(__LP64__) || defined(_WIN64) || (defined(__SIZEOF_POINTER__) && __SIZEOF_POINTER__ == 8)
    #define XOFFSET_ARCH_64BIT 1
#else
    #define XOFFSET_ARCH_64BIT 0
#endif

#if defined(__BYTE_ORDER__) && defined(__ORDER_LITTLE_ENDIAN__)
    #define XOFFSET_LITTLE_ENDIAN (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__)
#elif defined(_WIN32) || defined(_WIN64) || defined(__i386__) || defined(__x86_64__) || defined(_M_IX86) || defined(_M_X64)
    #define XOFFSET_LITTLE_ENDIAN 1
#else
    #define XOFFSET_LITTLE_ENDIAN 0
#endif

#ifndef XOFFSET_DISABLE_PLATFORM_CHECKS
    #if !XOFFSET_ARCH_64BIT
        #error "XOffsetDatastructure requires 64-bit architecture"
    #endif
    #if !XOFFSET_LITTLE_ENDIAN
        #error "XOffsetDatastructure requires little-endian architecture"
    #endif
#endif

#include <experimental/meta>
#include <type_traits>
#include <iostream>
#include <iomanip>
#include <string>
#include <tuple>
#include <vector>
#include <cstring>

// TypeLayout library — the authoritative type-signature engine.
// XOffset delegates ALL type safety and layout portability decisions to TypeLayout.
//   - boost::typelayout::get_layout_signature<T>()        — binary layout signature
//   - boost::typelayout::compat::classify_safety<T>()     — C2: local safety
//   - boost::typelayout::compat::is_serialization_free_local<T>() — C2 predicate
//   - TYPELAYOUT_ASSERT_COMPAT(a, b)                      — C1: cross-platform match
#include <boost/typelayout.hpp>
#include <boost/typelayout/tools/classify_safety.hpp>
#include <boost/typelayout/tools/sig_types.hpp>  // PlatformInfo
#include <boost/container/scoped_allocator.hpp>

// ============================================================================
// Target Architecture Specification
//
// XOffset Serialization-free model:
//   C1: layout_match(T, A)  — binary layout identical across platforms (CI)
//   C2: safe(T)             — no pointers, bitfields, wchar_t, etc. (compile-time)
//
// ArchSpec validates the current platform matches the target.
// TypeLayout provides the C1 (signatures) and C2 (classify_safety) engines.
// ============================================================================
namespace XOffsetDatastructure {

    /// Architecture specification — validates current platform matches target.
    struct ArchSpec {
        std::size_t pointer_size;
        bool        little_endian;
        std::size_t sizeof_int8;
        std::size_t sizeof_int16;
        std::size_t sizeof_int32;
        std::size_t sizeof_int64;
        std::size_t sizeof_float;
        std::size_t sizeof_double;
        std::size_t sizeof_bool;
        std::size_t sizeof_char;
        std::size_t pointer_align;
        std::size_t alignof_int32;
        std::size_t alignof_int64;
        std::size_t alignof_float;
        std::size_t alignof_double;

        /// Convert to TypeLayout's PlatformInfo for cross-platform comparison.
        /// Fields not present in PlatformInfo (sizeof_int8, etc.) are validated
        /// by the static_asserts below and are implicitly covered by TypeLayout's
        /// layout signature encoding.
        constexpr boost::typelayout::PlatformInfo to_platform_info(
            const char* name = "xoffset-target",
            const char* arch_prefix = "[64-le]") const
        {
            return boost::typelayout::PlatformInfo{
                .platform_name    = name,
                .arch_prefix      = arch_prefix,
                .types            = nullptr,
                .type_count       = 0,
                .pointer_size     = pointer_size,
                .sizeof_long      = sizeof(long),
                .sizeof_wchar_t   = sizeof(wchar_t),
                .sizeof_long_double = sizeof(long double),
                .max_align        = alignof(std::max_align_t),
            };
        }
    };

    // Architecture Presets.
    // Only Arch64LE is supported (64-bit little-endian).
    inline constexpr ArchSpec Arch64LE = {
        .pointer_size = 8, .little_endian = true,
        .sizeof_int8 = 1, .sizeof_int16 = 2, .sizeof_int32 = 4, .sizeof_int64 = 8,
        .sizeof_float = 4, .sizeof_double = 8, .sizeof_bool = 1, .sizeof_char = 1,
        .pointer_align = 8,
        .alignof_int32 = 4, .alignof_int64 = 8,
        .alignof_float = 4, .alignof_double = 8,
    };

    /// Active target architecture. Change this line to switch presets.
    inline constexpr ArchSpec TargetArchitecture = Arch64LE;

} // namespace XOffsetDatastructure

// ============================================================================
// Platform validation: current compiler environment ∈ TargetArchitecture
// ============================================================================
#ifndef XOFFSET_DISABLE_PLATFORM_CHECKS
static_assert(sizeof(void*) == XOffsetDatastructure::TargetArchitecture.pointer_size,
    "Platform pointer size does not match TargetArchitecture");
static_assert(XOFFSET_LITTLE_ENDIAN == XOffsetDatastructure::TargetArchitecture.little_endian,
    "Platform endianness does not match TargetArchitecture");
static_assert(sizeof(int8_t)  == XOffsetDatastructure::TargetArchitecture.sizeof_int8);
static_assert(sizeof(int16_t) == XOffsetDatastructure::TargetArchitecture.sizeof_int16);
static_assert(sizeof(int32_t) == XOffsetDatastructure::TargetArchitecture.sizeof_int32);
static_assert(sizeof(int64_t) == XOffsetDatastructure::TargetArchitecture.sizeof_int64);
static_assert(sizeof(float)   == XOffsetDatastructure::TargetArchitecture.sizeof_float);
static_assert(sizeof(double)  == XOffsetDatastructure::TargetArchitecture.sizeof_double);
static_assert(sizeof(bool)    == XOffsetDatastructure::TargetArchitecture.sizeof_bool);
static_assert(sizeof(char)    == XOffsetDatastructure::TargetArchitecture.sizeof_char);
static_assert(alignof(void*)  == XOffsetDatastructure::TargetArchitecture.pointer_align);
static_assert(alignof(int32_t) == XOffsetDatastructure::TargetArchitecture.alignof_int32,
    "Platform alignof(int32_t) does not match TargetArchitecture");
static_assert(alignof(int64_t) == XOffsetDatastructure::TargetArchitecture.alignof_int64,
    "Platform alignof(int64_t) does not match TargetArchitecture");
static_assert(alignof(float)   == XOffsetDatastructure::TargetArchitecture.alignof_float,
    "Platform alignof(float) does not match TargetArchitecture");
static_assert(alignof(double)  == XOffsetDatastructure::TargetArchitecture.alignof_double,
    "Platform alignof(double) does not match TargetArchitecture");
#endif // XOFFSET_DISABLE_PLATFORM_CHECKS

#include <boost/interprocess/allocators/allocator.hpp>
#include <boost/interprocess/offset_ptr.hpp>
#include <boost/interprocess/detail/managed_memory_impl.hpp>
#include <boost/interprocess/indexes/iset_index.hpp>
#include <boost/interprocess/exceptions.hpp>
#include <boost/interprocess/mem_algo/simple_seq_fit.hpp>
#include <boost/interprocess/mem_algo/rbtree_best_fit.hpp>
#include <boost/interprocess/sync/mutex_family.hpp>
#include <boost/container/vector.hpp>
#include <boost/container/flat_set.hpp>
#include <boost/container/flat_map.hpp>
#include <boost/container/string.hpp>
#include <boost/container/detail/next_capacity.hpp>
#include <boost/move/utility_core.hpp>
#include <boost/assert.hpp>

namespace boost {
namespace interprocess {

template <class MutexFamily, class VoidPointer = offset_ptr<void>, std::size_t MemAlignment = 0>
class x_best_fit : public rbtree_best_fit<MutexFamily, VoidPointer, MemAlignment>
{
    typedef rbtree_best_fit<MutexFamily, VoidPointer, MemAlignment> supertype;

public:
    typedef typename supertype::size_type size_type;

    x_best_fit(typename supertype::size_type size, typename supertype::size_type extra_hdr_bytes)
        : supertype(size, extra_hdr_bytes)
    {
    }
};

template <class MutexFamily, class VoidPointer = offset_ptr<void>>
class x_seq_fit : public simple_seq_fit<MutexFamily, VoidPointer>
{
    typedef simple_seq_fit<MutexFamily, VoidPointer> supertype;

public:
    typedef typename supertype::size_type size_type;

    x_seq_fit(typename supertype::size_type segment_size, typename supertype::size_type extra_hdr_bytes)
        : supertype(segment_size, extra_hdr_bytes)
    {
    }
};

// ============================================================================
// XManagedMemory — Managed memory segment backed by std::vector<char>
//
// Uses vector::reserve() for virtual address pre-allocation (lazy-commit by OS).
// grow() stays on a fast path while size <= capacity; vector relocation triggers
// close_impl/open_impl + epoch bump + re-reserve for address stability.
// ============================================================================
template <
    class CharType,
    class AllocationAlgorithm,
    template <class IndexConfig> class IndexType>
class XManagedMemory
    : public ipcdetail::basic_managed_memory_impl<CharType, AllocationAlgorithm, IndexType>
{
private:
    typedef ipcdetail::basic_managed_memory_impl<CharType, AllocationAlgorithm, IndexType> base_t;
    BOOST_MOVABLE_BUT_NOT_COPYABLE(XManagedMemory)

public:
    typedef typename base_t::size_type size_type;

    // ── Adaptive reservation policy ──
    // reserve_size = clamp(initial_size × GROWTH_HEADROOM, MIN_RESERVE, MAX_RESERVE)
    // Keeps virtual address space proportional to actual need, enabling
    // 10,000+ independent XBuffer instances without exhausting VA space.
    static constexpr std::size_t GROWTH_HEADROOM = 16;
    static constexpr std::size_t MIN_RESERVE = 64ULL * 1024;         // 64 KB
    static constexpr std::size_t MAX_RESERVE = 256ULL * 1024 * 1024; // 256 MB

    /// Overflow-safe adaptive reservation computation.
    static constexpr std::size_t compute_reservation(std::size_t size) {
        std::size_t r;
        if (size > MAX_RESERVE / GROWTH_HEADROOM) {
            r = MAX_RESERVE;
        } else {
            r = size * GROWTH_HEADROOM;
        }
        if (r < MIN_RESERVE) r = MIN_RESERVE;
        if (r > MAX_RESERVE) r = MAX_RESERVE;
        return r;
    }

    XManagedMemory() noexcept
    {
    }

    ~XManagedMemory()
    {
        this->priv_close();
    }

    // Returns a monotonically increasing counter that is bumped whenever
    // the buffer's base address changes (vector relocation during grow).
    // Fast-path grow() (within reserve capacity) does NOT change the address,
    // so epoch stays constant and XHandle<T> works in O(1).
    uint64_t epoch() const noexcept { return m_epoch; }

    /// Construct with adaptive reservation: reserve = clamp(size × 16, 64KB, 256MB).
    /// Physical RAM consumed = size bytes (reserve pages are untouched → lazy alloc).
    XManagedMemory(size_type size)
        : m_buffer(size, char(0))
    {
        m_buffer.reserve(compute_reservation(size));
        void *addr = m_buffer.data();
        if (!base_t::create_impl(addr, size))
        {
            this->priv_close();
            throw interprocess_exception("Could not initialize heap in XManagedMemory constructor");
        }
    }

    /// Construct with explicit max capacity (overrides adaptive reservation).
    /// Usage: XManagedMemory(4096, 64*1024*1024) reserves 64MB.
    XManagedMemory(size_type size, size_type max_reserved)
        : m_buffer(size, char(0))
    {
        m_buffer.reserve(max_reserved);
        void *addr = m_buffer.data();
        if (!base_t::create_impl(addr, size))
        {
            this->priv_close();
            throw interprocess_exception("Could not initialize heap in XManagedMemory constructor");
        }
    }

    /// Construct from external serialized data (load path).
    /// Copies data into the vector, then opens the existing segment.
    XManagedMemory(const char* data, size_type size)
        : m_buffer(data, data + size)
    {
        m_buffer.reserve(compute_reservation(size));
        void *addr = m_buffer.data();
        BOOST_ASSERT((0 == (((std::size_t)addr) & (AllocationAlgorithm::Alignment - size_type(1u)))));
        if (!base_t::open_impl(addr, size))
        {
            throw interprocess_exception("Could not initialize m_buffer in constructor");
        }
    }

    /// Construct from an existing std::vector<char> (legacy compatibility).
    XManagedMemory(std::vector<char> &externalBuffer)
        : m_buffer(externalBuffer)
    {
        m_buffer.reserve(compute_reservation(m_buffer.size()));
        void *addr = m_buffer.data();
        size_type size = m_buffer.size();
        BOOST_ASSERT((0 == (((std::size_t)addr) & (AllocationAlgorithm::Alignment - size_type(1u)))));
        if (!base_t::open_impl(addr, size))
        {
            throw interprocess_exception("Could not initialize m_buffer in constructor");
        }
    }

    /// Construct by moving an existing std::vector<char> (avoids copy).
    XManagedMemory(std::vector<char> &&externalBuffer)
        : m_buffer(std::move(externalBuffer))
    {
        m_buffer.reserve(compute_reservation(m_buffer.size()));
        void *addr = m_buffer.data();
        size_type size = m_buffer.size();
        BOOST_ASSERT((0 == (((std::size_t)addr) & (AllocationAlgorithm::Alignment - size_type(1u)))));
        if (!base_t::open_impl(addr, size))
        {
            throw interprocess_exception("Could not initialize m_buffer in move constructor");
        }
    }

    XManagedMemory(BOOST_RV_REF(XManagedMemory) moved) noexcept
    {
        this->swap(moved);
    }

    XManagedMemory &operator=(BOOST_RV_REF(XManagedMemory) moved) noexcept
    {
        XManagedMemory tmp(boost::move(moved));
        this->swap(tmp);
        return *this;
    }

    // Grows the buffer by extra_bytes.
    // Fast path (within capacity): address stable, no epoch bump.
    // Slow path (relocation): close/open + epoch++ + re-reserve.
    bool grow(size_type extra_bytes)
    {
        size_type old_size = m_buffer.size();
        char* old_addr = m_buffer.data();

        try {
            m_buffer.resize(old_size + extra_bytes, char(0));
        } catch (...) {
            return false;
        }

        if (m_buffer.data() == old_addr) {
            // Fast path: within reserve capacity, address stable
            base_t::grow(extra_bytes);
        } else {
            // Slow path: vector relocated — reopen segment at new address.
            // close_impl() just nulls mp_header (no memory access on old addr).
            // open_impl() then attaches to the relocated segment data.
            base_t::close_impl();
            if (!base_t::open_impl(m_buffer.data(), old_size)) {
                throw interprocess_exception(
                    "XManagedMemory: failed to reopen segment after vector relocation");
            }
            // Extend the segment to account for the newly available space
            base_t::grow(extra_bytes);
            ++m_epoch;  // Invalidate XHandle caches
            // Re-reserve to restore address stability for future grows.
            // This prevents consecutive relocations after exceeding capacity.
            m_buffer.reserve(compute_reservation(m_buffer.size()));
        }
        return true;
    }

    void swap(XManagedMemory &other) noexcept
    {
        base_t::swap(other);
        m_buffer.swap(other.m_buffer);
        std::swap(m_epoch, other.m_epoch);
    }

    // Shrinks the managed segment's logical size.  Does NOT shrink the vector
    // — preserves base address, avoids epoch invalidation.
    void shrink_to_fit()
    {
        base_t::shrink_to_fit();
        // Intentionally NOT shrinking the vector (excess capacity = future headroom).
    }

    /// Returns a pointer to the underlying std::vector<char> buffer.
    /// Primarily used in tests and for serialization (to_vector()).
    std::vector<char> *get_buffer()
    {
        return &m_buffer;
    }

    const void* get_address() const
    {
        return m_buffer.data();
    }

    size_type get_size() const
    {
        return m_buffer.size();
    }

    /// Returns the segment's logical size (byte-exact).
    /// This is the size recorded in the segment_manager header after
    /// shrink_to_fit(). Use this for serialization instead of get_size()
    /// which may include unused trailing space.
    size_type segment_size() const
    {
        return base_t::get_size();
    }

private:
    void priv_close()
    {
        base_t::destroy_impl();
        m_buffer.clear();
        m_buffer.shrink_to_fit();
    }

    std::vector<char> m_buffer;
    uint64_t m_epoch = 0;
};

} // namespace interprocess
} // namespace boost

namespace XOffsetDatastructure {
    using namespace boost::interprocess;

    // rbtree_best_fit: O(log n) allocation with automatic free-block coalescing.
    // Reduces fragmentation compared to simple_seq_fit, at the cost of slightly
    // larger per-allocation headers (~32-64 bytes vs ~16 bytes).
    // This is the right choice for XOffset's workload pattern (frequent
    // alloc+dealloc cycles from container growth and string reassignment).
    using XBufferCore = XManagedMemory<char, x_best_fit<null_mutex_family>, iset_index>;
    // Alternative allocator policy (sequential fit). Faster allocation O(n)
    // but no free-block coalescing — only suitable for append-only workloads.
    using XBufferCoreSeqFit = XManagedMemory<char, x_seq_fit<null_mutex_family>, iset_index>;

    template<typename T>
    concept HasIterator = requires(T t) {
        { t.begin() } -> std::input_or_output_iterator;
        { t.end() } -> std::input_or_output_iterator;
    };
    
    template<typename T>
    concept HasValueType = requires {
        typename T::value_type;
    };
    
    template<typename T>
    concept HasMappedType = requires {
        typename T::mapped_type;
    };
    
    template<typename T>
    concept HasKeyType = requires {
        typename T::key_type;
    };
    
    template<typename T>
    concept SequentialContainer = HasIterator<T> && HasValueType<T> && 
        requires(T t, typename T::value_type v) {
            { t.emplace_back(std::move(v)) };
        };
    
    template<typename T>
    concept SetLikeContainer = HasIterator<T> && HasValueType<T> && HasKeyType<T> && 
        !HasMappedType<T> &&
        requires(T t, typename T::value_type v) {
            { t.emplace(std::move(v)) };
        };
    
    template<typename T>
    concept MapLikeContainer = HasIterator<T> && HasKeyType<T> && HasMappedType<T> &&
        requires(T t, typename T::key_type k, typename T::mapped_type v) {
            { t.emplace(std::move(k), std::move(v)) };
        };
    
    template<typename T>
    concept SupportedContainer = SequentialContainer<T> || SetLikeContainer<T> || MapLikeContainer<T>;

    template <typename T>
    using XOffsetPtr = boost::interprocess::offset_ptr<T>;

    // ========================================================================
    // Container Implementation Details (detail namespace)
    //
    // Growth factor policy and internal vector option types are
    // implementation details — not part of the public API.
    // ========================================================================
    namespace detail {
        /// Custom growth factor: 1.1x (11/10) to minimize buffer waste
        struct growth_factor_custom
            : boost::container::dtl::grow_factor_ratio<0, 11, 10> {};

        /// Common vector options with custom growth factor
        using x_vector_options = boost::container::vector_options_t<
            boost::container::growth_factor<growth_factor_custom>>;

        /// Base scoped allocator (standard Boost version, used internally).
        template <typename T>
        using x_base_scoped_alloc = boost::container::scoped_allocator_adaptor<
            boost::interprocess::allocator<T, XBufferCore::segment_manager>>;

        // Forward declarations of reflection helpers (defined later in this header).
        // These are used by x_reflect_scoped_alloc::construct() and are resolved
        // at template instantiation time, so forward declaration is sufficient here.
        template <typename T, typename Alloc>
        void reflect_init_all(void* raw, Alloc alloc);

        template <typename T, typename Src>
        void reflect_transfer_init_all(void* dst, Src&& src,
            XBufferCore::segment_manager* sm);

        // ================================================================
        // needs_reflect_construct<T>
        //
        // True when T is a pure aggregate (no allocator_type, no user
        // constructor from SM*) that contains non-trivial members
        // (e.g. XString, XVector).  These types cannot be default-
        // constructed or move-constructed by the standard uses_allocator
        // protocol, so we intercept construct() and use C++26 reflection.
        //
        // Trivially-copyable types are excluded because the default
        // placement-new path already handles them correctly.
        // ================================================================
        template <typename T>
        concept needs_reflect_construct =
            std::is_class_v<T> &&
            !std::is_trivially_copyable_v<T> &&
            !requires { typename T::allocator_type; } &&
            !requires(XBufferCore::segment_manager* sm) { T(sm); };

        // ================================================================
        // x_reflect_scoped_alloc<T>
        //
        // Custom scoped allocator that intercepts construct() for pure
        // aggregates (needs_reflect_construct<U>).  For such types:
        //   - Default construct → reflect_init_all  (zero + alloc inject)
        //   - Move/Copy construct → reflect_transfer_init_all (per-member)
        //
        // For all other types, delegates to base scoped_allocator_adaptor
        // which uses the standard uses_allocator protocol.
        // ================================================================
        template <typename T>
        class x_reflect_scoped_alloc : public x_base_scoped_alloc<T> {
            using Base = x_base_scoped_alloc<T>;
        public:
            using Base::Base;

            /// Rebind: preserve our derived type across allocator rebinding.
            template <typename U>
            struct rebind { using other = x_reflect_scoped_alloc<U>; };

            /// Converting constructor from rebound allocator.
            template <typename U>
            x_reflect_scoped_alloc(const x_reflect_scoped_alloc<U>& other) noexcept
                : Base(other) {}

            // ── construct overload (0): default construction ──
            template <typename U>
            void construct(U* p) {
                if constexpr (needs_reflect_construct<U>) {
                    reflect_init_all<U>(
                        static_cast<void*>(p),
                        this->outer_allocator().get_segment_manager());
                } else {
                    Base::construct(p);
                }
            }

            // ── construct overload (1): single-arg (move or copy) ──
            template <typename U, typename Arg>
            void construct(U* p, Arg&& arg) {
                if constexpr (needs_reflect_construct<U> &&
                              std::is_same_v<std::decay_t<Arg>, U>) {
                    // Move or copy of the same type → per-member transfer
                    reflect_transfer_init_all<U>(
                        static_cast<void*>(p),
                        std::forward<Arg>(arg),
                        this->outer_allocator().get_segment_manager());
                } else {
                    Base::construct(p, std::forward<Arg>(arg));
                }
            }

            // ── construct overload (2+): multi-arg → delegate to base ──
            template <typename U, typename A1, typename A2, typename... Rest>
            void construct(U* p, A1&& a1, A2&& a2, Rest&&... rest) {
                Base::construct(p,
                    std::forward<A1>(a1),
                    std::forward<A2>(a2),
                    std::forward<Rest>(rest)...);
            }
        };

        /// The allocator used by all XOffset containers (XVector, XMap, XSet).
        /// Reflection-aware: automatically handles pure aggregates.
        template <typename T>
        using x_scoped_alloc = x_reflect_scoped_alloc<T>;

        /// Internal vector alias used as backing store for flat containers
        template <typename T>
        using x_vector_impl = boost::container::vector<
            T, x_scoped_alloc<T>, x_vector_options>;

        /// Internal flat_map alias
        template <typename K, typename V>
        using x_map_impl = boost::container::flat_map<K, V, std::less<void>,
            x_vector_impl<std::pair<K, V>>>;

        /// Internal flat_set alias
        template <typename T>
        using x_set_impl = boost::container::flat_set<T, std::less<void>, x_vector_impl<T>>;

    } // namespace detail

    /// Managed string with shared-memory allocator
    using XString = boost::container::basic_string<
        char, std::char_traits<char>, allocator<char, XBufferCore::segment_manager>>;

    /// Convenience allocator typedef for user-defined allocator-aware types.
    /// Usage:  using allocator_type = XAllocator;
    using XAllocator = boost::interprocess::allocator<char, XBufferCore::segment_manager>;

    // ========================================================================
    // Public Container Wrapper Classes
    //
    // scoped_allocator_adaptor handles emplace paths; wrapper overloads cover
    // push_back/insert/operator[]/resize where the base API needs a full T.
    // ========================================================================

    /// Managed vector with 1.1x growth factor and automatic allocator propagation.
    template <typename T>
    class XVector : public detail::x_vector_impl<T> {
        using Base = detail::x_vector_impl<T>;
        using SM = XBufferCore::segment_manager;
        auto* sm() { return this->get_stored_allocator().get_segment_manager(); }

    public:
        using Base::Base;           // inherit all constructors
        using Base::push_back;      // keep base push_back(const T&), push_back(T&&)
        using Base::insert;         // keep all base insert overloads
        using Base::resize;
        using Base::assign;

        // --- Overloads for allocator-aware element types ---
        // These forward to emplace so scoped_allocator_adaptor can inject
        // the allocator. The requires constraint activates when:
        //   1. T has an allocator_type (i.e. T is allocator-aware), AND
        //   2. Arg is NOT the same type as T (so T itself still routes to base).
        //
        // We use !is_same<decay_t<Arg>, T> instead of !is_convertible because
        // is_convertible only checks constructor declaration signatures, not
        // bodies. Boost.Interprocess allocators have no default constructor,
        // so basic_string(const char*) is declared (is_convertible says true)
        // but instantiation fails (hard error). is_same avoids this entirely.

        template<typename Arg>
            requires (requires { typename T::allocator_type; } &&
                      !std::is_same_v<std::decay_t<Arg>, T>)
        void push_back(Arg&& arg) {
            Base::emplace_back(std::forward<Arg>(arg));
        }

        template<typename Arg>
            requires (requires { typename T::allocator_type; } &&
                      !std::is_same_v<std::decay_t<Arg>, T>)
        typename Base::iterator insert(typename Base::const_iterator pos, Arg&& arg) {
            return Base::emplace(pos, std::forward<Arg>(arg));
        }

        template<typename Arg>
            requires (requires { typename T::allocator_type; } &&
                      !std::is_same_v<std::decay_t<Arg>, T>)
        typename Base::iterator insert(typename Base::const_iterator pos,
                                       typename Base::size_type n, Arg&& arg) {
            T tmp(std::forward<Arg>(arg), sm());
            return Base::insert(pos, n, tmp);
        }

        template<typename Arg>
            requires (requires { typename T::allocator_type; } &&
                      !std::is_same_v<std::decay_t<Arg>, T>)
        void resize(typename Base::size_type n, Arg&& arg) {
            T tmp(std::forward<Arg>(arg), sm());
            Base::resize(n, tmp);
        }

        template<typename Arg>
            requires (requires { typename T::allocator_type; } &&
                      !std::is_same_v<std::decay_t<Arg>, T>)
        void assign(typename Base::size_type n, Arg&& arg) {
            T tmp(std::forward<Arg>(arg), sm());
            Base::assign(n, tmp);
        }
    };

    /// Managed flat_map with transparent comparator and automatic allocator propagation.
    template <typename K, typename V>
    class XMap : public detail::x_map_impl<K, V> {
        using Base = detail::x_map_impl<K, V>;
        using SM = XBufferCore::segment_manager;
        auto* sm() { return this->get_stored_allocator().get_segment_manager(); }

    public:
        using Base::Base;
        using Base::operator[];
        using Base::erase;
        using Base::try_emplace;
        using Base::insert_or_assign;

        // --- operator[](key): heterogeneous key that needs allocator ---
        // Uses piecewise_construct so that V is constructed via
        // scoped_allocator_adaptor::construct() with zero args, which
        // auto-injects the allocator when V is allocator-aware (e.g. XString).
        // This avoids the hard error from V{} when V has no default ctor.
        template<typename KeyArg>
            requires (!std::is_same_v<std::decay_t<KeyArg>, K>)
        V& operator[](const KeyArg& key) {
            auto it = this->find(key);  // transparent comparator
            if (it != this->end()) return it->second;
            auto result = this->emplace(
                std::piecewise_construct,
                std::forward_as_tuple(key),
                std::forward_as_tuple());
            return result.first->second;
        }

        // --- erase(key): heterogeneous key ---
        template<typename KeyArg>
            requires (!std::is_same_v<std::decay_t<KeyArg>, K> &&
                      !std::is_convertible_v<const KeyArg&, typename Base::const_iterator>)
        typename Base::size_type erase(const KeyArg& key) {
            auto it = this->find(key);
            if (it == this->end()) return 0;
            Base::erase(it);
            return 1;
        }

        // --- try_emplace(key, args...): heterogeneous key ---
        // Uses piecewise_construct to ensure pair::first and pair::second
        // are each constructed through dispatch_uses_allocator individually.
        template<typename KeyArg, typename... Args>
            requires (!std::is_same_v<std::decay_t<KeyArg>, K>)
        std::pair<typename Base::iterator, bool> try_emplace(KeyArg&& key, Args&&... args) {
            auto it = this->find(key);
            if (it != this->end()) return {it, false};
            return this->emplace(
                std::piecewise_construct,
                std::forward_as_tuple(std::forward<KeyArg>(key)),
                std::forward_as_tuple(std::forward<Args>(args)...));
        }

        // --- insert_or_assign(key, obj): heterogeneous key ---
        template<typename KeyArg, typename M>
            requires (!std::is_same_v<std::decay_t<KeyArg>, K>)
        std::pair<typename Base::iterator, bool> insert_or_assign(KeyArg&& key, M&& obj) {
            auto it = this->find(key);
            if (it != this->end()) {
                it->second = std::forward<M>(obj);
                return {it, false};
            }
            return this->emplace(
                std::piecewise_construct,
                std::forward_as_tuple(std::forward<KeyArg>(key)),
                std::forward_as_tuple(std::forward<M>(obj)));
        }
    };

    /// Managed flat_set with transparent comparator and automatic allocator propagation.
    template <typename T>
    class XSet : public detail::x_set_impl<T> {
        using Base = detail::x_set_impl<T>;
        using SM = XBufferCore::segment_manager;

    public:
        using Base::Base;
        using Base::insert;
        using Base::erase;

        // --- insert(val): when val can't convert to T directly ---
        template<typename Arg>
            requires (!std::is_same_v<std::decay_t<Arg>, T>)
        std::pair<typename Base::iterator, bool> insert(const Arg& val) {
            return this->emplace(val);  // scoped_alloc handles T construction
        }

        // --- insert(pos, val): hint version ---
        template<typename Arg>
            requires (!std::is_same_v<std::decay_t<Arg>, T>)
        typename Base::iterator insert(typename Base::const_iterator pos, const Arg& val) {
            return this->emplace_hint(pos, val);
        }

        // --- erase(key): heterogeneous key ---
        template<typename KeyArg>
            requires (!std::is_same_v<std::decay_t<KeyArg>, T> &&
                      !std::is_convertible_v<const KeyArg&, typename Base::const_iterator>)
        typename Base::size_type erase(const KeyArg& key) {
            auto it = this->find(key);
            if (it == this->end()) return 0;
            Base::erase(it);
            return 1;
        }
    };

    // Verify wrapper classes add zero overhead — same size as the underlying
    // container implementation (no extra data members).
    static_assert(sizeof(XVector<int>) == sizeof(detail::x_vector_impl<int>),
        "XVector wrapper must be zero-overhead");
    static_assert(sizeof(XMap<int,int>) == sizeof(detail::x_map_impl<int,int>),
        "XMap wrapper must be zero-overhead");
    static_assert(sizeof(XSet<int>) == sizeof(detail::x_set_impl<int>),
        "XSet wrapper must be zero-overhead");

    class XBufferStats {
    public:
        struct MemoryStats {
            std::size_t total_size;
            std::size_t free_size;
            std::size_t used_size;
            
            double usage_percent() const {
                return total_size > 0 ? (used_size * 100.0 / total_size) : 0.0;
            }
            
            double free_percent() const {
                return total_size > 0 ? (free_size * 100.0 / total_size) : 0.0;
            }
        };

        static MemoryStats memory_stats(XBufferCore& xbuf) {
            MemoryStats stats = {};
            stats.total_size = xbuf.get_size();
            stats.free_size = xbuf.get_free_memory();
            stats.used_size = stats.total_size - stats.free_size;
            return stats;
        }

        static void print(XBufferCore& xbuf) {
            MemoryStats stats = memory_stats(xbuf);
            std::cout << "XBufferCore: " << stats.used_size << "/" << stats.total_size
                      << " bytes (" << std::fixed << std::setprecision(1) 
                      << stats.usage_percent() << "% used)" << std::endl;
        }
    };

    // Forward declarations for safety gate and validation
    template<typename T> constexpr void validate_xbuffer_type();

    // Internal constant for the single root object name.
    // Users never see this — all public APIs hide the naming layer.
    inline constexpr const char* XBUFFER_ROOT_NAME = "__root__";

    namespace detail {

        // ============================================================================
        // Type Safety Architecture — Unified on TypeLayout's classify_safety<T>()
        //
        // TypeLayout's layout signature is the single source of truth for safety.
        // XOffset containers are registered via TYPELAYOUT_OPAQUE_* macros so
        // classify_safety recurses into element types automatically.
        //
        // XOffset adds two domain-specific policies on top:
        //   1. Warning→Risk escalation (zero-encoding forbids pointers/unions)
        //   2. long/unsigned long rejection when size ≠ fixed-width (LP64 vs LLP64)
        //
        // Full guarantee: C1 (TYPELAYOUT_ASSERT_COMPAT) + C2 (classify_for_xoffset)
        // ============================================================================

        // Import the compile-time safety engine from TypeLayout
        using boost::typelayout::compat::SafetyLevel;
        using boost::typelayout::compat::classify_safety;
        using boost::typelayout::compat::is_serialization_free_local;
        using boost::typelayout::get_layout_signature;

        // ---- Core: classify a type for XOffset zero-encoding --------------------
        //
        // The ONLY classification entry point. No manual container recursion —
        // TypeLayout's opaque signature mechanism handles that automatically.
        //
        // Flow:
        //   1. long/unsigned long first-line rejection (platform-variable size)
        //   2. classify_safety<T>() — TypeLayout's signature-based ground truth
        //      (handles primitives, structs via reflection, containers via opaque)
        //   3. Warning→Risk escalation (XOffset zero-encoding policy)
        //
        // ⚠ LIMITATION: Only catches naked long/unsigned long at top-level T.
        // Struct members are desugared by P2996 — rely on TYPELAYOUT_ASSERT_COMPAT.
        // See docs/LONG_PORTABILITY_GUIDE.md.
        template<typename T>
        consteval SafetyLevel classify_for_xoffset() {
            using CleanT = std::remove_cv_t<T>;

            // Step 1: Platform-variable integers — reject long / unsigned long
            // ONLY when they are NOT equivalent to the fixed-width types.
            // On LP64 (Linux/macOS-64), long == int64_t — safe (same binary).
            // On LLP64 (Windows), long is 4 bytes ≠ int64_t — reject.
            // NB: int64_t is often a typedef for long on LP64, so we must NOT
            //     unconditionally reject long or we'd also reject int64_t.
            if constexpr ((std::is_same_v<CleanT, long> && !std::is_same_v<long, int64_t>) ||
                          (std::is_same_v<CleanT, unsigned long> && !std::is_same_v<unsigned long, uint64_t>)) {
                return SafetyLevel::Risk;
            }
            // Step 2 + 3: TypeLayout ground truth + XOffset escalation
            else {
                // classify_safety<T>() scans the layout signature for markers.
                // For opaque-registered containers (XVector, XString, etc.),
                // the signature already embeds element signatures — so this
                // single call handles both leaf types AND containers.
                constexpr auto level = classify_safety<CleanT>();

                // XOffset zero-encoding policy: escalate Warning→Risk.
                // Pointers, unions, vptr are "Warning" in TypeLayout (ok for
                // same-platform), but XOffset categorically rejects them.
                if constexpr (level == SafetyLevel::Warning)
                    return SafetyLevel::Risk;
                else
                    return level;
            }
        }

        // ====================================================================
        // Policy Traits — compile-time hooks for safety / layout verification
        //
        // Each Policy is a struct with:
        //   template<typename T>
        //   static consteval bool accept();
        //
        // Users can define custom policies; XOffset ships two built-in ones.
        // ====================================================================

        /// DefaultPolicy — Single-platform Serialization-free check (C2).
        ///
        /// Ensures the type passes TypeLayout's safety engine with XOffset's
        /// domain-specific escalation. A type accepted by this policy is
        /// "locally serialization-free" — it has no pointers, bitfields,
        /// wchar_t, or other binary-unstable constructs.
        ///
        /// For the full cross-platform guarantee (C1+C2), combine with
        /// TYPELAYOUT_ASSERT_COMPAT in CI.
        struct DefaultPolicy {
            template<typename T>
            static consteval bool accept() {
                return classify_for_xoffset<T>() == SafetyLevel::Safe;
            }
        };

        /// StrictPolicy<GoldSignature> — Full Serialization-free with layout lock.
        ///
        /// Combines DefaultPolicy (C2) with a compile-time layout signature
        /// match against a "gold" reference string (effectively enforcing C1
        /// at compile time for a single known target layout).
        /// This is the strongest single-compilation guarantee available.
        ///
        /// Usage:
        ///   constexpr auto GOLD = FixedString{"[64-le]record[s:24,a:8]{...}"};
        ///   static_assert(is_xbuffer_compatible<MyStruct, StrictPolicy<GOLD>>());
        template<auto GoldSignature>
        struct StrictPolicy {
            template<typename T>
            static consteval bool accept() {
                return get_layout_signature<T>() == GoldSignature
                    && classify_for_xoffset<T>() == SafetyLevel::Safe;
            }
        };

        // ====================================================================
        // is_xbuffer_compatible<T, Policy>() — unified compile-time entry point
        //
        // The single gate for all XBuffer type admission decisions.
        // Delegates entirely to Policy::accept<T>().
        //
        // With DefaultPolicy, this is equivalent to asking:
        //   "Is T XOffset-Serialization-free on this platform?"
        // For the full cross-platform guarantee, combine with Layer 3 (CI).
        // ====================================================================
        template<typename T, typename Policy = DefaultPolicy>
        consteval bool is_xbuffer_compatible() {
            return Policy::template accept<T>();
        }

        template<typename T>
        consteval const char* get_safety_error_message() {
            using CleanT = std::remove_cv_t<T>;

            if constexpr (is_xbuffer_compatible<CleanT>()) {
                return "Type is SAFE for XBufferCore";
            }
            // Detailed diagnostics for common failure modes
            else if constexpr (std::is_polymorphic_v<CleanT>) {
                return "UNSAFE: Type has virtual functions (polymorphic)";
            }
            else if constexpr (std::is_union_v<CleanT>) {
                return "UNSAFE: Union type not allowed";
            }
            else if constexpr (std::is_pointer_v<CleanT>) {
                return "UNSAFE: Raw pointer (use XOffsetPtr<T> with opt-in, see docs)";
            }
            else if constexpr (std::is_reference_v<CleanT>) {
                return "UNSAFE: Reference type not allowed";
            }
            else if constexpr (std::is_same_v<CleanT, std::string>) {
                return "UNSAFE: std::string (use XString instead)";
            }
            else if constexpr (requires { typename CleanT::allocator_type; }) {
                return "UNSAFE: std container (use XVector/XMap/XSet/XString instead)";
            }
            // Platform-dependent integers (long, unsigned long)
            // Note: long long / unsigned long long are allowed (always 8 bytes).
            else if constexpr (std::is_same_v<CleanT, long> ||
                               std::is_same_v<CleanT, unsigned long>) {
                return "UNSAFE: 'long'/'unsigned long' excluded — sizeof varies "
                       "across platforms (LP64 vs LLP64). Use int32_t/int64_t/uint32_t/uint64_t";
            }
            else if constexpr (std::is_class_v<CleanT>) {
                return "UNSAFE: Struct/class contains unsafe members";
            }
            else {
                return "UNSAFE: Type not allowed in XBufferCore";
            }
        }
    }
    
    /// Public type admission gate: is_xbuffer_safe<T>::value == true iff T is
    /// XOffset Serialization-free on this platform. Use reason() for diagnostics.
    template<typename T>
    struct is_xbuffer_safe {
        static constexpr bool value = detail::is_xbuffer_compatible<T>();
        
        static constexpr const char* reason() {
            return detail::get_safety_error_message<T>();
        }
    };
    
    // Per-member diagnostic helper: triggers a static_assert for each unsafe member,
    // so the compiler error points to the exact field name.
    //
    // Accepts an optional Policy template parameter (defaults to DefaultPolicy).
    // This ensures diagnose_unsafe_members uses the same admission criteria as
    // the policy that rejected the type, giving consistent diagnostics.
    //
    // Uses index-based expansion (not `template for`) to avoid P2996 compiler
    // limitations with const vector iterators in consteval context.
    namespace detail {
        template<typename T, typename Policy, std::size_t N>
        consteval void diagnose_base_at() {
            constexpr auto base_info =
                std::meta::bases_of(^^T, std::meta::access_context::unchecked())[N];
            using BaseT = [:std::meta::type_of(base_info):];
            static_assert(
                is_xbuffer_compatible<BaseT, Policy>(),
                "Unsafe base class detected in XBufferCore type");
        }

        template<typename T, typename Policy, std::size_t N>
        consteval void diagnose_member_at() {
            constexpr auto member_info =
                std::meta::nonstatic_data_members_of(^^T, std::meta::access_context::unchecked())[N];
            using MemberT = [:std::meta::type_of(member_info):];
            static_assert(
                is_xbuffer_compatible<MemberT, Policy>(),
                "Unsafe member detected in XBufferCore type (see compiler note for field name and type)");
        }

        template<typename T, typename Policy, std::size_t... Bs>
        consteval void diagnose_bases_impl(std::index_sequence<Bs...>) {
            (diagnose_base_at<T, Policy, Bs>(), ...);
        }

        template<typename T, typename Policy, std::size_t... Ms>
        consteval void diagnose_members_impl(std::index_sequence<Ms...>) {
            (diagnose_member_at<T, Policy, Ms>(), ...);
        }
    } // namespace detail

    template<typename T, typename Policy = detail::DefaultPolicy>
    consteval void diagnose_unsafe_members() {
        if constexpr (std::is_class_v<T> && !std::is_polymorphic_v<T> && !std::is_union_v<T>) {
            constexpr std::size_t base_count =
                std::meta::bases_of(^^T, std::meta::access_context::unchecked()).size();
            if constexpr (base_count > 0) {
                detail::diagnose_bases_impl<T, Policy>(std::make_index_sequence<base_count>{});
            }
            constexpr std::size_t member_count =
                std::meta::nonstatic_data_members_of(^^T, std::meta::access_context::unchecked()).size();
            if constexpr (member_count > 0) {
                detail::diagnose_members_impl<T, Policy>(std::make_index_sequence<member_count>{});
            }
        }
    }

    template<typename T>
    constexpr void validate_xbuffer_type() {
        static_assert(is_xbuffer_safe<T>::value, 
            "\n\n"
            "========================================\n"
            "  XBuffer Type Safety Error\n"
            "========================================\n"
            "The type you are trying to use is NOT SAFE for XBuffer.\n\n"
            "ALLOWED TYPES:\n"
            "  Basic Types:\n"
            "    int8_t, int16_t, int32_t, int64_t\n"
            "    uint8_t, uint16_t, uint32_t, uint64_t\n"
            "    float, double, bool, char\n\n"
            "  XBuffer Containers:\n"
            "    XString, XVector<T>, XMap<K,V>, XSet<T>\n\n"
            "  User-Defined Types:\n"
            "    struct/class containing only safe types\n"
            "    (no virtual functions/inheritance, no raw pointers)\n"
            "    (non-virtual inheritance IS allowed)\n\n"
            "NOT ALLOWED:\n"
            "  ✗ Virtual functions (polymorphic types)\n"
            "  ✗ Virtual inheritance\n"
            "  ✗ Raw pointers\n"
            "  ✗ References\n"
            "  ✗ std::string (use XString)\n"
            "  ✗ std::vector (use XVector<T>)\n"
            "  ✗ std::map (use XMap<K,V>)\n"
            "  ✗ std::set (use XSet<T>)\n"
            "  ✗ Union types\n"
            "  ✗ std::function (type-erased, contains hidden pointers)\n"
            "  ✗ std::any (type-erased, contains hidden pointers)\n"
            "  ✗ std::shared_ptr/unique_ptr/weak_ptr (smart pointers)\n"
            "========================================\n");
        // If the top-level assert fires and T is a struct, also fire per-member
        // asserts so the compiler names the exact offending field(s).
        if constexpr (!is_xbuffer_safe<T>::value) {
            diagnose_unsafe_members<T>();
        }
    }

    // ========================================================================
    // Reflection-Based Construction (Zero-Boilerplate Support)
    //
    // C++26 reflection enables constructing user types WITHOUT requiring any
    // user-written constructors, macros, or typedefs. Users just write:
    //
    //   struct Player {
    //       int32_t id{0};
    //       XString name;
    //       XVector<int32_t> items;
    //   };
    //   auto* p = xbuf.make<Player>();  // Just works!
    //
    // Implementation: allocate raw memory, then use reflection to
    // construct_at each member individually (POD→value-init, containers→alloc).
    //
    // For types WITH traditional allocator constructors, the old path is used
    // automatically (detected via has_segment_manager_ctor concept).
    // ========================================================================
    namespace detail {

        /// Concept: T has allocator_type typedef (allocator-aware)
        template <typename T>
        concept has_allocator_type_member = requires { typename T::allocator_type; };

        /// Concept: T is constructible from segment_manager* (traditional path)
        template <typename T>
        concept has_segment_manager_ctor = requires(XBufferCore::segment_manager* sm) {
            T(sm);
        };

        // ── Member / base count (consteval) ──
        template <typename T>
        consteval std::size_t reflect_member_count_of() {
            return std::meta::nonstatic_data_members_of(
                ^^T, std::meta::access_context::unchecked()).size();
        }

        template <typename T>
        consteval std::size_t reflect_base_count_of() {
            return std::meta::bases_of(
                ^^T, std::meta::access_context::unchecked()).size();
        }

        // ── Per-member init on raw memory (index-based) ──
        // Uses obj.[:member:] (reference syntax) to avoid P2996 compiler
        // assertion failure with -> operator on cast pointers.
        template <typename T, std::size_t N, typename Alloc>
        void reflect_init_nth(void* raw, Alloc alloc) {
            using namespace std::meta;
            constexpr auto member =
                nonstatic_data_members_of(^^T, access_context::unchecked())[N];
            using M = [:type_of(member):];
            T& obj = *reinterpret_cast<T*>(raw);
            if constexpr (has_allocator_type_member<M>) {
                std::construct_at(&(obj.[:member:]), alloc);
            } else {
                std::construct_at(&(obj.[:member:]));  // value-init (zero)
            }
        }

        // ── Fold-expression expander ──
        template <typename T, typename Alloc, std::size_t... Is>
        void reflect_init_expand(void* raw, Alloc alloc, std::index_sequence<Is...>) {
            (reflect_init_nth<T, Is>(raw, alloc), ...);
        }

        // ── Internal recursive impl (no memset — called for bases too) ──
        template <typename T, typename Alloc>
        void reflect_init_all_impl(void* raw, Alloc alloc);

        // ── Per-base init: cast to base subobject, recurse ──
        template <typename T, std::size_t N, typename Alloc>
        void reflect_init_base_nth(void* raw, Alloc alloc) {
            using namespace std::meta;
            constexpr auto base_info = bases_of(^^T, access_context::unchecked())[N];
            using BaseType = [:type_of(base_info):];
            T* obj = reinterpret_cast<T*>(raw);
            BaseType* base_ptr = static_cast<BaseType*>(obj);
            reflect_init_all_impl<BaseType>(static_cast<void*>(base_ptr), alloc);
        }

        template <typename T, typename Alloc, std::size_t... Is>
        void reflect_init_bases_expand(void* raw, Alloc alloc, std::index_sequence<Is...>) {
            (reflect_init_base_nth<T, Is>(raw, alloc), ...);
        }

        /// Internal: recursively init bases then direct members (no memset).
        template <typename T, typename Alloc>
        void reflect_init_all_impl(void* raw, Alloc alloc) {
            if constexpr (reflect_base_count_of<T>() > 0) {
                reflect_init_bases_expand<T>(raw, alloc,
                    std::make_index_sequence<reflect_base_count_of<T>()>{});
            }
            if constexpr (reflect_member_count_of<T>() > 0) {
                reflect_init_expand<T>(raw, alloc,
                    std::make_index_sequence<reflect_member_count_of<T>()>{});
            }
        }

        /// Construct all members of T on zeroed raw memory using reflection.
        /// POD members are value-initialized (zero), allocator-aware members
        /// receive the allocator.  Handles inheritance: base class members
        /// are initialized recursively before direct members.
        template <typename T, typename Alloc>
        void reflect_init_all(void* raw, Alloc alloc) {
            std::memset(raw, 0, sizeof(T));  // zero ONCE at top level
            reflect_init_all_impl<T>(raw, alloc);
        }

        // ================================================================
        // Reflection Transfer Helpers (Move / Copy with allocator injection)
        //
        // Used by x_reflect_scoped_alloc::construct() to handle move and
        // copy construction of pure aggregates inside XVector reallocation.
        //
        // For each member:
        //   - allocator-aware (XString, XVector, etc.) →
        //       construct_at(&dst.member, forward(src.member), sm)
        //       This invokes the container's move/copy+allocator constructor.
        //   - POD →
        //       construct_at(&dst.member, forward(src.member))
        //       Standard move/copy.
        // ================================================================

        // ── Per-member transfer (index-based, perfect-forwarding) ──
        template <typename T, std::size_t N, typename Src>
        void reflect_transfer_init_nth(void* dst, Src&& src,
                                       XBufferCore::segment_manager* sm) {
            using namespace std::meta;
            constexpr auto member =
                nonstatic_data_members_of(^^T, access_context::unchecked())[N];
            using M = [:type_of(member):];
            T& d = *reinterpret_cast<T*>(dst);
            if constexpr (has_allocator_type_member<M>) {
                // Container/string: move or copy + inject allocator
                std::construct_at(&(d.[:member:]),
                    std::forward<Src>(src).[:member:], sm);
            } else {
                // POD: direct move or copy
                std::construct_at(&(d.[:member:]),
                    std::forward<Src>(src).[:member:]);
            }
        }

        // ── Fold-expression expander for transfer ──
        template <typename T, typename Src, std::size_t... Is>
        void reflect_transfer_init_expand(void* dst, Src&& src,
                                          XBufferCore::segment_manager* sm,
                                          std::index_sequence<Is...>) {
            (reflect_transfer_init_nth<T, Is>(dst, std::forward<Src>(src), sm), ...);
        }

        // ── Internal recursive transfer impl (no memset) ──
        template <typename T, typename Src>
        void reflect_transfer_init_all_impl(void* dst, Src&& src,
                                            XBufferCore::segment_manager* sm);

        // ── Per-base transfer: cast both dst and src to base, recurse ──
        template <typename T, std::size_t N, typename Src>
        void reflect_transfer_base_nth(void* dst, Src&& src,
                                       XBufferCore::segment_manager* sm) {
            using namespace std::meta;
            constexpr auto base_info = bases_of(^^T, access_context::unchecked())[N];
            using BaseType = [:type_of(base_info):];

            T* dst_obj = reinterpret_cast<T*>(dst);
            BaseType* dst_base = static_cast<BaseType*>(dst_obj);

            // Preserve value category: T&& → BaseType&&, const T& → const BaseType&
            if constexpr (std::is_lvalue_reference_v<Src&&>) {
                reflect_transfer_init_all_impl<BaseType>(
                    static_cast<void*>(dst_base),
                    static_cast<const BaseType&>(src), sm);
            } else {
                reflect_transfer_init_all_impl<BaseType>(
                    static_cast<void*>(dst_base),
                    static_cast<BaseType&&>(std::move(src)), sm);
            }
        }

        template <typename T, typename Src, std::size_t... Is>
        void reflect_transfer_bases_expand(void* dst, Src&& src,
                                           XBufferCore::segment_manager* sm,
                                           std::index_sequence<Is...>) {
            (reflect_transfer_base_nth<T, Is>(dst, std::forward<Src>(src), sm), ...);
        }

        /// Internal: recursively transfer bases then direct members (no memset).
        template <typename T, typename Src>
        void reflect_transfer_init_all_impl(void* dst, Src&& src,
                                            XBufferCore::segment_manager* sm) {
            if constexpr (reflect_base_count_of<T>() > 0) {
                reflect_transfer_bases_expand<T>(dst, std::forward<Src>(src), sm,
                    std::make_index_sequence<reflect_base_count_of<T>()>{});
            }
            if constexpr (reflect_member_count_of<T>() > 0) {
                reflect_transfer_init_expand<T>(dst, std::forward<Src>(src), sm,
                    std::make_index_sequence<reflect_member_count_of<T>()>{});
            }
        }

        /// Transfer (move or copy) all members of T from src to dst,
        /// injecting the segment_manager for allocator-aware members.
        /// dst must point to raw (uninitialized) memory of sizeof(T).
        /// Src is T&& (move) or const T& (copy), resolved via forwarding.
        /// Handles inheritance: base class members are transferred first.
        template <typename T, typename Src>
        void reflect_transfer_init_all(void* dst, Src&& src,
                                       XBufferCore::segment_manager* sm) {
            std::memset(dst, 0, sizeof(T));  // zero ONCE at top level
            reflect_transfer_init_all_impl<T>(dst, std::forward<Src>(src), sm);
        }

        // ── ReflectRoot<T> ──
        // Wrapper that constructs T via reflection on raw storage.
        // Layout: alignas(T) unsigned char[sizeof(T)] — same size as T.
        // Has an allocator constructor so it works with Boost.IPC construct().
        // Users never see this type; it's purely internal plumbing.
        template <typename T>
        struct ReflectRoot {
            alignas(T) unsigned char storage[sizeof(T)];

            template <typename Alloc>
            ReflectRoot(Alloc alloc) {
                reflect_init_all<T>(storage, alloc);
            }

            T& ref() noexcept {
                return *std::launder(reinterpret_cast<T*>(storage));
            }
            const T& ref() const noexcept {
                return *std::launder(reinterpret_cast<const T*>(storage));
            }
        };

        /// Construct the root object in managed memory.
        /// Dispatches: types with allocator ctor → old path, others → reflection.
        template <typename T>
        T* construct_root(XBufferCore& xbuf) {
            auto* sm = xbuf.get_segment_manager();
            if constexpr (has_segment_manager_ctor<T>) {
                return xbuf.template construct<T>(XBUFFER_ROOT_NAME)(sm);
            } else {
                auto* wrapper = xbuf.template construct<ReflectRoot<T>>(
                    XBUFFER_ROOT_NAME)(sm);
                return &wrapper->ref();
            }
        }

        /// Find the root object in managed memory.
        /// Returns nullptr if not found.
        template <typename T>
        T* find_root(XBufferCore& xbuf) {
            if constexpr (has_segment_manager_ctor<T>) {
                return xbuf.template find<T>(XBUFFER_ROOT_NAME).first;
            } else {
                auto result = xbuf.template find<ReflectRoot<T>>(XBUFFER_ROOT_NAME);
                return result.first ? &result.first->ref() : nullptr;
            }
        }

    } // namespace detail (reflection)

    // ========================================================================
    // XHandle<T> — Epoch-cached safe handle for the root object
    //
    // Caches the raw pointer + buffer epoch. On dereference, if the epoch
    // hasn't changed, returns the cached pointer in O(1).
    // If the epoch changed (remap or shrink_to_fit), re-finds the root.
    //
    // Cost model:
    //   - Normal grow (within reservation): O(1) — epoch unchanged, pointer stable
    //   - After remap (exceeds reservation) or shrink: O(log n) one-time re-find
    // ========================================================================
    template <typename T>
    class XHandle {
    public:
        XHandle() noexcept : buffer_(nullptr) {}

        explicit XHandle(XBufferCore& buf) noexcept
            : buffer_(&buf), cached_ptr_(nullptr), cached_epoch_(0)
        {
            resolve();
        }

        // Dereference — returns cached pointer or re-finds if epoch changed
        T* operator->() const {
            return resolve();
        }

        T& operator*() const {
            return *resolve();
        }

        // Explicit access — same semantics as operator->
        T* get() const {
            return resolve();
        }

        // Check if handle points to a valid (existing) object
        explicit operator bool() const {
            return resolve() != nullptr;
        }

    private:
        T* resolve() const {
            if (!buffer_) return nullptr;
            uint64_t current_epoch = buffer_->epoch();
            if (cached_ptr_ && cached_epoch_ == current_epoch) {
                return cached_ptr_;   // O(1) fast path
            }
            // Epoch changed or first access — re-find via reflection dispatch
            cached_ptr_ = detail::find_root<T>(*buffer_);
            cached_epoch_ = current_epoch;
            return cached_ptr_;
        }

        XBufferCore* buffer_;
        mutable T* cached_ptr_ = nullptr;
        mutable uint64_t cached_epoch_ = 0;
    };

    // ========================================================================
    // XBuffer — Single-Root-Object Model
    //
    // XBuffer is designed around a single root object per buffer:
    //   - make<T>()       creates the one root object
    //   - root<T>()       retrieves it
    //   - has_root<T>()   checks if it exists
    //
    // This is a deliberate simplification over the underlying Boost.IPC
    // multi-named-object capability. The single-root model eliminates the
    // need for string-based naming, provides a cleaner API, and matches the
    // common serialization pattern (one top-level object with nested containers).
    //
    // For advanced multi-object scenarios, use the base XBufferCore class directly
    // with construct<T>("name") / find<T>("name").
    // ========================================================================
    class XBuffer : public XBufferCore {
    public:
        using XBufferCore::XBufferCore;

        /// Capacity hint for controlling vector reserve size.
        ///
        /// By default, XBuffer uses adaptive reservation:
        ///   reserved = clamp(initial_size × 16, 64KB, 256MB)
        ///
        /// Use max_capacity() to override when you know the buffer's maximum size:
        ///   XBuffer buf(4096, XBuffer::max_capacity(64 * 1024 * 1024));  // 64MB
        ///
        /// For many small buffers (10,000+), the default adaptive policy is optimal.
        /// For few large buffers that need guaranteed no-relocation growth, set a large value.
        struct MaxCapacity { std::size_t value; };
        static MaxCapacity max_capacity(std::size_t bytes) { return {bytes}; }

        /// Construct with explicit max capacity (overrides adaptive reservation).
        XBuffer(std::size_t size, MaxCapacity cap)
            : XBufferCore(size, cap.value) {}

        /// Creates the single root object of type T in the buffer.
        /// Supports both traditional types (with allocator ctor) and
        /// zero-boilerplate pure aggregates (via C++26 reflection).
        ///
        /// WARNING: The returned pointer is a raw T* that becomes DANGLING
        /// after grow(), shrink_to_fit(), or compact. Use root<T>() or
        /// make_handle<T>() for safer access patterns.
        template<typename T>
        T* make() {
            validate_xbuffer_type<T>();
            if (detail::find_root<T>(*this) != nullptr) {
                throw boost::interprocess::interprocess_exception(
                    "make<T>(): root object already exists. "
                    "Call root<T>() to access the existing object.");
            }
            return detail::construct_root<T>(*this);
        }
        
        /// Returns a reference to the root object.
        /// Works with both traditional and zero-boilerplate types.
        template<typename T>
        T& root() {
            T* ptr = detail::find_root<T>(*this);
            if (!ptr) {
                throw boost::interprocess::interprocess_exception(
                    "root<T>(): no root object in buffer. "
                    "Call make<T>() first or check with has_root<T>().");
            }
            return *ptr;
        }

        /// Returns true if a root object of type T exists in this buffer.
        template<typename T>
        bool has_root() {
            return detail::find_root<T>(*this) != nullptr;
        }

        /// Creates the root object and returns an epoch-cached XHandle<T>.
        template<typename T>
        XHandle<T> make_handle() {
            make<T>();
            return XHandle<T>(*this);
        }

        // Returns an epoch-cached XHandle<T> to the existing root object.
        template<typename T>
        XHandle<T> handle() {
            return XHandle<T>(*this);
        }

        template<typename T>
        boost::interprocess::allocator<T, XBufferCore::segment_manager> allocator() {
            validate_xbuffer_type<T>();
            return boost::interprocess::allocator<T, XBufferCore::segment_manager>(this->get_segment_manager());
        }

        // Returns the number of bytes actually used (excluding free space).
        std::size_t used_size() {
            return stats().used_size;
        }

        // Serializes the buffer to a compact string.
        // Shrinks the segment first, then copies exactly the segment's logical
        // size (base_t::get_size()) — byte-exact, no padding.
        //
        // shrink_to_fit() only updates the rbtree logical size, does NOT
        // shrink the vector → base address unchanged, XHandle caches valid.
        std::string save() {
            this->shrink_to_fit();
            const char* base = static_cast<const char*>(this->get_address());
            std::size_t exact_size = this->segment_size();
            return std::string(base, exact_size);
        }

        // Serializes the full buffer without shrinking.
        // Output = segment logical size (byte-exact, no page padding).
        // Faster than save() since it skips shrink_to_fit().
        std::string save_raw() {
            const char* base = static_cast<const char*>(this->get_address());
            std::size_t exact_size = this->segment_size();
            return std::string(base, exact_size);
        }

        // Serializes the buffer to a compact vector<char>.
        // Shrinks first, output = exact segment logical size.
        // Ideal for network transfer or persistent storage.
        std::vector<char> save_bytes() {
            this->shrink_to_fit();
            const char* base = static_cast<const char*>(this->get_address());
            std::size_t exact_size = this->segment_size();
            return std::vector<char>(base, base + exact_size);
        }

        static XBuffer load(const std::string& data) {
            XBuffer xbuf(data.data(), data.size());
            return xbuf;
        }

        static XBuffer load(const std::vector<char>& data) {
            XBuffer xbuf(data.data(), data.size());
            return xbuf;
        }

        XBufferStats::MemoryStats stats() {
            return XBufferStats::memory_stats(*this);
        }

        // Estimates a suitable buffer size for the given user data payload.
        // Accounts for segment_manager overhead + 20% headroom for container growth.
        static std::size_t estimate_buffer_size(std::size_t user_data_bytes) {
            std::size_t min_overhead = XBufferCore::segment_manager::get_min_size();
            std::size_t estimated = min_overhead + user_data_bytes;
            estimated += estimated / 5;  // +20% headroom
            return std::max(estimated, (std::size_t)512);
        }
    };

    // ========================================================================
    // TypedXBuffer<T> — Type-bound buffer with compile-time root type
    //
    // Wraps XBuffer and binds it to a specific root type T at compile time.
    // Eliminates the need to repeat <T> on every make/root/handle call.
    //
    // Usage:
    //   TypedXBuffer<GameData> buf(4096);
    //   auto* game = buf.make();         // no <GameData> needed
    //   auto& g = buf.root();            // no <GameData> needed
    //   auto bytes = buf.save();          // standard save
    //   auto loaded = TypedXBuffer<GameData>::load(bytes);
    // ========================================================================
    template<typename T>
    class TypedXBuffer : public XBuffer {
    public:
        using XBuffer::XBuffer;

        /// Creates the single root object of type T.
        T* make() { return XBuffer::make<T>(); }

        /// Returns a reference to the root object.
        T& root() { return XBuffer::root<T>(); }

        /// Returns true if a root object exists.
        bool has_root() { return XBuffer::has_root<T>(); }

        /// Creates root and returns an epoch-cached handle.
        XHandle<T> make_handle() { return XBuffer::make_handle<T>(); }

        /// Returns an epoch-cached handle to the existing root.
        XHandle<T> handle() { return XBuffer::handle<T>(); }

        /// Load from serialized data and return a typed buffer.
        static TypedXBuffer load(const std::string& data) {
            return TypedXBuffer(XBuffer::load(data));
        }

        static TypedXBuffer load(const std::vector<char>& data) {
            return TypedXBuffer(XBuffer::load(data));
        }

        /// Construct from an existing XBuffer (e.g., after compaction).
        TypedXBuffer(XBuffer&& other) noexcept
            : XBuffer(std::move(other)) {}
    };

    // ================================================================
    // XCompactor — Automatic memory compaction using C++26 reflection
    //
    // Defined after XBuffer so that compact<T>() can return
    // XBuffer directly, giving callers immediate access to root<T>(),
    // save(), etc.
    // ================================================================
    class XCompactor {
    public:
        // ================================================================
        // Migration strategy enum & trait — public so XOFFSET_REGISTER_*
        // macros can specialize migrate_as from outside the class.
        // ================================================================
        enum class MigrateStrategy {
            TrivialCopy,      // direct assignment (primitives, enums, POD)
            AllocatorAware,   // reconstruct with new allocator (XString-like)
            Container,        // iterate elements, recurse (XVector/XSet/XMap)
            Composite,        // reflect members, recurse (user structs)
            NotRegistered     // use built-in auto-detection
        };

        template<typename T>
        struct migrate_as { static constexpr MigrateStrategy value = MigrateStrategy::NotRegistered; };

        // Built-in registrations for XOffset types:
        // Registered via XOFFSET_REGISTER_* unified macros (see end of file).
        // User-defined types can still specialize migrate_as manually.

        // Single-object compaction: migrates the root object to a new,
        // tightly-packed buffer.  Returns XBuffer for ergonomic access.
        //
        // Uses progressive allocation: tries 2x first, then 2.5x, 3x, 4x.
        // Most workloads succeed at 2x; the larger multipliers are fallbacks
        // for deeply nested structures with many small allocations where
        // per-allocation header overhead is significant.
        template<typename T>
        static XBuffer compact(XBufferCore& old_xbuf) {
            validate_xbuffer_type<T>();
            auto stats = XBufferStats::memory_stats(old_xbuf);
            auto* old_obj = detail::find_root<T>(old_xbuf);

            // Progressive multipliers: try smaller first, fall back to larger
            constexpr double multipliers[] = {2.0, 2.5, 3.0, 4.0};
            for (double mult : multipliers) {
                std::size_t new_size = static_cast<std::size_t>(stats.used_size * mult);
                if (new_size < 4096) new_size = 4096;

                try {
                    XBuffer new_xbuf(new_size);
                    if (!old_obj) {
                        return new_xbuf;
                    }
                    auto* new_obj = detail::construct_root<T>(new_xbuf);
                    migrate_members(*old_obj, *new_obj, old_xbuf, new_xbuf);
                    new_xbuf.shrink_to_fit();
                    return new_xbuf;
                } catch (const boost::interprocess::bad_alloc&) {
                    // Not enough headroom — try next multiplier
                    continue;
                }
            }
            // All multipliers exhausted — should not happen in practice
            throw boost::interprocess::interprocess_exception(
                "XCompactor::compact failed: insufficient memory even at 4x multiplier");
        }

    private:

        // Resolve migration strategy: user-registered > auto-detect
        //
        // Detection order:
        //   1. Explicit registration via migrate_as<T> (highest priority)
        //   2. Trivially copyable types → TrivialCopy
        //   3. Safety gate: types with allocator_type that are NOT registered
        //      are likely containers/allocator-aware types that need special
        //      migration — fall through to Composite would be incorrect.
        //   4. Everything else → Composite (reflection-based member migration)
        template<typename T>
        static consteval MigrateStrategy resolve_strategy() {
            using CleanT = std::remove_cv_t<T>;
            if constexpr (migrate_as<CleanT>::value != MigrateStrategy::NotRegistered) {
                return migrate_as<CleanT>::value;
            } else if constexpr (std::is_trivially_copyable_v<CleanT>) {
                return MigrateStrategy::TrivialCopy;
            } else if constexpr (requires { typename CleanT::allocator_type; }) {
                // F7 safety gate — distinguishes two categories:
                //
                //  (a) Containers (has iterator + begin/end): these directly manage
                //      allocated memory via their internal allocator. Composite
                //      (reflection) migration would NOT swap the allocator, leading
                //      to dangling references. These MUST be registered.
                //
                //  (b) User-defined allocator-aware composites (e.g., a struct
                //      containing XString): these merely propagate their allocator
                //      to sub-objects. Composite migration IS correct here because
                //      each member is individually resolved & migrated.
                //
                if constexpr (requires(const CleanT& c) {
                    typename CleanT::iterator;
                    { c.begin() };
                    { c.end()   };
                }) {
                    // Category (a): unregistered container — reject.
                    static_assert(
                        migrate_as<CleanT>::value != MigrateStrategy::NotRegistered,
                        "XCompactor: container type has allocator_type but no migrate_as "
                        "registration. Register it via "
                        "XOFFSET_REGISTER_TYPE(YourType, AllocatorAware) or "
                        "XOFFSET_REGISTER_TYPE(YourType, Container). See docs.");
                    return MigrateStrategy::Composite; // unreachable
                } else {
                    // Category (b): allocator-aware composite struct — safe for
                    // reflection-based Composite migration.
                    return MigrateStrategy::Composite;
                }
            } else {
                return MigrateStrategy::Composite;
            }
        }

        // ================================================================
        // Migration dispatch (uses migrate_as)
        // ================================================================
        template<typename ElementType>
        static auto migrate_element(const ElementType& old_elem, XBufferCore& old_xbuf, XBufferCore& new_xbuf) {
            constexpr auto strategy = resolve_strategy<ElementType>();
            if constexpr (strategy == MigrateStrategy::TrivialCopy) {
                return old_elem;
            } else if constexpr (strategy == MigrateStrategy::AllocatorAware) {
                return ElementType(old_elem, new_xbuf.get_segment_manager());
            } else if constexpr (detail::has_segment_manager_ctor<ElementType>) {
                // Traditional path: type has allocator constructor
                ElementType new_elem(new_xbuf.get_segment_manager());
                migrate_members(old_elem, new_elem, old_xbuf, new_xbuf);
                return std::move(new_elem);
            } else {
                // Zero-boilerplate path: pure aggregate, use reflection
                alignas(ElementType) unsigned char buf[sizeof(ElementType)];
                detail::reflect_init_all<ElementType>(buf, new_xbuf.get_segment_manager());
                ElementType& new_elem = *std::launder(reinterpret_cast<ElementType*>(buf));
                migrate_members(old_elem, new_elem, old_xbuf, new_xbuf);
                return std::move(new_elem);
            }
        }
        
        template<typename ContainerType>
        static void migrate_container(const ContainerType& old_container, 
                                      ContainerType& new_container,
                                      XBufferCore& old_xbuf, XBufferCore& new_xbuf) {
            using ElementType = typename ContainerType::value_type;

            if constexpr (std::is_trivially_copyable_v<ElementType>) {
                new_container = old_container;
                return;
            }
            
            if constexpr (MapLikeContainer<ContainerType>) {
                for (const auto& [key, value] : old_container) {
                    auto new_key = migrate_element(key, old_xbuf, new_xbuf);
                    auto new_value = migrate_element(value, old_xbuf, new_xbuf);
                    new_container.emplace(std::move(new_key), std::move(new_value));
                }
            } else {
                for (const auto& elem : old_container) {
                    auto migrated_elem = migrate_element(elem, old_xbuf, new_xbuf);
                    if constexpr (SetLikeContainer<ContainerType>) {
                        new_container.emplace(std::move(migrated_elem));
                    } else {
                        new_container.emplace_back(std::move(migrated_elem));
                    }
                }
            }
        }
        
        template<typename MemberType>
        static void migrate_member(const MemberType& old_member, MemberType& new_member, 
                                  XBufferCore& old_xbuf, XBufferCore& new_xbuf) {
            constexpr auto strategy = resolve_strategy<MemberType>();
            if constexpr (strategy == MigrateStrategy::TrivialCopy) {
                new_member = old_member;
            } else if constexpr (strategy == MigrateStrategy::AllocatorAware) {
                new_member = MemberType(old_member, new_xbuf.get_segment_manager());
            } else if constexpr (strategy == MigrateStrategy::Container) {
                migrate_container(old_member, new_member, old_xbuf, new_xbuf);
            } else {
                migrate_members(old_member, new_member, old_xbuf, new_xbuf);
            }
        }
        
        template<typename T, std::size_t Index>
        static consteval auto get_member_at() {
            using namespace std::meta;
            auto members = nonstatic_data_members_of(^^T, access_context::unchecked());
            return members[Index];
        }
        
        template<typename T, std::size_t Index>
        static void migrate_member_at(const T& old_obj, T& new_obj,
                                      XBufferCore& old_xbuf, XBufferCore& new_xbuf) {
            using namespace std::meta;
            constexpr auto member = get_member_at<T, Index>();
            using MemberType = [:type_of(member):];
            const auto& old_member = old_obj.[:member:];
            auto& new_member = new_obj.[:member:];
            migrate_member<MemberType>(old_member, new_member, old_xbuf, new_xbuf);
        }
        
        template<typename T, std::size_t... Is>
        static void migrate_members_impl(const T& old_obj, T& new_obj,
                                         XBufferCore& old_xbuf, XBufferCore& new_xbuf,
                                         std::index_sequence<Is...>) {
            (migrate_member_at<T, Is>(old_obj, new_obj, old_xbuf, new_xbuf), ...);
        }

        // ── Per-base migration: cast to base subobject, recurse ──
        template<typename T, std::size_t N>
        static void migrate_base_at(const T& old_obj, T& new_obj,
                                    XBufferCore& old_xbuf, XBufferCore& new_xbuf) {
            using namespace std::meta;
            constexpr auto base_info = bases_of(^^T, access_context::unchecked())[N];
            using BaseType = [:type_of(base_info):];
            const BaseType& old_base = static_cast<const BaseType&>(old_obj);
            BaseType& new_base = static_cast<BaseType&>(new_obj);
            migrate_members(old_base, new_base, old_xbuf, new_xbuf);
        }

        template<typename T, std::size_t... Is>
        static void migrate_bases_impl(const T& old_obj, T& new_obj,
                                       XBufferCore& old_xbuf, XBufferCore& new_xbuf,
                                       std::index_sequence<Is...>) {
            (migrate_base_at<T, Is>(old_obj, new_obj, old_xbuf, new_xbuf), ...);
        }
        
        template<typename T>
        static void migrate_members(const T& old_obj, T& new_obj, 
                                   XBufferCore& old_xbuf, XBufferCore& new_xbuf) {
            using namespace std::meta;
            // Migrate base class members first (recursive)
            // Use P2996 for iteration bounds — same source as migrate_base_at/migrate_member_at
            constexpr std::size_t base_count = bases_of(^^T, access_context::unchecked()).size();
            if constexpr (base_count > 0) {
                migrate_bases_impl(old_obj, new_obj, old_xbuf, new_xbuf,
                                  std::make_index_sequence<base_count>{});
            }
            // Then migrate direct members
            constexpr std::size_t member_count = nonstatic_data_members_of(^^T, access_context::unchecked()).size();
            if constexpr (member_count > 0) {
                migrate_members_impl(old_obj, new_obj, old_xbuf, new_xbuf,
                                    std::make_index_sequence<member_count>{});
            }
        }
    };
}

// ============================================================================
// Unified Registration Macros — XOFFSET_REGISTER_*
//
// Each macro performs TWO registrations in one call:
//   1. TypeLayout opaque signature  (boost::typelayout namespace)
//      — This is the "Type Set Extension Hook": it tells the TypeLayout
//        signature engine to skip the container shell and embed the element
//        signature instead. classify_safety<Container<T>>() then automatically
//        recurses into T's signature for safety classification.
//   2. Migration strategy           (migrate_as specialization)
//      — Used by XCompactor for runtime data migration between buffers.
//
// sizeof/alignof are auto-deduced — no manual size/align parameters needed.
//
// Usage (must be placed OUTSIDE all namespaces, after XOffsetDatastructure
// namespace is closed):
//
//   XOFFSET_REGISTER_TYPE(XString, "string", AllocatorAware)
//   XOFFSET_REGISTER_CONTAINER(XVector, "vector", Container)
//   XOFFSET_REGISTER_CONTAINER(XSet, "set", Container)
//   XOFFSET_REGISTER_MAP(XMap, "map", Container)
//
// Strategy options: TrivialCopy, AllocatorAware, Container, Composite
// ============================================================================

// F9: Global namespace sentinel — used by XOFFSET_REGISTER_* macros to detect
// if the user accidentally placed the macro inside a namespace block.
// The trick: we define this struct at global scope. Inside each macro we check
// std::is_same_v<::_XOffset_NS_Sentinel, _XOffset_NS_Sentinel>.
// At global scope both resolve to the same type → true.
// Inside any namespace, unqualified lookup fails or finds a different type → compile error.
struct _XOffset_NS_Sentinel {};

// Helper macro: emits a static_assert that fires when called inside a namespace.
#define XOFFSET_CHECK_GLOBAL_NAMESPACE_(macro_name)                            \
    static_assert(                                                             \
        ::std::is_same_v<::_XOffset_NS_Sentinel, _XOffset_NS_Sentinel>,       \
        macro_name " must be used at global namespace scope, "                 \
        "not inside any namespace { } block. "                                 \
        "Move the macro call outside all namespace declarations.");

// --- XOFFSET_REGISTER_TYPE(Type, name, strategy) ---
// For non-template types (e.g., XString).
// Hook 1: TypeLayout opaque signature — extends the safe type set S
// Hook 2: Migration strategy — runtime data migration
#define XOFFSET_REGISTER_TYPE(Type, name, strategy)                            \
    XOFFSET_CHECK_GLOBAL_NAMESPACE_("XOFFSET_REGISTER_TYPE")                   \
    namespace boost { namespace typelayout {                                    \
        TYPELAYOUT_OPAQUE_TYPE_AUTO(XOffsetDatastructure::Type, name)           \
    }}                                                                         \
    template<> struct XOffsetDatastructure::XCompactor::migrate_as<            \
        XOffsetDatastructure::Type> {                                          \
        static constexpr XOffsetDatastructure::XCompactor::MigrateStrategy     \
            value = XOffsetDatastructure::XCompactor::MigrateStrategy::strategy; \
    };

// --- XOFFSET_REGISTER_CONTAINER(Template, name, strategy) ---
// For single-type-parameter templates (e.g., XVector<T>, XSet<T>).
// Hook 1: TypeLayout opaque signature — embeds element signature (recursive safety)
// Hook 2: Migration strategy — runtime data migration
#define XOFFSET_REGISTER_CONTAINER(Template, name, strategy)                   \
    XOFFSET_CHECK_GLOBAL_NAMESPACE_("XOFFSET_REGISTER_CONTAINER")              \
    namespace boost { namespace typelayout {                                    \
        TYPELAYOUT_OPAQUE_CONTAINER_AUTO(XOffsetDatastructure::Template, name)  \
    }}                                                                         \
    template<typename T_> struct XOffsetDatastructure::XCompactor::migrate_as< \
        XOffsetDatastructure::Template<T_>> {                                  \
        static constexpr XOffsetDatastructure::XCompactor::MigrateStrategy     \
            value = XOffsetDatastructure::XCompactor::MigrateStrategy::strategy; \
    };

// --- XOFFSET_REGISTER_MAP(Template, name, strategy) ---
// For two-type-parameter templates (e.g., XMap<K,V>).
// Hook 1: TypeLayout opaque signature — embeds key+value signatures
// Hook 2: Migration strategy — runtime data migration
#define XOFFSET_REGISTER_MAP(Template, name, strategy)                         \
    XOFFSET_CHECK_GLOBAL_NAMESPACE_("XOFFSET_REGISTER_MAP")                    \
    namespace boost { namespace typelayout {                                    \
        TYPELAYOUT_OPAQUE_MAP_AUTO(XOffsetDatastructure::Template, name)        \
    }}                                                                         \
    template<typename K_, typename V_>                                          \
    struct XOffsetDatastructure::XCompactor::migrate_as<                        \
        XOffsetDatastructure::Template<K_, V_>> {                              \
        static constexpr XOffsetDatastructure::XCompactor::MigrateStrategy     \
            value = XOffsetDatastructure::XCompactor::MigrateStrategy::strategy; \
    };

// ============================================================================
// Built-in XOffsetDatastructure container registrations
// ============================================================================
XOFFSET_REGISTER_TYPE(XString, "string", AllocatorAware)
XOFFSET_REGISTER_CONTAINER(XVector, "vector", Container)
XOFFSET_REGISTER_CONTAINER(XSet, "set", Container)
XOFFSET_REGISTER_MAP(XMap, "map", Container)

#endif
