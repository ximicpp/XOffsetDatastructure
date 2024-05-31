#ifndef X_OFFSET_DATA_STRUCTURE_SIMPLE_STORAGE_BITMAP_HPP
#define X_OFFSET_DATA_STRUCTURE_SIMPLE_STORAGE_BITMAP_HPP
#include <iostream>
#include <cstddef>
#include <algorithm>
#include <functional>
#include <boost/interprocess/offset_ptr.hpp>
#include <boost/container/vector.hpp>
#include <boost/container/flat_set.hpp>
#include "xconfig.hpp"
#include "xmisc.hpp"

#ifdef OFFSET_DATA_STRUCTURE_DEBUG_MODE
#define DEBUG_ADD_BLOCK(ptr) ((void)0)
#define DEBUG_SET_BASE_ADDR(stoargePtr, ptr) ((void)0)
#define DEBUG_PRINT_BLOCK(stoargePtr) stoargePtr->printBlockStatus()
#define DEBUG_ADD_ALLOC_NUM(num) ((void)0)
#define DEBUG_ADD_FREE_NUM(num) ((void)0)
#else
#define DEBUG_ADD_BLOCK(ptr) ((void)0)
#define DEBUG_SET_BASE_ADDR(stoargePtr, ptr) ((void)0)
#define DEBUG_PRINT_BLOCK(stoargePtr) ((void)0)
#define DEBUG_ADD_ALLOC_NUM(num) ((void)0)
#define DEBUG_ADD_FREE_NUM(num) ((void)0)
#endif

namespace XOffsetDatastructure
{
    template <typename SizeType>
    class XStorageBitmap
    {
    public:
        typedef SizeType size_type;
        constexpr static size_type MAX_CHUNK_NUM = 1024;

    private:
#if OFFSET_DATA_STRUCTURE_POINTER_TYPE == 0
        boost::interprocess::offset_ptr<void> mStartPointer;
#elif OFFSET_DATA_STRUCTURE_POINTER_TYPE == 1
        void *mStartPointer;
#endif
        // VoidPointer mStartPointer;
        size_type mSize;                 // chunk number
        BitArray<MAX_CHUNK_NUM> mBitArray; // 1 for used, 0 for free

    public:
#if OFFSET_DATA_STRUCTURE_POINTER_TYPE == 0
        XStorageBitmap() : mStartPointer(), mSize(0) {}
#elif OFFSET_DATA_STRUCTURE_POINTER_TYPE == 1
        XStorageBitmap() : mStartPointer(nullptr), mSize(0) {}
#endif
        XStorageBitmap(const XStorageBitmap &) = delete;
        void operator=(const XStorageBitmap &) = delete;

    public:
        size_type trimBuffer()
        {
            auto index = mBitArray.FindTailClearBits(mSize - 1);
            // std::cout << "trimBuffer index: " << index << std::endl;
            // std::cout << mBitArray.toString() << std::endl;
            // printBlockStatus();
            if (index == -1)
            {
                return 0;
            }
            assert(index < mSize);
            // mBitArray.setBits(index, mSize - 1, true);
            mBitArray.SetBits(index, mSize - index);
            auto size = (mSize - index) * CHUNK_SIZE;
            mSize = index;
            // std::cout << mBitArray.toString() << std::endl;
            // printBlockStatus();
            return size;
        }

        void initBuffer(void *start, size_type size)
        {
            assert(size <= CHUNK_SIZE * MAX_CHUNK_NUM);
            assert(size % CHUNK_SIZE == 0);
            mSize = size / CHUNK_SIZE;
            // mBitArray.setBits(0, mSize - 1, false);
            // std::cout << mBitArray.toString() << std::endl;
            mBitArray.clearBits(0, mSize);
            mBitArray.SetBits(mSize, MAX_CHUNK_NUM);
            // std::cout << mBitArray.toString() << std::endl;
            mStartPointer = start;
        }

        void setBuffer(void *start, size_type size)
        {
            mSize = size / CHUNK_SIZE;
            mStartPointer = start;
        }

        void expandBuffer(void *start, size_type size)
        {
            assert(size <= CHUNK_SIZE * MAX_CHUNK_NUM);
            assert(size % CHUNK_SIZE == 0);
            assert(size / CHUNK_SIZE >= mSize);
            auto oldSize = mSize;
            mSize = size / CHUNK_SIZE;
            if (mSize * CHUNK_SIZE != size)
                throw std::runtime_error("oversize error");
            // mBitArray.setBits(oldSize, mSize - 1, false);
            mBitArray.clearBits(oldSize, mSize - oldSize);
            mStartPointer = start;
        }

        inline void *mallocN(const size_type numChunks, size_type partition_size)
        {
            assert(partition_size == CHUNK_SIZE);
            // std::cout << mBitArray.toString() << std::endl;
            // std::cout << "malloc n: " << numChunks << std::endl;
            auto startIndex = mBitArray.findClearBitsAndSet(numChunks, 0);
            // std::cout << mBitArray.toString() << std::endl;
            if (startIndex == -1)
            {
                return nullptr;
            }
            // std::cout << "after malloc startIndex: " << startIndex << " n: " << numChunks << std::endl;
#if OFFSET_DATA_STRUCTURE_POINTER_TYPE == 0
            return static_cast<void *>(static_cast<std::byte *>(mStartPointer.get()) + startIndex * CHUNK_SIZE);
#elif OFFSET_DATA_STRUCTURE_POINTER_TYPE == 1
            return static_cast<void *>(static_cast<std::byte *>(mStartPointer) + startIndex * CHUNK_SIZE);
#endif
        }

        inline void freeN(void *const ptr, const size_type numChunks, const size_type partition_size)
        {
            assert(partition_size == CHUNK_SIZE);
#if OFFSET_DATA_STRUCTURE_POINTER_TYPE == 0
            assert((static_cast<std::byte *>(ptr) - static_cast<std::byte *>(mStartPointer.get())) % CHUNK_SIZE == 0);
            auto startIndex = (static_cast<std::byte *>(ptr) - static_cast<std::byte *>(mStartPointer.get())) / CHUNK_SIZE;
#elif OFFSET_DATA_STRUCTURE_POINTER_TYPE == 1
            assert((static_cast<std::byte *>(ptr) - static_cast<std::byte *>(mStartPointer)) % CHUNK_SIZE == 0);
            auto startIndex = (static_cast<std::byte *>(ptr) - static_cast<std::byte *>(mStartPointer)) / CHUNK_SIZE;
#endif
            // mBitArray.setBits(startIndex, startIndex + numChunks - 1, false);
            mBitArray.clearBits(startIndex, numChunks);
            // std::cout << "after free startIndex: " << startIndex << " numChunks: " << chunkNum << std::endl;
        }

        void printBlockStatus(bool verbose = false)
        {
            int inter_chunk_fragments = 0;
            for (auto i = 0; i < mSize; ++i)
            {
                std::cout << mBitArray.getBit(i);
                if (!mBitArray.getBit(i))
                {
                    ++inter_chunk_fragments;
                }
            }
            std::cout << std::endl;
            std::cout << "inter chunk fragments: " << 1.0f * inter_chunk_fragments / mSize << std::endl;

            // std::cout << mBitArray.toString() << std::endl;
        }
    };
}
#endif