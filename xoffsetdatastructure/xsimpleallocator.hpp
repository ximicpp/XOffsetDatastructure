#ifndef X_OFFSET_DATA_STRUCTURE_SIMPLE_ALLOCATOR_HPP
#define X_OFFSET_DATA_STRUCTURE_SIMPLE_ALLOCATOR_HPP

#include <iostream>
#include <string>
#include <boost/interprocess/offset_ptr.hpp>
#include "xconfig.hpp"
#include "xmisc.hpp"
#include <map>


namespace XOffsetDatastructure
{
	// class __test
	// {
	// 	public:
	// 		__test()
	// 		{
	// 			std::cout << "__test()" << std::endl;
	// 		}
	// 		~__test()
	// 		{
	// 			std::cout << "~__test()" << std::endl;
	// 		}
	// 		int a;
	// };

	template <typename T, typename Storage>
	class XSimpleAllocator
	{
	public:
		using value_type = T;
#if OFFSET_DATA_STRUCTURE_POINTER_TYPE == 0
		using pointer = boost::interprocess::offset_ptr<T>;
#elif OFFSET_DATA_STRUCTURE_POINTER_TYPE == 1
		using pointer = T*;
#endif

#ifdef OFFSET_DATA_STRUCTURE_DEBUG_MODE
		std::map<void *, std::size_t> allocMap;
#endif

		XSimpleAllocator() = default;
		XSimpleAllocator(Storage *externalStorage) : storagePointer(externalStorage) {}
		// template <typename U, typename S>
		// XSimpleAllocator(const XSimpleAllocator<U, S> &) = delete;

		template <typename U, typename S>
		XSimpleAllocator(const XSimpleAllocator<U, S> &other) : storagePointer(other.storagePointer)
		{
			// std::cout << "XSimpleAllocator(const XSimpleAllocator<U, S> &other)" << std::endl;
		}
	
		pointer allocate(std::size_t n)
		{
			std::size_t totalSize = n * sizeof(T);
#ifdef OFFSET_DATA_STRUCTURE_DEBUG_USE_DEFAULT_MALLOC_FREE
			return pointer(malloc(totalSize));
#endif
			std::size_t chunkNum = (totalSize + CHUNK_SIZE - 1) / CHUNK_SIZE;
			T *ptr = reinterpret_cast<T *>(storagePointer->mallocN(chunkNum, CHUNK_SIZE));
			if (ptr == nullptr)
			{
				// auto a = __test();
#if OFFSET_DATA_STRUCTURE_EXCEPTION_HANDLE_TYPE == 0
				throw XOffsetDatastructure::XBadAllocException(chunkNum * CHUNK_SIZE);
#elif OFFSET_DATA_STRUCTURE_EXCEPTION_HANDLE_TYPE == 1
				std::longjmp(XJmpBuffer::getJmpBuffer(), chunkNum * CHUNK_SIZE);
				// a.a = 0;
#elif OFFSET_DATA_STRUCTURE_EXCEPTION_HANDLE_TYPE == 2
				// return boost::leaf::new_error(XOffsetDatastructure::XBadAllocExceptionLeaf{chunkNum * CHUNK_SIZE});
				boost::leaf::throw_exception(boost::leaf::new_error(XOffsetDatastructure::XBadAllocExceptionLeaf{chunkNum * CHUNK_SIZE}));
#endif
			}
#ifdef OFFSET_DATA_STRUCTURE_DEBUG_MODE
			allocMap[ptr] = chunkNum * CHUNK_SIZE - totalSize;
			std::size_t sum = 0;
			for (auto &item : allocMap)
			{
				sum += item.second;
				std::cout << item.first << " " << item.second << std::endl;
			}
			std::cout << "intra-chunk fragments after malloc: " << sum << std::endl;
#endif
			// std::cout << "Allocate: " << totalSize << " " <<  chunkNum * CHUNK_SIZE << std::endl;
#if OFFSET_DATA_STRUCTURE_POINTER_TYPE == 0
			DEBUG_PRINT_BLOCK(storagePointer.get());
#elif OFFSET_DATA_STRUCTURE_POINTER_TYPE == 1
			DEBUG_PRINT_BLOCK(storagePointer);
#endif
			return pointer(ptr);
		}

		void deallocate(pointer ptr, std::size_t n)
		{
			std::size_t totalSize = n * sizeof(T);
#ifdef OFFSET_DATA_STRUCTURE_DEBUG_USE_DEFAULT_MALLOC_FREE
			return free(ptr);
#endif
			std::size_t chunkNum = (totalSize + CHUNK_SIZE - 1) / CHUNK_SIZE;
#if OFFSET_DATA_STRUCTURE_POINTER_TYPE == 0
			storagePointer->freeN(ptr.get(), chunkNum, CHUNK_SIZE);
#ifdef OFFSET_DATA_STRUCTURE_DEBUG_MODE
			allocMap.erase(ptr.get());
			std::size_t sum = 0;
			for (auto &item : allocMap)
			{
				sum += item.second;
				std::cout << item.first << " " << item.second << std::endl;
			}
			std::cout << "intra-chunk fragments after free: " << sum << std::endl;
#endif
			// std::cout << "Allocate: " << -int(totalSize) << " " <<  -int(chunkNum * CHUNK_SIZE) << std::endl;
			DEBUG_PRINT_BLOCK(storagePointer.get());
#elif OFFSET_DATA_STRUCTURE_POINTER_TYPE == 1
			storagePointer->freeN(ptr, chunkNum, CHUNK_SIZE);
			// std::cout << "Deallocate: " << totalSize << std::endl;
			DEBUG_PRINT_BLOCK(storagePointer);
#endif
		}

		bool operator==(const XSimpleAllocator &other) const
		{
#if OFFSET_DATA_STRUCTURE_POINTER_TYPE == 0
			return /*storagePointer && */ storagePointer.get() == other.storagePointer.get();
#elif OFFSET_DATA_STRUCTURE_POINTER_TYPE == 1
			return /*storagePointer && */ storagePointer == other.storagePointer;
#endif
		}

	public:
#if OFFSET_DATA_STRUCTURE_POINTER_TYPE == 0
		boost::interprocess::offset_ptr<Storage> storagePointer;
#elif OFFSET_DATA_STRUCTURE_POINTER_TYPE == 1
		Storage* storagePointer;
#endif
	};
}
#endif