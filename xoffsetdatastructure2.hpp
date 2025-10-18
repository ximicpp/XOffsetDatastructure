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

	// ============================================================================
	// XBufferCompactor: Memory Compaction Interface
	// ============================================================================
	// NOTE: Memory compaction features are not available in the current version.
	//       This functionality requires either:
	//       1. Manual implementation with user-defined migrate() methods
	//       2. C++26 reflection support (not yet available in standard compilers)
	//
	//       This class is reserved for future implementation.
	// ============================================================================
	class XBufferCompactor {
	public:
		// Placeholder: Memory compaction interface
		// To be implemented when C++26 reflection becomes available
		template<typename T>
		static XBuffer compact(XBuffer& old_xbuf) {
			static_assert(sizeof(T) == 0, 
				"Memory compaction is not yet implemented in this version. "
				"This feature will be available in a future release with C++26 reflection support.");
			return XBuffer();
		}
	};

	// ============================================================================
	// XBufferPlanner: Capacity Planning and Reservation
	// ============================================================================
	// Purpose: Pre-allocate buffer space to avoid out-of-memory errors during operations
	// Philosophy: "Plan first, execute safely"
	// ============================================================================
	
	class XBufferPlanner {
	private:
		// ==================== Internal Constants ====================
		
		// Segment manager overhead per allocation
		static constexpr std::size_t SEGMENT_MANAGER_OVERHEAD = 32;
		
		// Memory alignment padding (typically 8 or 16 bytes)
		static constexpr std::size_t ALIGNMENT_PADDING = 16;
		
		// Container control structure overhead
		static constexpr std::size_t CONTAINER_CONTROL_OVERHEAD = 64;
		
		// Extra reserve factor for container growth
		static constexpr double DEFAULT_RESERVE_FACTOR = 0.1;  // 10%
		
	public:
		// ==================== Basic Size Estimation ====================
		
		/// Estimate size for a single object (including overhead)
		template<typename T>
		static std::size_t estimate_object_size() {
			// Object size + segment manager overhead + alignment padding
			std::size_t obj_size = sizeof(T);
			std::size_t total = obj_size + SEGMENT_MANAGER_OVERHEAD;
			
			// Round up to alignment boundary
			total = ((total + ALIGNMENT_PADDING - 1) / ALIGNMENT_PADDING) * ALIGNMENT_PADDING;
			
			return total;
		}
		
		/// Estimate size for XVector with element count
		template<typename T>
		static std::size_t estimate_vector_size(std::size_t count) {
			if (count == 0) return CONTAINER_CONTROL_OVERHEAD;
			
			// Vector control structure
			std::size_t vector_struct_size = sizeof(XVector<T>);
			
			// Element storage (with reserve factor)
			std::size_t element_size = sizeof(T);
			std::size_t reserve_count = static_cast<std::size_t>(count * DEFAULT_RESERVE_FACTOR);
			std::size_t elements_size = (count + reserve_count) * element_size;
			
			// Total: control structure + elements + segment overhead + alignment
			std::size_t total = vector_struct_size + elements_size + SEGMENT_MANAGER_OVERHEAD;
			total = ((total + ALIGNMENT_PADDING - 1) / ALIGNMENT_PADDING) * ALIGNMENT_PADDING;
			
			return total;
		}
		
		/// Estimate size for XMap with entry count
		template<typename K, typename V>
		static std::size_t estimate_map_size(std::size_t count) {
			if (count == 0) return CONTAINER_CONTROL_OVERHEAD;
			
			// flat_map uses vector<pair<K,V>> internally
			std::size_t map_struct_size = sizeof(XMap<K, V>);
			
			// Entry storage (pair<K,V> with reserve)
			std::size_t entry_size = sizeof(std::pair<K, V>);
			std::size_t reserve_count = static_cast<std::size_t>(count * 0.2);  // 20% for maps
			std::size_t entries_size = (count + reserve_count) * entry_size;
			
			// Total
			std::size_t total = map_struct_size + entries_size + SEGMENT_MANAGER_OVERHEAD;
			total = ((total + ALIGNMENT_PADDING - 1) / ALIGNMENT_PADDING) * ALIGNMENT_PADDING;
			
			return total;
		}
		
		/// Estimate size for XSet with entry count
		template<typename T>
		static std::size_t estimate_set_size(std::size_t count) {
			if (count == 0) return CONTAINER_CONTROL_OVERHEAD;
			
			// flat_set uses vector<T> internally
			std::size_t set_struct_size = sizeof(XSet<T>);
			
			// Element storage (with reserve)
			std::size_t element_size = sizeof(T);
			std::size_t reserve_count = static_cast<std::size_t>(count * 0.2);  // 20% for sets
			std::size_t elements_size = (count + reserve_count) * element_size;
			
			// Total
			std::size_t total = set_struct_size + elements_size + SEGMENT_MANAGER_OVERHEAD;
			total = ((total + ALIGNMENT_PADDING - 1) / ALIGNMENT_PADDING) * ALIGNMENT_PADDING;
			
			return total;
		}
		
		/// Estimate size for XString with max length
		static std::size_t estimate_string_size(std::size_t max_length) {
			// XString structure size
			std::size_t string_struct_size = sizeof(XString);
			
			// Character data (length + null terminator)
			std::size_t data_size = max_length + 1;
			
			// Total
			std::size_t total = string_struct_size + data_size + SEGMENT_MANAGER_OVERHEAD;
			total = ((total + ALIGNMENT_PADDING - 1) / ALIGNMENT_PADDING) * ALIGNMENT_PADDING;
			
			return total;
		}
		
		/// Get base overhead (segment manager initialization)
		static std::size_t get_base_overhead() {
			// Empirical value: segment manager initialization overhead
			return 1024;
		}
		
		// ==================== Estimator (Fluent API) ====================
		
		/// Fluent API for chaining capacity estimations
		class Estimator {
		private:
			std::size_t total_bytes_;
			
		public:
			/// Constructor with base overhead
			Estimator() : total_bytes_(1024) {}  // Base segment manager overhead
			
			/// Add capacity for N objects of type T
			template<typename T>
			Estimator& add_objects(std::size_t count) {
				total_bytes_ += estimate_object_size<T>() * count;
				return *this;
			}
			
			/// Add capacity for XVector<T> with element_count elements
			template<typename T>
			Estimator& add_vector(std::size_t element_count) {
				total_bytes_ += estimate_vector_size<T>(element_count);
				return *this;
			}
			
			/// Add capacity for XMap<K,V> with entry_count entries
			template<typename K, typename V>
			Estimator& add_map(std::size_t entry_count) {
				total_bytes_ += estimate_map_size<K, V>(entry_count);
				return *this;
			}
			
			/// Add capacity for XSet<T> with entry_count entries
			template<typename T>
			Estimator& add_set(std::size_t entry_count) {
				total_bytes_ += estimate_set_size<T>(entry_count);
				return *this;
			}
			
			/// Add capacity for count strings with avg_length characters
			Estimator& add_strings(std::size_t count, std::size_t avg_length) {
				total_bytes_ += count * estimate_string_size(avg_length);
				return *this;
			}
			
			/// Add raw bytes
			Estimator& add_bytes(std::size_t bytes) {
				total_bytes_ += bytes;
				return *this;
			}
			
			/// Add safety margin (default 20%)
			/// percent: 0.0 to 1.0 (e.g., 0.2 = 20%)
			Estimator& add_margin(double percent = 0.2) {
				if (percent < 0.0 || percent > 1.0) {
					throw std::invalid_argument("Margin percent must be between 0.0 and 1.0");
				}
				total_bytes_ = static_cast<std::size_t>(total_bytes_ * (1.0 + percent));
				return *this;
			}
			
			/// Get total estimated bytes
			std::size_t total() const {
				return total_bytes_;
			}
			
			/// Implicit conversion to size_t for convenience
			operator std::size_t() const {
				return total_bytes_;
			}
		};
	};
	
	// ============================================================================
	// XBufferWithPlanner: XBuffer with Built-in Planning Support
	// ============================================================================
	
	/// Extended XBuffer with capacity planning methods
	class XBufferWithPlanner : public XBuffer {
	public:
		// Inherit constructors
		using XBuffer::XBuffer;
		
		// Expose base class methods
		using XBuffer::get_size;
		using XBuffer::get_free_memory;
		using XBuffer::grow;
		using XBuffer::construct;
		using XBuffer::find;
		using XBuffer::find_or_construct;
		using XBuffer::get_segment_manager;
		
		/// Reserve at least 'bytes' of free space
		/// Grows the buffer if necessary
		void reserve(std::size_t bytes) {
			std::size_t available = available_space();
			if (available < bytes) {
				std::size_t growth = bytes - available;
				this->grow(growth);
			}
		}
		
		/// Get buffer size
		std::size_t size() const {
			return this->get_size();
		}
		
		/// Reserve space based on estimator
		void reserve(const XBufferPlanner::Estimator& estimator) {
			reserve(estimator.total());
		}
		
		/// Check if buffer has at least 'bytes' of free space
		bool has_capacity(std::size_t bytes) const {
			return available_space() >= bytes;
		}
		
		/// Get available free space
		std::size_t available_space() const {
			return this->get_free_memory();
		}
		
		/// Get memory statistics
		XBufferVisualizer::MemoryStats stats() const {
			// Create non-const reference for stats query
			XBuffer& non_const_buffer = const_cast<XBuffer&>(static_cast<const XBuffer&>(*this));
			return XBufferVisualizer::get_memory_stats(non_const_buffer);
		}
		
		/// Print memory statistics
		void print_stats() const {
			auto s = stats();
			std::cout << "XBuffer: " << s.used_size << "/" << s.total_size 
			          << " bytes (" << std::fixed << std::setprecision(1) 
			          << s.usage_percent() << "% used, " 
			          << s.free_size << " bytes free)" << std::endl;
		}
	};
}
#endif
