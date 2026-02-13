#ifndef X_OFFSET_DATA_STRUCTURE_HPP
#define X_OFFSET_DATA_STRUCTURE_HPP

#if defined(_MSC_VER)
    #define TYPESIG_PLATFORM_WINDOWS 1
    #define IS_LITTLE_ENDIAN 1
    #define FUNCTION_SIGNATURE __FUNCSIG__
#elif defined(__clang__) || defined(__GNUC__)
    #define TYPESIG_PLATFORM_WINDOWS 0
    #define FUNCTION_SIGNATURE __PRETTY_FUNCTION__
    #define IS_LITTLE_ENDIAN (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__)
#else
    #error "Unsupported compiler"
#endif

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
#include <sstream>
#include <string>
#include <tuple>
#include <vector>

// TypeLayout library — the authoritative type-signature engine
// Use boost::typelayout directly for all type signature operations:
//   - boost::typelayout::get_definition_signature<T>()
//   - boost::typelayout::get_layout_signature<T>()
//   - boost::typelayout::definition_signatures_match<T1, T2>()
//   - boost::typelayout::layout_signatures_match<T1, T2>()
#include <boost/typelayout.hpp>
#include <boost/container/scoped_allocator.hpp>

// ============================================================================
// Target Architecture Definition
//
// XOffset defines two explicit sets:
//   A (Architecture Set) — what platform the data lives on
//   S (Safe Type Subset) — what types can be stored
// Zero-copy safety = Platform ∈ A ∧ Type ∈ S
// ============================================================================
namespace XOffsetDatastructure {

    /// Architecture specification descriptor (pure data, no logic)
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
    };

    // Architecture Presets.
    // Currently only Arch64LE is actively supported and tested.
    // The other presets are defined for forward-compatibility and
    // cross-platform migration tooling (see §6.2 in CORE_FORMAL_MODEL.md).
    inline constexpr ArchSpec Arch64LE = {
        .pointer_size = 8, .little_endian = true,
        .sizeof_int8 = 1, .sizeof_int16 = 2, .sizeof_int32 = 4, .sizeof_int64 = 8,
        .sizeof_float = 4, .sizeof_double = 8, .sizeof_bool = 1, .sizeof_char = 1,
        .pointer_align = 8,
        .alignof_int32 = 4, .alignof_int64 = 8,
        .alignof_float = 4, .alignof_double = 8,
    };
    inline constexpr ArchSpec Arch64BE = {
        .pointer_size = 8, .little_endian = false,
        .sizeof_int8 = 1, .sizeof_int16 = 2, .sizeof_int32 = 4, .sizeof_int64 = 8,
        .sizeof_float = 4, .sizeof_double = 8, .sizeof_bool = 1, .sizeof_char = 1,
        .pointer_align = 8,
        .alignof_int32 = 4, .alignof_int64 = 8,
        .alignof_float = 4, .alignof_double = 8,
    };
    inline constexpr ArchSpec Arch32LE = {
        .pointer_size = 4, .little_endian = true,
        .sizeof_int8 = 1, .sizeof_int16 = 2, .sizeof_int32 = 4, .sizeof_int64 = 8,
        .sizeof_float = 4, .sizeof_double = 8, .sizeof_bool = 1, .sizeof_char = 1,
        .pointer_align = 4,
        .alignof_int32 = 4, .alignof_int64 = 8,
        .alignof_float = 4, .alignof_double = 8,
    };
    inline constexpr ArchSpec Arch32BE = {
        .pointer_size = 4, .little_endian = false,
        .sizeof_int8 = 1, .sizeof_int16 = 2, .sizeof_int32 = 4, .sizeof_int64 = 8,
        .sizeof_float = 4, .sizeof_double = 8, .sizeof_bool = 1, .sizeof_char = 1,
        .pointer_align = 4,
        .alignof_int32 = 4, .alignof_int64 = 8,
        .alignof_float = 4, .alignof_double = 8,
    };

    /// Active target architecture. Change this line to switch presets.
    inline constexpr ArchSpec TargetArchitecture = Arch64LE;

} // namespace XOffsetDatastructure

// ============================================================================
// Platform validation: current compiler environment ∈ TargetArchitecture
// ============================================================================
static_assert(sizeof(void*) == XOffsetDatastructure::TargetArchitecture.pointer_size,
    "Platform pointer size does not match TargetArchitecture");
static_assert(IS_LITTLE_ENDIAN == XOffsetDatastructure::TargetArchitecture.little_endian,
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

    XManagedMemory() noexcept
    {
    }

    ~XManagedMemory()
    {
        this->priv_close();
    }

    // Returns a monotonically increasing counter that is bumped whenever
    // the buffer's backing memory is reallocated (grow / shrink / compact).
    // Used by XHandle<T> to detect stale cached pointers.
    uint64_t epoch() const noexcept { return m_epoch; }

    XManagedMemory(size_type size)
        : m_buffer(size, char(0))
    {
        void *addr = m_buffer.data();
        if (!base_t::create_impl(addr, size))
        {
            this->priv_close();
            throw interprocess_exception("Could not initialize heap in XManagedMemory constructor");
        }
    }

    XManagedMemory(const char* data, size_type size)
        : m_buffer(data, data + size)
    {
        void *addr = m_buffer.data();
        BOOST_ASSERT((0 == (((std::size_t)addr) & (AllocationAlgorithm::Alignment - size_type(1u)))));
        if (!base_t::open_impl(addr, size))
        {
            throw interprocess_exception("Could not initialize m_buffer in constructor");
        }
    }

    XManagedMemory(std::vector<char> &externalBuffer) 
        : m_buffer(std::move(externalBuffer))
    {
        void *addr = m_buffer.data();
        size_type size = m_buffer.size();
        BOOST_ASSERT((0 == (((std::size_t)addr) & (AllocationAlgorithm::Alignment - size_type(1u)))));
        if (!base_t::open_impl(addr, size))
        {
            throw interprocess_exception("Could not initialize m_buffer in constructor");
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

    // Grows the buffer by extra_bytes. Best-effort rollback on failure:
    // if resize succeeds but re-open fails, restores original size.
    //
    // WARNING: All existing pointers, references, and iterators into the
    // buffer are INVALIDATED after this call. The underlying std::vector
    // may relocate to a new heap address. Re-acquire pointers via root<T>().
    bool grow(size_type extra_bytes)
    {
        const size_type original_size = m_buffer.size();
        try
        {
            m_buffer.resize(original_size + extra_bytes);
            base_t::close_impl();
            if (!base_t::open_impl(&m_buffer[0], m_buffer.size()))
            {
                m_buffer.resize(original_size);
                base_t::open_impl(&m_buffer[0], m_buffer.size());
                return false;
            }
            base_t::grow(extra_bytes);
            ++m_epoch;
            return true;
        }
        catch(...)
        {
            // Best-effort rollback: try to restore original size
            try { m_buffer.resize(original_size); } catch(...) {}
            try { base_t::open_impl(&m_buffer[0], m_buffer.size()); } catch(...) {}
            return false;
        }
    }

    void swap(XManagedMemory &other) noexcept
    {
        base_t::swap(other);
        m_buffer.swap(other.m_buffer);
    }

    // WARNING: Invalidates ALL existing pointers/references into this buffer.
    // After calling, re-acquire object pointers via root<T>() or handle<T>().
    //
    // Optimized: 2 copies instead of 3 — reuses close/open pattern from grow().
    void shrink_to_fit()
    {
        base_t::shrink_to_fit();
        m_buffer.resize(base_t::get_size());
        base_t::close_impl();
        base_t::open_impl(&m_buffer[0], m_buffer.size());
        ++m_epoch;
    }

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

private:
    void priv_close()
    {
        base_t::destroy_impl();
        std::vector<char>().swap(m_buffer);
    }

    std::vector<char> m_buffer;
    uint64_t m_epoch = 0;
};

} // namespace interprocess
} // namespace boost


namespace XOffsetDatastructure {
    using namespace boost::interprocess;

    using XBuffer = XManagedMemory<char, x_seq_fit<null_mutex_family>, iset_index>;
    // Alternative allocator policy (rbtree best-fit). Currently unused —
    // provided for future experimentation with allocation strategies.
    using XBufferBestFit = XManagedMemory<char, x_best_fit<null_mutex_family>, iset_index>;

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

        /// Scoped allocator adaptor: automatically propagates the allocator
        /// to element construction via allocator_traits::construct().
        /// This is the C++ standard answer to "allocator propagation in
        /// nested containers" (N2554, scoped_allocator_adaptor).
        /// Zero overhead: inherits from OuterAlloc, adds no data members.
        template <typename T>
        using x_scoped_alloc = boost::container::scoped_allocator_adaptor<
            boost::interprocess::allocator<T, XBuffer::segment_manager>>;

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
        char, std::char_traits<char>, allocator<char, XBuffer::segment_manager>>;

    /// Convenience allocator typedef for user-defined allocator-aware types.
    /// Usage:  using allocator_type = XAllocator;
    using XAllocator = boost::interprocess::allocator<char, XBuffer::segment_manager>;

    // ========================================================================
    // Public Container Wrapper Classes
    //
    // Two layers of allocator convenience:
    //   Layer 1: scoped_allocator_adaptor — auto-injects allocator in all
    //            emplace/emplace_back/emplace_hint paths (via construct()).
    //   Layer 2: Wrapper overloads below — cover push_back, insert, operator[],
    //            erase, resize, assign where the base API signature requires
    //            a fully-constructed T or key_type.
    //
    // Detection logic (requires constraints):
    //   "If args can't directly construct T, but (args..., SM*) can → inject SM*"
    //   "If key arg isn't convertible to K → use find + emplace"
    // ========================================================================

    /// Managed vector with 1.1x growth factor and automatic allocator propagation.
    template <typename T>
    class XVector : public detail::x_vector_impl<T> {
        using Base = detail::x_vector_impl<T>;
        using SM = XBuffer::segment_manager;
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
        using SM = XBuffer::segment_manager;
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
        using SM = XBuffer::segment_manager;

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

    class XBufferVisualizer {
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

        static MemoryStats get_memory_stats(XBuffer& xbuf) {
            MemoryStats stats = {};
            stats.total_size = xbuf.get_size();
            stats.free_size = xbuf.get_free_memory();
            stats.used_size = stats.total_size - stats.free_size;
            return stats;
        }

        static void print_stats(XBuffer& xbuf) {
            MemoryStats stats = get_memory_stats(xbuf);
            std::cout << "XBuffer: " << stats.used_size << "/" << stats.total_size 
                      << " bytes (" << std::fixed << std::setprecision(1) 
                      << stats.usage_percent() << "% used)" << std::endl;
        }
    };

    // Forward declaration for safety gate in XBufferCompactor
    namespace detail {
        template<typename T> consteval bool is_safe_type();
    }
    template<typename T> constexpr void validate_xbuffer_type();

    // Internal constant for the single root object name.
    // Users never see this — all public APIs hide the naming layer.
    inline constexpr const char* XBUFFER_ROOT_NAME = "__root__";

    namespace detail {

        // ============================================================================
        // Safe Type Subset — Explicit Whitelist (is_safe_leaf)
        //
        // The Safe Type Subset S is defined by two mechanisms:
        //   1. is_safe_leaf<T>  — declares leaf types that are directly safe
        //   2. Recursive check  — composite types are safe if all members ∈ S
        // No blacklist needed: types not in the whitelist are rejected.
        // ============================================================================

        template<typename T> struct is_safe_leaf : std::false_type {};

        // — LEAF-1: Primitives (fixed-width, architecture-independent) —
        template<> struct is_safe_leaf<int8_t>   : std::true_type {};
        template<> struct is_safe_leaf<int16_t>  : std::true_type {};
        template<> struct is_safe_leaf<int32_t>  : std::true_type {};
        template<> struct is_safe_leaf<int64_t>  : std::true_type {};
        template<> struct is_safe_leaf<uint8_t>  : std::true_type {};
        template<> struct is_safe_leaf<uint16_t> : std::true_type {};
        template<> struct is_safe_leaf<uint32_t> : std::true_type {};
        template<> struct is_safe_leaf<uint64_t> : std::true_type {};
        template<> struct is_safe_leaf<float>    : std::true_type {};
        template<> struct is_safe_leaf<double>   : std::true_type {};
        template<> struct is_safe_leaf<bool>     : std::true_type {};
        template<> struct is_safe_leaf<char>     : std::true_type {};

        // — LEAF-2: Enums —
        // Not registered here; enums are checked dynamically in is_safe_type()
        // via TypeLayout's is_fixed_enum<T>().

        // — LEAF-3/4: XString & XContainers —
        // Registered via XOFFSET_REGISTER_* unified macros (see end of file).
        // User-defined types can still specialize is_safe_leaf manually.

        // — LEAF-5: XOffsetPtr<T> — NOT registered by default.
        //
        // XOffsetPtr is reference-semantic (points to data it does not own).
        // All LEAF-1~4 types are value-semantic and self-contained.
        // XOffsetPtr's validity depends on external state (the target object),
        // so it is excluded from the default Safe Type Subset.
        //
        // User opt-in: specialize is_safe_leaf for your specific use case:
        //
        //   template<>
        //   struct XOffsetDatastructure::detail::is_safe_leaf<XOffsetPtr<MyType>>
        //       : std::true_type {};
        //
        // If you also need compaction support, register a migrate_as strategy.
        // The library does NOT provide built-in migration for XOffsetPtr.

        // ============================================================================
        // Recursive safety check helpers
        // ============================================================================

        template<typename T>
        consteval bool has_bases() {
            using namespace std::meta;
            auto bases = bases_of(^^T, access_context::unchecked());
            return bases.size() > 0;
        }

        template<typename T>
        consteval bool is_safe_type();

        template<typename T, std::size_t Index>
        consteval bool is_member_safe_at() {
            using namespace std::meta;
            constexpr auto member = nonstatic_data_members_of(^^T, access_context::unchecked())[Index];
            using MemberType = [:type_of(member):];
            return is_safe_type<MemberType>();
        }
        
        template<typename T, std::size_t... Indices>
        consteval bool check_all_members_impl(std::index_sequence<Indices...>) {
            return (is_member_safe_at<T, Indices>() && ...);
        }
        
        template<typename T>
        consteval bool are_all_members_safe() {
            using namespace std::meta;
            
            if constexpr (!std::is_class_v<T>) return false;
            if constexpr (std::is_polymorphic_v<T>) return false;
            if constexpr (has_bases<T>()) return false;
            if constexpr (std::is_union_v<T>) return false;
            
            constexpr std::size_t member_count = nonstatic_data_members_of(^^T, access_context::unchecked()).size();
            if constexpr (member_count == 0) {
                return true;
            } else {
                return check_all_members_impl<T>(std::make_index_sequence<member_count>{});
            }
        }

        // ============================================================================
        // is_safe_type<T>() — unified safety check using whitelist
        // ============================================================================
        template<typename T>
        consteval bool is_safe_type() {
            using CleanT = std::remove_cv_t<T>;

            // 1. Leaf types: check whitelist
            if constexpr (is_safe_leaf<CleanT>::value) {
                // Containers need recursive element check
                if constexpr (requires { typename CleanT::key_type; typename CleanT::mapped_type; }) {
                    return is_safe_type<typename CleanT::key_type>() &&
                           is_safe_type<typename CleanT::mapped_type>();
                } else if constexpr (requires { typename CleanT::value_type; }) {
                    return is_safe_type<typename CleanT::value_type>();
                }
                return true;
            }

            // 2. Enums: delegate to TypeLayout
            if constexpr (std::is_enum_v<CleanT>) {
                return boost::typelayout::is_fixed_enum<CleanT>();
            }

            // 3. Composite types: recursive member check
            if constexpr (std::is_class_v<CleanT>) {
                return are_all_members_safe<CleanT>();
            }

            // 4. Everything else: rejected (pointers, references, etc.)
            return false;
        }
        
        template<typename T>
        consteval const char* get_safety_error_message() {
            using CleanT = std::remove_cv_t<T>;
            
            if constexpr (is_safe_type<CleanT>()) {
                return "Type is SAFE for XBuffer";
            }
            else if constexpr (std::is_polymorphic_v<CleanT>) {
                return "UNSAFE: Type has virtual functions (polymorphic)";
            }
            else if constexpr (has_bases<CleanT>()) {
                return "UNSAFE: Inheritance not allowed (use composition)";
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
            else if constexpr (std::is_class_v<CleanT>) {
                return "UNSAFE: Struct/class contains unsafe members";
            }
            else {
                return "UNSAFE: Type not allowed in XBuffer";
            }
        }
    }
    
    template<typename T>
    struct is_xbuffer_safe {
        static constexpr bool value = detail::is_safe_type<T>();
        
        static constexpr const char* reason() {
            return detail::get_safety_error_message<T>();
        }
    };
    
    // Per-member diagnostic helper: triggers a static_assert for each unsafe member,
    // so the compiler error points to the exact field name.
    template<typename T>
    consteval void diagnose_unsafe_members() {
        if constexpr (std::is_class_v<T> && !std::is_polymorphic_v<T> && !std::is_union_v<T>) {
            template for (constexpr auto member :
                std::meta::nonstatic_data_members_of(^^T, std::meta::access_context::unchecked())) {
                using MemberT = [:std::meta::type_of(member):];
                static_assert(
                    detail::is_safe_type<MemberT>(),
                    "Unsafe member detected in XBuffer type (see compiler note for field name and type)");
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
            "    (no virtual functions, no raw pointers)\n\n"
            "NOT ALLOWED:\n"
            "  ✗ Virtual functions (polymorphic types)\n"
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
    // XHandle<T> — Epoch-cached safe handle for the root object
    //
    // Caches the raw pointer + buffer epoch. On dereference, if the epoch
    // hasn't changed (no grow/shrink/compact happened), returns the cached
    // pointer in O(1). If the epoch changed, re-finds the root object
    // and updates the cache.
    //
    // Cost model:
    //   - No resize between accesses: O(1) (single uint64_t comparison)
    //   - After resize: O(log n) one-time re-find, then O(1) again
    // ========================================================================
    template <typename T>
    class XHandle {
    public:
        XHandle() noexcept : buffer_(nullptr) {}

        explicit XHandle(XBuffer& buf) noexcept
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
            // Epoch changed or first access — re-find
            auto result = buffer_->find<T>(XBUFFER_ROOT_NAME);
            cached_ptr_ = result.first;
            cached_epoch_ = current_epoch;
            return cached_ptr_;
        }

        XBuffer* buffer_;
        mutable T* cached_ptr_ = nullptr;
        mutable uint64_t cached_epoch_ = 0;
    };

    // ========================================================================
    // XBufferExt — Single-Root-Object Model
    //
    // XBufferExt is designed around a single root object per buffer:
    //   - make<T>()       creates the one root object
    //   - root<T>()       retrieves it
    //   - has_root<T>()   checks if it exists
    //
    // This is a deliberate simplification over the underlying Boost.IPC
    // multi-named-object capability. The single-root model eliminates the
    // need for string-based naming, provides a cleaner API, and matches the
    // common serialization pattern (one top-level object with nested containers).
    //
    // For advanced multi-object scenarios, use the base XBuffer class directly
    // with construct<T>("name") / find<T>("name").
    // ========================================================================
    class XBufferExt : public XBuffer {
    public:
        using XBuffer::XBuffer;

        // Constructs the single root object of type T in the buffer.
        //
        // WARNING: The returned pointer is a raw T* that becomes DANGLING
        // after grow(), shrink_to_fit(), or compact. Use root<T>() or
        // make_handle<T>() for safer access patterns.
        template<typename T>
        T* make() {
            validate_xbuffer_type<T>();
            if (this->find<T>(XBUFFER_ROOT_NAME).first != nullptr) {
                throw boost::interprocess::interprocess_exception(
                    "make<T>(): root object already exists. "
                    "Call root<T>() to access the existing object.");
            }
            return this->construct<T>(XBUFFER_ROOT_NAME)(this->get_segment_manager());
        }
        
        // Returns a reference to the root object. Use after deserialization
        // or after grow/shrink to re-acquire a valid reference.
        //
        // Throws if the root object does not exist.
        template<typename T>
        T& root() {
            auto result = this->find<T>(XBUFFER_ROOT_NAME);
            if (!result.first) {
                throw boost::interprocess::interprocess_exception(
                    "root<T>(): no root object in buffer. "
                    "Call make<T>() first or check with has_root<T>().");
            }
            return *result.first;
        }

        // Returns true if a root object of type T exists in this buffer.
        template<typename T>
        bool has_root() {
            return this->find<T>(XBUFFER_ROOT_NAME).first != nullptr;
        }

        // Creates the root object and returns an epoch-cached XHandle<T>.
        // The handle automatically re-finds the object after grow/shrink/compact.
        template<typename T>
        XHandle<T> make_handle() {
            validate_xbuffer_type<T>();
            this->construct<T>(XBUFFER_ROOT_NAME)(this->get_segment_manager());
            return XHandle<T>(*this);
        }

        // Returns an epoch-cached XHandle<T> to the existing root object.
        template<typename T>
        XHandle<T> handle() {
            return XHandle<T>(*this);
        }

        template<typename T>
        boost::interprocess::allocator<T, XBuffer::segment_manager> allocator() {
            validate_xbuffer_type<T>();
            return boost::interprocess::allocator<T, XBuffer::segment_manager>(this->get_segment_manager());
        }

        // Returns the number of bytes actually used (excluding free space).
        std::size_t used_size() {
            return stats().used_size;
        }

        // Serializes the buffer to a compact string by shrinking first.
        // Output size ≈ used_size(). This is the recommended serialization method.
        //
        // WARNING: Invalidates all existing pointers. Re-acquire via root<T>() after calling.
        std::string save_to_string() {
            this->shrink_to_fit();
            auto* buffer = this->get_buffer();
            return std::string(buffer->begin(), buffer->end());
        }

        // Serializes the full buffer including free space (no shrink).
        // Faster than save_to_string() but output includes unused padding.
        // Use when performance matters and output size is not a concern.
        std::string save_to_string_full() {
            auto* buffer = this->get_buffer();
            return std::string(buffer->begin(), buffer->end());
        }

        // Serializes the buffer to a compact vector<char> by shrinking first.
        // Output size ≈ used_size(). Ideal for network transfer or persistent storage.
        //
        // WARNING: Invalidates all existing pointers. Re-acquire via root<T>() after calling.
        std::vector<char> save_to_vector() {
            this->shrink_to_fit();
            auto* buf = this->get_buffer();
            return std::vector<char>(buf->begin(), buf->end());
        }

        static XBufferExt load_from_string(const std::string& data) {
            std::vector<char> buffer(data.begin(), data.end());
            XBufferExt xbuf(buffer);
            return xbuf;
        }

        static XBufferExt load_from_vector(const std::vector<char>& data) {
            std::vector<char> buffer(data);
            XBufferExt xbuf(buffer);
            return xbuf;
        }

        XBufferVisualizer::MemoryStats stats() {
            return XBufferVisualizer::get_memory_stats(*this);
        }

        // Estimates a suitable buffer size for the given user data payload.
        // Accounts for segment_manager overhead + 20% headroom for container growth.
        static std::size_t estimate_buffer_size(std::size_t user_data_bytes) {
            std::size_t min_overhead = XBuffer::segment_manager::get_min_size();
            std::size_t estimated = min_overhead + user_data_bytes;
            estimated += estimated / 5;  // +20% headroom
            return std::max(estimated, (std::size_t)512);
        }
    };

    // ================================================================
    // XBufferCompactor — Automatic memory compaction using C++26 reflection
    //
    // Defined after XBufferExt so that compact_automatic<T>() can return
    // XBufferExt directly, giving callers immediate access to root<T>(),
    // save_to_string(), etc.
    // ================================================================
    class XBufferCompactor {
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
        // tightly-packed buffer.  Returns XBufferExt for ergonomic access.
        template<typename T>
        static XBufferExt compact_automatic(XBuffer& old_xbuf) {
            validate_xbuffer_type<T>();
            auto stats = XBufferVisualizer::get_memory_stats(old_xbuf);
            std::size_t new_size = stats.used_size + (stats.used_size / 10);
            if (new_size < 4096) new_size = 4096;
            
            XBufferExt new_xbuf(new_size);
            auto* old_obj = old_xbuf.find<T>(XBUFFER_ROOT_NAME).first;
            if (!old_obj) {
                return new_xbuf;
            }
            
            auto* new_obj = new_xbuf.construct<T>(XBUFFER_ROOT_NAME)(new_xbuf.get_segment_manager());
            migrate_members(*old_obj, *new_obj, old_xbuf, new_xbuf);
            new_xbuf.shrink_to_fit();
            return new_xbuf;
        }

    private:

        // Resolve migration strategy: user-registered > auto-detect
        template<typename T>
        static consteval MigrateStrategy resolve_strategy() {
            using CleanT = std::remove_cv_t<T>;
            if constexpr (migrate_as<CleanT>::value != MigrateStrategy::NotRegistered) {
                return migrate_as<CleanT>::value;
            } else if constexpr (std::is_trivially_copyable_v<CleanT>) {
                return MigrateStrategy::TrivialCopy;
            } else {
                return MigrateStrategy::Composite;
            }
        }

        // ================================================================
        // Migration dispatch (uses is_safe_leaf + migrate_as)
        // ================================================================
        template<typename ElementType>
        static auto migrate_element(const ElementType& old_elem, XBuffer& old_xbuf, XBuffer& new_xbuf) {
            constexpr auto strategy = resolve_strategy<ElementType>();
            if constexpr (strategy == MigrateStrategy::TrivialCopy) {
                return old_elem;
            } else if constexpr (strategy == MigrateStrategy::AllocatorAware) {
                return ElementType(old_elem, new_xbuf.get_segment_manager());
            } else {
                ElementType new_elem(new_xbuf.get_segment_manager());
                migrate_members(old_elem, new_elem, old_xbuf, new_xbuf);
                return new_elem;
            }
        }
        
        template<typename ContainerType>
        static void migrate_container(const ContainerType& old_container, 
                                      ContainerType& new_container,
                                      XBuffer& old_xbuf, XBuffer& new_xbuf) {
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
                                  XBuffer& old_xbuf, XBuffer& new_xbuf) {
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
                                      XBuffer& old_xbuf, XBuffer& new_xbuf) {
            using namespace std::meta;
            constexpr auto member = get_member_at<T, Index>();
            using MemberType = [:type_of(member):];
            const auto& old_member = old_obj.[:member:];
            auto& new_member = new_obj.[:member:];
            migrate_member<MemberType>(old_member, new_member, old_xbuf, new_xbuf);
        }
        
        template<typename T, std::size_t... Is>
        static void migrate_members_impl(const T& old_obj, T& new_obj,
                                         XBuffer& old_xbuf, XBuffer& new_xbuf,
                                         std::index_sequence<Is...>) {
            (migrate_member_at<T, Is>(old_obj, new_obj, old_xbuf, new_xbuf), ...);
        }
        
        template<typename T>
        static void migrate_members(const T& old_obj, T& new_obj, 
                                   XBuffer& old_xbuf, XBuffer& new_xbuf) {
            constexpr std::size_t member_count = boost::typelayout::get_member_count<T>();
            migrate_members_impl(old_obj, new_obj, old_xbuf, new_xbuf,
                                std::make_index_sequence<member_count>{});
        }
    };
}

// ============================================================================
// Unified Registration Macros — XOFFSET_REGISTER_*
//
// Each macro performs THREE registrations in one call:
//   1. TypeLayout opaque signature  (boost::typelayout namespace)
//   2. Safety whitelist entry       (is_safe_leaf specialization)
//   3. Migration strategy           (migrate_as specialization)
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

// --- XOFFSET_REGISTER_TYPE(Type, name, strategy) ---
// For non-template types (e.g., XString).
#define XOFFSET_REGISTER_TYPE(Type, name, strategy)                            \
    namespace boost { namespace typelayout {                                    \
        TYPELAYOUT_OPAQUE_TYPE_AUTO(XOffsetDatastructure::Type, name)           \
    }}                                                                         \
    template<> struct XOffsetDatastructure::detail::is_safe_leaf<               \
        XOffsetDatastructure::Type> : std::true_type {};                       \
    template<> struct XOffsetDatastructure::XBufferCompactor::migrate_as<       \
        XOffsetDatastructure::Type> {                                          \
        static constexpr XOffsetDatastructure::XBufferCompactor::MigrateStrategy \
            value = XOffsetDatastructure::XBufferCompactor::MigrateStrategy::strategy; \
    };

// --- XOFFSET_REGISTER_CONTAINER(Template, name, strategy) ---
// For single-type-parameter templates (e.g., XVector<T>, XSet<T>).
#define XOFFSET_REGISTER_CONTAINER(Template, name, strategy)                   \
    namespace boost { namespace typelayout {                                    \
        TYPELAYOUT_OPAQUE_CONTAINER_AUTO(XOffsetDatastructure::Template, name)  \
    }}                                                                         \
    template<typename T_> struct XOffsetDatastructure::detail::is_safe_leaf<    \
        XOffsetDatastructure::Template<T_>> : std::true_type {};               \
    template<typename T_> struct XOffsetDatastructure::XBufferCompactor::migrate_as< \
        XOffsetDatastructure::Template<T_>> {                                  \
        static constexpr XOffsetDatastructure::XBufferCompactor::MigrateStrategy \
            value = XOffsetDatastructure::XBufferCompactor::MigrateStrategy::strategy; \
    };

// --- XOFFSET_REGISTER_MAP(Template, name, strategy) ---
// For two-type-parameter templates (e.g., XMap<K,V>).
#define XOFFSET_REGISTER_MAP(Template, name, strategy)                         \
    namespace boost { namespace typelayout {                                    \
        TYPELAYOUT_OPAQUE_MAP_AUTO(XOffsetDatastructure::Template, name)        \
    }}                                                                         \
    template<typename K_, typename V_>                                          \
    struct XOffsetDatastructure::detail::is_safe_leaf<                          \
        XOffsetDatastructure::Template<K_, V_>> : std::true_type {};           \
    template<typename K_, typename V_>                                          \
    struct XOffsetDatastructure::XBufferCompactor::migrate_as<                  \
        XOffsetDatastructure::Template<K_, V_>> {                              \
        static constexpr XOffsetDatastructure::XBufferCompactor::MigrateStrategy \
            value = XOffsetDatastructure::XBufferCompactor::MigrateStrategy::strategy; \
    };

// ============================================================================
// Built-in XOffsetDatastructure container registrations
// ============================================================================
XOFFSET_REGISTER_TYPE(XString, "string", AllocatorAware)
XOFFSET_REGISTER_CONTAINER(XVector, "vector", Container)
XOFFSET_REGISTER_CONTAINER(XSet, "set", Container)
XOFFSET_REGISTER_MAP(XMap, "map", Container)

#endif
