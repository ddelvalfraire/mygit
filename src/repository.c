#include "repository.h"
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>

#define VCS_DIR ".vcs"
#define HEAD_FILE "HEAD"
#define HEAD_REF "refs/heads/master"
#define VCS_PATH_MAX 4096
#define VCS_NAME_MAX 256

struct repository
{
    char worktree[VCS_PATH_MAX];
    char vcsdir[VCS_PATH_MAX];
    char index_path[VCS_PATH_MAX];
    char head_ref[VCS_NAME_MAX];
    char branch_name[VCS_NAME_MAX];
};

static const char *REPO_DIRS[] = {
    "objects",
    "refs",
    "refs/heads",
    "refs/tags",
    NULL};

static int create_directory(const char *dir)
{
    struct stat st = {0};
    if (stat(dir, &st) == -1)
    {
        if (mkdir(dir, 0755) != 0)
        {
            fprintf(stderr, "Error creating directory %s: %s\n", dir, strerror(errno));
            return -1;
        }
    }
    return 0;
}

static int init_repository_dirs(void)
{
    // Create main .vcs dir
    if (create_directory(VCS_DIR) != 0)
    {
        return -1;
    }

    // Create subdirectories
    char path[VCS_PATH_MAX];
    for (const char **dir = REPO_DIRS; *dir; dir++)
    {
        snprintf(path, sizeof(path), "%s/%s", VCS_DIR, *dir);
        if (create_directory(path) != 0)
        {
            return -1;
        }
    }
    return 0;
}

static int init_head_file(void)
{
    char head_path[VCS_PATH_MAX];
    snprintf(head_path, sizeof(head_path), "%s/%s", VCS_DIR, HEAD_FILE);

    FILE *fp = fopen(head_path, "w");
    if (!fp)
    {
        fprintf(stderr, "Error creating HEAD file: %s\n", strerror(errno));
        return -1;
    }

    if (fprintf(fp, "ref: %s\n", HEAD_REF) < 0)
    {
        fprintf(stderr, "Error writing to HEAD file: %s\n", strerror(errno));
        fclose(fp);
        return -1;
    }

    fclose(fp);
    return 0;
}

repository_t *repository_init(const char *path)
{
    repository_t *repo = malloc(sizeof(repository_t));
    if (!repo)
        return NULL;

    // Setup paths
    if (!realpath(path, repo->worktree))
    {
        fprintf(stderr, "Error resolving path '%s': %s\n", path, strerror(errno));
        free(repo);
        return NULL;
    }
    snprintf(repo->vcsdir, VCS_PATH_MAX, "%s/%s", repo->worktree, VCS_DIR);
    snprintf(repo->index_path, VCS_PATH_MAX, "%s/index", repo->vcsdir);

    // Check if .vcs already exists
    struct stat st;
    if (stat(repo->vcsdir, &st) == 0)
    {
        return repo; // Already initialized
    }

    // Initialize repository directories
    if (init_repository_dirs() != 0)
    {
        free(repo);
        return NULL;
    }

    // Initialize HEAD file
    if (init_head_file() != 0)
    {
        free(repo);
        return NULL;
    }

    // Set HEAD and branch name
    strncpy(repo->head_ref, HEAD_REF, VCS_NAME_MAX - 1);
    strncpy(repo->branch_name, "master", VCS_NAME_MAX - 1);

    printf("Initialized empty repository in %s\n", repo->vcsdir);
    return repo;
}

void repository_free(repository_t *repo)
{
    if (repo)
    {
        free(repo);
    }
}