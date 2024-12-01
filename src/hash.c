#include <openssl/evp.h>
#include <zlib.h>
#include <stdio.h>
#include <errno.h>

#include "hash.h"
#include "config.h"

vcs_error_t sha256_hash(const void *data, size_t size, char *hash_out) {
    if (!data || !hash_out) {
        return VCS_ERROR_NULL_INPUT;
    }

    EVP_MD_CTX *ctx = EVP_MD_CTX_new();
    if (!ctx) {
        return VCS_ERROR_MEMORY_ALLOCATION_FAILED;
    }

    unsigned char hash[HASH_SIZE];
    if (!EVP_DigestInit_ex(ctx, EVP_sha256(), NULL) ||
        !EVP_DigestUpdate(ctx, data, size) ||
        !EVP_DigestFinal_ex(ctx, hash, NULL)) {
        EVP_MD_CTX_free(ctx);
        return VCS_ERROR_HASH_FAILED;
    }

    vcs_error_t err = sha256_hash_to_hex(hash, hash_out);
    EVP_MD_CTX_free(ctx);
    return err;
}

static vcs_error_t validate_file_size(FILE *file) {
    if (!file || fseek(file, 0, SEEK_END) != 0) {
        return VCS_ERROR_IO;
    }

    long file_size = ftell(file);
    if (file_size < 0) {
        return VCS_ERROR_IO;
    }
    if (file_size > MAX_FILE_SIZE) {
        return VCS_ERROR_FILE_TOO_LARGE;
    }

    rewind(file);
    return VCS_OK;
}

vcs_error_t sha256_hash_file(const char *filepath, char *hash_out) {
    if (!filepath || !hash_out) {
        return VCS_ERROR_NULL_INPUT;
    }

    FILE *file = fopen(filepath, "rb");
    if (!file) {
        return errno == ENOENT ? VCS_ERROR_FILE_DOES_NOT_EXIST : VCS_ERROR_IO;
    }

    vcs_error_t err = validate_file_size(file);
    if (err != VCS_OK) {
        fclose(file);
        return err;
    }

    EVP_MD_CTX *ctx = EVP_MD_CTX_new();
    if (!ctx) {
        fclose(file);
        return VCS_ERROR_MEMORY_ALLOCATION_FAILED;
    }

    if (!EVP_DigestInit_ex(ctx, EVP_sha256(), NULL)) {
        EVP_MD_CTX_free(ctx);
        fclose(file);
        return VCS_ERROR_HASH_FAILED;
    }

    unsigned char buffer[8192];
    size_t bytes_read;
    while ((bytes_read = fread(buffer, 1, sizeof(buffer), file)) > 0) {
        if (!EVP_DigestUpdate(ctx, buffer, bytes_read)) {
            EVP_MD_CTX_free(ctx);
            fclose(file);
            return VCS_ERROR_HASH_FAILED;
        }
    }

    if (ferror(file)) {
        EVP_MD_CTX_free(ctx);
        fclose(file);
        return VCS_ERROR_IO;
    }

    unsigned char hash[HASH_SIZE];
    if (!EVP_DigestFinal_ex(ctx, hash, NULL)) {
        EVP_MD_CTX_free(ctx);
        fclose(file);
        return VCS_ERROR_HASH_FAILED;
    }

    err = sha256_hash_to_hex(hash, hash_out);

    EVP_MD_CTX_free(ctx);
    fclose(file);
    return err;
}

vcs_error_t sha256_hash_to_hex(const unsigned char *hash, char *hex_out) {
    if (!hash || !hex_out)
    {
        return VCS_ERROR_NULL_INPUT;
    }

    static const char hex_chars[] = "0123456789abcdef";
    for (int i = 0; i < HASH_SIZE; i++)
    {
        hex_out[i * 2] = hex_chars[hash[i] >> 4];
        hex_out[i * 2 + 1] = hex_chars[hash[i] & 0x0f];
    }
    hex_out[HASH_SIZE * 2] = '\0';

    return VCS_OK;
}