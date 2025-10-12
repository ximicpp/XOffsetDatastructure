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
	// XBuffer Memory Visualization Utilities
	// ========================================================================

	class XBufferVisualizer {
	public:
		// 获取内存使用统计信息
		struct MemoryStats {
			std::size_t total_size;
			std::size_t free_size;
			std::size_t used_size;
			std::size_t num_free_blocks;
			std::size_t num_allocated_blocks;
			std::size_t largest_free_block;
			
			double usage_percent() const {
				return total_size > 0 ? (used_size * 100.0 / total_size) : 0.0;
			}
			
			double fragmentation_percent() const {
				return free_size > 0 ? ((free_size - largest_free_block) * 100.0 / free_size) : 0.0;
			}
		};

		// Get XBuffer memory statistics
		static MemoryStats get_memory_stats(XBuffer& xbuf) {
			MemoryStats stats = {};
			stats.total_size = xbuf.get_size();
			stats.free_size = xbuf.get_free_memory();
			stats.used_size = stats.total_size - stats.free_size;
			
			// Estimate number of free blocks (not directly available in API)
			stats.num_free_blocks = 0;  // Not available in current API
			stats.num_allocated_blocks = 0;  // Not available in current API
			
			// Try to estimate largest free block using binary search
			try {
				void* ptr = xbuf.allocate(stats.free_size);
				stats.largest_free_block = stats.free_size;
				xbuf.deallocate(ptr);
			} catch(...) {
				// Binary search for largest allocatable block
				std::size_t low = 0, high = stats.free_size;
				stats.largest_free_block = 0;
				while (low <= high && high > 0) {
					std::size_t mid = low + (high - low) / 2;
					try {
						void* ptr = xbuf.allocate(mid);
						xbuf.deallocate(ptr);
						stats.largest_free_block = mid;
						low = mid + 1;
					} catch(...) {
						if (mid == 0) break;
						high = mid - 1;
					}
				}
			}
			
			return stats;
		}

		// 打印内存统计信息
		static void print_memory_stats(XBuffer& xbuf, const char* title = "XBuffer Memory Statistics") {
			MemoryStats stats = get_memory_stats(xbuf);
			
			std::cout << "\n" << std::string(70, '=') << std::endl;
			std::cout << title << std::endl;
			std::cout << std::string(70, '=') << std::endl;
			
			std::cout << "Total Size:           " << format_size(stats.total_size) << std::endl;
			std::cout << "Used Size:            " << format_size(stats.used_size) 
			          << " (" << std::fixed << std::setprecision(2) << stats.usage_percent() << "%)" << std::endl;
			std::cout << "Free Size:            " << format_size(stats.free_size) 
			          << " (" << std::fixed << std::setprecision(2) << (100.0 - stats.usage_percent()) << "%)" << std::endl;
			std::cout << "Largest Free Block:   " << format_size(stats.largest_free_block) << std::endl;
			std::cout << "Free Blocks:          " << stats.num_free_blocks << std::endl;
			std::cout << "Fragmentation:        " << std::fixed << std::setprecision(2) 
			          << stats.fragmentation_percent() << "%" << std::endl;
			
			std::cout << std::string(70, '-') << std::endl;
		}

		// Print memory usage bar chart
		static void print_memory_bar(XBuffer& xbuf, int bar_width = 50) {
			MemoryStats stats = get_memory_stats(xbuf);
			
			int used_blocks = static_cast<int>(stats.usage_percent() * bar_width / 100.0);
			int free_blocks = bar_width - used_blocks;
			
			std::cout << "Memory Usage: [";
			for (int i = 0; i < used_blocks; ++i) std::cout << "#";
			for (int i = 0; i < free_blocks; ++i) std::cout << ".";
			std::cout << "] " << std::fixed << std::setprecision(1) 
			          << stats.usage_percent() << "%" << std::endl;
		}

		// Print all named objects
		static void print_named_objects(XBuffer& xbuf) {
			std::cout << "\n" << std::string(70, '=') << std::endl;
			std::cout << "Named Objects in XBuffer" << std::endl;
			std::cout << std::string(70, '=') << std::endl;
			
			auto* segment_mgr = xbuf.get_segment_manager();
			auto named_it = segment_mgr->named_begin();
			auto named_end = segment_mgr->named_end();
			
			if (named_it == named_end) {
				std::cout << "(No named objects)" << std::endl;
			} else {
				int index = 0;
				for (; named_it != named_end; ++named_it, ++index) {
					const auto& obj = *named_it;
					std::cout << std::setw(3) << index << ". " 
					          << std::setw(30) << std::left << obj.name() 
					          << " | Offset: 0x" << std::hex << std::setw(8) << std::setfill('0') 
					          << get_offset(xbuf, obj.value()) << std::dec << std::setfill(' ')
					          << std::endl;
				}
			}
			std::cout << std::string(70, '-') << std::endl;
		}

		// 打印完整的可视化报告
		static void print_full_report(XBuffer& xbuf, const char* title = "XBuffer Visualization Report") {
			std::cout << "\n" << std::string(70, '=') << std::endl;
			std::cout << title << std::endl;
			std::cout << std::string(70, '=') << std::endl;
			std::cout << std::endl;
			
			print_memory_bar(xbuf, 60);
			print_memory_stats(xbuf, "Memory Statistics");
			print_named_objects(xbuf);
			
			std::cout << std::string(70, '=') << std::endl;
			std::cout << std::endl;
		}

	private:
		// 格式化大小显示（B, KB, MB）
		static std::string format_size(std::size_t bytes) {
			std::ostringstream oss;
			if (bytes < 1024) {
				oss << bytes << " B";
			} else if (bytes < 1024 * 1024) {
				oss << std::fixed << std::setprecision(2) << (bytes / 1024.0) << " KB";
			} else {
				oss << std::fixed << std::setprecision(2) << (bytes / (1024.0 * 1024.0)) << " MB";
			}
			return oss.str();
		}

		// 获取对象相对于buffer起始位置的偏移
		static std::size_t get_offset(XBuffer& xbuf, const void* ptr) {
			const char* base = static_cast<const char*>(xbuf.get_address());
			const char* target = static_cast<const char*>(ptr);
			return target - base;
		}
	};

	// 便捷函数
	inline void visualize_xbuffer(XBuffer& xbuf, const char* title = nullptr) {
		XBufferVisualizer::print_full_report(xbuf, title ? title : "XBuffer Visualization Report");
	}

	inline void print_xbuffer_stats(XBuffer& xbuf) {
		XBufferVisualizer::print_memory_stats(xbuf);
	}

	inline void print_xbuffer_objects(XBuffer& xbuf) {
		XBufferVisualizer::print_named_objects(xbuf);
	}
}
#endif