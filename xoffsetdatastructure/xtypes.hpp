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

namespace XOffsetDatastructure
{
#if OFFSET_DATA_STRUCTURE_STORAGE_ALGORITHM == 0
	using XSimpleStorage = XOffsetDatastructure::XSimpleStorageFreelist<std::size_t>;
#elif OFFSET_DATA_STRUCTURE_STORAGE_ALGORITHM == 1
	using XSimpleStorage = XOffsetDatastructure::XStorageBitmap<std::size_t>;
#endif
	template <typename T>
	using XVector = boost::container::vector<T, XOffsetDatastructure::XSimpleAllocator<T, XSimpleStorage>>;
	template <typename T>
	using XSet = boost::container::flat_set<T, std::less<T>, XOffsetDatastructure::XSimpleAllocator<T, XSimpleStorage>>;
	// using XSet = boost::container::set<T, std::less<T>, XOffsetDatastructure::XSimpleAllocator<T, XSimpleStorage>>;
	template <typename K, typename V>
	using XMap = boost::container::flat_map<K, V, std::less<K>, XOffsetDatastructure::XSimpleAllocator<std::pair<K, V>, XSimpleStorage>>;
	// using XMap = boost::container::map<K, V, std::less<K>, XOffsetDatastructure::XSimpleAllocator<std::pair<const K, V>, XSimpleStorage>>;
	using XString = boost::container::basic_string<char, std::char_traits<char>, XOffsetDatastructure::XSimpleAllocator<char, XSimpleStorage>>;
}
#endif