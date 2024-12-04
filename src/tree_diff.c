#include "tree_diff.h"
#include "myignore.h"
#include "staging.h"
#include "object.h"
#include "object_types.h"
#include "util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <limits.h>
#include <uthash.h>
#include <utarray.h>

#define RED "\x1b[31m"
#define GREEN "\x1b[32m"
#define RESET "\x1b[0m"
#define NUM_STATUSES 7

typedef enum
{
    STATUS_DELETED,
    STATUS_ADDED,
    STATUS_MODIFIED,
    STATUS_UNMODIFIED,
    STATUS_UNTRACKED,
    STATUS_STAGED,
    STATUS_STAGED_MODIFIED
} diff_status_t;

typedef struct
{
    char filepath[PATH_MAX];
    char hash_in_commit[HEX_SIZE];      // null when not in commit tree
    char hash_in_index[HEX_SIZE];       // null when not in index
    char hash_in_working_dir[HEX_SIZE]; // null when not in working dir
    int in_working_dir;
    int in_index;
    int in_commit;
    diff_status_t status;
    UT_hash_handle hh;
} file_entry_t;

typedef struct
{
    char **files;    // Array of file paths
    size_t count;    // Number of files
    size_t capacity; // Current capacity
} status_list_t;

struct diff
{
    file_entry_t *entries;
    size_t size;
};

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

file_entry_t *file_entry_init(const char *filepath)
{
    file_entry_t *file_entry = malloc(sizeof(file_entry_t));
    if (!file_entry)
        return NULL;

    strcpy(file_entry->filepath, filepath);
    memset(file_entry->hash_in_commit, 0, HEX_SIZE);
    memset(file_entry->hash_in_index, 0, HEX_SIZE);
    file_entry->in_working_dir = 0;
    file_entry->in_index = 0;
    file_entry->in_commit = 0;
    file_entry->status = STATUS_UNTRACKED;

    return file_entry;
}

static int walk_commit_tree_recursive(object_t *tree, diff_t *diff)
{
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
            walk_commit_tree_recursive(subtree, diff);
        }
        else if (S_ISREG(entry->mode))
        {
            file_entry_t *file_entry;
            HASH_FIND_STR(diff->entries, entry->name, file_entry);
            if (!file_entry)
            {
                file_entry = file_entry_init(entry->name);
                if (!file_entry)
                {
                    printf("Error: Failed to allocate memory\n");
                    return -1;
                }
                HASH_ADD_STR(diff->entries, filepath, file_entry);
                diff->size++;
            }
            strcpy(file_entry->hash_in_commit, entry->hash);
            file_entry->in_commit = 1;
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

    object_t *tree = object_init(OBJ_TREE);
    if (object_read(tree, tree_hash) != 0)
    {
        printf("Error: Failed to read tree object\n");
        return -1;
    }

    if (walk_commit_tree_recursive(tree, diff) != 0)
    {
        printf("Error: Failed to walk commit tree\n");
        return -1;
    }
    return 0;
}
void walk_staging(const index_t *index, diff_t *diff)
{
    index_hash_entry_t *entry;
    for (entry = index->entries; entry != NULL; entry = entry->hh.next)
    {
        file_entry_t *file_entry;
        HASH_FIND_STR(diff->entries, entry->path, file_entry);
        if (!file_entry)
        {
            file_entry = file_entry_init(entry->path);
            if (!file_entry)
            {
                printf("Error: Failed to allocate memory\n");
                return;
            }
            diff->size++;
            HASH_ADD_STR(diff->entries, filepath, file_entry);
        }
        strcpy(file_entry->hash_in_index, entry->entry.hash);
        file_entry->in_index = 1;
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
        if (S_ISREG(st.st_mode))
        {
            file_entry_t *file_entry = file_entry_init(relative_path);
            if (!file_entry)
            {
                printf("Error: Failed to allocate memory\n");
                closedir(dir);
                return -1;
            }

            if (compute_file_hash(full_path, file_entry->hash_in_working_dir) != 0)
            {
                free(file_entry);
                closedir(dir);
                return -1;
            }

            file_entry->in_working_dir = 1;
            HASH_ADD_STR(diff->entries, filepath, file_entry);
            diff->size++;
        }
    }

    closedir(dir);
    return 0;
}

static void status_list_init(status_list_t *list)
{
    list->files = malloc(sizeof(char *) * 10); // Initial capacity
    list->count = 0;
    list->capacity = 10;
}

static void status_list_add(status_list_t *list, const char *filepath)
{
    if (list->count == list->capacity)
    {
        list->capacity *= 2;
        list->files = realloc(list->files, sizeof(char *) * list->capacity);
    }
    list->files[list->count] = strdup(filepath);
    list->count++;
}
static void status_list_free(status_list_t *list)
{
    for (size_t i = 0; i < list->count; i++)
    {
        free(list->files[i]);
    }
    free(list->files);
    list->count = 0;
    list->capacity = 0;
}

void diff_print(const diff_t *diff)
{
    // Initialize arrays for each status
    status_list_t lists[NUM_STATUSES];
    for (int i = 0; i < NUM_STATUSES; i++)
    {
        status_list_init(&lists[i]);
    }

    // Categorize files into appropriate lists
    file_entry_t *entry;
    for (entry = diff->entries; entry != NULL; entry = entry->hh.next)
    {
        if (entry->in_working_dir && !entry->in_index && !entry->in_commit)
        {
            status_list_add(&lists[STATUS_UNTRACKED], entry->filepath);
        }
        else if (entry->in_working_dir && entry->in_index && !entry->in_commit)
        {
            if (strcmp(entry->hash_in_working_dir, entry->hash_in_index) == 0)
            {
                status_list_add(&lists[STATUS_UNMODIFIED], entry->filepath);
            }
            else
            {
                status_list_add(&lists[STATUS_MODIFIED], entry->filepath);
            }
        }
        else if (entry->in_working_dir && entry->in_index && entry->in_commit)
        {
            if (strcmp(entry->hash_in_index, entry->hash_in_commit) == 0)
            {
                if (strcmp(entry->hash_in_working_dir, entry->hash_in_index) == 0)
                {
                    status_list_add(&lists[STATUS_UNMODIFIED], entry->filepath);
                }
                else
                {
                    status_list_add(&lists[STATUS_STAGED], entry->filepath);
                }
            }
            else
            {
                status_list_add(&lists[STATUS_STAGED_MODIFIED], entry->filepath);
            }
        }
        else if (!entry->in_working_dir && entry->in_index && entry->in_commit)
        {
            status_list_add(&lists[STATUS_DELETED], entry->filepath);
        }
        else if (!entry->in_working_dir && !entry->in_index && entry->in_commit)
        {
            status_list_add(&lists[STATUS_DELETED], entry->filepath);
        }
        else if (!entry->in_working_dir && entry->in_index && !entry->in_commit)
        {
            status_list_add(&lists[STATUS_DELETED], entry->filepath);
        }
        else if (entry->in_working_dir && !entry->in_index && entry->in_commit)
        {
            status_list_add(&lists[STATUS_MODIFIED], entry->filepath); // or create new STATUS_REMOVED_FROM_INDEX
        }
        else
        {
            printf("Error: Unhandled case\n");
        }
    }

    // Print staged changes
    if (lists[STATUS_STAGED].count > 0)
    {
        printf(GREEN "Changes to be committed:\n" RESET);
        printf("  (use \"vcs reset HEAD <file>...\" to unstage)\n\n");

        for (size_t i = 0; i < lists[STATUS_STAGED].count; i++)
        {
            printf("\t" GREEN "modified: %s\n" RESET, lists[STATUS_STAGED].files[i]);
        }
        printf("\n");
    }

    // Print unstaged changes
    if (lists[STATUS_MODIFIED].count > 0 || lists[STATUS_DELETED].count > 0)
    {
        printf("Changes not staged for commit:\n");
        printf("  (use \"vcs add <file>...\" to update what will be committed)\n");
        printf("  (use \"vcs restore <file>...\" to discard changes in working directory)\n\n");

        for (size_t i = 0; i < lists[STATUS_MODIFIED].count; i++)
        {
            printf("\t" RED "modified: %s\n" RESET, lists[STATUS_MODIFIED].files[i]);
        }
        for (size_t i = 0; i < lists[STATUS_DELETED].count; i++)
        {
            printf("\t" RED "deleted: %s\n" RESET, lists[STATUS_DELETED].files[i]);
        }
        printf("\n");
    }

    if (lists[STATUS_UNTRACKED].count > 0)
    {
        printf("Untracked files:\n");
        printf("  (use \"vcs add <file>...\" to include in what will be committed)\n\n");
        for (size_t i = 0; i < lists[STATUS_UNTRACKED].count; i++)
        {
            printf("\t" RED "%s\n" RESET, lists[STATUS_UNTRACKED].files[i]);
        }
        printf("\n");
    }

    if (lists[STATUS_UNTRACKED].count > 0)
    {
        printf("no changes added to commit (use \"vcs add\" and/or \"vcs commit -a\")\n");
    }

    // Free all lists
    for (int i = 0; i < NUM_STATUSES; i++)
    {
        status_list_free(&lists[i]);
    }
}