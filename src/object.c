#include "object.h"

#include <stdlib.h>
#include <stdio.h>
#include <openssl/evp.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>



typedef struct
{
    char hash[HEX_SIZE];
    object_type_t type;
    size_t content_size;
} object_header_t;

struct object
{
    object_header_t header;
    void *data;
};

typedef struct
{
    unsigned char data[FILE_MAX];
    size_t size;
} blob_data_t;

static void *object_data_allocate(object_type_t type)
{
    switch (type)
    {
    case OBJ_BLOB:
        return malloc(sizeof(blob_data_t));
    default:
        return NULL;
    }
}

object_t *object_init(object_type_t type)
{
    object_t *obj = malloc(sizeof(object_t));
    if (!obj)
        return NULL;

    obj->header.type = type;
    obj->header.content_size = 0;
    obj->data = object_data_allocate(type);

    return obj;
}

void object_free(object_t *obj)
{

    if (obj->data)
        free(obj->data);

    if (obj)
        free(obj);

    obj = NULL;
}

static int hash_to_hex(const unsigned char *hash, char *hex)
{
    for (int i = 0; i < SHA256_SIZE; i++)
    {
        sprintf(hex + i * 2, "%02x", hash[i]);
    }
    return 0;
}

static const char *object_type_to_string(object_type_t type)
{
    switch (type)
    {
    case OBJ_BLOB:
        return "blob";
    case OBJ_TREE:
        return "tree";
    case OBJ_COMMIT:
        return "commit";
    case OBJ_TAG:
        return "tag";
    default:
        return "unknown";
    }
}

static int object_compute_hash(object_t *obj, char *hash) {
    EVP_MD_CTX *ctx = EVP_MD_CTX_new();
    unsigned char hash_bytes[SHA256_SIZE];
    unsigned int hash_len;

    if (!ctx) {
        return -1;
    }


    char header[256];
    int header_len = snprintf(header, sizeof(header), "%s %zu%c",
                            object_type_to_string(obj->header.type),
                            obj->header.content_size,
                            '\0');
    if (header_len < 0) {
        EVP_MD_CTX_free(ctx);
        return -1;
    }

    if (!EVP_DigestInit_ex(ctx, EVP_sha256(), NULL)) {
        EVP_MD_CTX_free(ctx);
        return -1;
    }

    // Hash header first
    if (!EVP_DigestUpdate(ctx, header, header_len + 1)) {
        EVP_MD_CTX_free(ctx);
        return -1;
    }

    // Hash content based on type
    switch (obj->header.type) {
    case OBJ_BLOB: {
        blob_data_t *blob = (blob_data_t *)obj->data;
        if (!EVP_DigestUpdate(ctx, blob->data, blob->size)) {
            EVP_MD_CTX_free(ctx);
            return -1;
        }
        break;
    }
    default:
        EVP_MD_CTX_free(ctx);
        return -1;
    }

    if (!EVP_DigestFinal_ex(ctx, hash_bytes, &hash_len)) {
        EVP_MD_CTX_free(ctx);
        return -1;
    }

    EVP_MD_CTX_free(ctx);
    hash_to_hex(hash_bytes, hash);
    return 0;
}

static size_t get_filesize(const char *filepath) {
    struct stat st;
    if (stat(filepath, &st) == 0) {
        return st.st_size;
    }
    return 0;
}

static int update_blob(object_t *obj, object_data_t data) {
    blob_data_t *blob = (blob_data_t *)obj->data;
    
    // Check file size first
    size_t size = get_filesize(data.blob.filepath);
    if (size > FILE_MAX) {
        fprintf(stderr, "Error: File '%s' is too large (%zu bytes, max is %d)\n", 
                data.blob.filepath, size, FILE_MAX);
        return -1;
    }

    FILE *fp = fopen(data.blob.filepath, "rb");
    if (!fp) {
        fprintf(stderr, "Error opening file '%s'\n", data.blob.filepath);
        return -1;
    }

    blob->size = fread(blob->data, 1, size, fp);
    if (ferror(fp)) {
        fprintf(stderr, "Error reading file '%s'\n", data.blob.filepath);
        fclose(fp);
        return -1;
    }

    if (blob->size != size) {
        fprintf(stderr, "Error: File '%s' read size mismatch\n", data.blob.filepath);
        fclose(fp);
        return -1;
    }

    obj->header.content_size = blob->size;
    
    fclose(fp);
    return 0;
}

int object_update(object_t *obj, object_data_t data)
{
    switch (obj->header.type)
    {
    case OBJ_BLOB:
    {
        return update_blob(obj, data);
    }
    default:
        fprintf(stderr, "Error: Unsupported type\n");
        return -1;
    }
}

static int create_directory(const char *path)
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

static int object_write_header(FILE *fp, object_t *obj)
{
    int written = fprintf(fp, "%s %zu%c",
                          object_type_to_string(obj->header.type),
                          obj->header.content_size,
                          '\0');
    return written < 0 ? -1 : 0;
}

int object_write(object_t *obj, char *out_hash) {
    char hash[HEX_SIZE];
    if (object_compute_hash(obj, hash) != 0) {
        fprintf(stderr, "Error: Failed to compute hash\n");
        return -1;
    }

    // Create hash prefix directory
    char prefix_dir[PATH_MAX];
    snprintf(prefix_dir, sizeof(prefix_dir), ".vcs/objects/%c%c", 
             hash[0], hash[1]);
    if (create_directory(prefix_dir) < 0) {
        return -1;
    }

    // Full path with hash suffix
    char obj_path[PATH_MAX];
    snprintf(obj_path, sizeof(obj_path), "%s/%s", prefix_dir, hash + 2);

    FILE *fp = fopen(obj_path, "wb");  // Binary mode
    if (!fp) {
        fprintf(stderr, "Error: Failed to open '%s'\n", obj_path);
        return -1;
    }

    if (object_write_header(fp, obj) < 0) {
        fprintf(stderr, "Error: Failed to write header\n");
        fclose(fp);
        return -1;
    }

    switch (obj->header.type) {
    case OBJ_BLOB: {
        blob_data_t *blob = (blob_data_t *)obj->data;
        if (fwrite(blob->data, 1, blob->size, fp) != blob->size) {
            fprintf(stderr, "Error: Failed to write data\n");
            fclose(fp);
            return -1;
        }
        break;
    }
    default:
        fprintf(stderr, "Error: Unsupported type\n");
        fclose(fp);
        return -1;
    }

    fclose(fp);

    if (out_hash) {
        strcpy(out_hash, hash);
    }
    return 0;
}