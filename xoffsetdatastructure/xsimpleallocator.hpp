#ifndef X_OFFSET_DATA_STRUCTURE_SIMPLE_ALLOCATOR_HPP
#define X_OFFSET_DATA_STRUCTURE_SIMPLE_ALLOCATOR_HPP

#include <iostream>
#include <string>
#include <boost/interprocess/offset_ptr.hpp>
#include "xconfig.hpp"
#include "xmisc.hpp"

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
			// std::cout << "Allocate: " << totalSize << std::endl;
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
			// std::cout << "Deallocate: " << totalSize << std::endl;
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