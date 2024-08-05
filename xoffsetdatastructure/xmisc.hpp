#ifndef X_OFFSET_DATA_STRUCTURE_MISC_HPP
#define X_OFFSET_DATA_STRUCTURE_MISC_HPP
#include <exception>
#include <iostream>
#include <sstream>
#include <string>
#include <cassert>
#include <csetjmp>
#if OFFSET_DATA_STRUCTURE_EXCEPTION_HANDLE_TYPE == 2
#include "boost/leaf.hpp"
#endif

typedef uint64_t CHUNK, *PCHUNK;
#define CHUNK_BYTES (sizeof(CHUNK))
#define CHUNK_BITS (CHUNK_BYTES * 8)
#define CHUNK_CLEAR ((CHUNK)0)
#define CHUNK_SET ((CHUNK) - 1)
#define CHUNK_HIGH_BIT ((CHUNK)1 << (CHUNK_BITS - 1))
#define BitScanForwardChunk _BitScanForward64
#define BitScanReverseChunk _BitScanReverse64
#define ULONG unsigned long
#define INVALID_INDEX ((ULONG) - 1)

#ifdef _WIN64
#include <intrin.h>
#endif

#ifdef __APPLE__
#define __forceinline inline
inline bool _BitScanForward64(ULONG* Index, ULONG Mask) {
    if (Mask == 0) {
        return false;
    }
    *Index = __builtin_ctzll(Mask);
    return true;
}
inline bool _BitScanReverse64(ULONG* Index, ULONG Mask) {
    if (Mask == 0) {
        return false;
    }
    *Index = 63 - __builtin_clzll(Mask);
    return true;
}
#endif

#ifdef __linux__
#define __forceinline inline

inline bool _BitScanForward64(ULONG* Index, ULONG Mask) {
    if (Mask == 0) {
        return false;
    }
    *Index = (ULONG)__builtin_ffsll(Mask) - 1;
    return true;
}

inline bool _BitScanReverse64(ULONG* Index, ULONG Mask) {
    if (Mask == 0) {
        return false;
    }
    *Index = 63 - __builtin_clzll(Mask);
    return true;
}
#endif

namespace XOffsetDatastructure
{
#if OFFSET_DATA_STRUCTURE_EXCEPTION_HANDLE_TYPE == 2
    struct XBadAllocExceptionLeaf
    {
        std::size_t mSize;
    };
#endif
    class XBadAllocException : public std::bad_alloc
    {
    public:
        XBadAllocException(std::size_t size) : mSize(size) {}
        virtual const char *what() const noexcept
        {
            return "XBadAllocException";
        }
        std::size_t size() const noexcept
        {
            return mSize;
        }

    private:
        std::size_t mSize;
    };

    static inline bool findCleanRange(uint64_t *bitmap, size_t bitmapSize,
                                      size_t startSearch, size_t sizeRequested,
                                      size_t *bitPos, size_t *availableSize)
    {
        uint64_t bitset;
        uint64_t previousSetBit = ULONG_MAX;
        uint64_t current;
        size_t k = startSearch / 64;
        if (k >= bitmapSize)
            return false;
        bitset = bitmap[k];
        uint64_t mask = ~(ULONG_MAX << (startSearch % 64));
        bitset ^= mask;
        do
        {
            if (bitset == 0)
            {
                if (previousSetBit == ULONG_MAX)
                {
                    previousSetBit = k * 64 - 1;
                }
                current = (k + 1) * 64;
                if (current > previousSetBit + sizeRequested &&
                    current > startSearch)
                {
                    *bitPos = previousSetBit + 1;
                    *availableSize = (current - previousSetBit - 1);
                    return true;
                }
            }
            while (bitset != 0)
            {
                unsigned long r;
                BitScanForwardChunk(&r, bitset);
                current = k * 64 + (uint64_t)r;
                auto tmpPreviousSetBit = previousSetBit == ULONG_MAX ? -1 : previousSetBit;
                if ((current > tmpPreviousSetBit + sizeRequested) &&
                    current > startSearch)
                {
                    *bitPos = tmpPreviousSetBit + 1;
                    *availableSize = (current - tmpPreviousSetBit - 1);
                    return true;
                }
                previousSetBit = current;
                bitset &= (bitset - 1);
            }
            if (++k >= bitmapSize)
                break;
            bitset = bitmap[k];
        } while (true);
        return false;
    }

    const unsigned char FillMask[] = {0x00, 0x01, 0x03, 0x07, 0x0F, 0x1F, 0x3F, 0x7F, 0xFF};
    const unsigned char ZeroMask[] = {0xFF, 0xFE, 0xFC, 0xF8, 0xf0, 0xe0, 0xc0, 0x80, 0x00};

    template <std::size_t N>
    class BitArray
    {

    private:
        constexpr static std::size_t WORD_SIZE = sizeof(uint64_t) * 8;
        uint64_t data[N / WORD_SIZE + 1];
        inline int bindex(int b) { return b / WORD_SIZE; }
        inline int boffset(int b) { return b % WORD_SIZE; }

    public:
        BitArray()
        {
            std::memset(data, 0xff, sizeof(uint64_t) * (N / WORD_SIZE + 1));
        }
        inline void setBit(int b)
        {
            data[bindex(b)] |= 1ull << (boffset(b));
        }
        inline void clearBit(int b)
        {
            data[bindex(b)] &= ~(1ull << (boffset(b)));
        }
        inline bool getBit(int b)
        {
            return data[bindex(b)] & (1ull << (boffset(b)));
        }

        inline ULONG ClearMSBInChunk(CHUNK chunk)
        {
            ULONG Index;
            if (BitScanReverseChunk(&Index, chunk) == false)
            {
                Index = CHUNK_BITS;
            }
            else
            {
                Index = CHUNK_BITS - Index - 1;
            }
            return Index;
        }
        inline ULONG ClearLSBInChunk(CHUNK chunk)
        {
            ULONG Index;
            if (BitScanForwardChunk(&Index, chunk) == false)
            {
                Index = CHUNK_BITS;
            }
            return Index;
        }
        inline bool FindClearRunInChunk(CHUNK Chunk, ULONG RunLength, ULONG *StartBit)
        {
            CHUNK chunkMask;
            ULONG shift;
            ULONG shiftsRemaining;
            assert(RunLength < CHUNK_BITS);
            assert(RunLength > 1);
            chunkMask = ~Chunk;
            shiftsRemaining = RunLength;
            do
            {
                shift = shiftsRemaining / 2;
                chunkMask &= chunkMask >> shift;
                if (chunkMask == 0)
                {
                    return false;
                }
                shiftsRemaining -= shift;
            } while (shiftsRemaining > 1);
            BitScanForwardChunk(StartBit, chunkMask);
            return true;
        }

#define READ_CHUNK(n) (*(chunkPtr + (n)))
        inline ULONG FindClearBitsRange(ULONG numberToFind, ULONG rangeStart, ULONG rangeEnd)
        {
            ULONG bitsRemaining;
            CHUNK chunk;
            PCHUNK chunkArray;
            PCHUNK chunkPtr;
            PCHUNK clearChunkRunEnd;
            ULONG clearChunksRequired;
            CHUNK firstChunk;
            CHUNK headChunkMask;
            ULONG headClearBits;
            PCHUNK lastPossibleClearChunk;
            ULONG lastPossibleStartBit;
            PCHUNK lastPossibleStartChunk;
            PCHUNK rangeStartChunk;
            PCHUNK rangeEndChunk;
            ULONG runStartBit;
            ULONG tailClearBits;
            if ((rangeEnd - rangeStart + 1) < numberToFind)
            {
                return INVALID_INDEX;
            }
            chunkArray = data;
            rangeStartChunk = chunkArray + (rangeStart / CHUNK_BITS);
            lastPossibleStartBit = rangeEnd - numberToFind + 1;
            lastPossibleStartChunk = chunkArray + lastPossibleStartBit / CHUNK_BITS;
            headChunkMask = ((CHUNK)1 << (rangeStart % CHUNK_BITS)) - 1;
            chunkPtr = rangeStartChunk;
            firstChunk = READ_CHUNK(0) | headChunkMask;
            if (numberToFind > (2 * CHUNK_BITS - 1))
            {
                lastPossibleClearChunk = lastPossibleStartChunk;
                if ((lastPossibleStartBit % CHUNK_BITS) != 0)
                {
                    lastPossibleClearChunk += 1;
                }
                if (firstChunk == CHUNK_CLEAR)
                {
                    headClearBits = 0;
                    goto largeLoopEntry;
                }
                chunkPtr += 1;
                if (READ_CHUNK(0) == CHUNK_CLEAR)
                {
                    headClearBits = ClearMSBInChunk(firstChunk);
                    goto largeLoopEntry;
                }
                while (true)
                {
                findRunStartLarge:
                    if (chunkPtr > lastPossibleClearChunk)
                    {
                        return INVALID_INDEX;
                    }
                    chunkPtr += 1;
                    if (READ_CHUNK(0) == CHUNK_CLEAR)
                    {
                        break;
                    }
                }
                headClearBits = ClearMSBInChunk(READ_CHUNK(-1));
            largeLoopEntry:
                runStartBit = (ULONG)((chunkPtr - chunkArray) * CHUNK_BITS);
                runStartBit -= headClearBits;
                if (runStartBit > lastPossibleStartBit)
                {
                    return INVALID_INDEX;
                }
                clearChunksRequired = (numberToFind - headClearBits) / CHUNK_BITS;
                clearChunkRunEnd = chunkPtr + clearChunksRequired;
                chunkPtr += 1;
                while (chunkPtr != clearChunkRunEnd)
                {
                    if (READ_CHUNK(0) != CHUNK_CLEAR)
                    {
                        goto findRunStartLarge;
                    }
                    chunkPtr += 1;
                }
                tailClearBits = (numberToFind - headClearBits) % CHUNK_BITS;
                if (tailClearBits != 0)
                {
                    if (ClearLSBInChunk(READ_CHUNK(0)) < tailClearBits)
                    {
                        goto findRunStartLarge;
                    }
                }
            }
            else if (numberToFind >= CHUNK_BITS)
            {
                chunk = firstChunk;
                while (true)
                {
                    while (true)
                    {
                        if ((chunk & CHUNK_HIGH_BIT) == 0)
                        {
                            break;
                        }
                        chunkPtr += 1;
                        if (chunkPtr > lastPossibleStartChunk)
                        {
                            return INVALID_INDEX;
                        }
                        chunk = READ_CHUNK(0);
                    }
                    headClearBits = ClearMSBInChunk(chunk);
                    runStartBit = (ULONG)((chunkPtr - chunkArray) * CHUNK_BITS);
                    runStartBit += CHUNK_BITS - headClearBits;
                    if (runStartBit > lastPossibleStartBit)
                    {
                        return INVALID_INDEX;
                    }
                    assert(numberToFind >= headClearBits);
                    bitsRemaining = numberToFind - headClearBits;
                    if (bitsRemaining == 0)
                    {
                        break;
                    }
                    chunkPtr += 1;
                    chunk = READ_CHUNK(0);
                    if (bitsRemaining >= CHUNK_BITS)
                    {
                        if (chunk != CHUNK_CLEAR)
                        {
                            continue;
                        }
                        bitsRemaining -= CHUNK_BITS;
                        if (bitsRemaining == 0)
                        {
                            break;
                        }
                        chunkPtr += 1;
                        chunk = READ_CHUNK(0);
                    }
                    tailClearBits = ClearLSBInChunk(chunk);
                    if (tailClearBits >= bitsRemaining)
                    {
                        break;
                    }
                }
            }
            else if (numberToFind > 1)
            {
                tailClearBits = 0;
                rangeEndChunk = chunkArray + (rangeEnd / CHUNK_BITS);
                chunk = firstChunk;
                while (true)
                {
                    if (chunk == CHUNK_SET)
                    {
                        do
                        {
                            chunkPtr += 1;
                            if (chunkPtr > lastPossibleStartChunk)
                            {
                                return INVALID_INDEX;
                            }
                            chunk = READ_CHUNK(0);
                        } while (chunk == CHUNK_SET);
                        tailClearBits = 0;
                    }
                    headClearBits = ClearLSBInChunk(chunk);
                    if ((headClearBits + tailClearBits) >= numberToFind)
                    {
                        runStartBit = 0 - tailClearBits;
                    }
                    else
                    {
                        if (FindClearRunInChunk(chunk,
                                                numberToFind,
                                                &runStartBit) == false)
                        {
                            if (chunkPtr == rangeEndChunk)
                            {
                                return INVALID_INDEX;
                            }
                            tailClearBits = ClearMSBInChunk(chunk);
                            chunkPtr += 1;
                            chunk = READ_CHUNK(0);
                            continue;
                        }
                    }
                    runStartBit += (ULONG)((chunkPtr - chunkArray) * CHUNK_BITS);
                    if (runStartBit > lastPossibleStartBit)
                    {
                        return INVALID_INDEX;
                    }
                    else
                    {
                        break;
                    }
                }
            }
            else
            {
                chunk = firstChunk;
                while (chunk == CHUNK_SET)
                {
                    chunkPtr += 1;
                    if (chunkPtr > lastPossibleStartChunk)
                    {
                        return INVALID_INDEX;
                    }
                    chunk = READ_CHUNK(0);
                }
                BitScanForwardChunk(&runStartBit, ~chunk);
                runStartBit += (ULONG)((chunkPtr - chunkArray) * CHUNK_BITS);
                if (runStartBit > lastPossibleStartBit)
                {
                    return INVALID_INDEX;
                }
            }
            return runStartBit;
        }

        inline int FindClearBits(std::size_t numberToFind, std::size_t hintIndex)
        {
            std::size_t sizeOfBitMap = N;
            std::size_t rangeStart = hintIndex >= sizeOfBitMap ? 0 : hintIndex;
            std::size_t rangeEnd = sizeOfBitMap - 1;
            if (numberToFind == 0)
            {
                return rangeStart;
            }
            ULONG bitIndex;
            while (true)
            {
                bitIndex = FindClearBitsRange(static_cast<ULONG>(numberToFind), static_cast<ULONG>(rangeStart), static_cast<ULONG>(rangeEnd));
                if (bitIndex != INVALID_INDEX || rangeStart == 0)
                {
                    break;
                }
                rangeEnd = hintIndex + numberToFind;
                if (rangeEnd > sizeOfBitMap)
                {
                    rangeEnd = sizeOfBitMap;
                }
                rangeEnd -= 1;
                rangeStart = 0;
            }
            return bitIndex == INVALID_INDEX ? -1 : static_cast<int>(bitIndex);
        }

        inline void SetBits(std::size_t startingIndex, std::size_t numberToSet)
        {
            char *CurrentByte;
            ULONG BitOffset;
            if (numberToSet == 0)
            {
                return;
            }
            CurrentByte = (reinterpret_cast<char *>(data)) + (startingIndex / 8);
            BitOffset = startingIndex % 8;
            if ((BitOffset + numberToSet) <= 8)
            {
                *CurrentByte |= (FillMask[numberToSet] << BitOffset);
            }
            else
            {
                if (BitOffset > 0)
                {
                    *CurrentByte |= ZeroMask[BitOffset];
                    CurrentByte += 1;
                    numberToSet -= 8 - BitOffset;
                }
                if (numberToSet > 8)
                {
                    std::memset(CurrentByte, 0xff, numberToSet / 8);
                    CurrentByte += numberToSet / 8;
                    numberToSet %= 8;
                }
                if (numberToSet > 0)
                {
                    *CurrentByte |= FillMask[numberToSet];
                }
            }
        }
        inline void clearBits(std::size_t staringIndex, std::size_t numberToClear)
        {
            char *CurrentByte;
            ULONG BitOffset;
            if (numberToClear == 0)
            {
                return;
            }
            CurrentByte = (reinterpret_cast<char *>(data)) + (staringIndex / 8);
            BitOffset = staringIndex % 8;
            if ((BitOffset + numberToClear) <= 8)
            {
                *CurrentByte &= ~(FillMask[numberToClear] << BitOffset);
            }
            else
            {
                if (BitOffset > 0)
                {
                    *CurrentByte &= FillMask[BitOffset];
                    CurrentByte += 1;
                    numberToClear -= 8 - BitOffset;
                }
                if (numberToClear > 8)
                {
                    std::memset(CurrentByte, 0x00, numberToClear / 8);
                    CurrentByte += numberToClear / 8;
                    numberToClear %= 8;
                }
                if (numberToClear > 0)
                {
                    *CurrentByte &= ZeroMask[numberToClear];
                }
            }
        }

        int findClearBitsAndSet(std::size_t numberToFind, std::size_t hintIndex)
        {
            int startingIndex = FindClearBits(numberToFind, hintIndex);
            if (startingIndex != -1)
            {
                SetBits(startingIndex, numberToFind);
            }
            return startingIndex;
        }

        inline int FindTailClearBits(int endIndex)
        {
            int index = endIndex;
            if (getBit(index) == true)
            {
                return -1;
            }
            index--;
            while (index >= 0)
            {
                if (getBit(index) == false)
                {
                    index--;
                }
                else
                {
                    return index + 1;
                }
            }
            return -1; // No tail set bits found
        }
        inline std::string toString()
        {
            std::stringstream ss;
            for (int i = 0; i < N; i++)
            {
                ss << getBit(i);
            }
            return ss.str();
        }
    };

#if OFFSET_DATA_STRUCTURE_EXCEPTION_HANDLE_TYPE == 1
    class XJmpBuffer
    {
    public:
        static inline std::jmp_buf &getJmpBuffer()
        {
            static std::jmp_buf instance;
            return instance;
        }
    };
#endif

    template <typename Func, typename ExpandFunc>
    __forceinline void XRetryIfBadAlloc(Func func, ExpandFunc expandFunc)
    {
        // static int xx = 0;
#if OFFSET_DATA_STRUCTURE_EXCEPTION_HANDLE_TYPE == 0
        bool success = false;
        while (!success)
        {
            try
            {
                func();
                success = true;
            }
            catch (const XOffsetDatastructure::XBadAllocException &exception)
            {
                expandFunc(exception.size());
                // std::cout << "Retry " << xx++ << std::endl;
            }
        }
#elif OFFSET_DATA_STRUCTURE_EXCEPTION_HANDLE_TYPE == 1
        auto size = setjmp(XJmpBuffer::getJmpBuffer());
        if (size)
        {
            expandFunc(size);
            // std::cout << "Retry " << xx++ << std::endl;
        }
        func();
#elif OFFSET_DATA_STRUCTURE_EXCEPTION_HANDLE_TYPE == 2
        boost::leaf::try_catch(
            [&]
            {
                func();
            },
            [&](const XOffsetDatastructure::XBadAllocExceptionLeaf &exception)
            {
                expandFunc(exception.mSize);
            });
#endif
    }

#if OFFSET_DATA_STRUCTURE_RETRY_ATUO == 0
#define RETRY_IF_BAD_ALLOC(func, holder) func;
#else
// #define RETRY_IF_BAD_ALLOC(func, holder) XOffsetDatastructure::XRetryIfBadAlloc([&]() { func; }, [&](std::size_t st) { holder.expand(((st + XOffsetDatastructure::EXPANDMASK) >> XOffsetDatastructure::EXPANDSIZE) << XOffsetDatastructure::EXPANDSIZE); })

// inline and almost zero-overhead
#define RETRY_IF_BAD_ALLOC(func, holder) \
    while (true) { \
        try { \
            func; \
            break; \
        } catch (const XOffsetDatastructure::XBadAllocException& exception) { \
            holder.expand(((exception.size() + XOffsetDatastructure::EXPANDMASK) >> XOffsetDatastructure::EXPANDSIZE) << XOffsetDatastructure::EXPANDSIZE); \
        } \
    }

#endif
}

#endif
