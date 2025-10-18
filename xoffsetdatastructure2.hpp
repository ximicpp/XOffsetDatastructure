#ifndef X_OFFSET_DATA_STRUCTURE_2_HPP
#define X_OFFSET_DATA_STRUCTURE_2_HPP
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
		template<typename T>
		static XBuffer compact_automatic(XBuffer& old_xbuf) {
#if __cpp_reflection >= 202306L  // Check for C++26 reflection support
			// Calculate new buffer size
			auto stats = XBufferVisualizer::get_memory_stats(old_xbuf);
			std::size_t new_size = stats.used_size + (stats.used_size / 10);
			if (new_size < 4096) new_size = 4096;
			
			XBuffer new_xbuf(new_size);
			
			// Find old object in fragmented buffer
			auto* old_obj = old_xbuf.find<T>("MyTest").first;
			if (!old_obj) {
				return new_xbuf;
			}
			
			// Create new object in compact buffer
			auto* new_obj = new_xbuf.construct<T>("MyTest")(new_xbuf.get_segment_manager());
			
			// Migrate all members automatically using reflection
			migrate_members(*old_obj, *new_obj, old_xbuf, new_xbuf);
			
			// Shrink to fit: Remove unused memory
			new_xbuf.shrink_to_fit();
			
			return new_xbuf;
#else
			(void)old_xbuf;
			static_assert(sizeof(T) == 0, 
				"compact_automatic requires C++26 reflection (P2996R5 or later). "
				"Your compiler does not support __cpp_reflection >= 202306L. "
				"Please use compact_manual<T> instead or upgrade to a C++26-compliant compiler.");
			return XBuffer();
#endif
		}

	private:
#if __cpp_reflection >= 202306L
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
		
		// ====================================================================
		// C++20 Concepts for container classification
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
		
		// Check if type is a simple POD (int, float, etc.)
		template<typename T>
		constexpr bool is_simple_pod_v = std::is_trivially_copyable_v<T> && 
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
		
		// Migrate all members of a struct using C++26 reflection
		template<typename T>
		static void migrate_members(const T& old_obj, T& new_obj, 
		                           XBuffer& old_xbuf, XBuffer& new_xbuf) {
			// Get all non-static data members via reflection
			constexpr auto members = std::meta::members_of(^T);
			
			// Iterate through each member at compile-time
			template for (constexpr auto member : members) {
				// Skip static members and member functions
				if constexpr (std::meta::is_nonstatic_data_member(member)) {
					// Get member pointer and type
					constexpr auto member_ptr = std::meta::pointer_to_member(member);
					using MemberType = typename std::meta::type_of(member);
					
					// Access old and new member via pointer-to-member
					const auto& old_member = old_obj.*member_ptr;
					auto& new_member = new_obj.*member_ptr;
					
					// Migrate this member based on its type
					migrate_member<MemberType>(old_member, new_member, old_xbuf, new_xbuf);
				}
			}
		}
#endif
	};
}
#endif
