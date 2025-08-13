#ifndef COMPRESSION_H
#define COMPRESSION_H

#include <stddef.h>
#include <stdint.h>

uint8_t* lzw_compress(const uint8_t* input, size_t input_size, size_t* out_size_bytes);
uint8_t* lzw_decompress(const uint8_t* input, size_t input_size, size_t* out_size_bytes);

#endif // COMPRESSION_H
