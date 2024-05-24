#include "zlibcompressor.hpp"
#include <iostream>
#include <zlib.h>
#include <cassert>

// Compression function
void compressData(const char* input, std::size_t inputSize, char* compressed, std::size_t* compressedSize)
{
    z_stream stream;
    stream.zalloc = Z_NULL;
    stream.zfree = Z_NULL;
    stream.opaque = Z_NULL;

    // Initialize compression stream
    if (deflateInit(&stream, Z_DEFAULT_COMPRESSION) != Z_OK)
    {
        std::cout << "Failed to initialize compression stream" << std::endl;
        return;
    }

    stream.avail_in = inputSize;           // Input data size
    stream.next_in = (Bytef*)input;        // Input data pointer
    stream.avail_out = *compressedSize;    // Output buffer size
    stream.next_out = (Bytef*)compressed;  // Output buffer pointer

    // Perform compression
    if (deflate(&stream, Z_FINISH) != Z_STREAM_END)
    {
        std::cout << "Compression failed" << std::endl;
        return;
    }

    *compressedSize = stream.total_out;    // Compressed data size

    // End compression stream
    deflateEnd(&stream);
}

// Decompression function
void decompressData(const char* compressed, std::size_t compressedSize, char* decompressed, std::size_t* decompressedSize)
{
    z_stream stream;
    stream.zalloc = Z_NULL;
    stream.zfree = Z_NULL;
    stream.opaque = Z_NULL;

    // Initialize decompression stream
    if (inflateInit(&stream) != Z_OK)
    {
        std::cout << "Failed to initialize decompression stream" << std::endl;
        return;
    }

    stream.avail_in = compressedSize;         // Input data size
    stream.next_in = (Bytef*)compressed;      // Input data pointer
    stream.avail_out = *decompressedSize;     // Output buffer size
    stream.next_out = (Bytef*)decompressed;   // Output buffer pointer

    // Perform decompression
    if (inflate(&stream, Z_FINISH) != Z_STREAM_END)
    {
        std::cout << "Decompression failed" << std::endl;
        return;
    }

    *decompressedSize = stream.total_out;     // Decompressed data size

    // End decompression stream
    inflateEnd(&stream);
}


std::size_t compressAndDecompress(const void* input, std::size_t inputSize)
{
    // const char* input = r1.getBuffer().data();
    // std::size_t inputSize = r1.getBuffer().size() + 1;
    // Compression
    std::size_t compressedSize = compressBound(inputSize);
    char* compressedData = new char[compressedSize];
    compressData(reinterpret_cast<const char*>(input), inputSize, compressedData, &compressedSize);
    // Decompression
    std::size_t decompressedSize = inputSize;
    char* decompressedData = new char[decompressedSize];
    decompressData(compressedData, compressedSize, decompressedData, &decompressedSize);
    assert(inputSize == decompressedSize);

    // std::cout << "Original data size: " << inputSize << std::endl;
    // std::cout << "Compressed data size: " << compressedSize << std::endl;
    
    if (inputSize != decompressedSize) 
    {
        std::cout << "Decompressed data size: " << decompressedSize << std::endl;
        std::cout << "Decompression failed" << std::endl;
        return -1;
    }
    
    return compressedSize;
}

    
