#include "../../../src/utils/assert2.h"
#include "../../../src/utils/logger.h"
#include "../../../src/utils/compression.h"
#include <string.h>
#include <stdlib.h>

static int compression_test_count = 0;
static int compression_start_case(void *(*func)(void *), const char *name);

void* compress_decompress_gzip_test() {
    const char *input_data = "This is a test string for gzip compression and decompression.";
    size_t input_size = strlen(input_data) + 1; // Include null terminator

    char compressed_data[1024] = {0};
    char decompressed_data[1024] = {0};

    // Compress the input data
    int compressed_size = compress_gzip(input_data, input_size, compressed_data);
    assert(compressed_size > 0);

    // Decompress the data
    int decompressed_size = decompress_gzip(compressed_data, compressed_size, decompressed_data, sizeof(decompressed_data));
    assert(decompressed_size > 0);

    // Verify the decompressed data matches the original input data
    assert(memcmp(input_data, decompressed_data, input_size) == 0);

    return NULL;
}

void* compress_decompress_deflate_test() {
    const char *input_data = "This is a test string for deflate compression and decompression.";
    size_t input_size = strlen(input_data) + 1; // Include null terminator

    char compressed_data[1024] = {0};
    char decompressed_data[1024] = {0};

    // Compress the input data
    int compressed_size = compress_deflate(input_data, input_size, compressed_data);
    assert(compressed_size > 0);

    // Decompress the data
    int decompressed_size = decompress_deflate(compressed_data, compressed_size, decompressed_data, sizeof(decompressed_data));
    assert(decompressed_size > 0);

    // Verify the decompressed data matches the original input data
    assert(memcmp(input_data, decompressed_data, input_size) == 0);

    return NULL;
}

void* compress_decompress_empty_input_test() {
    const char *input_data = "";
    size_t input_size = strlen(input_data) + 1; // Include null terminator

    char compressed_data[1024] = {0};
    char decompressed_data[1024] = {0};

    // Compress the input data with gzip
    int compressed_size_gzip = compress_gzip(input_data, input_size, compressed_data);
    assert(compressed_size_gzip >= 0);

    // Decompress the data with gzip
    int decompressed_size_gzip = decompress_gzip(compressed_data, compressed_size_gzip, decompressed_data, sizeof(decompressed_data));
    assert(decompressed_size_gzip >= 0);

    // Verify the decompressed data matches the original input data
    assert(memcmp(input_data, decompressed_data, input_size) == 0);

    // Compress the input data with deflate
    int compressed_size_deflate = compress_deflate(input_data, input_size, compressed_data);
    assert(compressed_size_deflate >= 0);

    // Decompress the data with deflate
    int decompressed_size_deflate = decompress_deflate(compressed_data, compressed_size_deflate, decompressed_data, sizeof(decompressed_data));
    assert(decompressed_size_deflate >= 0);

    // Verify the decompressed data matches the original input data
    assert(memcmp(input_data, decompressed_data, input_size) == 0);

    return NULL;
}

void* compress_decompress_large_input_test() {
    const char *input_data = "This is a test string for large input compression and decompression.";
    size_t input_size = strlen(input_data) * 100; // Repeat the string to make it large

    char *large_input_data = malloc(input_size + 1);
    char compressed_data[65536] = {0};
    char decompressed_data[65536] = {0};

    // Repeat the input data to create a large input
    for (size_t i = 0; i < input_size; i++) {
        large_input_data[i] = input_data[i % strlen(input_data)];
    }
    large_input_data[input_size] = '\0';

    // Compress the large input data with gzip
    int compressed_size_gzip = compress_gzip(large_input_data, input_size + 1, compressed_data);
    assert(compressed_size_gzip > 0);

    // Decompress the data with gzip
    int decompressed_size_gzip = decompress_gzip(compressed_data, compressed_size_gzip, decompressed_data, sizeof(decompressed_data));
    assert(decompressed_size_gzip > 0);

    // Verify the decompressed data matches the original large input data
    assert(memcmp(large_input_data, decompressed_data, input_size + 1) == 0);

    // Compress the large input data with deflate
    int compressed_size_deflate = compress_deflate(large_input_data, input_size + 1, compressed_data);
    assert(compressed_size_deflate > 0);
    assert(compressed_size_deflate < 65536);

    // Decompress the data with deflate
    int decompressed_size_deflate = decompress_deflate(compressed_data, compressed_size_deflate, decompressed_data, sizeof(decompressed_data));
    assert(decompressed_size_deflate > 0);
    assert(decompressed_size_deflate < 65536);

    // Verify the decompressed data matches the original large input data
    assert(memcmp(large_input_data, decompressed_data, input_size + 1) == 0);

    free(large_input_data);

    return NULL;
}

int compression_test() {
    compression_start_case(compress_decompress_gzip_test, "compress_decompress_gzip_test");
    compression_start_case(compress_decompress_deflate_test, "compress_decompress_deflate_test");
    compression_start_case(compress_decompress_empty_input_test, "compress_decompress_empty_input_test");
    compression_start_case(compress_decompress_large_input_test, "compress_decompress_large_input_test");

    return compression_test_count;
}

static int compression_start_case(void *(*func)(void *), const char *name) {
    log_info("Starting test case %d: %s", compression_test_count++, name);

    func(NULL);

    return 0;
}
