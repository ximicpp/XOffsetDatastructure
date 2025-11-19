#ifndef X_OFFSET_DATA_STRUCTURE_2_HPP
#define X_OFFSET_DATA_STRUCTURE_2_HPP

// XOffsetDatastructure2 - Single Header Library for Offset-Based Data Structures
// Platform Detection & Configuration
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

// Standard Library Headers
#include <string_view>
#include <type_traits>
#include <vector>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <string>
#include <fstream>

// Boost Headers
#include <boost/pfr.hpp>
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

// XTypeSignature - Compile-Time Type Signature System
namespace XTypeSignature {
    inline constexpr int BASIC_ALIGNMENT = 8;
    inline constexpr int ANY_SIZE = 64;

    // Type Size Validations
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

    // ============================================================================
    // Field Type and Offset Calculation
    // MSVC: Uses manual field registration to avoid Boost.PFR issues
    // GCC/Clang: Uses Boost.PFR for automatic reflection
    // ============================================================================
    
    // Unified implementation for all compilers using lightweight Boost.PFR APIs
    // Get field type using PFR's tuple_element (avoids structure_to_tuple instantiation)
    template<typename T, size_t Index>
    struct field_type {
        using type = typename boost::pfr::tuple_element<Index, T>::type;
    };

    template<typename T, size_t Index>
    using field_type_t = typename field_type<T, Index>::type;

    template<typename T, size_t Index>
    consteval size_t get_field_offset() noexcept {
        if constexpr (Index == 0) {
            return 0;
        } else {
            using PrevType = field_type_t<T, Index - 1>;
            using CurrType = field_type_t<T, Index>;
            constexpr size_t prev_offset = get_field_offset<T, Index - 1>();
            constexpr size_t prev_size = sizeof(PrevType);
            constexpr size_t curr_align = alignof(CurrType);
            return (prev_offset + prev_size + (curr_align - 1)) & ~(curr_align - 1);
        }
    }

    // Field Signature Generation (Fold Expression)
    template<typename T, size_t Index>
    consteval auto build_single_field_signature() noexcept {
        using FieldType = field_type_t<T, Index>;
        
        return CompileString{"@"} +
               CompileString<32>::from_number(get_field_offset<T, Index>()) +
               CompileString{":"} +
               TypeSignature<FieldType>::calculate();
    }

    template<typename T, size_t Index, bool IsFirst>
    consteval auto build_field_with_comma() noexcept {
        if constexpr (IsFirst) {
            return build_single_field_signature<T, Index>();
        } else {
            return CompileString{","} + build_single_field_signature<T, Index>();
        }
    }

    template<typename T, size_t... Indices>
    consteval auto concatenate_field_signatures(std::index_sequence<Indices...>) noexcept {
        return (build_field_with_comma<T, Indices, (Indices == 0)>() + ...);
    }

    template <typename T>
    consteval auto get_fields_signature() noexcept {
        constexpr size_t count = boost::pfr::tuple_size_v<T>;
        if constexpr (count == 0) {
            return CompileString{""};
        } else {
            return concatenate_field_signatures<T>(std::make_index_sequence<count>{});
        }
    }

    // Macro to simplify basic type signature definitions
    #define DEFINE_BASIC_TYPE_SIGNATURE(TYPE, SIG, SIZE, ALIGN) \
        template <> struct TypeSignature<TYPE> { \
            static constexpr auto calculate() noexcept { \
                return CompileString{SIG "[s:" #SIZE ",a:" #ALIGN "]"}; \
            } \
        };

    DEFINE_BASIC_TYPE_SIGNATURE(int32_t,  "i32",  4, 4)
    DEFINE_BASIC_TYPE_SIGNATURE(uint32_t, "u32",  4, 4)
    DEFINE_BASIC_TYPE_SIGNATURE(int64_t,  "i64",  8, 8)
    DEFINE_BASIC_TYPE_SIGNATURE(uint64_t, "u64",  8, 8)
    DEFINE_BASIC_TYPE_SIGNATURE(float,    "f32",  4, 4)
    DEFINE_BASIC_TYPE_SIGNATURE(double,   "f64",  8, 8)
    DEFINE_BASIC_TYPE_SIGNATURE(bool,     "bool", 1, 1)
    DEFINE_BASIC_TYPE_SIGNATURE(char,     "char", 1, 1)

    #undef DEFINE_BASIC_TYPE_SIGNATURE
    
    template <typename T> struct TypeSignature<T*>   { static constexpr auto calculate() noexcept { return CompileString{"ptr[s:8,a:8]"}; } };
    template <>           struct TypeSignature<void*>{ static constexpr auto calculate() noexcept { return CompileString{"ptr[s:8,a:8]"}; } };

    template <typename T, size_t N>
    struct TypeSignature<T[N]> {
        static constexpr auto calculate() noexcept {
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
        static constexpr auto calculate() noexcept { return CompileString{"bytes[s:64,a:1]"}; }
    };

    template <typename T>
    struct TypeSignature<const T> {
        static consteval auto calculate() noexcept {
            return TypeSignature<T>::calculate();
        }
    };

    template <typename T>
    struct TypeSignature {
        static constexpr auto calculate() noexcept {
            if constexpr (std::is_aggregate_v<T> && !std::is_array_v<T>) {
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

    template <typename T>
    [[nodiscard]] consteval auto get_XTypeSignature() noexcept {
        return TypeSignature<T>::calculate();
    }

} // namespace XTypeSignature

using XTypeSignature::BASIC_ALIGNMENT;

// Boost Interprocess Extensions
namespace boost {
namespace interprocess {

template <class MutexFamily, class VoidPointer = offset_ptr<void>, std::size_t MemAlignment = 0>
class x_best_fit : public rbtree_best_fit<MutexFamily, VoidPointer, MemAlignment> {
    typedef rbtree_best_fit<MutexFamily, VoidPointer, MemAlignment> supertype;
public:
    typedef typename supertype::size_type size_type;
    x_best_fit(typename supertype::size_type size, typename supertype::size_type extra_hdr_bytes)
        : supertype(size, extra_hdr_bytes) {}
};

template <class MutexFamily, class VoidPointer = offset_ptr<void>>
class x_seq_fit : public simple_seq_fit<MutexFamily, VoidPointer> {
    typedef simple_seq_fit<MutexFamily, VoidPointer> supertype;
public:
    typedef typename supertype::size_type size_type;
    x_seq_fit(typename supertype::size_type segment_size, typename supertype::size_type extra_hdr_bytes)
        : supertype(segment_size, extra_hdr_bytes) {}
};

template <class CharType, class AllocationAlgorithm, template <class IndexConfig> class IndexType>
class XManagedMemory : public ipcdetail::basic_managed_memory_impl<CharType, AllocationAlgorithm, IndexType> {
private:
    typedef ipcdetail::basic_managed_memory_impl<CharType, AllocationAlgorithm, IndexType> base_t;
    BOOST_MOVABLE_BUT_NOT_COPYABLE(XManagedMemory)
public:
    typedef typename base_t::size_type size_type;
    XManagedMemory() noexcept {}
    ~XManagedMemory() {
        this->priv_close();
    }

    XManagedMemory(size_type size) : m_buffer(size, char(0)) {
        void *addr = m_buffer.data();
        if (!base_t::create_impl(addr, size)) {
            this->priv_close();
            throw interprocess_exception("Could not initialize heap in XManagedMemory constructor");
        }
    }

    XManagedMemory(const char* data, size_type size) : m_buffer(data, data + size) {
        void *addr = m_buffer.data();
        BOOST_ASSERT((0 == (((std::size_t)addr) & (AllocationAlgorithm::Alignment - size_type(1u)))));
        if (!base_t::open_impl(addr, size)) {
            throw interprocess_exception("Could not initialize m_buffer in constructor");
        }
    }

    XManagedMemory(std::vector<char> &externalBuffer) : m_buffer(std::move(externalBuffer)) {
        void *addr = m_buffer.data();
        size_type size = m_buffer.size();
        BOOST_ASSERT((0 == (((std::size_t)addr) & (AllocationAlgorithm::Alignment - size_type(1u)))));
        if (!base_t::open_impl(addr, size)) {
            throw interprocess_exception("Could not initialize m_buffer in constructor");
        }
    }

    XManagedMemory(BOOST_RV_REF(XManagedMemory) moved) noexcept {
        this->swap(moved);
    }

    XManagedMemory &operator=(BOOST_RV_REF(XManagedMemory) moved) noexcept {
        XManagedMemory tmp(boost::move(moved));
        this->swap(tmp);
        return *this;
    }

    bool grow(size_type extra_bytes) {
        try {
            m_buffer.resize(m_buffer.size() + extra_bytes);
            base_t::close_impl();
            base_t::open_impl(&m_buffer[0], m_buffer.size());
            base_t::grow(extra_bytes);
            return true;
        } catch(...) {
            return false;
        }
    }

    void swap(XManagedMemory &other) noexcept {
        base_t::swap(other);
        m_buffer.swap(other.m_buffer);
    }

    void update_after_shrink() {
        auto *pBuf = get_buffer();
        std::vector<char> new_buf(pBuf->data(), pBuf->data() + pBuf->size());
        XManagedMemory new_mem(new_buf);
        this->swap(new_mem);
    }

    void shrink_to_fit() {
        base_t::shrink_to_fit();
        m_buffer.resize(base_t::get_size());
        update_after_shrink();
    }

    std::vector<char> *get_buffer() {
        return &m_buffer;
    }

    const void* get_address() const {
        return m_buffer.data();
    }

    size_type get_size() const {
        return m_buffer.size();
    }

private:
    void priv_close() {
        base_t::destroy_impl();
        std::vector<char>().swap(m_buffer);
    }

    std::vector<char> m_buffer;
};

} // namespace interprocess
} // namespace boost

namespace XOffsetDatastructure2 {
	using namespace boost::interprocess;
	typedef XManagedMemory<char, x_seq_fit<null_mutex_family>, iset_index> XBufferBase;

	struct growth_factor_custom : boost::container::dtl::grow_factor_ratio<0, 11, 10> {};

    template <typename T>
    using XOffsetPtr = boost::interprocess::offset_ptr<T>;

#if OFFSET_DATA_STRUCTURE_2_CUSTOM_CONTAINER_GROWTH_FACTOR == 0
	template <typename T>
	using XVector = boost::container::vector<T, allocator<T, XBufferBase::segment_manager>>;
#elif OFFSET_DATA_STRUCTURE_2_CUSTOM_CONTAINER_GROWTH_FACTOR == 1
	using vector_option = boost::container::vector_options_t<boost::container::growth_factor<growth_factor_custom>>;
	template <typename T>
	using XVector = boost::container::vector<T, allocator<T, XBufferBase::segment_manager>, vector_option>;
#endif

#if OFFSET_DATA_STRUCTURE_2_CUSTOM_CONTAINER_GROWTH_FACTOR == 0
	template <typename T>
	using XSet = boost::container::flat_set<T, std::less<T>, allocator<T, XBufferBase::segment_manager>>;
	// using XSet = boost::container::set<T, std::less<T>, allocator<T, XBufferBase::segment_manager>>;
#elif OFFSET_DATA_STRUCTURE_2_CUSTOM_CONTAINER_GROWTH_FACTOR == 1
	using vector_option_flatset = boost::container::vector_options_t<boost::container::growth_factor<growth_factor_custom>>;
	template <typename T>
	using XVector_flatset = boost::container::vector<T, allocator<T, XBufferBase::segment_manager>, vector_option_flatset>;
	template <typename T>
	using XSet = boost::container::flat_set<T, std::less<T>, XVector_flatset<T>>;
#endif

#if OFFSET_DATA_STRUCTURE_2_CUSTOM_CONTAINER_GROWTH_FACTOR == 0
	template <typename K, typename V>
	using XMap = boost::container::flat_map<K, V, std::less<K>, allocator<std::pair<K, V>, XBufferBase::segment_manager>>;
	// using XMap = boost::container::map<K, V, std::less<K>, allocator<std::pair<const K, V>, XBufferBase::segment_manager>>;
#elif OFFSET_DATA_STRUCTURE_2_CUSTOM_CONTAINER_GROWTH_FACTOR == 1
	using vector_option_flatmap = boost::container::vector_options_t<boost::container::growth_factor<growth_factor_custom>>;
	template <typename K, typename V>
	using XVector_flatmap = boost::container::vector<std::pair<K, V>, allocator<std::pair<K, V>, XBufferBase::segment_manager>, vector_option_flatmap>;
	template <typename K, typename V>
	using XMap = boost::container::flat_map<K, V, std::less<K>, XVector_flatmap<K, V>>;
#endif

	using XString = boost::container::basic_string<char, std::char_traits<char>, allocator<char, XBufferBase::segment_manager>>;

	// XBuffer Memory Visualization
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

		static MemoryStats get_memory_stats(XBufferBase& xbuf) {
			MemoryStats stats = {};
			stats.total_size = xbuf.get_size();
			stats.free_size = xbuf.get_free_memory();
			stats.used_size = stats.total_size - stats.free_size;
			return stats;
		}

		static void print_stats(XBufferBase& xbuf) {
			MemoryStats stats = get_memory_stats(xbuf);
			std::cout << "XBuffer: " << stats.used_size << "/" << stats.total_size 
			          << " bytes (" << std::fixed << std::setprecision(1) 
			          << stats.usage_percent() << "% used)" << std::endl;
		}
	};

	// Type Safety Checking System using C++20 Concepts
	namespace detail {
		// Forward declaration of the helper function for recursion
		template<typename T>
		constexpr bool is_xbuffer_safe_impl();
		
		// 1. Basic Type Concept
		template<typename T>
		concept BasicType = 
			std::is_same_v<std::remove_cv_t<T>, int8_t> ||
			std::is_same_v<std::remove_cv_t<T>, int16_t> ||
			std::is_same_v<std::remove_cv_t<T>, int32_t> ||
			std::is_same_v<std::remove_cv_t<T>, int64_t> ||
			std::is_same_v<std::remove_cv_t<T>, uint8_t> ||
			std::is_same_v<std::remove_cv_t<T>, uint16_t> ||
			std::is_same_v<std::remove_cv_t<T>, uint32_t> ||
			std::is_same_v<std::remove_cv_t<T>, uint64_t> ||
			std::is_same_v<std::remove_cv_t<T>, float> ||
			std::is_same_v<std::remove_cv_t<T>, double> ||
			std::is_same_v<std::remove_cv_t<T>, bool> ||
			std::is_same_v<std::remove_cv_t<T>, char>;
		
		// 2. XString Type Concept
		template<typename T>
		concept XStringType = std::is_same_v<std::remove_cv_t<T>, XString>;
		
		// 3. Container Type Detection Concepts
		template<typename T>
		concept XVectorLike = 
			sizeof(std::remove_cv_t<T>) == 32 && 
			alignof(std::remove_cv_t<T>) == 8 &&
			requires { typename std::remove_cv_t<T>::value_type; };
		
		template<typename T>
		concept XSetLike = 
			sizeof(std::remove_cv_t<T>) == 32 && 
			alignof(std::remove_cv_t<T>) == 8 &&
			requires { typename std::remove_cv_t<T>::key_type; } &&
			!requires { typename std::remove_cv_t<T>::mapped_type; };
		
		template<typename T>
		concept XMapLike = 
			sizeof(std::remove_cv_t<T>) == 32 && 
			alignof(std::remove_cv_t<T>) == 8 &&
			requires { 
				typename std::remove_cv_t<T>::key_type; 
				typename std::remove_cv_t<T>::mapped_type; 
			};
		
		// 4. Safe Container Concepts (using helper function for recursion)
		template<typename T>
		concept SafeXVector = XVectorLike<T> && 
			is_xbuffer_safe_impl<typename std::remove_cv_t<T>::value_type>();
		
		template<typename T>
		concept SafeXSet = XSetLike<T> && 
			is_xbuffer_safe_impl<typename std::remove_cv_t<T>::key_type>();
		
		template<typename T>
		concept SafeXMap = XMapLike<T> && 
			is_xbuffer_safe_impl<typename std::remove_cv_t<T>::key_type>() &&
			is_xbuffer_safe_impl<typename std::remove_cv_t<T>::mapped_type>();
		
		// 5. Member Safety Checking Concepts
		template<typename T, size_t Index>
		concept MemberSafeAt = requires {
			requires is_xbuffer_safe_impl<typename boost::pfr::tuple_element<Index, T>::type>();
			requires !std::is_reference_v<typename boost::pfr::tuple_element<Index, T>::type>;
			requires !std::is_pointer_v<typename boost::pfr::tuple_element<Index, T>::type>;
		};
		
		// Helper to check all members
		template<typename T, size_t... Indices>
		constexpr bool check_all_members_safe(std::index_sequence<Indices...>) {
			return (MemberSafeAt<T, Indices> && ...);
		}
		
		// 6. Aggregate Type Concept with Safe Members
		template<typename T>
		concept HasSafeProperties = 
			!std::is_polymorphic_v<T> &&
			!std::is_union_v<T> &&
			(boost::pfr::tuple_size_v<T> == 0 || 
			 check_all_members_safe<T>(std::make_index_sequence<boost::pfr::tuple_size_v<T>>{}));
		
		template<typename T>
		concept ReflectableAggregate = 
			std::is_class_v<T> && 
			std::is_aggregate_v<T> &&
			!XStringType<T> &&
			HasSafeProperties<T>;
		
		// 7. Top-level XBufferSafe Concept
		template<typename T>
		concept XBufferSafe = 
			BasicType<T> || 
			XStringType<T> ||
			SafeXVector<T> ||
			SafeXSet<T> ||
			SafeXMap<T> ||
			ReflectableAggregate<T>;
		
		// Implementation of the recursive helper function
		template<typename T>
		constexpr bool is_xbuffer_safe_impl() {
			return XBufferSafe<T>;
		}
		
		// 8. Error Message Generation
		template<typename T>
		constexpr const char* get_safety_error_message() {
			using CleanT = std::remove_cv_t<T>;
			
			if constexpr (XBufferSafe<CleanT>) {
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
		
		// Legacy function wrapper for backward compatibility
		template<typename T>
		constexpr bool is_safe_type() {
			return XBufferSafe<T>;
		}
	}
	
	// Public API for Type Safety Checking
	template<typename T>
	struct is_xbuffer_safe {
		static constexpr bool value = detail::is_safe_type<T>();
		
		static constexpr const char* reason() {
			return detail::get_safety_error_message<T>();
		}
	};

	// XBufferExt: Extended XBuffer
	
	// Type trait for XString
	template<typename T>
	struct is_xstring : std::false_type {};
	template<>
	struct is_xstring<XString> : std::true_type {};

	// Type traits for containers (XVector, XSet, XMap)
	template<typename T>
	struct is_xvector : std::false_type {};
	template<typename T>
	struct is_xvector<XVector<T>> : std::true_type {};

	template<typename T>
	struct is_xset : std::false_type {};
	template<typename T>
	struct is_xset<XSet<T>> : std::true_type {};

	template<typename T>
	struct is_xmap : std::false_type {};
	template<typename K, typename V>
	struct is_xmap<XMap<K, V>> : std::true_type {};

	// Unified container detection
	template<typename T>
	struct is_xcontainer : std::false_type {};
	template<typename T>
	struct is_xcontainer<XVector<T>> : std::true_type {};
	template<typename T>
	struct is_xcontainer<XSet<T>> : std::true_type {};
	template<typename K, typename V>
	struct is_xcontainer<XMap<K, V>> : std::true_type {};

	class XBuffer : public XBufferBase {
	public:
		using XBufferBase::XBufferBase;

	// Object Creation API - Root Objects (named, persistent, findable)
	template<typename T>
	T* make_root(const char* name) {
		return this->construct<T>(name)(this->get_segment_manager());
	}
		
		// Allocator access methods (unified naming)
		template<typename T>
		boost::interprocess::allocator<T, XBufferBase::segment_manager> allocator() {
			return boost::interprocess::allocator<T, XBufferBase::segment_manager>(this->get_segment_manager());
		}

		template<typename T>
		boost::interprocess::allocator<T, XBufferBase::segment_manager> get_allocator() {
			return allocator<T>();
		}

		// ========== Convenient Factory Method for Field Values ==========
		// Unified interface: create<T>() for creating field values with automatic allocator injection
		// 
		// Usage examples:
		//   - XString: xbuf.create<XString>("text")
		//   - XVector: xbuf.create<XVector<int>>()  (empty vector)
		//   - XSet:    xbuf.create<XSet<int>>()     (empty set)
		//   - XMap:    xbuf.create<XMap<XString, int>>()  (empty map)
		//
		// NOTE: For custom structs used in emplace_back, it's more efficient to pass 
		//       the allocator directly rather than using create<T>():
		//       GOOD:  vec.emplace_back(xbuf.allocator<Item>(), args...)
		//       AVOID: vec.emplace_back(xbuf.create<Item>(args...))  // Creates temporary
		template<typename T, typename... Args>
		T create(Args&&... args) {
			auto al = this->get_allocator<T>();
			
			if constexpr (is_xstring<T>::value) {
				// XString: allocator as the last parameter
				return T(std::forward<Args>(args)..., al);
			} else if constexpr (is_xcontainer<T>::value) {
				// XVector/XSet/XMap: allocator as the first parameter
				return T(al, std::forward<Args>(args)...);
			} else {
				// User-defined struct: no automatic allocator injection
				// The user must handle allocator passing themselves if needed
				return T(std::forward<Args>(args)...);
			}
		}

        // Find and Utility Methods - Root Objects
        template<typename T>
        std::pair<T*, bool> find_root(const char* name) {
            auto result = XBufferBase::find<T>(name);
            return {result.first, result.second};
        }
        template<typename T>
        T* find_or_make_root(const char* name) {
            return this->find_or_construct<T>(name)(this->get_segment_manager());
        }

		// Serialization
		std::string save_to_string() {
			auto* buffer = this->get_buffer();
			return std::string(buffer->begin(), buffer->end());
		}
		static XBuffer load_from_string(const std::string& data) {
			std::vector<char> buffer(data.begin(), data.end());
			XBuffer xbuf(buffer);
			return xbuf;
		}

		void print_stats() {
			XBufferVisualizer::print_stats(*this);
		}
		XBufferVisualizer::MemoryStats stats() {
			return XBufferVisualizer::get_memory_stats(*this);
		}
	};

	// Memory Compaction (Experimental)
	template<typename T, typename = void>
	struct has_migrate : std::false_type {};
	template<typename T>
	struct has_migrate<T, std::void_t<decltype(T::migrate(std::declval<XBufferBase&>(), std::declval<XBufferBase&>()))>> : std::true_type {};

	class XBufferCompactor {
	public:
		template<typename T>
		static XBufferBase compact(XBufferBase& old_xbuf) {
			static_assert(sizeof(T) == 0, 
				"Memory compaction is not yet implemented in this version. "
				"This feature will be available in a future release with C++26 reflection support.");
			return XBufferBase();
		}
	};
}

// Type Signature Support for XOffsetDatastructure2 Containers
namespace XTypeSignature {
    template <>
    struct TypeSignature<XOffsetDatastructure2::XString> {
        static constexpr auto calculate() noexcept {
            return CompileString{"string[s:32,a:8]"};
        }
    };
    template <typename T>
    struct TypeSignature<XOffsetDatastructure2::XVector<T>> {
        static constexpr auto calculate() noexcept {
            return CompileString{"vector[s:32,a:8]<"} +
                   TypeSignature<T>::calculate() +
                   CompileString{">"};
        }
    };
    template <typename T>
    struct TypeSignature<XOffsetDatastructure2::XSet<T>> {
        static constexpr auto calculate() noexcept {
            return CompileString{"set[s:32,a:8]<"} +
                   TypeSignature<T>::calculate() +
                   CompileString{">"};
        }
    };
    template <typename K, typename V>
    struct TypeSignature<XOffsetDatastructure2::XMap<K, V>> {
        static constexpr auto calculate() noexcept {
            return CompileString{"map[s:32,a:8]<"} +
                   TypeSignature<K>::calculate() +
                   CompileString{","} +
                   TypeSignature<V>::calculate() +
                   CompileString{">"};
        }
    };
} // namespace XTypeSignature
#endif