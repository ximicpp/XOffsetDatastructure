#ifndef X_OFFSET_DATA_STRUCTURE_2_TYPES_HPP
#define X_OFFSET_DATA_STRUCTURE_2_TYPES_HPP
#include <boost/interprocess/offset_ptr.hpp>
#include <boost/container/vector.hpp>
#include <boost/container/flat_set.hpp>
#include <boost/container/set.hpp>
#include <boost/container/flat_map.hpp>
#include <boost/container/map.hpp>
#include <boost/container/string.hpp>
#include <boost/container/detail/next_capacity.hpp>
#include "xmanagedmemory.hpp"
#include "xallocationalgorithm.hpp"

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
}
#endif