#ifndef X_OFFSET_DATA_STRUCTURE_2_HPP
#define X_OFFSET_DATA_STRUCTURE_2_HPP

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
        #error "XOffsetDatastructure2 requires 64-bit architecture"
    #endif
    #if !XOFFSET_LITTLE_ENDIAN
        #error "XOffsetDatastructure2 requires little-endian architecture"
    #endif
#endif


#include <experimental/meta>
#include <type_traits>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

// TypeLayout library — the authoritative type-signature engine
// Use boost::typelayout directly for all type signature operations:
//   - boost::typelayout::get_definition_signature<T>()
//   - boost::typelayout::get_layout_signature<T>()
//   - boost::typelayout::definition_signatures_match<T1, T2>()
//   - boost::typelayout::layout_signatures_match<T1, T2>()
#include <boost/typelayout.hpp>

// ============================================================================
// Target Architecture Definition
//
// XOffset defines two explicit sets:
//   A (Architecture Set) — what platform the data lives on
//   S (Safe Type Subset) — what types can be stored
// Zero-copy safety = Platform ∈ A ∧ Type ∈ S
// ============================================================================
namespace XOffsetDatastructure2 {

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

    // Architecture Presets
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

} // namespace XOffsetDatastructure2

// ============================================================================
// Platform validation: current compiler environment ∈ TargetArchitecture
// ============================================================================
static_assert(sizeof(void*) == XOffsetDatastructure2::TargetArchitecture.pointer_size,
    "Platform pointer size does not match TargetArchitecture");
static_assert(IS_LITTLE_ENDIAN == XOffsetDatastructure2::TargetArchitecture.little_endian,
    "Platform endianness does not match TargetArchitecture");
static_assert(sizeof(int8_t)  == XOffsetDatastructure2::TargetArchitecture.sizeof_int8);
static_assert(sizeof(int16_t) == XOffsetDatastructure2::TargetArchitecture.sizeof_int16);
static_assert(sizeof(int32_t) == XOffsetDatastructure2::TargetArchitecture.sizeof_int32);
static_assert(sizeof(int64_t) == XOffsetDatastructure2::TargetArchitecture.sizeof_int64);
static_assert(sizeof(float)   == XOffsetDatastructure2::TargetArchitecture.sizeof_float);
static_assert(sizeof(double)  == XOffsetDatastructure2::TargetArchitecture.sizeof_double);
static_assert(sizeof(bool)    == XOffsetDatastructure2::TargetArchitecture.sizeof_bool);
static_assert(sizeof(char)    == XOffsetDatastructure2::TargetArchitecture.sizeof_char);
static_assert(alignof(void*)  == XOffsetDatastructure2::TargetArchitecture.pointer_align);
static_assert(alignof(int32_t) == XOffsetDatastructure2::TargetArchitecture.alignof_int32,
    "Platform alignof(int32_t) does not match TargetArchitecture");
static_assert(alignof(int64_t) == XOffsetDatastructure2::TargetArchitecture.alignof_int64,
    "Platform alignof(int64_t) does not match TargetArchitecture");
static_assert(alignof(float)   == XOffsetDatastructure2::TargetArchitecture.alignof_float,
    "Platform alignof(float) does not match TargetArchitecture");
static_assert(alignof(double)  == XOffsetDatastructure2::TargetArchitecture.alignof_double,
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

    // WARNING: All existing pointers/references into the buffer are invalidated
    // after this call. Re-acquire them via find<T>() or find_or_construct<T>().
    void update_after_shrink()
    {
        auto *pBuf = get_buffer();
        std::vector<char> new_buf(pBuf->data(), pBuf->data() + pBuf->size());
        XManagedMemory new_mem(new_buf);
        this->swap(new_mem);
    }

    // WARNING: Invalidates ALL existing pointers/references into this buffer.
    // After calling, re-acquire object pointers via find<T>().
    void shrink_to_fit()
    {
        base_t::shrink_to_fit();
        m_buffer.resize(base_t::get_size());
        update_after_shrink();
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
};

} // namespace interprocess
} // namespace boost


namespace XOffsetDatastructure2 {
    using namespace boost::interprocess;

    using XBuffer = XManagedMemory<char, x_seq_fit<null_mutex_family>, iset_index>;
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

        /// Internal vector alias used as backing store for flat containers
        template <typename T>
        using x_vector_impl = boost::container::vector<
            T, allocator<T, XBuffer::segment_manager>, x_vector_options>;
    } // namespace detail

    // ========================================================================
    // Public Container Aliases
    // ========================================================================

    /// Managed vector with 1.1x growth factor
    template <typename T>
    using XVector = detail::x_vector_impl<T>;

    /// Managed flat_set backed by detail::x_vector_impl
    template <typename T>
    using XSet = boost::container::flat_set<T, std::less<T>, detail::x_vector_impl<T>>;

    /// Managed flat_map backed by detail::x_vector_impl
    template <typename K, typename V>
    using XMap = boost::container::flat_map<K, V, std::less<K>,
        detail::x_vector_impl<std::pair<K, V>>>;

    /// Managed string with shared-memory allocator
    using XString = boost::container::basic_string<
        char, std::char_traits<char>, allocator<char, XBuffer::segment_manager>>;

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

    class XBufferCompactor {
    public:
        template<typename T>
        static XBuffer compact_automatic(XBuffer& old_xbuf, const char* object_name) {
            validate_xbuffer_type<T>();
            auto stats = XBufferVisualizer::get_memory_stats(old_xbuf);
            std::size_t new_size = stats.used_size + (stats.used_size / 10);
            if (new_size < 4096) new_size = 4096;
            
            XBuffer new_xbuf(new_size);
            auto* old_obj = old_xbuf.find<T>(object_name).first;
            if (!old_obj) {
                return new_xbuf;
            }
            
            auto* new_obj = new_xbuf.construct<T>(object_name)(new_xbuf.get_segment_manager());
            migrate_members(*old_obj, *new_obj, old_xbuf, new_xbuf);
            new_xbuf.shrink_to_fit();
            return new_xbuf;
        }
        
        template<typename T>
        static XBuffer compact_automatic_all(XBuffer& old_xbuf) {
            validate_xbuffer_type<T>();
            auto stats = XBufferVisualizer::get_memory_stats(old_xbuf);
            std::size_t new_size = stats.used_size + (stats.used_size / 10);
            if (new_size < 4096) new_size = 4096;
            
            XBuffer new_xbuf(new_size);
            auto* segment = old_xbuf.get_segment_manager();
            using const_named_it = typename XBuffer::segment_manager::const_named_iterator;
            const_named_it named_beg = segment->named_begin();
            const_named_it named_end = segment->named_end();
            
            std::size_t migrated_count = 0;
            
            for(const_named_it it = named_beg; it != named_end; ++it) {
                const char* name = it->name();
                auto* old_obj = old_xbuf.find<T>(name).first;
                if (old_obj) {
                    auto* new_obj = new_xbuf.construct<T>(name)(new_xbuf.get_segment_manager());
                    migrate_members(*old_obj, *new_obj, old_xbuf, new_xbuf);
                    ++migrated_count;
                }
            }
            if (migrated_count > 0) {
                new_xbuf.shrink_to_fit();
            }
            
            return new_xbuf;
        }

    private:
        // ================================================================
        // Migration strategy trait (extensible by users)
        //
        // Users who register is_safe_leaf<MyType> can also register
        // migrate_as<MyType> to tell the Compactor how to migrate it.
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

        // Built-in registrations for XOffset types
        template<>             struct migrate_as<XString>  { static constexpr MigrateStrategy value = MigrateStrategy::AllocatorAware; };
        template<typename T>   struct migrate_as<XVector<T>> { static constexpr MigrateStrategy value = MigrateStrategy::Container; };
        template<typename T>   struct migrate_as<XSet<T>>    { static constexpr MigrateStrategy value = MigrateStrategy::Container; };
        template<typename K, typename V> struct migrate_as<XMap<K,V>> { static constexpr MigrateStrategy value = MigrateStrategy::Container; };

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
                // Allocator-aware copy: reconstruct with new allocator
                // Requires ElementType(const ElementType&, allocator_type) constructor
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

        // — LEAF-3: XString —
        template<> struct is_safe_leaf<XString>  : std::true_type {};

        // — LEAF-4: XContainers (precise template matching) —
        template<typename T>             struct is_safe_leaf<XVector<T>>  : std::true_type {};
        template<typename T>             struct is_safe_leaf<XSet<T>>     : std::true_type {};
        template<typename K, typename V> struct is_safe_leaf<XMap<K, V>>  : std::true_type {};

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
        //   struct XOffsetDatastructure2::detail::is_safe_leaf<XOffsetPtr<MyType>>
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
    }

    class XBufferExt : public XBuffer {
    public:
        using XBuffer::XBuffer;

        template<typename T>
        T* make(const char* name) {
            validate_xbuffer_type<T>();
            return this->construct<T>(name)(this->get_segment_manager());
        }
        
        template<typename T>
        boost::interprocess::allocator<T, XBuffer::segment_manager> allocator() {
            validate_xbuffer_type<T>();
            return boost::interprocess::allocator<T, XBuffer::segment_manager>(this->get_segment_manager());
        }

        template<typename T>
        std::pair<T*, bool> find_ex(const char* name) {
            auto result = this->find<T>(name);
            return {result.first, result.second};
        }
        
        template<typename T>
        T* find_or_make(const char* name) {
            validate_xbuffer_type<T>();
            return this->find_or_construct<T>(name)(this->get_segment_manager());
        }

        std::string save_to_string() {
            auto* buffer = this->get_buffer();
            return std::string(buffer->begin(), buffer->end());
        }
        
        static XBufferExt load_from_string(const std::string& data) {
            std::vector<char> buffer(data.begin(), data.end());
            XBufferExt xbuf(buffer);
            return xbuf;
        }

        XBufferVisualizer::MemoryStats stats() {
            return XBufferVisualizer::get_memory_stats(*this);
        }
    };
}

// ============================================================================
// TypeLayout specializations for XOffsetDatastructure2 containers
//
// Registered using TYPELAYOUT_OPAQUE_* macros so the type-signature engine
// can resolve XOffsetDatastructure2 container types correctly.
// ============================================================================
namespace boost {
namespace typelayout {

    TYPELAYOUT_OPAQUE_TYPE(XOffsetDatastructure2::XString, "string", 32, 8)
    TYPELAYOUT_OPAQUE_CONTAINER(XOffsetDatastructure2::XVector, "vector", 32, 8)
    TYPELAYOUT_OPAQUE_CONTAINER(XOffsetDatastructure2::XSet, "set", 32, 8)
    TYPELAYOUT_OPAQUE_MAP(XOffsetDatastructure2::XMap, "map", 32, 8)

} // namespace typelayout
} // namespace boost

#endif
