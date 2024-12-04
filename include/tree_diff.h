#ifndef TREE_DIFF_H
#define TREE_DIFF_H

#include "config.h"

#include <uthash.h>

typedef enum 
{
    STATUS_DELETED,
    STATUS_ADDED,
    STATUS_MODIFIED,
    STATUS_UNMODIFIED,
    STATUS_UNTRACKED
} diff_status_t;

typedef struct
{
    char filepath[PATH_MAX];
    char hash[HEX_SIZE]; // null when not in commit tree
    diff_status_t status;
    UT_hash_handle hh;
} file_entry_t;

typedef struct
{
    file_entry_t *entries;
    size_t size;
} diff_t;

diff_t *diff_init();
void diff_free(diff_t *diff);

int walk_working_dir(const char *path, diff_t *diff);
int walk_commit_tree(const char *commit_hash, diff_t *diff);
void diff_print(const diff_t *diff);

#endif // TREE_DIFF_H