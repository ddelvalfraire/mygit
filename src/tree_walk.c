#include <ftw.h>
#include <stddef.h>
#include <string.h>
#include <ctype.h>

#include "tree_walk.h"
#include "config.h"

static UT_array *g_path_list;
static vcs_error_t g_error;

static bool is_valid_filename(const char *path)
{
    // Get basename of path
    const char *basename = strrchr(path, '/');
    basename = basename ? basename + 1 : path;

    // Skip hidden files and special names
    if (basename[0] == '.' ||
        strcmp(basename, "..") == 0 ||
        strchr(basename, '\\') != NULL ||
        strcmp(basename, ".vcs") == 0)
    {
        return false;
    }

    // Check for valid characters
    for (const char *p = basename; *p; p++)
    {
        if (!isprint(*p) || *p == '\\')
        {
            return false;
        }
    }

    return true;
}

static int ftw_callback(const char *path, const struct stat *sb, int typeflag)
{
    // Skip invalid filenames and .vcs directory
    if (!is_valid_filename(path) || strstr(path, "/.vcs/") != NULL)
    {
        return 0;
    }

    // Only add regular files
    if (typeflag == FTW_F)
    {
        if (utarray_len(g_path_list) >= MAX_PATHS)
        {
            g_error = VCS_TOO_MANY_ARGUMENTS;
            return 1; // Stop traversal
        }

       utarray_push_back(g_path_list, &path);

    }
    return 0;
}

vcs_error_t add_paths_recursively(const char *path, UT_array *result)
{
    if (!path || !result)
    {
        return VCS_ERROR_NULL_INPUT;
    }

    g_path_list = result;
    g_error = VCS_OK;

    // Start traversal
    int flags = FTW_PHYS; // Don't follow symlinks
    int max_fd = 50;      // Maximum parallel open directories

    if (ftw(path, ftw_callback, max_fd) != 0 && g_error == VCS_OK)
    {
        return VCS_ERROR_IO;
    }

    return g_error;
}