# Using Modern C++ to Build XOffsetDatastructure: A Zero-Encoding and Zero-Decoding High-Performance Serialization Library in the Game Industry

## 0. Abstract

XOffsetDatastructure is a serialization library designed to reduce or even eliminate the performance consumption of serialization and deserialization in the game industry by utilizing zero-encoding and zero-decoding. It is also a collection of high-performance data structures designed for efficient read and in-place/non-in-place write, with performance comparable to STL.  

Custom allocators are used to keep fields in contiguous memory, enabling them to be sent directly. Offset pointers are used to maintain pointer validity after buffer resizing and memory movement caused by modifications to variable-size fields. Some other techniques, such as containers that support offset pointers and auto-resizing mechanisms, are involved.

The performance statistics demonstrate that XOffsetDatastructure is a high-performance library, offering advantages in terms of encoding and decoding time, read and write performance, and message size compared to other modern C++ serialization libraries.

## 1. Why

The original intention of XOffsetDatastructure is presented in this chapter.  

### 1.1 Purpose

We aim to reduce or even eliminate the performance consumption of serialization and deserialization, which is typically substantial.  

According to a study by Kanev et al.[\[1\]](https://doi.org/10.1145/2749469.2750392), serialization and RPCs are responsible for 12% of all fleet cycles across all applications at Google. According to another study by Palkar et al.[\[2\]](https://doi.org/10.14778/3236187.3236207), modern big data applications can spend 80–90% of CPU time parsing data.    

In the game industry, serialization/deserialization performance consumption has always been a factor that affects user experience and game performance. In one of our typical commercial games, over 20% of CPU time is spent on serialization and deserialization. In the game domain, various game data needs to be transferred and modified between different servers, as well as between servers and clients.    

This is the original intention behind proposing this solution: to solve the performance problem of game data transfer.  

### 1.2 Related Work

Current serialization solutions fall into the following two categories.  

The first category of serialization solutions includes MessagePack[\[3\]](https://msgpack.org/), Protocol Buffers[\[4\]](https://protobuf.dev/), among others.  

*   [Serialization] Serialization requires traversing and encoding all fields, which leads to a long serialization time.  
    *   [MessagePack] `msgpack::pack` must recursively traverse all the fields, encode them, and write them to a buffer.  
    *   [Protocol Buffers] `SerializeToString` is similar.  
*   [Deserialization] Deserialization also requires traversing to parse all the fields, which leads to a long deserialization time.  
    *   [MessagePack] `msgpack::unpack` must recursively traverse all fields, decode them one by one, and assemble them into in-memory data structures.  
    *   [Protocol Buffers] `ParseFromString` is similar.  

The second category of serialization solutions includes FlatBuffers[\[5\]](https://flatbuffers.dev/), Cap'n Proto[\[6\]](https://capnproto.org/), among others.  

*   [Serialization] Although the cost of the serialization interface is low, the actual serialization requires setting the fields in memory using the corresponding `FlatBufferBuilder/MessageBuilder`. Consequently, the actual cost of serialization is high.  
    *   [FlatBuffers] FlatBuffers requires all fields to be set recursively using `FlatBufferBuilder`.  
    *   [Cap'n Proto] Cap'n Proto is similar.   
*   [Deserialization] Data can be read directly. However, deserialized data structures do not support efficient in-place and non-in-place modifications. To deserialize this into data structures that support efficient in-place and non-in-place modifications, all fields must be traversed and decoded, which can make the actual deserialization time-consuming.  
    *   [FlatBuffers] Fields can be read directly using `flatbuffers::GetRoot`. However, effective in-place and non-in-place modifications are not supported.  
    *   [Cap'n Proto] Cap'n Proto is similar.   

## 2. What

The main features of XOffsetDatastructure are presented in this chapter.  

### 2.1 Zero-encoding and Zero-decoding

Buffer/data can be sent directly without additional encoding and decoding by using contiguous memory for storage.  

*   For example, you can use `XTypeHolder::getBuffer` to get the data that can be sent directly.  

```cpp
inline const std::vector<std::byte>& XTypeHolder::getBuffer() const
{
    return mBuffer; // std::vector<std::byte> mBuffer;
}
```

*   You can read and write directly after using `XTypeHolder::XTypeHolder(std::vector<std::byte> &externalBuffer)`.  

```cpp
XTypeHolder::XTypeHolder(std::vector<std::byte> &externalBuffer)
    : mBuffer(std::move(externalBuffer)) // std::vector<std::byte> mBuffer;
{
    // ...
}
```

### 2.2 Read and in-place/non-in-place Write

Fields can be read and written directly, including both in-place and non-in-place modifications. All operations keep fields in contiguous memory so they can be sent directly.  

```cpp
XTypeHolder<Character> character;
character->speed = 522.0;
character->pos = {81.5, 12.3, 502.7};
character->skills.emplace_back(1054, 3);
character->items.emplace(std::piecewise_construct, std::forward_as_tuple(0xda4ccb62c899d751), std::forward_as_tuple(8056, 0xda4ccb62c899d751, 99, 0x171f7f8205fa4a1d));
character->items.erase(0xda4ccb62c899d751);
// get wire format with zero encoding
character.getBuffer();
```

### 2.3 Various Types and Containers

All base types, custom types, different containers, and nested types are supported.  

```cpp
struct Item
{
    uint32_t id;
    // ...
};
struct Equip : public Item
{
    uint32_t level;
    XVector<float> attributes;
};
struct Character
{
    uint64_t id;
    XString name;
    // ...
    Position pos;
    XVector<float> attributes;
    XVector<Equip> equips;
    XMap<uint64_t, Item> items;
};
```

### 2.4 High Performance Read and Write

The data read/write performance of XOffsetDatastructure is comparable to that of STL and Boost Container, and even better.    

XOffsetDatastructure can be used directly as base data structures in memory.  

| Containers               | Read/Write(100 thousand times, ms) |
| ------------------------ | ---------------------------------- |
| std::*                   | 68.0446                            |
| boost::*                 | 56.273                             |
| XOffsetDatastructure::*  | 49.8465                            |

### 2.5 Pointer Validity

Provides pointers that remain valid even after buffer resizing and memory movement caused by modifications to variable-size fields.  

```cpp
XTypeHolder<Character> character;
// ...
auto attributes = character.getOffsetPtr<XVector<float>>(character.attributes);
// ...
attributes->push_back(2.718);
```

## 3. Performance Statistics

The performance statistics of XOffsetDatastructure are presented in this chapter.  

### 3.1 Encoding and Decoding

The advantage of XOffsetDatastructure in terms of encoding and decoding performance is evident. The encoding and decoding time is almost zero due to zero-encoding and zero-decoding.    
Note: Cista++(offset)[\[7\]](https://cista.rocks/), Cista++(raw)[\[8\]](https://cista.rocks/), zpp::bits[\[9\]](https://github.com/eyalz800/zpp_bits) are some modern C++ serialization libraries.  

| Algorithms           | Encoding/Decoding(100 thousand times, ms) |
| -------------------- | ----------------------------------------- |
| Cap'n Proto          | 259.225                                   |
| Cista++(offset)      | 584.549                                   |
| Cista++(raw)         | 531.557                                   |
| FlatBuffers          | 679.442                                   |
| MessagePack          | 957.7                                     |
| XOffsetDatastructure | 0.0530667                                 |
| zpp::bits            | 453.136                                   |
| Protocol Buffers     | 1578.06                                   |

### 3.2 Read and Write Performance

XOffsetDatastructure has advantages in read and write performance.    
Note: For solutions that lack support for in-place and non-in-place modifications, consider using STL for in-memory read and write.  

| Algorithms          | Containers Implementation| Read/Write (100 thousand times, ms)  |
| ------------------- | ------------------------ | ------------------------------------ |
| Cap'n Proto         | std::*                   | 68.0446                              |
| Cista++(offset)     | cista::offset::*         | 69.2177                              |
| Cista++(raw)        | cista::raw:*             | 65.5362                              |
| FlatBuffers         | std::*                   | 68.0446                              |
| MessagePack         | std::*                   | 68.0446                              |
| XOffsetDatastructure| XOffsetDatastructure::*  | 49.8465                              |
| zpp::bits           | std::*                   | 75.3239                              |
| Protocol Buffers    | google::protobuf::*      | 56.5378                              |

### 3.3 Message Size

The message size comparison is shown in the table below. The difference between the compressed message sizes is not significant, and XOffsetDatastructure is slightly superior compared to the other solutions.    
Note: The compressed message size is crucial because network messages are typically transmitted in compressed form.

| Algorithms           | Wire Format Size (zlib/normal, bytes) |
| -------------------- | ------------------------------------- |
| Cap'n Proto          | 3205/3656                             |
| Cista++(offset)      | 3336/5392                             |
| Cista++(raw)         | 3294/5392                             |
| FlatBuffers          | 3247/3672                             |
| MessagePack          | 3064/3222                             |
| XOffsetDatastructure | 2749/5656                             |
| zpp::bits            | 2769/2916                             |
| Protocol Buffers     | 3759/4100                             |

## 4. How

The main implementation technologies of XOffsetDatastructure are presented in this chapter.  

### 4.1 Custom Allocator

XOffsetDatastructure maintains a block of memory space with contiguous virtual addresses. This memory is divided into chunks, and a bitmap is used to mark the usage of each chunk. To expedite operations such as bit lookups, intrinsic functions are also utilized.  

```cpp
class XStorageBitmap
{
private:
    BitArray<MAX_CHUNK_NUM> mBitArray;
    Pointer mStartPointer;
    // ...
public:
    // ...
    inline void *mallocN(const std::size_t numChunks)
    {
        auto startIndex = mBitArray.findClearBitsAndSet(numChunks, 0);
        if (startIndex == -1)
        {
            return nullptr;
        }
        return mStartPointer + startIndex * CHUNK_SIZE;
    }
    inline void freeN(void *const ptr, const std::size_t numChunks)
    {
        auto startIndex = (ptr - mStartPointer) / CHUNK_SIZE;
        mBitArray.clearBits(startIndex, numChunks);
    }
    // ...
};
```

It can, of course, be replaced by other efficient allocators.  

### 4.2 Offset Pointers

There are two types of offset pointers used by XOffsetDatastructure, where the specification of an address is in terms of an offset rather than an absolute address.  

*   One type is to record an offset from `this` pointer. We use `boost::interprocess::offset_ptr` directly.
    
```cpp
boost::interprocess::offset_ptr<void> ptr;
```

*   One type is to record an offset from a custom base address.

```cpp
template <typename T>
struct XOffsetPtr
{
    // ...
    inline T *get()
    {
        return mStartPointer + mOffset;
    }
    // ...
    void *mStartPointer;
    std::ptrdiff_t mOffset;
};
```

### 4.3 Containers with Offset Pointers

Boost provides several containers that support custom pointer types[\[10\]](https://github.com/boostorg/container). These can be used directly as follows.  

```cpp
using XString = boost::container::basic_string<char, std::char_traits<char>, XSimpleAllocator<char, XSimpleStorage>>;
```

```cpp
template <typename T>
using XVector = boost::container::vector<T, XSimpleAllocator<T, XSimpleStorage>>;
```

```cpp
template <typename T>
using XFlatSet = boost::container::flat_set<T, std::less<T>, XSimpleAllocator<T, XSimpleStorage>>;
template <typename T>
using XSet = boost::container::set<T, std::less<T>, XSimpleAllocator<T, XSimpleStorage>>;
```

```cpp
template <typename K, typename V>
using XFlatMap = boost::container::flat_map<K, V, std::less<K>, XSimpleAllocator<std::pair<K, V>, XSimpleStorage>>;
template <typename K, typename V>
using XMap = boost::container::map<K, V, std::less<K>, XSimpleAllocator<std::pair<const K, V>, XSimpleStorage>>;
```

### 4.4 Auto-resizing Mechanisms

Auto-resizing mechanisms are provided for ease of use. (as opposed to pre-allocation) 

```cpp
RETRY_IF_BAD_ALLOC(rootptr->mSet.insert(4), holder);
```

All containers should be `bad_alloc` exception-safe. To ensure auto-resizing, we have checked all containers that we support.  

We have implemented auto-resizing in three ways:  

* The first approach is to use the standard exception mechanism, which throws a `bad_alloc` exception when there is not enough contiguous memory available internally. The exception will be caught and handled where it can be handled, and the operation will be retried.
  
* The second method involves `longjmp`, which jumps to the error handling code to extend and retry if there is not enough internal contiguous memory available.

* The third option is to use Boost Leaf[\[11\]](https://github.com/boostorg/leaf), which functions similarly to the first approach. (We don't use it like std::expected because it's not proper here to change all the return values of the entire call stack.)

The performance of the three options is compared below:  

| Method    | Time (ms) |
|-----------|-----------|
| Exception | 1735      |
| longjmp   | 1395      |
| Leaf      | 3177      |

## 5. Limitations and Plans

The limitations and plans of XOffsetDatastructure are presented in this chapter.  

### 5.1 Memory Fragmentation

As demonstrated above, there is room for improvement in the custom allocator, which exhibits both intra-chunk and inter-chunk fragmentation.  

*   The fragmentation problem can be mitigated by improvements in allocators. It is worth noting that the allocator is an active area of research and could receive a fair amount of support. How to improve the custom allocator is a topic that will be continued.

*   An efficient compact mechanism can also reduce fragmentation. How to improve the efficiency of the compact mechanism is a direction we will continue to explore.

### 5.2 Compatibility

There are potential incompatibilities among the various types of architecture. However, the machines we currently use are all little-endian, and no other incompatibilities have been observed. Compatibility detection metadata is now provided, and no further parsing will be performed for incompatible machines.  

Efficiently processing compatibility is an issue that needs to be continued.

## 6. Summary

XOffsetDatastructure is a serialization library designed to reduce or even eliminate the performance consumption of serialization and deserialization in the game industry by utilizing zero-encoding and zero-decoding.  

XOffsetDatastructure is also a collection of high-performance data structures designed for efficient read and in-place/non-in-place write, with performance comparable to STL.   

## 7. Appendix

### 7.1 Growth Factor

| Growth Factor  | Wire Format Size  | Read/Write  | Encoding/Decoding  |
| -------------- | ----------------- | ----------- | ------------------ |
| 0.1            | 3992/2671         | 50.1851ms   | 0.03455ms          |
| 0.2            | 4568/2709         | 51.327ms    | 0.02648ms          |
| 0.3            | 4952/2740         | 53.6701ms   | 0.0272ms           |
| 0.4            | 4760/2755         | 51.2758ms   | 0.02699ms          |
| 0.5            | 8344/2782         | 51.5814ms   | 0.03568ms          |
| 0.6            | 5656/2749         | 51.2941ms   | 0.02678ms          |
| 0.7            | 8408/2849         | 52.3287ms   | 0.02724ms          |
| 0.8            | 7064/2700         | 52.4735ms   | 0.02808ms          |
| 0.9            | 8344/2742         | 50.7637ms   | 0.02616ms          |
| 1.0            | 9624/2743         | 50.7227ms   | 0.0262ms           |


### 7.2 Memory Usage

10000 units

| Algorithms               | Containers Implementation| Memory Usage (10 thousand units, MB) |
| ------------------------ | ------------------------ | ------------------------------------ |
| MessagePack              | std::*                   | 135                                  |
| XOffsetDatastructure(10) | XOffsetDatastructure::*  | 125                                  |
| XOffsetDatastructure(60) | XOffsetDatastructure::*  | 182                                  |
| XOffsetDatastructure(30) | XOffsetDatastructure::*  | 160                                  |

XOffsetDatastructure(10)
111111111111111111111111111111111111100111111110000111111111
inter chunk fragments: 0.1

XOffsetDatastructure(30)
111100000111111111111000001111110001111111111111111111111111111111111111111
inter chunk fragments: 0.173333

XOffsetDatastructure(60)
11110111000000111111100000000000111111111111111111111111111111111111111111111111111111
inter chunk fragments: 0.209302

XOffsetDatastructure(100)
1111011100111111000000000000000000000000000000000000000000000000000011111111111111111111111111111111111111111111111111111111111111111111111111111111
inter chunk fragments: 0.371622

### 7.3 Compaction

