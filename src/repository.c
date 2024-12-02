#include "repository.h"
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <dirent.h>
#include <utarray.h>

#define VCS_DIR ".vcs"
#define HEAD_FILE "HEAD"
#define HEAD_REF "refs/heads/master"
#define VCS_PATH_MAX 4096
#define VCS_NAME_MAX 256

struct repository {
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
    NULL
};

static int create_directory(const char *dir) {
    struct stat st = {0};
    if (stat(dir, &st) == -1) {
        if (mkdir(dir, 0755) != 0) {
            fprintf(stderr, "Error creating directory %s: %s\n", dir, strerror(errno));
            return -1;
        }
    }
    return 0;
}

static int init_repository_dirs(const char *vcsdir) {
    // Create main .vcs dir
    if (create_directory(vcsdir) != 0) {
        return -1;
    }

    // Create subdirectories
    char path[VCS_PATH_MAX];
    for (const char **dir = REPO_DIRS; *dir; dir++) {
        snprintf(path, sizeof(path), "%s/%s", vcsdir, *dir);
        if (create_directory(path) != 0) {
            return -1;
        }
    }
    return 0;
}

static int init_head_file(const char *vcsdir) {
    char head_path[VCS_PATH_MAX];
    snprintf(head_path, sizeof(head_path), "%s/%s", vcsdir, HEAD_FILE);

    FILE *fp = fopen(head_path, "w");
    if (!fp) {
        fprintf(stderr, "Error creating HEAD file: %s\n", strerror(errno));
        return -1;
    }

    if (fprintf(fp, "ref: %s\n", HEAD_REF) < 0) {
        fprintf(stderr, "Error writing to HEAD file: %s\n", strerror(errno));
        fclose(fp);
        return -1;
    }

    fclose(fp);
    return 0;
}

static repository_t *repository_alloc(const char *path) {
    repository_t *repo = malloc(sizeof(repository_t));
    if (!repo) return NULL;

    // Setup paths
    if (!realpath(path, repo->worktree)) {
        fprintf(stderr, "Error resolving path '%s': %s\n", path, strerror(errno));
        free(repo);
        return NULL;
    }
    snprintf(repo->vcsdir, VCS_PATH_MAX, "%s/%s", repo->worktree, VCS_DIR);
    snprintf(repo->index_path, VCS_PATH_MAX, "%s/index", repo->vcsdir);

    return repo;
}

repository_t *repository_init() {
    repository_t *repo = repository_alloc(".");
    if (!repo) return NULL;

    // Check if .vcs already exists
    struct stat st;
    if (stat(repo->vcsdir, &st) == 0) {
        fprintf(stderr, "Repository already exists at '%s'\n", repo->vcsdir);
        repository_free(repo);
        return NULL;
    }

    // Initialize repository directories
    if (init_repository_dirs(repo->vcsdir) != 0) {
        repository_free(repo);
        return NULL;
    }

    // Initialize HEAD file
    if (init_head_file(repo->vcsdir) != 0) {
        repository_free(repo);
        return NULL;
    }

    // Set HEAD and branch name
    strncpy(repo->head_ref, HEAD_REF, VCS_NAME_MAX - 1);
    strncpy(repo->branch_name, "master", VCS_NAME_MAX - 1);

    printf("Initialized empty repository in %s\n", repo->vcsdir);
    return repo;
}

repository_t *repository_open() {
    repository_t *repo = repository_alloc(".");
    if (!repo) return NULL;

    // Check if .vcs exists
    struct stat st;
    if (stat(repo->vcsdir, &st) != 0 || !S_ISDIR(st.st_mode)) {
        fprintf(stderr, "Error: No repository found at '%s'\n", repo->vcsdir);
        repository_free(repo);
        return NULL;
    }

    // Load HEAD reference
    char head_path[VCS_PATH_MAX];
    snprintf(head_path, sizeof(head_path), "%s/%s", repo->vcsdir, HEAD_FILE);
    FILE *fp = fopen(head_path, "r");
    if (!fp) {
        fprintf(stderr, "Error opening HEAD file: %s\n", strerror(errno));
        repository_free(repo);
        return NULL;
    }

    if (!fgets(repo->head_ref, VCS_NAME_MAX, fp)) {
        fprintf(stderr, "Error reading HEAD file: %s\n", strerror(errno));
        fclose(fp);
        repository_free(repo);
        return NULL;
    }
    fclose(fp);

    // Remove newline character from head_ref
    repo->head_ref[strcspn(repo->head_ref, "\n")] = 0;

    // Set branch name
    if (strncmp(repo->head_ref, "ref: refs/heads/", 16) == 0) {
        strncpy(repo->branch_name, repo->head_ref + 16, VCS_NAME_MAX - 1);
    } else {
        strncpy(repo->branch_name, "detached", VCS_NAME_MAX - 1);
    }

    return repo;
}

void repository_free(repository_t *repo) {
    if (repo) {
        free(repo);
    }
}

static int is_directory(const char *path) {
    struct stat st;
    if (stat(path, &st) != 0) {
        return 0;
    }
    return S_ISDIR(st.st_mode);
}

static int is_file(const char *path) {
    struct stat st;
    if (stat(path, &st) != 0) {
        return 0;
    }
    return S_ISREG(st.st_mode);
}

static void add_file(UT_array *arr, const char *filepath)
{
    utarray_push_back(arr, &filepath);
}

static void add_directory(UT_array *arr, const char *dirpath)
{
    DIR *dir = opendir(dirpath);
    if (!dir) {
        fprintf(stderr, "Error: Failed to open directory '%s': %s\n", dirpath, strerror(errno));
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        char path[VCS_PATH_MAX];
        snprintf(path, sizeof(path), "%s/%s", dirpath, entry->d_name);
        if (is_directory(path)) {
            add_directory(arr, path);
        } else if (is_file(path)) {
            add_file(arr, path);
        }
    }

    closedir(dir);
}

int parse_file_args(int argc, char **argv, UT_array *arr) {
    for (int i = 2; i < argc; i++) {
        if (is_directory(argv[i])) {
            add_directory(arr, argv[i]);
        } else if (is_file(argv[i])) {
            add_file(arr, argv[i]);
        } else {
            fprintf(stderr, "Error: '%s' is not a valid file or directory\n", argv[i]);
            return -1;
        }
    }

    return 0;
}

int repository_add(repository_t *repo, int size, char **files) {
    UT_array *arr;
    utarray_new(arr, &ut_str_icd);

    if (parse_file_args(size, files, arr) != 0) {
        utarray_free(arr);
        return -1;
    }

    char **p = NULL;
    while ((p = (char **)utarray_next(arr, p)))
    {
        printf("%s\n", *p);
    }

    utarray_free(arr);
    return 0;
}