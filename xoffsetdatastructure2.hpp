#ifndef X_OFFSET_DATA_STRUCTURE_2_HPP
#define X_OFFSET_DATA_STRUCTURE_2_HPP

// =============================================================================
// XOffsetDatastructure2 - Single Header Library (C++26 Reflection Version)
// =============================================================================

// ============================================================================
// Platform Detection & Configuration
// ============================================================================
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

// ============================================================================
// Standard Library Headers
// ============================================================================
#include <experimental/meta>
#include <array>
#include <string_view>
#include <type_traits>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>
#include <fstream>

// ========================================================================
// XTypeSignature - Compile-Time Type Signature System (C++26 Reflection)
// (COMMENTED OUT - Focus on compact first)
// ========================================================================
#if 0
namespace XTypeSignature {
    inline constexpr int BASIC_ALIGNMENT = 8;
    inline constexpr int ANY_SIZE = 64;

    // ========================================================================
    // Type Size Validations
    // ========================================================================
    // Only validate types we actually use. See docs/CROSS_PLATFORM_TYPES.md
    
    // Fixed-size integer types (safe and recommended)
    static_assert(sizeof(int8_t) == 1, "int8_t must be 1 byte");
    static_assert(sizeof(uint8_t) == 1, "uint8_t must be 1 byte");
    static_assert(sizeof(int16_t) == 2, "int16_t must be 2 bytes");
    static_assert(sizeof(uint16_t) == 2, "uint16_t must be 2 bytes");
    static_assert(sizeof(int32_t) == 4, "int32_t must be 4 bytes");
    static_assert(sizeof(uint32_t) == 4, "uint32_t must be 4 bytes");
    static_assert(sizeof(int64_t) == 8, "int64_t must be 8 bytes");
    static_assert(sizeof(uint64_t) == 8, "uint64_t must be 8 bytes");
    
    // Floating point types
    static_assert(sizeof(float) == 4, "float must be 4 bytes");
    static_assert(sizeof(double) == 8, "double must be 8 bytes");
    
    // Character and boolean types
    static_assert(sizeof(char) == 1, "char must be 1 byte");
    static_assert(sizeof(bool) == 1, "bool must be 1 byte");
    
    // Platform requirements
    static_assert(sizeof(void*) == 8, "Pointer size must be 8 bytes (64-bit required)");
    static_assert(alignof(void*) == 8, "Pointer alignment must be 8 bytes");
    static_assert(sizeof(size_t) == 8, "size_t must be 8 bytes (64-bit architecture required)");
    static_assert(IS_LITTLE_ENDIAN, "Little-endian architecture required");

    template <typename T>
    struct always_false : std::false_type {};

    // ========================================================================
    // Compile-Time String for Type Signatures
    // ========================================================================
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

    // Forward declarations
    template <typename T>
    struct TypeSignature;

    // ========================================================================
    // Field Offset Calculation (C++26 Reflection)
    // ========================================================================
    template<typename T, size_t Index>
    consteval size_t get_field_offset() noexcept {
        auto members = std::meta::members_of(^T);
        
        if constexpr (Index == 0) {
            return 0;
        } else {
            // Use reflection to get member offset directly
            auto member = members[Index];
            return std::meta::offset_of(member);
        }
    }

    // ========================================================================
    // Generate Signature for All Fields (C++26 Reflection)
    // ========================================================================
    
    // Helper: Count total number of data members
    // Due to P2996 limitations, we cannot use template for to accumulate count
    // Instead, we use the size of the members vector directly
    template <typename T>
    consteval std::size_t get_member_count() noexcept {
        auto all_members = std::meta::members_of(^T);
        return all_members.size();
    }
    
    // Main function: Generate signature for all fields
    // NOTE: Due to P2996 limitations, we can't dynamically build type signatures
    // template for cannot accumulate results to external variables
    // Splice [:type_of(member):] requires member to be constexpr (template for provides this)
    // But we can't store intermediate CompileString results in a loop
    //
    // Current implementation: Return only field count
    // For full signatures, use manual TypeSignature specializations (see examples/)
    template <typename T>
    consteval auto get_fields_signature() noexcept {
        constexpr std::size_t count = get_member_count<T>();
        
        // Return simplified signature with field count only
        if constexpr (count > 0) {
            return CompileString{"fields:"} +
                   CompileString<32>::from_number(count);
        } else {
            return CompileString{""};
        }
    }

    // ========================================================================
    // Basic Type Signatures
    // ========================================================================
    template <> struct TypeSignature<int32_t>  { static consteval auto calculate() noexcept { return CompileString{"i32[s:4,a:4]"}; } };
    template <> struct TypeSignature<uint32_t> { static consteval auto calculate() noexcept { return CompileString{"u32[s:4,a:4]"}; } };
    template <> struct TypeSignature<int64_t>  { static consteval auto calculate() noexcept { return CompileString{"i64[s:8,a:8]"}; } };
    template <> struct TypeSignature<uint64_t> { static consteval auto calculate() noexcept { return CompileString{"u64[s:8,a:8]"}; } };
    template <> struct TypeSignature<float>    { static consteval auto calculate() noexcept { return CompileString{"f32[s:4,a:4]"}; } };
    template <> struct TypeSignature<double>   { static consteval auto calculate() noexcept { return CompileString{"f64[s:8,a:8]"}; } };
    template <> struct TypeSignature<bool>     { static consteval auto calculate() noexcept { return CompileString{"bool[s:1,a:1]"}; } };
    template <> struct TypeSignature<char>     { static consteval auto calculate() noexcept { return CompileString{"char[s:1,a:1]"}; } };
    
    // Pointer types
    template <typename T> struct TypeSignature<T*>   { static consteval auto calculate() noexcept { return CompileString{"ptr[s:8,a:8]"}; } };
    template <>           struct TypeSignature<void*>{ static consteval auto calculate() noexcept { return CompileString{"ptr[s:8,a:8]"}; } };

    // ========================================================================
    // Array Types
    // ========================================================================
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

    // ========================================================================
    // Generic Type Signature (C++26 Reflection)
    // ========================================================================
    template <typename T>
    struct TypeSignature {
        static consteval auto calculate() noexcept {
            // In C++26, we can use reflection on any class type (not just aggregates)
            if constexpr (std::is_class_v<T> && !std::is_array_v<T>) {
                return CompileString{"struct[s:"} +
                       CompileString<32>::from_number(sizeof(T)) +
                       CompileString{",a:"} +
                       CompileString<32>::from_number(alignof(T)) +
                       CompileString{"]{"} +
                       get_fields_signature<T>() +
                       CompileString{"}"};
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

    // ========================================================================
    // Public API
    // ========================================================================
    template <typename T>
    [[nodiscard]] consteval auto get_XTypeSignature() noexcept {
        return TypeSignature<T>::calculate();
    }

} // namespace XTypeSignature
#endif // Type Signature (commented out)

// using XTypeSignature::BASIC_ALIGNMENT;

// ============================================================================
// Boost Interprocess Extensions
// ============================================================================
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


namespace XOffsetDatastructure2
{
	using namespace boost::interprocess;

	// typedef XManagedMemory<char, x_best_fit<null_mutex_family>, iset_index> XBuffer;
	typedef XManagedMemory<char, x_seq_fit<null_mutex_family>, iset_index> XBuffer;

	// ====================================================================
	// C++20 Concepts for container classification (Namespace scope)
	// ====================================================================
	
	// Concept: Has iterator support (begin/end)
	template<typename T>
	concept HasIterator = requires(T t) {
		{ t.begin() } -> std::input_or_output_iterator;
		{ t.end() } -> std::input_or_output_iterator;
	};
	
	// Concept: Has value_type
	template<typename T>
	concept HasValueType = requires {
		typename T::value_type;
	};
	
	// Concept: Has mapped_type (for maps)
	template<typename T>
	concept HasMappedType = requires {
		typename T::mapped_type;
	};
	
	// Concept: Has key_type (for maps and sets)
	template<typename T>
	concept HasKeyType = requires {
		typename T::key_type;
	};
	
	// Concept: Sequential container (has push_back or emplace_back)
	template<typename T>
	concept SequentialContainer = HasIterator<T> && HasValueType<T> && 
		requires(T t, typename T::value_type v) {
			{ t.emplace_back(std::move(v)) };
		};
	
	// Concept: Set-like container (has key_type but not mapped_type, has emplace)
	template<typename T>
	concept SetLikeContainer = HasIterator<T> && HasValueType<T> && HasKeyType<T> && 
		!HasMappedType<T> &&
		requires(T t, typename T::value_type v) {
			{ t.emplace(std::move(v)) };
		};
	
	// Concept: Map-like container (has both key_type and mapped_type)
	template<typename T>
	concept MapLikeContainer = HasIterator<T> && HasKeyType<T> && HasMappedType<T> &&
		requires(T t, typename T::key_type k, typename T::mapped_type v) {
			{ t.emplace(std::move(k), std::move(v)) };
		};
	
	// Concept: Any supported container
	template<typename T>
	concept SupportedContainer = SequentialContainer<T> || SetLikeContainer<T> || MapLikeContainer<T>;

	struct growth_factor_custom
		// : boost::container::dtl::grow_factor_ratio<0, 15, 10> // growth_factor_50
		// : boost::container::dtl::grow_factor_ratio<0, 16, 10> // growth_factor_60
		// : boost::container::dtl::grow_factor_ratio<0, 14, 10>	// 40
		// : boost::container::dtl::grow_factor_ratio<0, 13, 10>	// 30
		// : boost::container::dtl::grow_factor_ratio<0, 12, 10>	// 20
		: boost::container::dtl::grow_factor_ratio<0, 11, 10> // 10
	// : boost::container::dtl::grow_factor_ratio<0, 17, 10> // growth_factor_70
	// : boost::container::dtl::grow_factor_ratio<0, 18, 10> // growth_factor_80
	// : boost::container::dtl::grow_factor_ratio<0, 19, 10> // growth_factor_90
	// : boost::container::dtl::grow_factor_ratio<0, 2, 1>	// 100
	{
	};

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
	// using XSet = boost::container::set<T, std::less<T>, allocator<T, XBuffer::segment_manager>>;
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
	// using XMap = boost::container::map<K, V, std::less<K>, allocator<std::pair<const K, V>, XBuffer::segment_manager>>;
#elif OFFSET_DATA_STRUCTURE_2_CUSTOM_CONTAINER_GROWTH_FACTOR == 1
	using vector_option_flatmap = boost::container::vector_options_t<boost::container::growth_factor<growth_factor_custom>>;
	template <typename K, typename V>
	using XVector_flatmap = boost::container::vector<std::pair<K, V>, allocator<std::pair<K, V>, XBuffer::segment_manager>, vector_option_flatmap>;
	template <typename K, typename V>
	using XMap = boost::container::flat_map<K, V, std::less<K>, XVector_flatmap<K, V>>;
#endif

	using XString = boost::container::basic_string<char, std::char_traits<char>, allocator<char, XBuffer::segment_manager>>;

	// ========================================================================
	// XBuffer Memory Visualization
	// ========================================================================

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

	// ========================================================================
	// Memory Compaction
	// ========================================================================

	// SFINAE helper to detect if a type has a static migrate method
	template<typename T, typename = void>
	struct has_migrate : std::false_type {};

	template<typename T>
	struct has_migrate<T, std::void_t<decltype(T::migrate(std::declval<XBuffer&>(), std::declval<XBuffer&>()))>> : std::true_type {};

	class XBufferCompactor {
	public:
		// Manual compaction: Type-based with automatic migrate detection
		// Uses C++17 if constexpr to detect if type T has migrate method
		// Returns original buffer if type doesn't have migrate method
		template<typename T>
		static XBuffer compact_manual(XBuffer& old_xbuf) {
			if constexpr (has_migrate<T>::value) {
				// Type has migrate method, perform compaction
				auto stats = XBufferVisualizer::get_memory_stats(old_xbuf);
				std::size_t new_size = stats.used_size + (stats.used_size / 10);
				if (new_size < 4096) new_size = 4096;
				
				XBuffer new_xbuf(new_size);
				T::migrate(old_xbuf, new_xbuf);
				
				// Shrink to fit: Remove unused memory
				new_xbuf.shrink_to_fit();
				
				return new_xbuf;
			} else {
				// Type doesn't have migrate method, return original buffer (move semantics)
				return std::move(old_xbuf);
			}
		}
		
		// Automatic compaction (C++26): Fully automatic via reflection
		// 
		// Key difference from compact_manual:
		// - compact_manual: Requires user-defined T::migrate() method
		// - compact_automatic: Uses C++26 reflection to auto-generate migration
		//
		// Core idea: Use std::meta::members_of(^T) to iterate all fields,
		// automatically handle POD types, containers, and nested objects.
		// No manual T::migrate() needed!
		//
		// Implementation based on C++26 reflection proposal (P2996R5):
		// - Uses ^T syntax for type reflection
		// - std::meta::members_of(^T) to get all member reflections
		// - std::meta::name_of(), type_of() for member introspection
		// - Compile-time type detection and recursive migration
		// 
		// @param old_xbuf The buffer to compact
		// @param object_name Name of the object to migrate (default: "MyTest")
		// @return New compacted buffer
		template<typename T>
		static XBuffer compact_automatic(XBuffer& old_xbuf, const char* object_name = "MyTest") {
			// Calculate new buffer size
			auto stats = XBufferVisualizer::get_memory_stats(old_xbuf);
			std::size_t new_size = stats.used_size + (stats.used_size / 10);
			if (new_size < 4096) new_size = 4096;
			
			XBuffer new_xbuf(new_size);
			
			// Find old object in fragmented buffer (using provided name)
			auto* old_obj = old_xbuf.find<T>(object_name).first;
			if (!old_obj) {
				return new_xbuf;
			}
			
			// Create new object in compact buffer (using same name)
			auto* new_obj = new_xbuf.construct<T>(object_name)(new_xbuf.get_segment_manager());
			
			// Migrate all members automatically using reflection
			migrate_members(*old_obj, *new_obj, old_xbuf, new_xbuf);
			
			// Shrink to fit: Remove unused memory
			new_xbuf.shrink_to_fit();
			
			return new_xbuf;
		}
		
		// Compact all objects of type T in the buffer
		// Automatically finds and migrates all instances of type T
		// 
		// @param old_xbuf The buffer to compact
		// @return New compacted buffer with all T objects migrated
		template<typename T>
		static XBuffer compact_automatic_all(XBuffer& old_xbuf) {
			// Calculate new buffer size
			auto stats = XBufferVisualizer::get_memory_stats(old_xbuf);
			std::size_t new_size = stats.used_size + (stats.used_size / 10);
			if (new_size < 4096) new_size = 4096;
			
			XBuffer new_xbuf(new_size);
			
			// Get segment manager for iteration
			auto* segment = old_xbuf.get_segment_manager();
			
			// Iterate through all named objects
			typedef typename XBuffer::segment_manager::const_named_iterator const_named_it;
			const_named_it named_beg = segment->named_begin();
			const_named_it named_end = segment->named_end();
			
			std::size_t migrated_count = 0;
			
			for(const_named_it it = named_beg; it != named_end; ++it) {
				const char* name = it->name();
				
				// Try to find object of type T with this name
				auto* old_obj = old_xbuf.find<T>(name).first;
				
				if (old_obj) {
					// Found an object of type T, migrate it
					auto* new_obj = new_xbuf.construct<T>(name)(new_xbuf.get_segment_manager());
					migrate_members(*old_obj, *new_obj, old_xbuf, new_xbuf);
					++migrated_count;
				}
			}
			
			// Shrink to fit if we migrated anything
			if (migrated_count > 0) {
				new_xbuf.shrink_to_fit();
			}
			
			return new_xbuf;
		}

	private:
		// ====================================================================
		// Type trait helpers for reflection-based migration
		// ====================================================================
		
		// Check if type is XString
		template<typename T>
		struct is_xstring : std::false_type {};
		
		template<>
		struct is_xstring<XString> : std::true_type {};
		
		// Helper to extract value_type from containers
		template<typename T, typename = void>
		struct container_value_type {};
		
		template<typename T>
		struct container_value_type<T, std::void_t<typename T::value_type>> {
			using type = typename T::value_type;
		};
		
		template<typename T>
		using container_value_type_t = typename container_value_type<T>::type;
		
		// Check if type is a simple POD (int, float, etc.)
		// Note: Concepts defined at namespace scope above
		template<typename T>
		static constexpr bool is_simple_pod_v = std::is_trivially_copyable_v<T> && 
		                                         !SupportedContainer<T> && 
		                                         !is_xstring<T>::value;
		
		// ====================================================================
		// Reflection-based member migration
		// ====================================================================
		
		// Helper: Migrate a single element (POD, XString, or nested object)
		template<typename ElementType>
		static auto migrate_element(const ElementType& old_elem, XBuffer& old_xbuf, XBuffer& new_xbuf) {
			if constexpr (is_simple_pod_v<ElementType>) {
				// POD types: direct copy
				return old_elem;
			}
			else if constexpr (is_xstring<ElementType>::value) {
				// XString: reconstruct with new allocator
				return XString(old_elem.c_str(), new_xbuf.get_segment_manager());
			}
			else {
				// Nested objects: construct and recursively migrate
				ElementType new_elem(new_xbuf.get_segment_manager());
				migrate_members(old_elem, new_elem, old_xbuf, new_xbuf);
				return new_elem;
			}
		}
		
		// Helper: Migrate container using iterators (unified for sequential and set-like containers)
		template<typename ContainerType>
		static void migrate_container_sequential(const ContainerType& old_container, 
		                                         ContainerType& new_container,
		                                         XBuffer& old_xbuf, XBuffer& new_xbuf) {
			using ElementType = container_value_type_t<ContainerType>;
			
			if constexpr (is_simple_pod_v<ElementType>) {
				// POD elements: direct copy (allocator-aware assignment)
				new_container = old_container;
			}
			else {
				// Non-POD elements: iterate and migrate each element
				for (const auto& elem : old_container) {
					auto migrated_elem = migrate_element(elem, old_xbuf, new_xbuf);
					
					// Use concept to determine insertion method
					if constexpr (SetLikeContainer<ContainerType>) {
						// Set-like: use emplace
						new_container.emplace(std::move(migrated_elem));
					} else {
						// Sequential: use emplace_back
						new_container.emplace_back(std::move(migrated_elem));
					}
				}
			}
		}
		
		// Helper: Migrate XMap using iterators
		template<typename MapType>
		static void migrate_map(const MapType& old_map, MapType& new_map,
		                       XBuffer& old_xbuf, XBuffer& new_xbuf) {
			using KeyType = typename MapType::key_type;
			using ValueType = typename MapType::mapped_type;
			
			// Iterate through key-value pairs
			for (const auto& [key, value] : old_map) {
				// Migrate key and value
				auto new_key = migrate_element(key, old_xbuf, new_xbuf);
				auto new_value = migrate_element(value, old_xbuf, new_xbuf);
				
				// Insert into new map
				new_map.emplace(std::move(new_key), std::move(new_value));
			}
		}
		
		// Main: Migrate a single member based on its type (using concepts)
		template<typename MemberType>
		static void migrate_member(const MemberType& old_member, MemberType& new_member, 
		                          XBuffer& old_xbuf, XBuffer& new_xbuf) {
			if constexpr (is_simple_pod_v<MemberType>) {
				// Simple POD types: direct copy
				new_member = old_member;
			}
			else if constexpr (is_xstring<MemberType>::value) {
				// XString: reconstruct with new allocator
				new_member = XString(old_member.c_str(), new_xbuf.get_segment_manager());
			}
			else if constexpr (SequentialContainer<MemberType> || SetLikeContainer<MemberType>) {
				// Sequential containers (XVector) or Set-like containers (XSet)
				// Both support iterator-based migration
				migrate_container_sequential(old_member, new_member, old_xbuf, new_xbuf);
			}
			else if constexpr (MapLikeContainer<MemberType>) {
				// Map-like containers (XMap)
				// Requires key-value pair migration
				migrate_map(old_member, new_member, old_xbuf, new_xbuf);
			}
			else {
				// Nested user-defined struct: recursive migration
				migrate_members(old_member, new_member, old_xbuf, new_xbuf);
			}
		}
		
		// ====================================================================
		// Reflection-based member migration using index sequence
		// ====================================================================
		
		// Helper: Get member count
		template<typename T>
		static consteval std::size_t get_member_count_impl() {
			using namespace std::meta;
			return nonstatic_data_members_of(^^T, access_context::unchecked()).size();
		}
		
		// Helper: Get member info at specific index (compile-time)
		template<typename T, std::size_t Index>
		static consteval auto get_member_at() {
			using namespace std::meta;
			auto members = nonstatic_data_members_of(^^T, access_context::unchecked());
			return members[Index];
		}
		
		// Helper: Migrate a single member at Index using splice
		template<typename T, std::size_t Index>
		static void migrate_member_at(const T& old_obj, T& new_obj,
		                              XBuffer& old_xbuf, XBuffer& new_xbuf) {
			using namespace std::meta;
			
			// Get member info at compile-time (constexpr context)
			constexpr auto member = get_member_at<T, Index>();
			
			// Get member type using splice (this works because member is constexpr!)
			using MemberType = [:type_of(member):];
			
			// Access members using splice (pattern from test_splice_operations.cpp)
			// Since member is constexpr, we can use obj.[:member:] syntax!
			const auto& old_member = old_obj.[:member:];
			auto& new_member = new_obj.[:member:];
			
			// Migrate this member based on its type
			migrate_member<MemberType>(old_member, new_member, old_xbuf, new_xbuf);
		}
		
		// Helper: Migrate all members using fold expression
		template<typename T, std::size_t... Is>
		static void migrate_members_impl(const T& old_obj, T& new_obj,
		                                 XBuffer& old_xbuf, XBuffer& new_xbuf,
		                                 std::index_sequence<Is...>) {
			// Fold expression: expand migrate_member_at<T, 0>(), migrate_member_at<T, 1>(), ...
			(migrate_member_at<T, Is>(old_obj, new_obj, old_xbuf, new_xbuf), ...);
		}
		
		// Main: Migrate all members of a struct using C++26 reflection
		template<typename T>
		static void migrate_members(const T& old_obj, T& new_obj, 
		                           XBuffer& old_xbuf, XBuffer& new_xbuf) {
			// Get member count at compile-time
			constexpr std::size_t member_count = get_member_count_impl<T>();
			
			// Use index sequence + fold expression to migrate all members
			migrate_members_impl(old_obj, new_obj, old_xbuf, new_xbuf,
			                    std::make_index_sequence<member_count>{});
		}
	};

	// ========================================================================
	// XBufferExt: Extended XBuffer with Unified make() API
	// ========================================================================
	// Helper trait to detect XString type
	template<typename T>
	struct is_xstring : std::false_type {};
	template<>
	struct is_xstring<XString> : std::true_type {};

	// Helper trait to detect allocator
	template<typename T>
	struct is_allocator : std::false_type {};
	template<typename T, typename M>
	struct is_allocator<boost::interprocess::allocator<T, M>> : std::true_type {};

	class XBufferExt : public XBuffer {
	public:
		using XBuffer::XBuffer;

		// ============================================================================
		// Object Creation API
		// ============================================================================
		
		// Create a named object that can be found later
		// Usage: auto* game = xbuf.make<GameData>("save");
		template<typename T>
		T* make(const char* name) {
			return this->construct<T>(name)(this->get_segment_manager());
		}
		
		// Get allocator for constructing allocator-aware types
		// Usage: XString str("text", xbuf.allocator<XString>());
		//        vec.emplace_back(xbuf.allocator<Item>());
		template<typename T>
		boost::interprocess::allocator<T, XBuffer::segment_manager> allocator() {
			return boost::interprocess::allocator<T, XBuffer::segment_manager>(this->get_segment_manager());
		}

		// ============================================================================
		// Find and Utility Methods
		// ============================================================================
		// Find object
		template<typename T>
		std::pair<T*, bool> find_ex(const char* name) {
			auto result = this->find<T>(name);
			return {result.first, result.second};
		}
		// Find or make object
		template<typename T>
		T* find_or_make(const char* name) {
			return this->find_or_construct<T>(name)(this->get_segment_manager());
		}

		// Serialization and Deserialization
		// Save to string
		std::string save_to_string() {
			auto* buffer = this->get_buffer();
			return std::string(buffer->begin(), buffer->end());
		}
		// Load from string
		static XBufferExt load_from_string(const std::string& data) {
			std::vector<char> buffer(data.begin(), data.end());
			XBufferExt xbuf(buffer);
			return xbuf;
		}

		// Memory statistics
		void print_stats() {
			XBufferVisualizer::print_stats(*this);
		}
		XBufferVisualizer::MemoryStats stats() {
			return XBufferVisualizer::get_memory_stats(*this);
		}
	};
}

// ============================================================================
// Type Signature Support for XOffsetDatastructure2 Containers
// (COMMENTED OUT - Focus on compact first)
// ============================================================================
#if 0
namespace XTypeSignature {
    // XString signature
    template <>
    struct TypeSignature<XOffsetDatastructure2::XString> {
        static consteval auto calculate() noexcept {
            return CompileString{"string[s:32,a:8]"};
        }
    };
    
    // XVector<T> signature
    template <typename T>
    struct TypeSignature<XOffsetDatastructure2::XVector<T>> {
        static consteval auto calculate() noexcept {
            return CompileString{"vector[s:32,a:8]<"} +
                   TypeSignature<T>::calculate() +
                   CompileString{">"};
        }
    };
    
    // XSet<T> signature
    template <typename T>
    struct TypeSignature<XOffsetDatastructure2::XSet<T>> {
        static consteval auto calculate() noexcept {
            return CompileString{"set[s:32,a:8]<"} +
                   TypeSignature<T>::calculate() +
                   CompileString{">"};
        }
    };
    
    // XMap<K,V> signature
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
#endif // Type Signature for containers (commented out)
#endif // X_OFFSET_DATA_STRUCTURE_2_HPP
