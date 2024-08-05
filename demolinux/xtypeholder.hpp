#ifndef X_OFFSET_DATA_STRUCTURE_TYPE_HOLDER_HPP
#define X_OFFSET_DATA_STRUCTURE_TYPE_HOLDER_HPP
#include <cassert>
#include <vector>
#include <boost/interprocess/offset_ptr.hpp>
#include "xsimplestoragefreelist.hpp"
#include "xconfig.hpp"
#include "xsimplestoragebitmap.hpp"
#include "xsimpleallocator.hpp"

namespace XOffsetDatastructure
{
    template <typename T>
    struct XOffsetPtr;

#if OFFSET_DATA_STRUCTURE_STORAGE_ALGORITHM == 0
    using XSimpleStorage = XOffsetDatastructure::XSimpleStorageFreelist<std::size_t>;
#elif OFFSET_DATA_STRUCTURE_STORAGE_ALGORITHM == 1
    using XSimpleStorage = XOffsetDatastructure::XStorageBitmap<std::size_t>;
#endif

    constexpr int headerSize = sizeof(XSimpleStorage);

    template <typename T>
    class XTypeHolder
    {
    public:
#if OFFSET_DATA_STRUCTURE_POINTER_TYPE == 0
        XTypeHolder(std::size_t dataSize = 4096)
#elif OFFSET_DATA_STRUCTURE_POINTER_TYPE == 1
        XTypeHolder(std::size_t dataSize = 1024 * 32)
#endif
        {
            int n = 1;
            assert(*(uint8_t*)&n == 1); // little endian.
            mBuffer.resize(dataSize + headerSize);
            XSimpleStorage *storage = new (reinterpret_cast<void *>(&mBuffer.front())) XSimpleStorage();
            DEBUG_SET_BASE_ADDR(storage, &mBuffer.front());
            storage->initBuffer(&mBuffer.front() + headerSize, mBuffer.size() - headerSize);
            // storage->setBuffer(&mBuffer.front() + headerSize, mBuffer.size() - headerSize);
            // storage->addBlock((&mBuffer.front() + headerSize), dataSize, XOffsetDatastructure::CHUNK_SIZE);
            XOffsetDatastructure::XSimpleAllocator<T, XSimpleStorage> xtypeAllocator(storage);
            mPtr = new (boost::movelib::to_raw_pointer(xtypeAllocator.allocate(1))) T(storage);
        }
        XTypeHolder(std::vector<std::byte> &externalBuffer)
            : mBuffer(std::move(externalBuffer))
        {
            int n = 1;
            assert(*(uint8_t *)&n == 1); // little endian.
            // std::swap(externalBuffer, mBuffer);
            // mBuffer = std::move(externalBuffer);
            mPtr = reinterpret_cast<T *>(headerSize + reinterpret_cast<intptr_t>(mBuffer.data()));
        }
#if OFFSET_DATA_STRUCTURE_POINTER_TYPE == 0
        void expand(std::size_t num)
        {
            num = (num + XOffsetDatastructure::CHUNK_SIZE - 1) / XOffsetDatastructure::CHUNK_SIZE * XOffsetDatastructure::CHUNK_SIZE;
#if OFFSET_DATA_STRUCTURE_STORAGE_ALGORITHM == 1
            assert(num < XOffsetDatastructure::CHUNK_SIZE * XOffsetDatastructure::XStorageBitmap<std::size_t>::MAX_CHUNK_NUM);
            if (num >= XOffsetDatastructure::CHUNK_SIZE * XOffsetDatastructure::XStorageBitmap<std::size_t>::MAX_CHUNK_NUM)
            {
                throw std::runtime_error("oversize error");
            }
#endif
            // std::cout << "expanding begin" << std::endl;
            auto oldData = mBuffer.data();
            DEBUG_PRINT_BLOCK((reinterpret_cast<XSimpleStorage *>(&mBuffer.front())));
            std::size_t oldSize = mBuffer.size();
            mBuffer.resize(mBuffer.size() + num);
            XSimpleStorage *storage = reinterpret_cast<XSimpleStorage *>(&mBuffer.front());
            DEBUG_SET_BASE_ADDR(storage, &mBuffer.front());
            storage->expandBuffer(&mBuffer.front() + headerSize, mBuffer.size() - headerSize);
            // storage->setBuffer(&mBuffer.front() + headerSize, mBuffer.size() - headerSize);
            // storage->addBlock(&mBuffer.front() + oldSize, num, XOffsetDatastructure::CHUNK_SIZE);
            mPtr = reinterpret_cast<T *>(headerSize + reinterpret_cast<intptr_t>(mBuffer.data()));
            auto newData = mBuffer.data();
            DEBUG_PRINT_BLOCK((reinterpret_cast<XSimpleStorage *>(&mBuffer.front())));
            // std::cout << "oldData: " << static_cast<void*>(oldData) << " " << oldSize << std::endl;
            // std::cout << "newData: " << static_cast<void*>(newData) << " " << mBuffer.size() << " " << mBuffer.size() - oldSize << std::endl;
            // std::cout << "expanding end" << std::endl;
        }
#elif OFFSET_DATA_STRUCTURE_POINTER_TYPE == 1
        void expand(std::size_t num) {}
#endif
        void trim()
        {
            DEBUG_PRINT_BLOCK((reinterpret_cast<XSimpleStorage*>(&mBuffer.front())));
            XSimpleStorage *storage = reinterpret_cast<XSimpleStorage *>(&mBuffer.front());
            auto trimSize = storage->trimBuffer();
            // std::cout << "mBuffer.size() before trimming: " << mBuffer.size() << std::endl;
            // std::cout << "trimSize: " << trimSize << std::endl;
            mBuffer.resize(mBuffer.size() - trimSize);
            // std::cout << "shrink_to_fit: "<< &mBuffer.front() << std::endl;
            mBuffer.shrink_to_fit(); // realloc
            mPtr = reinterpret_cast<T *>(headerSize + reinterpret_cast<intptr_t>(mBuffer.data())); // After shrink_to_fit, mPtr needs to be updated.
            // std::cout << "shrink_to_fit: "<< &mBuffer.front() << std::endl;
            // std::cout << "mBuffer.size() after trimming: " << mBuffer.size() << std::endl;
            storage = reinterpret_cast<XSimpleStorage *>(&mBuffer.front());
            storage->setBuffer(&mBuffer.front() + headerSize, mBuffer.size() - headerSize);
            DEBUG_PRINT_BLOCK((reinterpret_cast<XSimpleStorage*>(&mBuffer.front())));
        }
        inline T *operator->()
        {
            return get();
        }
        inline T *get()
        {
            return mPtr;
        }
        inline const std::vector<std::byte> &getBuffer() const
        {
            return mBuffer;
        }
        inline XSimpleStorage *getStorage()
        {
            return reinterpret_cast<XSimpleStorage *>(&mBuffer.front());
        }
        template <typename U>
        inline XOffsetPtr<U> getOffsetPtr(U &obj)
        {
            std::ptrdiff_t offset = reinterpret_cast<std::byte *>(&obj) - &mBuffer.front();
            return XOffsetPtr<U>(this, offset);
        }
        template <typename U>
        inline XOffsetPtr<U> getOffsetPtr(U *ptr)
        {
            std::ptrdiff_t offset = reinterpret_cast<std::byte *>(ptr) - &mBuffer.front();
            return XOffsetPtr<U>(this, offset);
        }
        inline XOffsetPtr<T> getOffsetPtr()
        {
            std::ptrdiff_t offset = reinterpret_cast<std::byte *>(mPtr) - &mBuffer.front();
            return XOffsetPtr<T>(this, offset);
        }

    private:
        T *mPtr;
        std::vector<std::byte> mBuffer;
    };

    template <typename T>
    struct XOffsetPtr
    {
        XOffsetPtr(void *typeholder, std::ptrdiff_t offset) : mTypeHolder(typeholder), mOffset(offset) {}
        inline T *get()
        {
            return reinterpret_cast<T *>(reinterpret_cast<char *>((reinterpret_cast<XTypeHolder<void> *>(mTypeHolder))->getStorage()) + mOffset);
        }
        inline T &operator*()
        {
            return *get();
        }
        inline T *operator->()
        {
            return get();
        }
        // test
        // void expand(std::size_t num)
        // {
        //     ((XTypeHolder<T>*)mTypeHolder)->expand(num);
        // }
        // void trim()
        // {
        //     ((XTypeHolder<T>*)mTypeHolder)->trim();
        // }
        void *mTypeHolder;
        std::ptrdiff_t mOffset;
    };

}
#endif