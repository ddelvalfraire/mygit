#include "tree_diff.h"
#include "myignore.h"
#include "staging.h"
#include "object.h"
#include "object_types.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <limits.h>

diff_t *diff_init()
{
    diff_t *diff = malloc(sizeof(diff_t));
    if (!diff)
        return NULL;

    diff->size = 0;
    diff->entries = NULL;

    return diff;
}

void diff_free(diff_t *diff)
{
    file_entry_t *entry, *tmp;
    HASH_ITER(hh, diff->entries, entry, tmp)
    {
        HASH_DEL(diff->entries, entry);
        free(entry);
    }

    free(diff);
}

file_entry_t *file_entry_init(const char *filepath, diff_status_t status)
{
    file_entry_t *file_entry = malloc(sizeof(file_entry_t));
    if (!file_entry)
        return NULL;

    strcpy(file_entry->filepath, filepath);
    memset(file_entry->hash, 0, HEX_SIZE);
    file_entry->status = status;

    return file_entry;
}

int walk_commit_tree_recursive(object_t *tree)
{
    printf("Walking commit tree\n");
    tree_data_t *data = (tree_data_t *)tree->data;
    tree_entry_t *entry, *tmp;
    HASH_ITER(hh, data->entries, entry, tmp)
    {
        if (S_ISDIR(entry->mode))
        {
            object_t *subtree = object_init(OBJ_TREE);
            if (object_read(subtree, entry->hash) != 0)
            {
                printf("Error: Failed to read tree object\n");
                return -1;
            }
            printf("Subtree: %s\n", entry->name);

            walk_commit_tree_recursive(subtree);
        }
        else if (S_ISREG(entry->mode))
        {
            printf("File: %s\n", entry->name);
        }
    }
    return 0;
}

int walk_commit_tree(const char *commit_hash, diff_t *diff)
{
    char tree_hash[HEX_SIZE];
    if (object_get_commit_tree_hash(commit_hash, tree_hash) != 0)
    {
        printf("Error: Failed to get commit tree hash\n");
        return -1;
    }
    printf("tree hash: %s\n", tree_hash);
    object_t *tree = object_init(OBJ_TREE);
    if (object_read(tree, tree_hash) != 0)
    {
        printf("Error: Failed to read tree object\n");
        return -1;
    }

    if (walk_commit_tree_recursive(tree) != 0)
    {
        printf("Error: Failed to walk commit tree\n");
        return -1;
    }
    return 0;
}
void walk_staging(index_t *index, diff_t *diff)
{
    index_hash_entry_t *entry;
    for (entry = index->entries; entry != NULL; entry = entry->hh.next)
    {
    }
}

void diff_print(const diff_t *diff)
{
    file_entry_t *entry, *tmp;
    HASH_ITER(hh, diff->entries, entry, tmp)
    {
        // printf("File: %s, Status: %d\n", entry->filepath, entry->status);
    }
}

int walk_working_dir(const char *path, diff_t *diff)
{
    // Initialize ignore list once at the root level
    static ignore_list_t ignore_list;
    static int ignore_list_initialized = 0;

    if (!ignore_list_initialized)
    {
        memset(&ignore_list, 0, sizeof(ignore_list));
        load_myignore(".", &ignore_list);
        ignore_list_initialized = 1;
    }

    DIR *dir = opendir(path);
    if (!dir)
    {
        fprintf(stderr, "Error: Failed to open directory '%s'\n", path);
        return -1;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL)
    {
        // Skip . and ..
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        // Construct full and relative paths
        char full_path[PATH_MAX];
        char relative_path[PATH_MAX];

        snprintf(full_path, sizeof(full_path), "%s/%s", path, entry->d_name);

        // Create relative path by removing "./" prefix if present
        if (strncmp(path, "./", 2) == 0)
            snprintf(relative_path, sizeof(relative_path), "%s/%s", path + 2, entry->d_name);
        else if (strcmp(path, ".") == 0)
            snprintf(relative_path, sizeof(relative_path), "%s", entry->d_name);
        else
            snprintf(relative_path, sizeof(relative_path), "%s/%s", path, entry->d_name);

        struct stat st;
        if (stat(full_path, &st) != 0)
        {
            fprintf(stderr, "Error: Failed to stat '%s'\n", full_path);
            continue;
        }

        // Check if path should be ignored
        if (is_ignored(relative_path, &ignore_list))
            continue;

        if (S_ISDIR(st.st_mode))
        {
            walk_working_dir(full_path, diff);
        }
        else if (S_ISREG(st.st_mode))
        {
            file_entry_t *file_entry = malloc(sizeof(file_entry_t));
            if (!file_entry)
            {
                printf("Error: Failed to allocate memory\n");
                closedir(dir);
                return -1;
            }

            strcpy(file_entry->filepath, relative_path);
            file_entry->status = STATUS_UNTRACKED;
            HASH_ADD_STR(diff->entries, filepath, file_entry);
            diff->size++;
        }
    }

    closedir(dir);
    return 0;
}
