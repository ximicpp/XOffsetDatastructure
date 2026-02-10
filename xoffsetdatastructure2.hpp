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

#ifndef OFFSET_DATA_STRUCTURE_2_CUSTOM_CONTAINER_GROWTH_FACTOR
#define OFFSET_DATA_STRUCTURE_2_CUSTOM_CONTAINER_GROWTH_FACTOR 1
#endif

#include <experimental/meta>
#include <type_traits>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <any>

// TypeLayout library — the authoritative type-signature engine
#include <boost/typelayout.hpp>

// ============================================================================
// XTypeSignature — Backward-compatible API layer delegating to boost::typelayout
//
// All original XTypeSignature APIs are preserved but marked [[deprecated]].
// New code should use boost::typelayout directly:
//   - boost::typelayout::get_definition_signature<T>()
//   - boost::typelayout::get_layout_signature<T>()
//   - boost::typelayout::definition_signatures_match<T1, T2>()
//   - boost::typelayout::layout_signatures_match<T1, T2>()
// ============================================================================

namespace XTypeSignature {
    inline constexpr int BASIC_ALIGNMENT = 8;
    inline constexpr int ANY_SIZE = 64;

    // Platform assertions (unchanged — still useful as a safety net)
    static_assert(sizeof(int8_t) == 1, "int8_t must be 1 byte");
    static_assert(sizeof(uint8_t) == 1, "uint8_t must be 1 byte");
    static_assert(sizeof(int16_t) == 2, "int16_t must be 2 bytes");
    static_assert(sizeof(uint16_t) == 2, "uint16_t must be 2 bytes");
    static_assert(sizeof(int32_t) == 4, "int32_t must be 4 bytes");
    static_assert(sizeof(uint32_t) == 4, "uint32_t must be 4 bytes");
    static_assert(sizeof(int64_t) == 8, "int64_t must be 8 bytes");
    static_assert(sizeof(uint64_t) == 8, "uint64_t must be 8 bytes");
    
    static_assert(sizeof(float) == 4, "float must be 4 bytes");
    static_assert(sizeof(double) == 8, "double must be 8 bytes");
    
    static_assert(sizeof(char) == 1, "char must be 1 byte");
    static_assert(sizeof(bool) == 1, "bool must be 1 byte");
    
    static_assert(sizeof(void*) == 8, "Pointer size must be 8 bytes (64-bit required)");
    static_assert(alignof(void*) == 8, "Pointer alignment must be 8 bytes");
    static_assert(sizeof(size_t) == 8, "size_t must be 8 bytes (64-bit architecture required)");
    static_assert(IS_LITTLE_ENDIAN, "Little-endian architecture required");

    // Re-export FixedString from TypeLayout as CompileString for backward compat
    template <size_t N>
    using CompileString = boost::typelayout::FixedString<N>;

    // Re-export TypeSignature from TypeLayout (Definition mode by default)
    template <typename T>
    using TypeSignature = boost::typelayout::TypeSignature<T, boost::typelayout::SignatureMode::Definition>;

    // Backward-compatible helper: get member count via TypeLayout
    using boost::typelayout::get_member_count;

    // -----------------------------------------------------------------------
    // Deprecated API — delegates to boost::typelayout
    // -----------------------------------------------------------------------

    /// @deprecated Use boost::typelayout::get_definition_signature<T>() instead
    template <typename T>
    [[deprecated("Use boost::typelayout::get_definition_signature<T>() instead")]]
    [[nodiscard]] consteval auto get_XTypeSignature() noexcept {
        return boost::typelayout::get_definition_signature<T>();
    }

} // namespace XTypeSignature

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

    bool grow(size_type extra_bytes)
    {
        try
        {
            m_buffer.resize(m_buffer.size() + extra_bytes);
            base_t::close_impl();
            base_t::open_impl(&m_buffer[0], m_buffer.size());
            base_t::grow(extra_bytes);
            return true;
        }
        catch(...)
        {
            return false;
        }
    }

    void swap(XManagedMemory &other) noexcept
    {
        base_t::swap(other);
        m_buffer.swap(other.m_buffer);
    }

    void update_after_shrink()
    {
        auto *pBuf = get_buffer();
        std::vector<char> new_buf(pBuf->data(), pBuf->data() + pBuf->size());
        XManagedMemory new_mem(new_buf);
        this->swap(new_mem);
    }

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

    struct growth_factor_custom : boost::container::dtl::grow_factor_ratio<0, 11, 10> {};

    template <typename T>
    using XOffsetPtr = boost::interprocess::offset_ptr<T>;

#if OFFSET_DATA_STRUCTURE_2_CUSTOM_CONTAINER_GROWTH_FACTOR == 0
    template <typename T>
    using XVector = boost::container::vector<T, allocator<T, XBuffer::segment_manager>>;
#elif OFFSET_DATA_STRUCTURE_2_CUSTOM_CONTAINER_GROWTH_FACTOR == 1
    using vector_option = boost::container::vector_options_t<boost::container::growth_factor<growth_factor_custom>>;
    template <typename T>
    using XVector = boost::container::vector<T, allocator<T, XBuffer::segment_manager>, vector_option>;
#endif

#if OFFSET_DATA_STRUCTURE_2_CUSTOM_CONTAINER_GROWTH_FACTOR == 0
    template <typename T>
    using XSet = boost::container::flat_set<T, std::less<T>, allocator<T, XBuffer::segment_manager>>;
#elif OFFSET_DATA_STRUCTURE_2_CUSTOM_CONTAINER_GROWTH_FACTOR == 1
    using vector_option_flatset = boost::container::vector_options_t<boost::container::growth_factor<growth_factor_custom>>;
    template <typename T>
    using XVector_flatset = boost::container::vector<T, allocator<T, XBuffer::segment_manager>, vector_option_flatset>;
    template <typename T>
    using XSet = boost::container::flat_set<T, std::less<T>, XVector_flatset<T>>;
#endif

#if OFFSET_DATA_STRUCTURE_2_CUSTOM_CONTAINER_GROWTH_FACTOR == 0
    template <typename K, typename V>
    using XMap = boost::container::flat_map<K, V, std::less<K>, allocator<std::pair<K, V>, XBuffer::segment_manager>>;
#elif OFFSET_DATA_STRUCTURE_2_CUSTOM_CONTAINER_GROWTH_FACTOR == 1
    using vector_option_flatmap = boost::container::vector_options_t<boost::container::growth_factor<growth_factor_custom>>;
    template <typename K, typename V>
    using XVector_flatmap = boost::container::vector<std::pair<K, V>, allocator<std::pair<K, V>, XBuffer::segment_manager>, vector_option_flatmap>;
    template <typename K, typename V>
    using XMap = boost::container::flat_map<K, V, std::less<K>, XVector_flatmap<K, V>>;
#endif

    using XString = boost::container::basic_string<char, std::char_traits<char>, allocator<char, XBuffer::segment_manager>>;

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

    class XBufferCompactor {
    public:
        template<typename T>
        static XBuffer compact_automatic(XBuffer& old_xbuf, const char* object_name = "MyTest") {
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
        template<typename T>
        struct is_xstring : std::false_type {};
        
        template<>
        struct is_xstring<XString> : std::true_type {};
        
        template<typename T, typename = void>
        struct container_value_type {};
        
        template<typename T>
        struct container_value_type<T, std::void_t<typename T::value_type>> {
            using type = typename T::value_type;
        };
        
        template<typename T>
        using container_value_type_t = typename container_value_type<T>::type;
        
        template<typename T>
        static constexpr bool is_simple_pod_v = std::is_trivially_copyable_v<T> && 
                                                 !SupportedContainer<T> && 
                                                 !is_xstring<T>::value;
        
        template<typename ElementType>
        static auto migrate_element(const ElementType& old_elem, XBuffer& old_xbuf, XBuffer& new_xbuf) {
            if constexpr (is_simple_pod_v<ElementType>) {
                return old_elem;
            }
            else if constexpr (is_xstring<ElementType>::value) {
                return XString(old_elem.c_str(), new_xbuf.get_segment_manager());
            }
            else {
                ElementType new_elem(new_xbuf.get_segment_manager());
                migrate_members(old_elem, new_elem, old_xbuf, new_xbuf);
                return new_elem;
            }
        }
        
        template<typename ContainerType>
        static void migrate_container(const ContainerType& old_container, 
                                      ContainerType& new_container,
                                      XBuffer& old_xbuf, XBuffer& new_xbuf) {
            using ElementType = container_value_type_t<ContainerType>;
            
            if constexpr (is_simple_pod_v<ElementType>) {
                new_container = old_container;
                return;
            }
            
            if constexpr (MapLikeContainer<ContainerType>) {
                for (const auto& [key, value] : old_container) {
                    auto new_key = migrate_element(key, old_xbuf, new_xbuf);
                    auto new_value = migrate_element(value, old_xbuf, new_xbuf);
                    new_container.emplace(std::move(new_key), std::move(new_value));
                }
            }
            else {
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
            if constexpr (is_simple_pod_v<MemberType>) {
                new_member = old_member;
            }
            else if constexpr (is_xstring<MemberType>::value) {
                new_member = XString(old_member.c_str(), new_xbuf.get_segment_manager());
            }
            else if constexpr (SupportedContainer<MemberType>) {
                migrate_container(old_member, new_member, old_xbuf, new_xbuf);
            }
            else {
                migrate_members(old_member, new_member, old_xbuf, new_xbuf);
            }
        }
        
        template<typename T>
        static consteval std::size_t get_member_count_impl() {
            using namespace std::meta;
            return nonstatic_data_members_of(^^T, access_context::unchecked()).size();
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
            constexpr std::size_t member_count = get_member_count_impl<T>();
            migrate_members_impl(old_obj, new_obj, old_xbuf, new_xbuf,
                                std::make_index_sequence<member_count>{});
        }
    };

    namespace detail {
        template<typename T>
        consteval bool is_basic_type() {
            using CleanT = std::remove_cv_t<T>;
            return std::is_same_v<CleanT, int8_t> ||
                   std::is_same_v<CleanT, int16_t> ||
                   std::is_same_v<CleanT, int32_t> ||
                   std::is_same_v<CleanT, int64_t> ||
                   std::is_same_v<CleanT, uint8_t> ||
                   std::is_same_v<CleanT, uint16_t> ||
                   std::is_same_v<CleanT, uint32_t> ||
                   std::is_same_v<CleanT, uint64_t> ||
                   std::is_same_v<CleanT, float> ||
                   std::is_same_v<CleanT, double> ||
                   std::is_same_v<CleanT, bool> ||
                   std::is_same_v<CleanT, char>;
        }
        
        template<typename T>
        consteval bool is_xstring() {
            return std::is_same_v<std::remove_cv_t<T>, XString>;
        }

        template<typename T>
        consteval bool has_bases() {
            using namespace std::meta;
            auto bases = bases_of(^^T, access_context::unchecked());
            return bases.size() > 0;
        }
        
        // ============================================================================
        // Type-Erased Container Blacklist Detection
        // These types contain hidden pointers and are UNSAFE for cross-process use
        // ============================================================================
        
        template<typename T>
        struct is_std_function : std::false_type {};
        
        template<typename R, typename... Args>
        struct is_std_function<std::function<R(Args...)>> : std::true_type {};
        
        template<typename T>
        struct is_std_shared_ptr : std::false_type {};
        
        template<typename U>
        struct is_std_shared_ptr<std::shared_ptr<U>> : std::true_type {};
        
        template<typename T>
        struct is_std_unique_ptr : std::false_type {};
        
        template<typename U, typename D>
        struct is_std_unique_ptr<std::unique_ptr<U, D>> : std::true_type {};
        
        template<typename T>
        struct is_std_weak_ptr : std::false_type {};
        
        template<typename U>
        struct is_std_weak_ptr<std::weak_ptr<U>> : std::true_type {};
        
        template<typename T>
        consteval bool is_type_erased_container() {
            using CleanT = std::remove_cv_t<T>;
            
            // std::function - contains virtual function pointer
            if constexpr (is_std_function<CleanT>::value) {
                return true;
            }
            // std::any - contains type-erased storage with virtual dispatch
            if constexpr (std::is_same_v<CleanT, std::any>) {
                return true;
            }
            // std::shared_ptr - contains control block pointer
            if constexpr (is_std_shared_ptr<CleanT>::value) {
                return true;
            }
            // std::unique_ptr - contains raw pointer
            if constexpr (is_std_unique_ptr<CleanT>::value) {
                return true;
            }
            // std::weak_ptr - contains control block pointer
            if constexpr (is_std_weak_ptr<CleanT>::value) {
                return true;
            }
            
            return false;
        }
        
        template<typename T>
        consteval bool is_safe_type();
        
        template<typename T>
        consteval bool is_safe_xvector() {
            using CleanT = std::remove_cv_t<T>;
            if constexpr (requires { typename CleanT::value_type; }) {
                if constexpr (sizeof(CleanT) == 32 && alignof(CleanT) == 8) {
                    return is_safe_type<typename CleanT::value_type>();
                }
            }
            return false;
        }
        
        template<typename T>
        consteval bool is_safe_xset() {
            using CleanT = std::remove_cv_t<T>;
            if constexpr (requires { typename CleanT::key_type; }) {
                if constexpr (sizeof(CleanT) == 32 && alignof(CleanT) == 8) {
                    return is_safe_type<typename CleanT::key_type>();
                }
            }
            return false;
        }
        
        template<typename T>
        consteval bool is_safe_xmap() {
            using CleanT = std::remove_cv_t<T>;
            if constexpr (requires { typename CleanT::key_type; typename CleanT::mapped_type; }) {
                if constexpr (sizeof(CleanT) == 32 && alignof(CleanT) == 8) {
                    return is_safe_type<typename CleanT::key_type>() &&
                           is_safe_type<typename CleanT::mapped_type>();
                }
            }
            return false;
        }
		
        template<typename T>
        consteval std::size_t get_safe_member_count() {
            using namespace std::meta;
            return nonstatic_data_members_of(^^T, access_context::unchecked()).size();
        }
        
        template<typename T, std::size_t Index>
        consteval bool is_member_safe_at() {
            using namespace std::meta;
            
            constexpr auto member = nonstatic_data_members_of(^^T, access_context::unchecked())[Index];
            using MemberType = [:type_of(member):];
            
            if (!is_safe_type<MemberType>()) {
                return false;
            }
            
            if constexpr (std::is_reference_v<MemberType>) {
                return false;
            }
            if constexpr (std::is_pointer_v<MemberType>) {
                return false;
            }
            
            return true;
        }
        
        template<typename T, std::size_t... Indices>
        consteval bool check_all_members_impl(std::index_sequence<Indices...>) {
            return (is_member_safe_at<T, Indices>() && ...);
        }
        
        template<typename T>
        consteval bool are_all_members_safe() {
            using namespace std::meta;
            
            if constexpr (!std::is_class_v<T>) {
                return false;
            }
            
            if constexpr (std::is_polymorphic_v<T>) {
                return false;
            }
            
            if constexpr (has_bases<T>()) {
                return false;
            }

            if constexpr (std::is_union_v<T>) {
                return false;
            }
            
            constexpr std::size_t member_count = nonstatic_data_members_of(^^T, access_context::unchecked()).size();
            if constexpr (member_count == 0) {
                return true;
            } else {
                return check_all_members_impl<T>(std::make_index_sequence<member_count>{});
            }
        }
        
        template<typename T>
        consteval bool is_safe_type() {
            using CleanT = std::remove_cv_t<T>;
            
            // First check: reject type-erased containers (std::function, std::any, etc.)
            if constexpr (is_type_erased_container<CleanT>()) {
                return false;
            }
            
            if constexpr (is_basic_type<CleanT>()) {
                return true;
            }
            
            if constexpr (is_xstring<CleanT>()) {
                return true;
            }
            
            if constexpr (std::is_class_v<CleanT>) {
                if constexpr (is_safe_xvector<CleanT>()) {
                    return true;
                }
                if constexpr (is_safe_xset<CleanT>()) {
                    return true;
                }
                if constexpr (is_safe_xmap<CleanT>()) {
                    return true;
                }
            }
            
            if constexpr (std::is_class_v<CleanT> && !is_xstring<CleanT>()) {
                return are_all_members_safe<CleanT>();
            }
            
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
                return "UNSAFE: Raw pointer (use XOffsetPtr<T> instead)";
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
            "  ✗ Raw pointers (use XOffsetPtr<T>)\n"
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
// These are registered in boost::typelayout so both TypeLayout and the
// XTypeSignature compatibility layer can resolve them.
// ============================================================================
namespace boost {
namespace typelayout {

    template <SignatureMode Mode>
    struct TypeSignature<XOffsetDatastructure2::XString, Mode> {
        static consteval auto calculate() noexcept {
            return FixedString{"string[s:32,a:8]"};
        }
    };

    template <typename T, SignatureMode Mode>
    struct TypeSignature<XOffsetDatastructure2::XVector<T>, Mode> {
        static consteval auto calculate() noexcept {
            return FixedString{"vector[s:32,a:8]<"} +
                   TypeSignature<T, Mode>::calculate() +
                   FixedString{">"};
        }
    };

    template <typename T, SignatureMode Mode>
    struct TypeSignature<XOffsetDatastructure2::XSet<T>, Mode> {
        static consteval auto calculate() noexcept {
            return FixedString{"set[s:32,a:8]<"} +
                   TypeSignature<T, Mode>::calculate() +
                   FixedString{">"};
        }
    };

    template <typename K, typename V, SignatureMode Mode>
    struct TypeSignature<XOffsetDatastructure2::XMap<K, V>, Mode> {
        static consteval auto calculate() noexcept {
            return FixedString{"map[s:32,a:8]<"} +
                   TypeSignature<K, Mode>::calculate() +
                   FixedString{","} +
                   TypeSignature<V, Mode>::calculate() +
                   FixedString{">"};
        }
    };

} // namespace typelayout
} // namespace boost

#endif
