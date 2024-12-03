#ifndef STAGING_H
#define STAGING_H

#include "config.h"

#include <stdlib.h>
#include <uthash.h>
#include <utarray.h>


typedef struct
{
    uint32_t ctime_sec;
    uint32_t ctime_nsec;
    uint32_t mtime_sec;
    uint32_t mtime_nsec;
    uint32_t dev;
    uint32_t ino;
    uint32_t mode;
    uint32_t uid;
    uint32_t gid;
    uint32_t size;
    uint16_t flags;
    char hash[HEX_SIZE];
    char path[PATH_MAX];
} index_entry_t;

typedef struct
{
    char path[PATH_MAX]; // key
    index_entry_t entry; // value
    UT_hash_handle hh;   // makes this structure hashable
} index_hash_entry_t;

typedef struct
{
    uint32_t signature; // DIRC
    uint32_t version;   // 2
    uint32_t entry_count;
} index_header_t;

// Modified index structure
typedef struct
{
    index_header_t header;
    index_hash_entry_t *entries; // Hash table of entries
    char *filepath;
} index_t;

index_t *index_init(const char *filepath);
void index_free(index_t *index);

int index_stage_files(index_t *index, UT_array *files);
int index_write(index_t *index);

#endif // STAGING_H