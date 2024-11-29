#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>
#include <stdbool.h>
#include <ctype.h>

#define MAX_BRANCH_NAME 100
#define NULL_COMMIT "0000000000000000000000000000000000000000"
#define BRANCH_FMT ".vcs/refs/heads/%s"
#define HEAD_PATH ".vcs/HEAD"

typedef enum
{
    VCS_OK,
    VCS_ERROR_NO_HEAD,
    VCS_ERROR_INVALID_HEAD,
    VCS_ERROR_IO,
    VCS_ERROR_BRANCH_EXISTS,
    VCS_ERROR_INVALID_BRANCH,
    VCS_ERROR_CREATE_FAILED,
    VCS_ERROR_BRANCH_DOES_NOT_EXIST
} vcs_error_t;

void clear_terminal();
void vcs_init();
vcs_error_t create_branch(const char *name);
vcs_error_t checkout_branch(const char *name);
void vcs_status();

// utils
vcs_error_t initialize_master_branch();
bool is_valid_branch_name(const char *name);
void trim(char *str);
bool file_exists(const char *path);
vcs_error_t get_current_branch(char *buffer, size_t buffer_size);
const char* vcs_error_string(vcs_error_t err);


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
            printf("added\n");
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
    default:
        return "Unknown error";
    }
}

bool file_exists(const char *path)
{
    struct stat st;
    return stat(path, &st) == 0;
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

void clear_terminal()
{
// Use system commands to clear the terminal
#ifdef _WIN32
    system("cls"); // Windows
#else
    system("clear"); // Unix/Linux/Mac
#endif
}