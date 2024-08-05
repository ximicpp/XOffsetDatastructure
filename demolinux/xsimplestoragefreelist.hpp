#ifndef X_OFFSET_DATA_STRUCTURE_SIMPLE_STORAGE_FREELIST_HPP
#define X_OFFSET_DATA_STRUCTURE_SIMPLE_STORAGE_FREELIST_HPP
#include <iostream>
#include <cstddef>
#include <algorithm>
#include <functional>
#include <boost/interprocess/offset_ptr.hpp>
#include <boost/container/vector.hpp>
#include <boost/container/flat_set.hpp>
#include "xconfig.hpp"

#ifdef OFFSET_DATA_STRUCTURE_DEBUG_MODE
#define DEBUG_ADD_BLOCK(ptr) addedBlocks.insert(static_cast<char *>(ptr) - static_cast<char *>(basePtr))
#define DEBUG_SET_BASE_ADDR(stoargePtr, ptr) stoargePtr->basePtr = static_cast<void *>(ptr)
#define DEBUG_PRINT_BLOCK(stoargePtr) stoargePtr->printBlockStatus()
#define DEBUG_ADD_ALLOC_NUM(num) allocNums.push_back(static_cast<int>(num))
#define DEBUG_ADD_FREE_NUM(num) allocNums.push_back(-static_cast<int>(num))
#else
#define DEBUG_ADD_BLOCK(ptr) ((void)0)
#define DEBUG_SET_BASE_ADDR(stoargePtr, ptr) ((void)0)
#define DEBUG_PRINT_BLOCK(stoargePtr) ((void)0)
#define DEBUG_ADD_ALLOC_NUM(num) ((void)0)
#define DEBUG_ADD_FREE_NUM(num) ((void)0)
#endif

namespace XOffsetDatastructure
{

    using VoidPointer = boost::interprocess::offset_ptr<void>;

    template <typename SizeType>
    class XSimpleStorageFreelist
    {
    public:
        typedef SizeType size_type;

    private:
        VoidPointer mFirstPointer;
        VoidPointer mStartPointer;
        size_type mSize;
#ifdef OFFSET_DATA_STRUCTURE_DEBUG_MODE
    public:
        void *basePtr;
        boost::container::flat_set<std::ptrdiff_t> addedBlocks;
        boost::container::vector<int> allocNums;
#endif
    public:
        XSimpleStorageFreelist() : mFirstPointer(), mStartPointer(), mSize(0) {}
        XSimpleStorageFreelist(const XSimpleStorageFreelist &) = delete;
        void operator=(const XSimpleStorageFreelist &) = delete;

    public:
        void setBuffer(void *start, size_type size)
        {
            mStartPointer = start;
            mSize = size;
        }

        size_type trimBuffer()
        {
            // 首先计算要去掉的block的范围：一个block以及其后所有的block都在freelist上。
            // 遍历freelist，如果在去掉的范围，那么去掉改节点
            // 不假设freelist的地址是从低到高
            // 假设size是chunksize的整数倍
            size_type trimSize = 0;
            if (!mFirstPointer)
                return trimSize;
            boost::container::flat_set<void *> freeBlocks;
            VoidPointer current = mFirstPointer;
            while (current)
            {
                freeBlocks.insert(current.get());
                current = nextof(current);
            }
            // for (auto &block : freeBlocks)
            // {
            //     std::cout << "block: " << block << " ";
            // }
            // std::cout << "freeBlocks.size(): " << freeBlocks.size() << std::endl;
            size_type blockNum = mSize / CHUNK_SIZE;
            char *lastFreeBlock = nullptr;
            for (auto i = blockNum - 1; i > 0; --i)
            {
                char *curPtr = static_cast<char *>(mStartPointer.get()) + i * CHUNK_SIZE;
                // std::cout << "block: " << i << " " << reinterpret_cast<unsigned int *>(curPtr) << " ";
                if (!freeBlocks.contains(curPtr))
                {
                    trimSize = (blockNum - i - 1) * CHUNK_SIZE;
                    break;
                }
                lastFreeBlock = curPtr;
            }
            // std::cout << "lastFreeBlock: " << reinterpret_cast<unsigned int *>(lastFreeBlock) << " ";
            if (lastFreeBlock == mFirstPointer.get())
            {
                mFirstPointer = VoidPointer();
            }
            if (!lastFreeBlock)
                return trimSize;
            VoidPointer *lastPointer = &mFirstPointer;
            VoidPointer *curPointer = &(nextof(mFirstPointer));
            while (*curPointer)
            {
                if (curPointer->get() >= lastFreeBlock)
                {
                    *lastPointer = nextof(*curPointer);
                    *curPointer = nextof(*curPointer);
                }
                else
                {
                    lastPointer = curPointer;
                    *curPointer = nextof(*curPointer);
                }
            }
            return trimSize;
        }

        void initBuffer(void *start, size_type size)
        {
            setBuffer(start, size);
            addBlock(start, size, XOffsetDatastructure::CHUNK_SIZE);
        }

        void expandBuffer(void *start, size_type size)
        {
            assert(size >= mSize);
            auto oldSize = mSize;
            setBuffer(start, size);
            addBlock(static_cast<char *>(start) + oldSize + sizeof(XSimpleStorageFreelist), size - oldSize, XOffsetDatastructure::CHUNK_SIZE);
        }

        // 当前只能调用一次，不然不能保证block的连续。
        // todo: 整合到构造函数中。将free的归还和初始化分离开。
        void addBlock(void *block,
                      const size_type sz, const size_type partition_sz)
        {
            char *old = static_cast<char *>(block) + ((sz - partition_sz) / partition_sz) * partition_sz;
            nextof(old) = mFirstPointer;
            DEBUG_ADD_BLOCK(old);
            if (old == block)
            {
                mFirstPointer = block;
                return;
            }
            for (char *iter = old - partition_sz; iter != block; old = iter, iter -= partition_sz)
            {
                nextof(iter) = old;
                DEBUG_ADD_BLOCK(iter);
            }
            nextof(block) = old;
            DEBUG_ADD_BLOCK(block);
            mFirstPointer = block;
        }

        void *mallocN(size_type n, size_type partition_size)
        {
            if (n == 0)
                return 0;
            VoidPointer *start = &mFirstPointer;
            VoidPointer *iter = nullptr;
            do
            {
                if (!nextof(start))
                    return 0;
                iter = tryMallocN(start, n, partition_size);
            } while (!iter);
            void *ptr = start->get();
            mFirstPointer = nextof(*iter);
            DEBUG_ADD_ALLOC_NUM(n);
            return ptr;
        }

        void freeN(void *const chunks, const size_type n,
                   const size_type partition_size)
        {
            if (n != 0)
                addBlock(chunks, n * partition_size, partition_size);
            DEBUG_ADD_FREE_NUM(n);
        }

        void printBlockStatus(bool verbose = false) const
        {
#ifdef OFFSET_DATA_STRUCTURE_DEBUG_MODE
            if (verbose)
                std::cout << "Block Status begin: " << std::endl;
            boost::container::flat_set<void *> freeBlocks;
            boost::container::flat_set<void *> addedBlocksWithRawPtr;
            char *lastBlock = nullptr;
            char *endPtr = static_cast<char *>(mStartPointer.get()) + mSize;
            for (auto &block : addedBlocks)
            {
                if (static_cast<char *>(basePtr) + block >= endPtr)
                    break;
                if (verbose)
                    std::cout << block << " ";
                if (!lastBlock)
                {
                    lastBlock = static_cast<char *>(basePtr) + block;
                }
                else
                {
                    if (static_cast<char *>(basePtr) + block - lastBlock != CHUNK_SIZE)
                    {
                        std::cout << std::endl
                                  << "error: " << static_cast<char *>(basePtr) + block - lastBlock << std::endl;
                        std::cout << static_cast<void *>(lastBlock) << std::endl;
                        std::cout << (static_cast<int *>(basePtr) + block) << std::endl;
                    }
                    lastBlock = static_cast<char *>(basePtr) + block;
                }
                addedBlocksWithRawPtr.insert(static_cast<char *>(basePtr) + block);
            }
            if (verbose)
                std::cout << std::endl;
            boost::container::vector<void *> sortedBlocks;
            sortedBlocks.assign(addedBlocksWithRawPtr.begin(), addedBlocksWithRawPtr.end());
            std::sort(sortedBlocks.begin(), sortedBlocks.end(), [](const void *a, const void *b)
                      { return a < b; });
            for (auto &block : sortedBlocks)
            {
                if (verbose)
                    std::cout << "Block: " << block << " ";
            }
            if (verbose)
                std::cout << std::endl;
            VoidPointer current = mFirstPointer;
            while (current)
            {
                freeBlocks.insert(current.get());
                current = nextof(current);
            }
            for (auto &current : freeBlocks)
            {
                if (verbose)
                    std::cout << "Free : " << current << " ";
            }
            if (verbose)
                std::cout << std::endl;
            for (auto &current : freeBlocks)
            {
                if (!addedBlocksWithRawPtr.contains(current))
                    std::cout << "FreeX: " << current << " ";
                assert(addedBlocksWithRawPtr.contains(current));
            }
            if (verbose)
                std::cout << std::endl;
            std::cout << "freeRate:" << 1.0f * freeBlocks.size() / sortedBlocks.size() << " " << freeBlocks.size() << "/" << sortedBlocks.size() << "(free/all)" << std::endl;
            for (int i = 0; i < sortedBlocks.size(); i++)
            {
                const auto &block = sortedBlocks[i];
                bool status = freeBlocks.find(block) != freeBlocks.end();
                std::cout << (status ? "_" : "*");
                if ((i + 1) % 64 == 0)
                {
                    std::cout << std::endl;
                }
            }
            if (verbose)
                std::cout << std::endl;
            if (verbose)
                std::cout << "freeRate:" << 1.0f * freeBlocks.size() / sortedBlocks.size() << " " << freeBlocks.size() << "/" << sortedBlocks.size() << "(free/all)" << std::endl;
            if (verbose)
                std::cout << "Allocate Nums: ";
            for (auto i : allocNums)
            {
                if (verbose)
                    std::cout << i << " ";
            }
            if (verbose)
                std::cout << std::endl;
            if (verbose)
                std::cout << "Block Status end: " << std::endl;
            std::cout << std::endl;
#endif
        }

    private:
        VoidPointer &nextof(VoidPointer &ptr)
        {
            return *(static_cast<VoidPointer *>(ptr.get()));
        }
        VoidPointer &nextof(void *ptr)
        {
            return *(static_cast<VoidPointer *>(ptr));
        }
        VoidPointer *tryMallocN(VoidPointer *&start, size_type n, size_type partition_size)
        {
            VoidPointer *iter = &nextof(start);
            while (--n != 0)
            {
                VoidPointer *next = &nextof(*iter);
                if (!*next || ((static_cast<char *>(next->get()) - static_cast<char *>(iter->get())) != partition_size))
                {
                    start = next;
                    return nullptr;
                }
                iter = next;
            }
            return iter;
        }
    };
}
#endif