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

#include "config.h"

typedef struct
{
    unsigned char raw[HASH_SIZE];
    char hex[HASH_STR_SIZE];
} hash_t;

typedef struct
{
    char dir[3];
    char filename[39];
    char path[MAX_PATH_LENGTH];
} blob_path_t;

typedef struct
{
    hash_t hash;
    blob_path_t path;
    uint8_t *data;
    size_t size;
} blob_t;

typedef enum
{
    VCS_OK,
    VCS_ERROR_NO_HEAD,
    VCS_ERROR_INVALID_HEAD,
    VCS_ERROR_IO,
    VCS_ERROR_BRANCH_EXISTS,
    VCS_ERROR_INVALID_BRANCH,
    VCS_ERROR_CREATE_FAILED,
    VCS_ERROR_BRANCH_DOES_NOT_EXIST,
    VCS_ERROR_NO_LOGS,
    VCS_ERROR_NULL_INPUT,
    VCS_ERROR_MEMORY_ALLOCATION_FAILED,
    VCS_ERROR_FILE_DOES_NOT_EXIST,
    VCS_ERROR_FILE_TOO_LARGE,
    VCS_ERROR_BLOB_PATH_TOO_LONG,
    VCS_TOO_MANY_ARGUMENTS
} vcs_error_t;

typedef struct
{
    char hash[HASH_STR_SIZE];
    char filepath[MAX_PATH_LENGTH];
    UT_hash_handle hh;

} staged_file_t;

typedef struct
{
    char **paths;
    size_t count;
} path_list_t;

typedef enum
{
    FILE_TYPE_REGULAR,
    FILE_TYPE_DIRECTORY,
    FILE_TYPE_INVALID
} file_type_t;

void clear_terminal();
void vcs_init();
void vcs_status();
vcs_error_t vcs_add(char **paths, size_t count);
vcs_error_t create_branch(const char *name);
vcs_error_t checkout_branch(const char *name);

// utils
void trim(char *str);
void remove_newline(char *str);
void free_staged_files(staged_file_t **files_mp);
void free_path_list(path_list_t *list);
vcs_error_t initialize_master_branch();
vcs_error_t get_current_branch(char *buffer, size_t buffer_size);
vcs_error_t hash_to_hex(const unsigned char *hash, char *hex, size_t hex_size);
vcs_error_t hash_content(const uint8_t *data, size_t size, hash_t *hash);
vcs_error_t load_staged_files(staged_file_t **files);
vcs_error_t create_blob(const char *filepath, blob_t *blob);
vcs_error_t upsert_staged_file(staged_file_t **files, const char *hash, const char *filepath);
vcs_error_t read_file_data(const char *path, blob_t *blob);
vcs_error_t create_blob_path(const char *path, blob_t *blob);
vcs_error_t save_blob(const blob_t *blob);
vcs_error_t write_staged_file(staged_file_t **files_mp);
vcs_error_t add_paths_recursively(const char *path, path_list_t *result);
file_type_t get_file_type(const char *path);
bool is_valid_path(const char *path);
bool is_valid_branch_name(const char *name);
bool file_exists(const char *path);
const char *vcs_error_string(vcs_error_t err);

// temporary function to parse add arguments
vcs_error_t parse_add_args(const char *args, path_list_t *paths) 
{
    if (!args || !*args || !paths) return VCS_ERROR_NULL_INPUT;

    paths->paths = malloc(MAX_PATHS * sizeof(char *));

    if (!paths->paths) return VCS_ERROR_MEMORY_ALLOCATION_FAILED;

    paths->count = 0;

    char *input = strdup(args);
    if (!input) {
        return VCS_ERROR_MEMORY_ALLOCATION_FAILED;
    }

    remove_newline(input);
    char *token = strtok(input, " ");
    vcs_error_t err = VCS_OK;

    while (token != NULL && err == VCS_OK) {
        err = add_paths_recursively(token, paths);
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
            path_list_t paths = {0};
            if (fgets(line, sizeof(line), stdin))
            {
                vcs_error_t err = parse_add_args(line, &paths);
                if (err == VCS_OK)
                {
                    err = vcs_add(paths.paths, paths.count);
                    if (err != VCS_OK)
                    {
                        printf("Error: %s\n", vcs_error_string(err));
                    }
                }
                else
                {
                    printf("Error: %s\n", vcs_error_string(err));
                }
                free_path_list(&paths);
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

vcs_error_t vcs_add(char **paths, size_t count)
{
    if (!paths || count == 0)
    {
        return VCS_ERROR_NULL_INPUT;
    }

    staged_file_t *staged_files = NULL;

    vcs_error_t err = load_staged_files(&staged_files);

    if (err != VCS_OK)
    {
        return err;
    }

    for (size_t i = 0; i < count; i++)
    {
       blob_t blob;
        err = create_blob(paths[i], &blob);
        if (err != VCS_OK)
        {
            printf("Error: Could not create blob for '%s'\n", paths[i]);
            continue;
        }

        err = upsert_staged_file(&staged_files, blob.hash.hex, paths[i]);
        if (err != VCS_OK)
        {
            printf("Error: Could not add '%s' to staging area\n", paths[i]);
            free(blob.data);
            continue;
        }

        err = save_blob(&blob);
        if (err != VCS_OK)
        {
            printf("Error: Could not save blob for '%s'\n", paths[i]);
            free(blob.data);
            continue;
        }

        free(blob.data); 
    }

    err = write_staged_file(&staged_files);
    if (err != VCS_OK)
    {
        printf("Error: Could not write staging area\n");
    }

    if (staged_files)
    {
        free_staged_files(&staged_files);
        staged_files = NULL;
    }

    return err;
}

vcs_error_t add_paths_recursively(const char *path, path_list_t *result)
{
    if (!path || !result)
        return VCS_ERROR_NULL_INPUT;
    if (result->count >= MAX_PATHS)
        return VCS_TOO_MANY_ARGUMENTS;

    file_type_t type = get_file_type(path);
    if (type == FILE_TYPE_INVALID)
    {
        printf("Error: Invalid path '%s'\n", path);
        return VCS_ERROR_FILE_DOES_NOT_EXIST;
    }

    if (type == FILE_TYPE_REGULAR)
    {
        result->paths[result->count] = strdup(path);
        if (!result->paths[result->count])
            return VCS_ERROR_MEMORY_ALLOCATION_FAILED;
        result->count++;
        return VCS_OK;
    }

    // Handle directory
    DIR *dir = opendir(path);
    if (!dir)
        return VCS_ERROR_IO;

    struct dirent *entry;
    vcs_error_t err = VCS_OK;

    while ((entry = readdir(dir)) != NULL && result->count < MAX_PATHS)
    {
        if (strcmp(entry->d_name, ".") == 0 ||
            strcmp(entry->d_name, "..") == 0 ||
            strcmp(entry->d_name, ".vcs") == 0)
        {
            continue;
        }

        char full_path[MAX_PATH_LENGTH];
        snprintf(full_path, sizeof(full_path), "%s/%s", path, entry->d_name);

        err = add_paths_recursively(full_path, result);
        if (err != VCS_OK && err != VCS_TOO_MANY_ARGUMENTS)
        {
            closedir(dir);
            return err;
        }
    }

    closedir(dir);
    return err;
}

void free_path_list(path_list_t *list)
{
    if (!list)
    {
        return;
    }
    for (size_t i = 0; i < list->count; i++)
    {
        free(list->paths[i]);
    }
    free(list->paths);
    list->count = 0;
}

vcs_error_t write_staged_file(staged_file_t **files_mp)
{
    if (!files_mp)
    {
        return VCS_ERROR_NULL_INPUT;
    }

    FILE *index = fopen(STAGED_PATH, "w");
    if (!index)
    {
        return VCS_ERROR_IO;
    }

    staged_file_t *file, *tmp;
    HASH_ITER(hh, *files_mp, file, tmp)
    {
        fprintf(index, "%s %s\n", file->hash, file->filepath);
    }

    fclose(index);
    return VCS_OK;
}

void free_staged_files(staged_file_t **files_mp)
{
    if (!files_mp || !*files_mp)
        return;

    staged_file_t *current, *tmp;
    HASH_ITER(hh, *files_mp, current, tmp)
    {
        HASH_DEL(*files_mp, current);
        free(current);
    }
    *files_mp = NULL;
}

vcs_error_t save_blob(const blob_t *blob) {
    if (!blob || !blob->data) {
        return VCS_ERROR_NULL_INPUT;
    }

    // Create directory if it doesn't exist
    char dir_path[MAX_PATH_LENGTH];
    snprintf(dir_path, sizeof(dir_path), ".vcs/objects/%s", blob->path.dir);
    
    if (mkdir(dir_path, 0777) < 0 && errno != EEXIST) {
        return VCS_ERROR_IO;
    }

    FILE *file = fopen(blob->path.path, "wb");
    if (!file) {
        return VCS_ERROR_IO;
    }

    // Format: "blob <size>\0<data>"
    size_t header_size = snprintf(NULL, 0, "blob %zu/", blob->size);
    char *header = malloc(header_size + 1);
    if (!header) {
        fclose(file);
        return VCS_ERROR_MEMORY_ALLOCATION_FAILED;
    }

    snprintf(header, header_size + 1, "blob %zu", blob->size);
    
    // Write header with null byte
    if (fwrite(header, 1, header_size, file) != header_size) {
        free(header);
        fclose(file);
        return VCS_ERROR_IO;
    }
    free(header);

    // Write data
    if (fwrite(blob->data, 1, blob->size, file) != blob->size) {
        fclose(file);
        return VCS_ERROR_IO;
    }

    fclose(file);
    return VCS_OK;
}

vcs_error_t create_blob(const char *filepath, blob_t *blob)
{
    vcs_error_t err = read_file_data(filepath, blob);
    if (err != VCS_OK)
    {
        return err;
    }

    err = hash_content(blob->data, blob->size, &blob->hash);
    if (err != VCS_OK)
    {
        free(blob->data);
        return err;
    }

    err = create_blob_path(filepath, blob);
    if (err != VCS_OK)
    {
        free(blob->data);
        return err;
    }

    return VCS_OK;
}

vcs_error_t create_blob_path(const char *filepath, blob_t *blob)
{
    if (!filepath || !blob || blob->hash.hex[0] == '\0')
    {
        return VCS_ERROR_NULL_INPUT;
    }

    blob_path_t *blob_path = &blob->path;

    blob_path->dir[0] = blob->hash.hex[0];
    blob_path->dir[1] = blob->hash.hex[1];
    blob_path->dir[2] = '\0';

    strncpy(blob_path->filename, blob->hash.hex + 2, 38); 
    blob_path->filename[38] = '\0';

    if (snprintf(blob_path->path, MAX_PATH_LENGTH, BLOB_PATH_FMT,
                 blob_path->dir, blob_path->filename) >= MAX_PATH_LENGTH)
    {
        return VCS_ERROR_BLOB_PATH_TOO_LONG;
    }

    return VCS_OK;
}

vcs_error_t read_file_data(const char *path, blob_t *blob) {
    if (!path || !blob) {
        return VCS_ERROR_NULL_INPUT;
    }

    if (!file_exists(path)) {
        return VCS_ERROR_FILE_DOES_NOT_EXIST;
    }

    FILE *file = fopen(path, "rb");
    if (!file) {
        return VCS_ERROR_IO;
    }

    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    rewind(file);

    if (file_size > MAX_FILE_SIZE) {
        fclose(file);
        return VCS_ERROR_FILE_TOO_LARGE;
    }

    uint8_t *buffer = malloc(file_size);
    if (!buffer) {
        fclose(file);
        return VCS_ERROR_MEMORY_ALLOCATION_FAILED;
    }

    size_t bytes_read = fread(buffer, 1, file_size, file);
    if (bytes_read != file_size) {
        free(buffer);
        fclose(file);
        return VCS_ERROR_IO;
    }

    blob->data = buffer;
    blob->size = file_size;
    
    fclose(file);
    return VCS_OK;
}

vcs_error_t upsert_staged_file(staged_file_t **files, const char *hash, const char *filepath)
{
    if (!files || !hash || !filepath)
    {
        return VCS_ERROR_NULL_INPUT;
    }

    staged_file_t *staged = malloc(sizeof(staged_file_t));
    if (!staged)
    {
        return VCS_ERROR_MEMORY_ALLOCATION_FAILED;
    }

    strncpy(staged->hash, hash, HASH_STR_SIZE - 1);
    staged->hash[HASH_STR_SIZE - 1] = '\0';
    strncpy(staged->filepath, filepath, MAX_PATH_LENGTH - 1);
    staged->filepath[MAX_PATH_LENGTH - 1] = '\0';

    staged_file_t *existing;
    HASH_REPLACE_STR(*files, filepath, staged, existing);
    if (existing)
    {
        free(existing);
    }

    return VCS_OK;
}



vcs_error_t load_staged_files(staged_file_t **files)
{
    if (!files)
    {
        return VCS_ERROR_NULL_INPUT;
    }

    *files = NULL; // Initialize hash table

    FILE *index = fopen(STAGED_PATH, "r");
    if (!index)
    {
        return VCS_OK; // Empty staging area is valid
    }

    char line[MAX_PATH_LENGTH + HASH_STR_SIZE + 2]; // +2 for space and newline
    while (fgets(line, sizeof(line), index))
    {
        // Remove newline
        line[strcspn(line, "\n")] = 0;

        // Parse hash and filepath
        char hash[HASH_STR_SIZE];
        char filepath[MAX_PATH_LENGTH];

        if (sscanf(line, "%40s %s", hash, filepath) == 2)
        {
            staged_file_t *staged = malloc(sizeof(staged_file_t));
            if (!staged)
            {
                fclose(index);
                return VCS_ERROR_IO;
            }

            strncpy(staged->hash, hash, HASH_STR_SIZE - 1);
            staged->hash[HASH_STR_SIZE - 1] = '\0';

            strncpy(staged->filepath, filepath, MAX_PATH_LENGTH - 1);
            staged->filepath[MAX_PATH_LENGTH - 1] = '\0';

            HASH_ADD_STR(*files, filepath, staged);
        }
    }

    fclose(index);
    return VCS_OK;
}


bool is_valid_path(const char *path)
{
    struct stat st;
    if (stat(path, &st) != 0)
    {
        return false;
    }
    return S_ISREG(st.st_mode) || S_ISDIR(st.st_mode);
}

vcs_error_t hash_content(const uint8_t *data, size_t size, hash_t *hash)
{
    if (!data || !hash)
    {
        return VCS_ERROR_NULL_INPUT;
    }

    memset(hash->raw, 0, HASH_SIZE);

    uint32_t h = 2166136261u;
    for (size_t i = 0; i < size; i++)
    {
        h ^= data[i];
        h *= 16777619;
        hash->raw[i % HASH_SIZE] ^= h;
    }

    return hash_to_hex(hash->raw, hash->hex, HASH_STR_SIZE);
}

vcs_error_t hash_to_hex(const unsigned char *hash, char *hex, size_t hex_size)
{
    if (!hash || !hex || hex_size < HASH_STR_SIZE)
    {
        return VCS_ERROR_NULL_INPUT;
    }

    for (int i = 0; i < HASH_SIZE; i++)
    {
        snprintf(hex + (i * 2), 3, "%02x", hash[i]);
    }

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

const char *vcs_error_string(vcs_error_t err)
{
    switch (err)
    {
    case VCS_OK:
        return "Success";
    case VCS_ERROR_NO_HEAD:
        return "HEAD file not found";
    case VCS_ERROR_INVALID_HEAD:
        return "Invalid HEAD file format";
    case VCS_ERROR_IO:
        return "I/O error";
    case VCS_ERROR_BRANCH_EXISTS:
        return "Branch already exists";
    case VCS_ERROR_INVALID_BRANCH:
        return "Invalid branch name";
    case VCS_ERROR_CREATE_FAILED:
        return "Failed to create branch";
    case VCS_ERROR_BRANCH_DOES_NOT_EXIST:
        return "Branch does not exist";
    case VCS_ERROR_NO_LOGS:
        return "No logs found";
    case VCS_ERROR_NULL_INPUT:
        return "Null input";
    case VCS_ERROR_MEMORY_ALLOCATION_FAILED:
        return "Memory allocation failed";
    case VCS_ERROR_FILE_DOES_NOT_EXIST:
        return "File does not exist";
    case VCS_ERROR_FILE_TOO_LARGE:
        return "File is too large";
    case VCS_ERROR_BLOB_PATH_TOO_LONG:
        return "Blob path is too long";
    case VCS_TOO_MANY_ARGUMENTS:
        return "Too many arguments";
    default:
        return "Unknown error";
    }
}

bool file_exists(const char *path)
{
    struct stat st;
    return stat(path, &st) == 0;
}

void remove_newline(char *str)
{
    if (!str)
    {
        return;
    }

    size_t len = strlen(str);
    if (len > 0 && str[len - 1] == '\n')
    {
        str[len - 1] = '\0';
    }
}

void trim(char *str)
{
    if (!str)
        return;

    // Trim leading space
    char *start = str;
    while (*start && isspace(*start))
        start++;

    if (start != str)
    {
        memmove(str, start, strlen(start) + 1);
    }

    // Trim trailing space
    char *end = str + strlen(str) - 1;
    while (end > str && isspace(*end))
        end--;
    *(end + 1) = '\0';
}

file_type_t get_file_type(const char *path)
{
    struct stat st;
    if (stat(path, &st) != 0)
        return FILE_TYPE_INVALID;
    if (S_ISREG(st.st_mode))
        return FILE_TYPE_REGULAR;
    if (S_ISDIR(st.st_mode))
        return FILE_TYPE_DIRECTORY;
    return FILE_TYPE_INVALID;
}


void clear_terminal()
{
// Use system commands to clear the terminal
#ifdef _WIN32
    system("cls"); // Windows
#else
    system("clear"); // Unix/Linux/Mac
#endif
}