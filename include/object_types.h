#ifndef OBJECT_TYPES_H
#define OBJECT_TYPES_H

#include "config.h"
#include "object.h"

#include <uthash.h>
#include <sys/syslimits.h>


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

typedef enum
{
    TREE_FILE,
    TREE_SUBDIR
} tree_entry_type_t;

typedef struct tree_entry
{
    mode_t mode;
    char name[PATH_MAX];
    char hash[HEX_SIZE];
    UT_hash_handle hh;
} tree_entry_t;

typedef struct tree_data
{
    tree_entry_t *entries;
    char dirname[PATH_MAX];
    size_t size;
} tree_data_t;

typedef struct
{
    char path[PATH_MAX];
    object_t *tree;
    UT_hash_handle hh;
} tree_map_t;

typedef struct
{
    char name[NAME_MAX];
    char email[NAME_MAX];
    time_t time;
    off_t offset;
} signature_t;

typedef struct
{
    char tree_hash[HEX_SIZE];
    signature_t author;
    signature_t committer;
    char message[COMMIT_MSG_MAX];
} commit_data_t;

#endif // OBJECT_TYPES_H