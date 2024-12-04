#include "util.h"
#include "config.h"

#include <stdio.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <openssl/evp.h>

int create_directory(const char *path)
{
    struct stat st = {0};
    if (stat(path, &st) == -1)
    {
        if (mkdir(path, 0755) == -1)
        {
            fprintf(stderr, "Error creating directory '%s': %s\n",
                    path, strerror(errno));
            return -1;
        }
    }
    return 0;
}

size_t get_filesize_by_filepath(const char *filepath)
{
    struct stat st;
    if (stat(filepath, &st) == 0)
    {
        return st.st_size;
    }
    return 0;
}

size_t get_filesize_by_fp(FILE *fp)
{
    struct stat st;
    if (fstat(fileno(fp), &st) == 0)
    {
        return st.st_size;
    }
    return 0;
}

int hash_to_hex(const unsigned char *hash, char *hex)
{
    for (int i = 0; i < SHA256_SIZE; i++)
    {
        sprintf(hex + i * 2, "%02x", hash[i]);
    }
    return 0;
}
int compute_file_hash(char *filepath, char *hash)
{
    FILE *fp = fopen(filepath, "rb");
    if (!fp) {
        fprintf(stderr, "Error opening file '%s': %s\n",
                filepath, strerror(errno));
        return -1;
    }

    // Get file size
    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    // Create header
    char header[OBJECT_HEADER_MAX];
    int header_len = snprintf(header, sizeof(header), "blob %ld", size);
    header[header_len] = '\0';  // Add null byte

    // Initialize hash context
    EVP_MD_CTX *ctx = EVP_MD_CTX_new();
    unsigned char hash_bytes[SHA256_SIZE];
    unsigned int hash_len;

    if (!ctx) {
        fclose(fp);
        return -1;
    }

    // Start hashing
    if (!EVP_DigestInit_ex(ctx, EVP_sha256(), NULL)) {
        fclose(fp);
        EVP_MD_CTX_free(ctx);
        return -1;
    }

    // Hash header
    if (!EVP_DigestUpdate(ctx, header, header_len + 1)) { // +1 for null byte
        fclose(fp);
        EVP_MD_CTX_free(ctx);
        return -1;
    }


    unsigned char buffer[4096];
    size_t bytes;
    while ((bytes = fread(buffer, 1, sizeof(buffer), fp)) > 0) {
        if (!EVP_DigestUpdate(ctx, buffer, bytes)) {
            fclose(fp);
            EVP_MD_CTX_free(ctx);
            return -1;
        }
    }

    if (!EVP_DigestFinal_ex(ctx, hash_bytes, &hash_len)) {
        fclose(fp);
        EVP_MD_CTX_free(ctx);
        return -1;
    }

    EVP_MD_CTX_free(ctx);
    fclose(fp);

    hash_to_hex(hash_bytes, hash);
    return 0;
}

int filepath_from_hash(const char *hash, char *filepath)
{
    const int fp_size = HEX_SIZE + 14; 
    snprintf(filepath, fp_size, ".vcs/objects/%c%c/%s",
             hash[0], hash[1], hash + 2);
    return 0;
}
