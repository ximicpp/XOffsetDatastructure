#ifndef X_OFFSET_DATA_STRUCTURE_ZLIBCOMPRESSOR_HPP
#define X_OFFSET_DATA_STRUCTURE_ZLIBCOMPRESSOR_HPP
#include <iostream>
#include <zlib.h>

void compressData(const char* input, std::size_t inputSize, char* compressed, std::size_t* compressedSize);
void decompressData(const char* compressed, std::size_t compressedSize, char* decompressed, std::size_t* decompressedSize);

std::size_t compressAndDecompress(const void* input, std::size_t inputSize);


#endif