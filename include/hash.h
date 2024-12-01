#ifndef HASH_H
#define HASH_H

#include <string.h>

#include "error.h" 

vcs_error_t sha256_hash(const void* data, size_t size, unsigned char* hash_out);

vcs_error_t sha256_hash_file(const char* filepath, unsigned char* hash_out);

vcs_error_t sha256_hash_to_hex(const unsigned char *hash, char *hex_out);

#endif // HASH_H