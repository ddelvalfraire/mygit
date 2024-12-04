#ifndef MYIGNORE_H
#define MYIGNORE_H

#include <limits.h>

#define MAX_PATTERNS 1024

typedef struct {
    char patterns[MAX_PATTERNS][PATH_MAX];
    int count;
} ignore_list_t;

void load_myignore(const char *path, ignore_list_t *ignore_list);
int is_ignored(const char *filepath, ignore_list_t *ignore_list);

#endif // MYIGNORE_H