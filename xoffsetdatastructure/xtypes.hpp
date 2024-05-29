#ifndef X_OFFSET_DATA_STRUCTURE_TYPES_HPP
#define X_OFFSET_DATA_STRUCTURE_TYPES_HPP
#include <boost/interprocess/offset_ptr.hpp>
#include <boost/container/vector.hpp>
#include <boost/container/flat_set.hpp>
#include <boost/container/set.hpp>
#include <boost/container/flat_map.hpp>
#include <boost/container/map.hpp>
#include <boost/container/string.hpp>
#include "xsimplestoragefreelist.hpp"
#include "xsimplestoragebitmap.hpp"
#include "xsimpleallocator.hpp"
#include <boost/container/detail/next_capacity.hpp>


namespace XOffsetDatastructure
{
	struct growth_factor_custom
	// : boost::container::dtl::grow_factor_ratio<0, 15, 10> // growth_factor_50
	// : boost::container::dtl::grow_factor_ratio<0, 16, 10> // growth_factor_60
	// : boost::container::dtl::grow_factor_ratio<0, 14, 10>	// 40
	// : boost::container::dtl::grow_factor_ratio<0, 13, 10>	// 30
	// : boost::container::dtl::grow_factor_ratio<0, 12, 10>	// 20
	: boost::container::dtl::grow_factor_ratio<0, 11, 10>	// 10
	// : boost::container::dtl::grow_factor_ratio<0, 17, 10> // growth_factor_70
	// : boost::container::dtl::grow_factor_ratio<0, 18, 10> // growth_factor_80
	// : boost::container::dtl::grow_factor_ratio<0, 19, 10> // growth_factor_90
	// : boost::container::dtl::grow_factor_ratio<0, 2, 1>	// 100
	{};

#if OFFSET_DATA_STRUCTURE_STORAGE_ALGORITHM == 0
	using XSimpleStorage = XOffsetDatastructure::XSimpleStorageFreelist<std::size_t>;
#elif OFFSET_DATA_STRUCTURE_STORAGE_ALGORITHM == 1
	using XSimpleStorage = XOffsetDatastructure::XStorageBitmap<std::size_t>;
#endif
	
#if OFFSET_DATA_STRUCTURE_CUSTOM_CONTAINER_GROWTH_FACTOR == 0
	template <typename T>
	using XVector = boost::container::vector<T, XOffsetDatastructure::XSimpleAllocator<T, XSimpleStorage>>;
#elif OFFSET_DATA_STRUCTURE_CUSTOM_CONTAINER_GROWTH_FACTOR == 1
	using vector_option = boost::container::vector_options_t< boost::container::growth_factor<growth_factor_custom> >;
	template <typename T>
	using XVector = boost::container::vector<T, XOffsetDatastructure::XSimpleAllocator<T, XSimpleStorage>, vector_option>;
#endif

#if OFFSET_DATA_STRUCTURE_CUSTOM_CONTAINER_GROWTH_FACTOR == 0
	template <typename T>
	using XSet = boost::container::flat_set<T, std::less<T>, XOffsetDatastructure::XSimpleAllocator<T, XSimpleStorage>>;
	// using XSet = boost::container::set<T, std::less<T>, XOffsetDatastructure::XSimpleAllocator<T, XSimpleStorage>>;
#elif OFFSET_DATA_STRUCTURE_CUSTOM_CONTAINER_GROWTH_FACTOR == 1
	using vector_option_flatset = boost::container::vector_options_t< boost::container::growth_factor<growth_factor_custom> >;
	template <typename T>
	using XVector_flatset = boost::container::vector<T, XOffsetDatastructure::XSimpleAllocator<T, XSimpleStorage>, vector_option_flatset>;
	template <typename T>
	using XSet = boost::container::flat_set<T, std::less<T>, XVector_flatset<T>>;
#endif

#if OFFSET_DATA_STRUCTURE_CUSTOM_CONTAINER_GROWTH_FACTOR == 0
	template <typename K, typename V>
	using XMap = boost::container::flat_map<K, V, std::less<K>, XOffsetDatastructure::XSimpleAllocator<std::pair<K, V>, XSimpleStorage>>;
	// using XMap = boost::container::map<K, V, std::less<K>, XOffsetDatastructure::XSimpleAllocator<std::pair<const K, V>, XSimpleStorage>>;
#elif OFFSET_DATA_STRUCTURE_CUSTOM_CONTAINER_GROWTH_FACTOR == 1
	using vector_option_flatmap = boost::container::vector_options_t< boost::container::growth_factor<growth_factor_custom> >;
	template <typename K, typename V>
	using XVector_flatmap = boost::container::vector<std::pair<K, V>, XOffsetDatastructure::XSimpleAllocator<std::pair<K, V>, XSimpleStorage>, vector_option_flatmap>;
	template <typename K, typename V>
	using XMap = boost::container::flat_map<K, V, std::less<K>, XVector_flatmap<K, V>>;
#endif

	using XString = boost::container::basic_string<char, std::char_traits<char>, XOffsetDatastructure::XSimpleAllocator<char, XSimpleStorage>>;
}
#endif