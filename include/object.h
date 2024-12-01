#ifndef OBJECT_H
#define OBJECT_H

#include <stdlib.h>

#include "config.h"
#include "error.h"

typedef enum
{
    OBJECT_TYPE_BLOB,
    OBJECT_TYPE_TREE,
    OBJECT_TYPE_COMMIT,
    OBJECT_TYPE_TAG,
    OBJECT_TYPE_UNKNOWN
} object_type_t;

typedef struct
{
    char name[MAX_AUTHOR_NAME_SIZE];
    char email[MAX_AUTHOR_EMAIL_SIZE];
    time_t timestamp;
    int32_t tz_offset; 
} author_t;

typedef struct
{
    char dir[3];
    char filename[OBJECT_FILENAME_SIZE];
    char path[MAX_PATH_LENGTH];
} object_path_t;

// Tree entry for directory listings
typedef struct
{
    char mode[7];
    char hash[HEX_STR_SIZE];
    char name[256];
} tree_entry_t;

// Common object header
typedef struct
{
    object_type_t type;
    size_t size;
    char hash[HEX_STR_SIZE];
    object_path_t path;
    void *data;
} vcs_object_t;

// Specific object types
typedef struct
{
    vcs_object_t header;
    char src_path[MAX_PATH_LENGTH];
} blob_object_t;

typedef struct
{
    vcs_object_t header;
    tree_entry_t *entries;
    size_t entry_count;
} tree_object_t;

typedef struct
{
    vcs_object_t header;
    char tree[HEX_STR_SIZE];
    char parent[HEX_STR_SIZE];
    author_t author;
    author_t committer;
    char *message;
} commit_object_t;

typedef struct
{
    vcs_object_t header;
    object_type_t type;
    char ref[HEX_STR_SIZE];
    char tag[128];
    author_t tagger;
    char *message;
} tag_object_t;

vcs_error_t object_init(vcs_object_t *obj, object_type_t type);
vcs_error_t object_read(const char *hash, vcs_object_t *obj);
vcs_error_t object_write(vcs_object_t *obj);
vcs_error_t object_from_file(const char *path, vcs_object_t *obj);
void object_free(vcs_object_t *obj);

#endif // OBJECT_H