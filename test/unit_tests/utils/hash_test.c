#include "../../../src/utils/assert2.h"
#include "../../../src/utils/hash.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

static int hash_test_count = 0;
static int hash_start_case(void *(*func)(void *), const char *name);

void* sha256_hash_hex_test() {
    const char *input = "This is a test string for SHA256 hashing.";
    char output[SHA256_DIGEST_LENGTH * 2 + 1] = {0};

    int output_size = sha256_hash_hex(input, strlen(input), output);
    assert(output_size == SHA256_DIGEST_LENGTH * 2 + 1);

    return NULL;
}

void* sha256_hash_hex_empty_input_test() {
    const char *input_data = "";
    long input_length = strlen(input_data);

    char outputBuffer[SHA256_DIGEST_LENGTH * 2 + 1] = {0};

    int result = sha256_hash_hex(input_data, input_length, outputBuffer);

    assert(result == SHA256_DIGEST_LENGTH * 2 + 1);
    assert(strcmp(outputBuffer, "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855") == 0);

    return NULL;
}

void* sha256_hash_hex_short_string_test() {
    const char *input_data = "test";
    long input_length = strlen(input_data);

    char outputBuffer[SHA256_DIGEST_LENGTH * 2 + 1] = {0};

    int result = sha256_hash_hex(input_data, input_length, outputBuffer);

    assert(result == SHA256_DIGEST_LENGTH * 2 + 1);
    assert(strcmp(outputBuffer, "9f86d081884c7d659a2feaa0c55ad015a3bf4f1b2b0b822cd15d6c15b0f00a08") == 0);

    return NULL;
}

void* sha256_hash_hex_long_string_test() {
    const char *input_data = "This is a longer test string to check the SHA256 hash function.";
    size_t input_size = strlen(input_data) * 100; // Repeat the string to make it large

    char *large_input_data = malloc(input_size);
    char outputBuffer[SHA256_DIGEST_LENGTH * 2 + 1] = {0};
    // Repeat the input data to create a large input
    for (size_t i = 0; i < input_size; i++) {
        large_input_data[i] = input_data[i % strlen(input_data)];
    }

    int result = sha256_hash_hex(large_input_data, input_size, outputBuffer);

    assert(result == SHA256_DIGEST_LENGTH * 2 + 1);
    assert(strcmp(outputBuffer, "15198daf542067e15f9b6ada822e9fd238b631d86a708ead9e14489c7f72bc7a") == 0);

    return NULL;
}

void* sha256_hash_hex_binary_data_test() {
    const unsigned char input_data[] = {0xde, 0xad, 0xbe, 0xef, 0x00, 0x01, 0x02, 0x03};
    long input_length = sizeof(input_data);

    char outputBuffer[SHA256_DIGEST_LENGTH * 2 + 1] = {0};

    int result = sha256_hash_hex((const char*)input_data, input_length, outputBuffer);

    assert(result == SHA256_DIGEST_LENGTH * 2 + 1);
    assert(strcmp(outputBuffer, "60f124171b44e2c7ea54510ed698727dfba5da38715ed8b255775c7b30afe7f6") == 0);

    return NULL;
}

int hash_test() {
  hash_start_case(sha256_hash_hex_test, "sha256_hash_hex_test");
  hash_start_case(sha256_hash_hex_empty_input_test, "sha256_hash_hex_empty_input_test");
  hash_start_case(sha256_hash_hex_short_string_test, "sha256_hash_hex_short_string_test");
  hash_start_case(sha256_hash_hex_long_string_test, "sha256_hash_hex_long_string_test");
  hash_start_case(sha256_hash_hex_binary_data_test, "sha256_hash_hex_binary_data_test");

    return hash_test_count;
}

static int hash_start_case(void *(*func)(void *), const char *name) {
    printf("Starting test case %d: %s\n", hash_test_count++, name);

    func(NULL);

    return 0;
}
