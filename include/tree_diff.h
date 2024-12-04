#ifndef TREE_DIFF_H
#define TREE_DIFF_H

#include "staging.h"

typedef struct diff diff_t;

diff_t *diff_init();
void diff_free(diff_t *diff);

int walk_working_dir(const char *path, diff_t *diff);
int walk_commit_tree(const char *commit_hash, diff_t *diff);
void walk_staging(const index_t *index, diff_t *diff);
void diff_print(const diff_t *diff);

#endif // TREE_DIFF_H