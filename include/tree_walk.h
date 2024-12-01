#ifndef TREE_WALK_H
#define TREE_WALK_H

#include <utarray.h>

#include "error.h"


vcs_error_t add_paths_recursively(const char *path, UT_array *result);

#endif // TREE_WALK_H