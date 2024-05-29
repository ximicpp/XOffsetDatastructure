#ifndef X_OFFSET_DATA_STRUCTURE_CONFIG_HPP
#define X_OFFSET_DATA_STRUCTURE_CONFIG_HPP
#include <exception>
#include <iostream>

// #define OFFSET_DATA_STRUCTURE_DEBUG_MODE
// #define OFFSET_DATA_STRUCTURE_DEBUG_USE_DEFAULT_MALLOC_FREE
#define OFFSET_DATA_STRUCTURE_RETRY_ATUO 1
// #define OFFSET_DATA_STRUCTURE_USE_EXCEPTION 1
#define OFFSET_DATA_STRUCTURE_EXCEPTION_HANDLE_TYPE 0 // 0: exception, 1: longjmp ("forced" unwinding) 2: boost leaf
#define OFFSET_DATA_STRUCTURE_STORAGE_ALGORITHM 1     // 0: simple storage freelist, 1: simple storage bitmap
#define OFFSET_DATA_STRUCTURE_POINTER_TYPE 0          // 0: boost::interprocess::offset_ptr<T>, 1: void*. Just for test
#define OFFSET_DATA_STRUCTURE_CUSTOM_CONTAINER_GROWTH_FACTOR 1 // 0: default, 1: custom growth factor

namespace XOffsetDatastructure
{
    constexpr std::size_t CHUNK_SIZE = 64;
    constexpr uint32_t EXPANDSIZE = 12; // 1 << 12
    constexpr uint32_t EXPANDMASK = (1 << EXPANDSIZE) - 1;

    // template <typename T>
    // using XOffsetPtr = boost::interprocess::offset_ptr<T>;
}

#endif