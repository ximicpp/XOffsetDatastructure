#ifndef X_OFFSET_DATA_STRUCTURE_2_HPP
#define X_OFFSET_DATA_STRUCTURE_2_HPP

// XOffsetDatastructure2 - C++26 Reflection-Based Offset Container Library
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
#include <array>
#include <string_view>
#include <type_traits>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <fstream>

// XTypeSignature - Compile-Time Type Signature System (C++26 Reflection)
namespace XTypeSignature {
    inline constexpr int BASIC_ALIGNMENT = 8;
    inline constexpr int ANY_SIZE = 64;

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

    template <typename T>
    struct always_false : std::false_type {};

    template <size_t N>
    struct CompileString {
        char value[N];
        static constexpr size_t size = N - 1;
        
        constexpr CompileString(const char (&str)[N]) {
            for (size_t i = 0; i < N; ++i) {
                value[i] = str[i];
            }
        }

        template <typename T>
        static constexpr CompileString<32> from_number(T num) noexcept {
            char result[32] = {};
            int idx = 0;
            if (num == 0) {
                result[0] = '0';
                idx = 1;
            } else {
                bool negative = std::is_signed_v<T> && num < 0;
                using UnsignedT = std::make_unsigned_t<T>;
                UnsignedT abs_num;
                if (negative) {
                    abs_num = UnsignedT(-(std::make_signed_t<T>(num)));
                } else {
                    abs_num = UnsignedT(num);
                }
                while (abs_num > 0) {
                    result[idx++] = '0' + char(abs_num % 10);
                    abs_num /= 10;
                }
                if (negative) {
                    result[idx++] = '-';
                }
                for (int i = 0; i < idx / 2; ++i) {
                    char temp = result[i];
                    result[i] = result[idx - 1 - i];
                    result[idx - 1 - i] = temp;
                }
            }
            result[idx] = '\0';
            return CompileString<32>(result);
        }

        template <size_t M>
        constexpr auto operator+(const CompileString<M>& other) const noexcept {
            constexpr size_t new_size = N + M - 1;
            char result[new_size] = {};
            size_t pos = 0;
            while (pos < N - 1 && value[pos] != '\0') {
                result[pos] = value[pos];
                ++pos;
            }
            size_t j = 0;
            while (j < M) {
                result[pos++] = other.value[j++];
            }
            return CompileString<new_size>(result);
        }

        template <size_t M>
        constexpr bool operator==(const CompileString<M>& other) const noexcept {
            size_t i = 0;
            while (i < N && i < M && value[i] != '\0' && other.value[i] != '\0') {
                if (value[i] != other.value[i]) {
                    return false;
                }
                ++i;
            }
            return value[i] == other.value[i];
        }

        constexpr bool operator==(const char* other) const noexcept {
            size_t i = 0;
            while (i < N && value[i] != '\0' && other[i] != '\0') {
                if (value[i] != other[i]) {
                    return false;
                }
                ++i;
            }
            return value[i] == other[i];
        }

        void print() const {
            for (size_t i = 0; i < N && value[i] != '\0'; ++i) {
                std::cout << value[i];
            }
        }
    };

    template <typename T>
    struct TypeSignature;

    template<typename T, size_t Index>
    consteval size_t get_field_offset() noexcept {
        using namespace std::meta;
        auto members = nonstatic_data_members_of(^^T, access_context::unchecked());
        
        if constexpr (Index == 0) {
            return 0;
        } else {
            return offset_of(members[Index]).bytes;
        }
    }

    template <typename T>
    consteval std::size_t get_member_count() noexcept {
        using namespace std::meta;
        auto all_members = nonstatic_data_members_of(^^T, access_context::unchecked());
        return all_members.size();
    }
    
    template<typename T, std::size_t Index>
    static consteval auto get_field_signature() noexcept {
        using namespace std::meta;
        constexpr auto member = nonstatic_data_members_of(^^T, access_context::unchecked())[Index];
        
        using FieldType = [:type_of(member):];
        constexpr std::size_t offset = offset_of(member).bytes;
        return CompileString{"@"} +
               CompileString<32>::from_number(offset) +
               CompileString{":"} +
               TypeSignature<FieldType>::calculate();
    }

    template<typename T, std::size_t Index, bool IsFirst>
    consteval auto build_field_with_comma() noexcept {
        if constexpr (IsFirst) {
            return get_field_signature<T, Index>();
        } else {
            return CompileString{","} + get_field_signature<T, Index>();
        }
    }
    template<typename T, std::size_t... Indices>
    consteval auto concatenate_field_signatures(std::index_sequence<Indices...>) noexcept {
        return (build_field_with_comma<T, Indices, (Indices == 0)>() + ...);
    }
    template <typename T>
    consteval auto get_fields_signature() noexcept {
        constexpr std::size_t count = get_member_count<T>();
        if constexpr (count == 0) {
            return CompileString{""};
        } else {
            return concatenate_field_signatures<T>(std::make_index_sequence<count>{});
        }
    }

    template <> struct TypeSignature<int32_t>  { static consteval auto calculate() noexcept { return CompileString{"i32[s:4,a:4]"}; } };
    template <> struct TypeSignature<uint32_t> { static consteval auto calculate() noexcept { return CompileString{"u32[s:4,a:4]"}; } };
    template <> struct TypeSignature<int64_t>  { static consteval auto calculate() noexcept { return CompileString{"i64[s:8,a:8]"}; } };
    template <> struct TypeSignature<uint64_t> { static consteval auto calculate() noexcept { return CompileString{"u64[s:8,a:8]"}; } };
    template <> struct TypeSignature<float>    { static consteval auto calculate() noexcept { return CompileString{"f32[s:4,a:4]"}; } };
    template <> struct TypeSignature<double>   { static consteval auto calculate() noexcept { return CompileString{"f64[s:8,a:8]"}; } };
    template <> struct TypeSignature<bool>     { static consteval auto calculate() noexcept { return CompileString{"bool[s:1,a:1]"}; } };
    template <> struct TypeSignature<char>     { static consteval auto calculate() noexcept { return CompileString{"char[s:1,a:1]"}; } };
    
    template <typename T>
    struct TypeSignature<const T> {
        static consteval auto calculate() noexcept {
            return TypeSignature<T>::calculate();
        }
    };
    
    template <typename T> struct TypeSignature<T*>   { static consteval auto calculate() noexcept { return CompileString{"ptr[s:8,a:8]"}; } };
    template <>           struct TypeSignature<void*>{ static consteval auto calculate() noexcept { return CompileString{"ptr[s:8,a:8]"}; } };

    template <typename T, size_t N>
    struct TypeSignature<T[N]> {
        static consteval auto calculate() noexcept {
            if constexpr (std::is_same_v<T, char>) {
                return CompileString{"bytes[s:"} +
                       CompileString<32>::from_number(N) +
                       CompileString{",a:1]"};
            } else {
                return CompileString{"array[s:"} +
                       CompileString<32>::from_number(sizeof(T[N])) +
                       CompileString{",a:"} +
                       CompileString<32>::from_number(alignof(T[N])) +
                       CompileString{"]<"} +
                       TypeSignature<T>::calculate() +
                       CompileString{","} +
                       CompileString<32>::from_number(N) +
                       CompileString{">"};
            }
        }
    };

    template <> struct TypeSignature<char[ANY_SIZE]> {
        static consteval auto calculate() noexcept { return CompileString{"bytes[s:64,a:1]"}; }
    };

    template <typename T>
    struct TypeSignature {
        static consteval auto calculate() noexcept {
            if constexpr (std::is_class_v<T> && !std::is_array_v<T>) {
                if constexpr (std::is_polymorphic_v<T>) {
                    return CompileString{"struct[s:"} +
                           CompileString<32>::from_number(sizeof(T)) +
                           CompileString{",a:"} +
                           CompileString<32>::from_number(alignof(T)) +
                           CompileString{",polymorphic]{"} +
                           get_fields_signature<T>() +
                           CompileString{"}"};
                } else {
                    return CompileString{"struct[s:"} +
                           CompileString<32>::from_number(sizeof(T)) +
                           CompileString{",a:"} +
                           CompileString<32>::from_number(alignof(T)) +
                           CompileString{"]{"} +
                           get_fields_signature<T>() +
                           CompileString{"}"};
                }
            }
            else if constexpr (std::is_pointer_v<T>) {
                return TypeSignature<void*>::calculate();
            }
            else if constexpr (std::is_array_v<T>) {
                return TypeSignature<std::remove_extent_t<T>[]>::calculate();
            }
            else {
                static_assert(always_false<T>::value, 
                    "Type is not supported for automatic reflection");
                return CompileString{""};
            }
        }
    };

    template <typename T>
    [[nodiscard]] consteval auto get_XTypeSignature() noexcept {
        return TypeSignature<T>::calculate();
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
#include <boost/container/set.hpp>
#include <boost/container/flat_map.hpp>
#include <boost/container/map.hpp>
#include <boost/container/string.hpp>
#include <boost/container/detail/next_capacity.hpp>
#include <boost/move/utility_core.hpp>
#include <boost/assert.hpp>
#include <vector>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <string>


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

    // ============================================================================
    // Type Safety: Whitelist-Based Validation
    // Only explicitly allowed types can be used in XBuffer
    // ============================================================================
    
    namespace detail {
        // Step 1: Check if type is a basic primitive type
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
        
        // Step 2: Check if type is XString
        template<typename T>
        consteval bool is_xstring() {
            return std::is_same_v<std::remove_cv_t<T>, XString>;
        }
        
        // Forward declaration for recursive checking
        template<typename T>
        consteval bool is_safe_type();
        
        // Step 3: Check if X container element types are safe (recursive)
        template<typename T>
        consteval bool is_safe_xvector() {
            using CleanT = std::remove_cv_t<T>;
            // Check if it's XVector<U> and U is safe
            if constexpr (requires { typename CleanT::value_type; }) {
                // Simple heuristic: check if size/alignment matches XVector
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
		
        // Get member count for constexpr use
        template<typename T>
        consteval std::size_t get_safe_member_count() {
            using namespace std::meta;
            return nonstatic_data_members_of(^^T, access_context::unchecked()).size();
        }
        
        // Helper: Check single member at index
        template<typename T, std::size_t Index>
        consteval bool is_member_safe_at() {
            using namespace std::meta;
            
            // Get member directly by index (avoid storing vector)
            constexpr auto member = nonstatic_data_members_of(^^T, access_context::unchecked())[Index];
            using MemberType = [:type_of(member):];
            
            // Check if member type is safe
            if (!is_safe_type<MemberType>()) {
                return false;
            }
            
            // Additional critical checks
            if constexpr (std::is_reference_v<MemberType>) {
                return false;  // References cannot be serialized
            }
            if constexpr (std::is_pointer_v<MemberType>) {
                return false;  // Raw pointers forbidden (use XOffsetPtr instead)
            }
            
            return true;
        }
        
        // Check all members using fold expression (compile-time)
        template<typename T, std::size_t... Indices>
        consteval bool check_all_members_impl(std::index_sequence<Indices...>) {
            return (is_member_safe_at<T, Indices>() && ...);
        }
        
        // Step 4: Check if user-defined type's members are all safe (recursive)
        template<typename T>
        consteval bool are_all_members_safe() {
            using namespace std::meta;
            
            // Must be class/struct
            if constexpr (!std::is_class_v<T>) {
                return false;
            }
            
            // Must NOT be polymorphic (no virtual functions)
            if constexpr (std::is_polymorphic_v<T>) {
                return false;
            }
            
            // Must NOT be union
            if constexpr (std::is_union_v<T>) {
                return false;
            }
            
            // Check all non-static members recursively using fold expression
            constexpr std::size_t member_count = nonstatic_data_members_of(^^T, access_context::unchecked()).size();
            if constexpr (member_count == 0) {
                return true;  // Empty struct is safe
            } else {
                return check_all_members_impl<T>(std::make_index_sequence<member_count>{});
            }
        }
        
        // Main safety checker: Whitelist-based validation (recursive)
        template<typename T>
        consteval bool is_safe_type() {
            using CleanT = std::remove_cv_t<T>;
            
            // Whitelist check order:
            
            // 1. Basic primitive types (int32_t, float, etc.)
            if constexpr (is_basic_type<CleanT>()) {
                return true;
            }
            
            // 2. XString
            if constexpr (is_xstring<CleanT>()) {
                return true;
            }
            
            // 3. X containers (with recursive element type checking)
            // Note: We use structural checks (size/alignment) since we can't
            // directly compare template types at this point
            if constexpr (std::is_class_v<CleanT>) {
                // Try XVector
                if constexpr (is_safe_xvector<CleanT>()) {
                    return true;
                }
                // Try XSet
                if constexpr (is_safe_xset<CleanT>()) {
                    return true;
                }
                // Try XMap
                if constexpr (is_safe_xmap<CleanT>()) {
                    return true;
                }
            }
            
            // 4. User-defined class/struct (recursive member checking)
            if constexpr (std::is_class_v<CleanT> && !is_xstring<CleanT>()) {
                return are_all_members_safe<CleanT>();
            }
            
            // Everything else: FORBIDDEN (not in whitelist)
            return false;
        }
        
        // Generate simplified error message
        template<typename T>
        consteval const char* get_safety_error_message() {
            using CleanT = std::remove_cv_t<T>;
            
            if constexpr (is_safe_type<CleanT>()) {
                return "Type is SAFE for XBuffer";
            }
            else if constexpr (std::is_polymorphic_v<CleanT>) {
                return "UNSAFE: Type has virtual functions (polymorphic)";
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
    } // namespace detail
    
    // Public interface: Type safety checker
    template<typename T>
    struct is_xbuffer_safe {
        static constexpr bool value = detail::is_safe_type<T>();
        
        static constexpr const char* reason() {
            return detail::get_safety_error_message<T>();
        }
    };
    
    // Simplified type validation with clear error message
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
} // namespace XOffsetDatastructure2

// Type Signature Support for XOffsetDatastructure2 Containers
namespace XTypeSignature {
    template <>
    struct TypeSignature<XOffsetDatastructure2::XString> {
        static consteval auto calculate() noexcept {
            return CompileString{"string[s:32,a:8]"};
        }
    };
    template <typename T>
    struct TypeSignature<XOffsetDatastructure2::XVector<T>> {
        static consteval auto calculate() noexcept {
            return CompileString{"vector[s:32,a:8]<"} +
                   TypeSignature<T>::calculate() +
                   CompileString{">"};
        }
    };
    template <typename T>
    struct TypeSignature<XOffsetDatastructure2::XSet<T>> {
        static consteval auto calculate() noexcept {
            return CompileString{"set[s:32,a:8]<"} +
                   TypeSignature<T>::calculate() +
                   CompileString{">"};
        }
    };
    template <typename K, typename V>
    struct TypeSignature<XOffsetDatastructure2::XMap<K, V>> {
        static consteval auto calculate() noexcept {
            return CompileString{"map[s:32,a:8]<"} +
                   TypeSignature<K>::calculate() +
                   CompileString{","} +
                   TypeSignature<V>::calculate() +
                   CompileString{">"};
        }
    };
} // namespace XTypeSignature
#endif // X_OFFSET_DATA_STRUCTURE_2_HPP
