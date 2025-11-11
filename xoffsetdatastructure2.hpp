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

    // Helper: Get field type using PFR's tuple_element (lighter than structure_to_tuple)
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

    template <> struct TypeSignature<int32_t>  { static constexpr auto calculate() noexcept { return CompileString{"i32[s:4,a:4]"}; } };
    template <> struct TypeSignature<uint32_t> { static constexpr auto calculate() noexcept { return CompileString{"u32[s:4,a:4]"}; } };
    template <> struct TypeSignature<int64_t>  { static constexpr auto calculate() noexcept { return CompileString{"i64[s:8,a:8]"}; } };
    template <> struct TypeSignature<uint64_t> { static constexpr auto calculate() noexcept { return CompileString{"u64[s:8,a:8]"}; } };
    template <> struct TypeSignature<float>    { static constexpr auto calculate() noexcept { return CompileString{"f32[s:4,a:4]"}; } };
    template <> struct TypeSignature<double>   { static constexpr auto calculate() noexcept { return CompileString{"f64[s:8,a:8]"}; } };
    template <> struct TypeSignature<bool>     { static constexpr auto calculate() noexcept { return CompileString{"bool[s:1,a:1]"}; } };
    template <> struct TypeSignature<char>     { static constexpr auto calculate() noexcept { return CompileString{"char[s:1,a:1]"}; } };
    
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
	
	// Base implementation - users should use XBuffer instead
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

	// Type Safety Checking System
	namespace detail {
		// 1. Basic Type Checking
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
		
		// 2. XString Type Checking
		template<typename T>
		consteval bool is_xstring() {
			return std::is_same_v<std::remove_cv_t<T>, XString>;
		}
		
		// Forward declaration for recursive checking
		template<typename T>
		consteval bool is_safe_type();
		
		// 3. XVector Type Checking
		template<typename T>
		consteval bool is_safe_xvector() {
			using CleanT = std::remove_cv_t<T>;
			// XVector has size=32, align=8
			if constexpr (sizeof(CleanT) == 32 && alignof(CleanT) == 8) {
				// Try to extract value_type
				if constexpr (requires { typename CleanT::value_type; }) {
					return is_safe_type<typename CleanT::value_type>();
				}
			}
			return false;
		}
		
		// 4. XSet Type Checking
		template<typename T>
		consteval bool is_safe_xset() {
			using CleanT = std::remove_cv_t<T>;
			// XSet has size=32, align=8
			if constexpr (sizeof(CleanT) == 32 && alignof(CleanT) == 8) {
				// Try to extract key_type (XSet uses key_type, not value_type)
				if constexpr (requires { typename CleanT::key_type; }) {
					return is_safe_type<typename CleanT::key_type>();
				}
			}
			return false;
		}
		
		// 5. XMap Type Checking
		template<typename T>
		consteval bool is_safe_xmap() {
			using CleanT = std::remove_cv_t<T>;
			// XMap has size=32, align=8
			if constexpr (sizeof(CleanT) == 32 && alignof(CleanT) == 8) {
				// Try to extract key_type and mapped_type
				if constexpr (requires { typename CleanT::key_type; typename CleanT::mapped_type; }) {
					return is_safe_type<typename CleanT::key_type>() &&
					       is_safe_type<typename CleanT::mapped_type>();
				}
			}
			return false;
		}
		
		// 6. Member Safety Checking (using Boost.PFR)
		template<typename T, size_t Index>
		consteval bool is_member_safe_at() {
			using MemberType = std::tuple_element_t<Index, 
				decltype(boost::pfr::structure_to_tuple(std::declval<T>()))>;
			
			// Check if member type is safe
			if (!is_safe_type<MemberType>()) {
				return false;
			}
			
			// Reject references
			if constexpr (std::is_reference_v<MemberType>) {
				return false;
			}
			
			// Reject raw pointers (should use XOffsetPtr)
			if constexpr (std::is_pointer_v<MemberType>) {
				return false;
			}
			
			return true;
		}
		
		// 7. Check All Members
		template<typename T, size_t... Indices>
		consteval bool check_all_members_impl(std::index_sequence<Indices...>) {
			return (is_member_safe_at<T, Indices>() && ...);
		}
		
		template<typename T>
		consteval bool are_all_members_safe() {
			// Must be a class/struct
			if constexpr (!std::is_class_v<T>) {
				return false;
			}
			
			// Reject polymorphic types (with virtual functions)
			if constexpr (std::is_polymorphic_v<T>) {
				return false;
			}
			
			// Reject union types
			if constexpr (std::is_union_v<T>) {
				return false;
			}
			
			// Must be aggregate for Boost.PFR
			if constexpr (!std::is_aggregate_v<T>) {
				return false;
			}
			
			// Check all members
			constexpr size_t member_count = boost::pfr::tuple_size_v<T>;
			if constexpr (member_count == 0) {
				return true;
			} else {
				return check_all_members_impl<T>(std::make_index_sequence<member_count>{});
			}
		}
		
		// 8. Comprehensive Type Safety Check
		template<typename T>
		consteval bool is_safe_type() {
			using CleanT = std::remove_cv_t<T>;
			
			// Basic types are safe
			if constexpr (is_basic_type<CleanT>()) {
				return true;
			}
			
			// XString is safe
			if constexpr (is_xstring<CleanT>()) {
				return true;
			}
			
			// Check XOffset containers (MUST check before attempting PFR reflection)
			if constexpr (is_safe_xvector<CleanT>()) {
				return true;
			}
			if constexpr (is_safe_xset<CleanT>()) {
				return true;
			}
			if constexpr (is_safe_xmap<CleanT>()) {
				return true;
			}
			
			// User-defined struct/class: check all members recursively
			// Only attempt this for aggregate types (Boost.PFR requirement)
			if constexpr (std::is_class_v<CleanT> && std::is_aggregate_v<CleanT> && !is_xstring<CleanT>()) {
				return are_all_members_safe<CleanT>();
			}
			
			return false;
		}
		
		// 9. Error Message Generation
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
				// This might be a std container
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
	
	// Public API for Type Safety Checking
	template<typename T>
	struct is_xbuffer_safe {
		static constexpr bool value = detail::is_safe_type<T>();
		
		static constexpr const char* reason() {
			return detail::get_safety_error_message<T>();
		}
	};

	// XBufferExt: Extended XBuffer
	template<typename T>
	struct is_xstring : std::false_type {};
	template<>
	struct is_xstring<XString> : std::true_type {};

	// Main XBuffer interface - this is what users should use
	class XBuffer : public XBufferBase {
	public:
		using XBufferBase::XBufferBase;

		// Object Creation API
		template<typename T>
		T* make(const char* name) {
			return this->construct<T>(name)(this->get_segment_manager());
		}
		
		template<typename T>
		boost::interprocess::allocator<T, XBufferBase::segment_manager> allocator() {
			return boost::interprocess::allocator<T, XBufferBase::segment_manager>(this->get_segment_manager());
		}

		// Find and Utility Methods
		template<typename T>
		T* find(const char* name) {
			auto result = this->XBufferBase::find<T>(name);
			return result.first;
		}
		
		template<typename T>
		T* find_or_make(const char* name) {
			return this->find_or_construct<T>(name)(this->get_segment_manager());
		}
		
		template<typename T>
		T& get_or_create(const char* name) {
			T* ptr = find_or_make<T>(name);
			return *ptr;
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

		// Statistics
		void print_stats() {
			XBufferVisualizer::print_stats(*this);
		}
		
		XBufferVisualizer::MemoryStats stats() {
			return XBufferVisualizer::get_memory_stats(*this);
		}
	};
	
	// Backward compatibility alias (deprecated)
	using XBufferExt = XBuffer;

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