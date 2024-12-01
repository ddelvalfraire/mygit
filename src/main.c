#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>
#include <stdbool.h>
#include <ctype.h>
#include <time.h>
#include <dirent.h>
#include <uthash.h>
#include <utarray.h>

#include "config.h"
#include "error.h"
#include "io.h"
#include "utils.h"
#include "tree_walk.h"
#include "staging.h"
#include "object.h"
#include "hash.h"

void vcs_init();
void vcs_status();
vcs_error_t vcs_add(UT_array *filepaths);
vcs_error_t create_branch(const char *name);
vcs_error_t checkout_branch(const char *name);

// utils
void trim(char *str);
void remove_newline(char *str);
vcs_error_t initialize_master_branch();
vcs_error_t get_current_branch(char *buffer, size_t buffer_size);
bool is_valid_branch_name(const char *name);
bool is_initialized();

// temporary function to parse add arguments
vcs_error_t parse_add_args(const char *args, UT_array *filepaths)
{
    if (!args || !*args || !filepaths)
        return VCS_ERROR_NULL_INPUT;

    char *input = strdup(args);
    if (!input)
    {
        return VCS_ERROR_MEMORY_ALLOCATION_FAILED;
    }

    remove_newline(input);
    char *token = strtok(input, " ");
    vcs_error_t err = VCS_OK;

    while (token != NULL && err == VCS_OK)
    {
        err = add_paths_recursively(token, filepaths);
        token = strtok(NULL, " ");
    }

    free(input);
    return err;
}

int main(void)
{
    char command[256];
    printf("vcs started\n");
    while (1)
    {
        printf("vcs> ");
        scanf("%s", command);

        if (strcmp(command, "exit") == 0)
        {
            printf("vcs ended\n");
            break;
        }
        else if (strcmp(command, "init") == 0)
        {
            vcs_init();
        }
        else if (strcmp(command, "add") == 0)
        {
            char line[1024];
            UT_array *filepaths;
            utarray_new(filepaths, &ut_str_icd);

            if (fgets(line, sizeof(line), stdin))
            {
                vcs_error_t err = parse_add_args(line, filepaths);
                if (err == VCS_OK)
                {
                    err = vcs_add(filepaths);
                    if (err != VCS_OK)
                    {
                        printf("Error: %s\n", vcs_error_string(err));
                    }
                }
                else
                {
                    printf("Error: %s\n", vcs_error_string(err));
                }
                utarray_free(filepaths);
            }
        }
        else if (strcmp(command, "commit") == 0)
        {
            printf("committed\n");
        }
        else if (strcmp(command, "status") == 0)
        {
            vcs_status();
        }
        else if (strcmp(command, "log") == 0)
        {
            printf("log\n");
        }
        else if (strcmp(command, "checkout") == 0)
        {
            printf("checkout\n");
        }
        else if (strcmp(command, "clear") == 0)
        {
            clear_terminal();
        }
        else
        {
            printf("unknown command\n");
        }
    }
    return 0;
}

vcs_error_t vcs_add(UT_array *filepaths)
{
    if (!is_initialized())
    {
        return VCS_ERROR_NO_HEAD;
    }

    if (!filepaths)
    {
        return VCS_ERROR_NULL_INPUT;
    }

    staging_area_t staging;
    vcs_error_t err = staging_init(&staging, STAGED_PATH);
    if (err != VCS_OK)
    {
        return err;
    }

    char **p_filepath = NULL;
    while ((p_filepath = (char **)utarray_next(filepaths, p_filepath)))
    {
        char *filepath = *p_filepath;
        // Validate file exists and is readable
        if (!file_exists(filepath))
        {
            printf("Error: File '%s' does not exist\n", filepath);
            continue;
        }

        // Create blob object
        vcs_object_t blob;
        err = object_init(&blob, OBJECT_TYPE_BLOB);
        if (err != VCS_OK)
        {
            staging_clear(&staging);
            return err;
        }

        blob_object_t *blob_data = (blob_object_t *)blob.data;
        strncpy(blob_data->src_path, filepath, MAX_PATH_LENGTH - 1);

        unsigned char hash[HASH_SIZE];
        err = sha256_hash_file(filepath, hash);
        if (err != VCS_OK)
        {
            object_free(&blob);
            staging_clear(&staging);
            return err;
        }

        err = object_read(hash, &blob);
        if (err != VCS_OK)
        {
            object_free(&blob);
            staging_clear(&staging);
            return err;
        }

        err = object_write(&blob);
        if (err != VCS_OK)
        {
            object_free(&blob);
            staging_clear(&staging);
            return err;
        }

        err = staging_add_entry(&staging, filepath, hash);
        if (err != VCS_OK)
        {
            object_free(&blob);
            staging_clear(&staging);
            return err;
        }
        printf("Added %s\n", filepath);

        object_free(&blob);
    }

    err = staging_save(&staging);
    if (err != VCS_OK)
    {
        staging_clear(&staging);
        return err;
    }

    staging_clear(&staging);
    return VCS_OK;
}

void vcs_init()
{
    DIR *dir = opendir(".vcs");
    if (dir)
    {
        printf("vcs already initialized\n");
        closedir(dir);
    }
    else if (ENOENT == errno)
    {
        initialize_master_branch();
        printf("vcs initialized\n");
    }
    else
    {
        printf("error initializing vcs\n");
    }
}

vcs_error_t initialize_master_branch()
{
    if (mkdir(".vcs", 0777) < 0 ||
        mkdir(".vcs/objects", 0777) < 0 ||
        mkdir(".vcs/commits", 0777) < 0 ||
        mkdir(".vcs/refs", 0777) < 0 ||
        mkdir(".vcs/refs/heads", 0777) < 0 ||
        mkdir(".vcs/refs/tags", 0777) < 0 ||
        mkdir(".vcs/logs", 0777) < 0)
    {
        return VCS_ERROR_IO;
    }

    // Create master branch
    vcs_error_t err = create_branch("master");
    if (err != VCS_OK)
    {
        printf("Error: Could not create master branch\n");
        return err;
    }

    // Checkout master branch
    err = checkout_branch("master");
    if (err != VCS_OK)
    {
        printf("Error: Could not checkout master branch\n");
        return err;
    }

    printf("master branch created\n");
    return VCS_OK;
}

vcs_error_t create_branch(const char *name)
{
    if (!name || !*name)
    {
        return VCS_ERROR_INVALID_BRANCH;
    }

    char trimmed_name[MAX_BRANCH_NAME + 1];
    strncpy(trimmed_name, name, MAX_BRANCH_NAME);
    trimmed_name[MAX_BRANCH_NAME] = '\0';
    trim(trimmed_name);

    if (!is_valid_branch_name(trimmed_name))
    {
        printf("Error: Invalid branch name '%s'\n", trimmed_name);
        return VCS_ERROR_INVALID_BRANCH;
    }

    char branch_path[256];
    snprintf(branch_path, sizeof(branch_path), BRANCH_FMT, trimmed_name);

    if (file_exists(branch_path))
    {
        printf("Error: Branch '%s' already exists\n", trimmed_name);
        return VCS_ERROR_BRANCH_EXISTS;
    }

    FILE *branch = fopen(branch_path, "w");
    if (!branch)
    {
        printf("Error: Could not create branch file\n");
        return VCS_ERROR_CREATE_FAILED;
    }

    if (fprintf(branch, "%s\n", NULL_COMMIT) < 0)
    {
        fclose(branch);
        return VCS_ERROR_IO;
    }

    fclose(branch);
    return VCS_OK;
}

vcs_error_t checkout_branch(const char *name)
{
    char branch_path[256];
    snprintf(branch_path, sizeof(branch_path), BRANCH_FMT, name);

    if (!file_exists(branch_path))
    {
        printf("Error: Branch '%s' does not exist\n", name);
        return VCS_ERROR_BRANCH_DOES_NOT_EXIST;
    }

    FILE *head = fopen(HEAD_PATH, "w");
    if (!head)
    {
        printf("Error: Could not update HEAD file\n");
        return VCS_ERROR_IO;
    }

    fprintf(head, "ref: refs/heads/%s\n", name);
    fclose(head);
    return 0;
}

bool is_valid_branch_name(const char *name)
{
    if (!name || !*name)
        return false;

    size_t len = strlen(name);
    if (len > MAX_BRANCH_NAME)
        return false;

    // Cannot start with '.' or '-'
    if (name[0] == '.' || name[0] == '-')
        return false;

    // Cannot end with '/'
    if (name[len - 1] == '/')
        return false;

    // Cannot contain '..' or '//'
    if (strstr(name, "..") || strstr(name, "//"))
        return false;

    while (*name)
    {
        char c = *name;
        if (!(isalnum(c) || c == '-' || c == '_' || c == '/'))
        {
            return false;
        }
        name++;
    }
    return true;
}

void vcs_status()
{
    if (!is_initialized())
    {
        printf("vcs not initialized\n");
        return;
    }

    char branch_name[256];
    vcs_error_t err = get_current_branch(branch_name, sizeof(branch_name));

    if (err != VCS_OK)
    {
        printf("Error: %s\n", vcs_error_string(err));
        return;
    }

    printf("On branch %s\n", branch_name);
}

vcs_error_t get_current_branch(char *buffer, size_t buffer_size)
{
    if (!buffer || buffer_size == 0)
    {
        return VCS_ERROR_INVALID_HEAD;
    }

    FILE *head = fopen(HEAD_PATH, "r");
    if (!head)
    {
        return VCS_ERROR_NO_HEAD;
    }

    if (!fgets(buffer, buffer_size, head))
    {
        fclose(head);
        return VCS_ERROR_IO;
    }
    fclose(head);

    // Remove "ref: refs/heads/" prefix and trailing newline
    char *prefix = "ref: refs/heads/";
    if (strncmp(buffer, prefix, strlen(prefix)) != 0)
    {
        return VCS_ERROR_INVALID_HEAD;
    }

    memmove(buffer, buffer + strlen(prefix), strlen(buffer) - strlen(prefix) + 1);
    size_t len = strlen(buffer);
    if (len > 0 && buffer[len - 1] == '\n')
    {
        buffer[len - 1] = '\0';
    }

    return VCS_OK;
}

bool is_initialized()
{
    DIR *dir = opendir(".vcs");
    if (dir)
    {
        closedir(dir);
        return true;
    }
    return false;
}