#include "repository.h"
#include "object.h"
#include "staging.h"
#include "config.h"

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <dirent.h>
#include <utarray.h>
#include <uthash.h>

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
    char recent_commit[HEX_SIZE];
    int initialized;
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

static int init_repository_dirs(const char *vcsdir)
{
    // Create main .vcs dir
    if (create_directory(vcsdir) != 0)
    {
        return -1;
    }

    // Create subdirectories
    char path[VCS_PATH_MAX];
    for (const char **dir = REPO_DIRS; *dir; dir++)
    {
        snprintf(path, sizeof(path), "%s/%s", vcsdir, *dir);
        if (create_directory(path) != 0)
        {
            return -1;
        }
    }
    return 0;
}

static int init_head_file(const char *vcsdir)
{
    char head_path[VCS_PATH_MAX];
    snprintf(head_path, sizeof(head_path), "%s/%s", vcsdir, HEAD_FILE);

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

static repository_t *repository_alloc(const char *path)
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

    return repo;
}

static int init_master_branch(const char *vcsdir)
{
    char master_path[VCS_PATH_MAX];
    snprintf(master_path, sizeof(master_path), "%s/refs/heads/master", vcsdir);

    FILE *fp = fopen(master_path, "w");
    if (!fp)
    {
        fprintf(stderr, "Error creating master branch: %s\n", strerror(errno));
        return -1;
    }
    fclose(fp);
    return 0;
}

repository_t *repository_init()
{
    repository_t *repo = repository_alloc(".");
    if (!repo)
        return NULL;

    // Check if .vcs already exists
    struct stat st;
    if (stat(repo->vcsdir, &st) == 0)
    {
        fprintf(stderr, "Repository already exists at '%s'\n", repo->vcsdir);
        repository_free(repo);
        return NULL;
    }

    // Initialize repository directories
    if (init_repository_dirs(repo->vcsdir) != 0)
    {
        repository_free(repo);
        return NULL;
    }

    // Initialize HEAD file
    if (init_head_file(repo->vcsdir) != 0)
    {
        repository_free(repo);
        return NULL;
    }

    // Initialize master branch
    if (init_master_branch(repo->vcsdir) != 0)
    {
        repository_free(repo);
        return NULL;
    }

    // Set HEAD and branch name
    strncpy(repo->head_ref, HEAD_REF, VCS_NAME_MAX - 1);
    strncpy(repo->branch_name, "master", VCS_NAME_MAX - 1);
    memset(repo->recent_commit, 0, HEX_SIZE);

    printf("Initialized empty repository in %s\n", repo->vcsdir);
    return repo;
}

static int load_latest_commit(repository_t *repo) {
    char branch_path[VCS_PATH_MAX];
    snprintf(branch_path, sizeof(branch_path), "%s/refs/heads/%s",
             repo->vcsdir, repo->branch_name);

    FILE *fp = fopen(branch_path, "r");
    if (!fp) {
        // No commits yet - no file
        memset(repo->recent_commit, 0, HEX_SIZE);
        return 0;
    }

    // Check if file is empty
    if (fgets(repo->recent_commit, HEX_SIZE, fp) == NULL || 
        repo->recent_commit[0] == '\0') {
        // No commits yet - empty file
        memset(repo->recent_commit, 0, HEX_SIZE);
        fclose(fp);
        return 0;
    }

    // Remove newline
    repo->recent_commit[strcspn(repo->recent_commit, "\n")] = 0;
    fclose(fp);
    return 0;
}

repository_t *repository_open()
{
    repository_t *repo = repository_alloc(".");
    if (!repo)
        return NULL;

    // Check if .vcs exists
    struct stat st;
    if (stat(repo->vcsdir, &st) != 0 || !S_ISDIR(st.st_mode))
    {
        fprintf(stderr, "Error: No repository found at '%s'\n", repo->vcsdir);
        repository_free(repo);
        return NULL;
    }

    // Load HEAD reference
    char head_path[VCS_PATH_MAX];
    snprintf(head_path, sizeof(head_path), "%s/%s", repo->vcsdir, HEAD_FILE);
    FILE *fp = fopen(head_path, "r");
    if (!fp)
    {
        fprintf(stderr, "Error opening HEAD file: %s\n", strerror(errno));
        repository_free(repo);
        return NULL;
    }

    if (!fgets(repo->head_ref, VCS_NAME_MAX, fp))
    {
        fprintf(stderr, "Error reading HEAD file: %s\n", strerror(errno));
        fclose(fp);
        repository_free(repo);
        return NULL;
    }
    fclose(fp);

    // Remove newline character from head_ref
    repo->head_ref[strcspn(repo->head_ref, "\n")] = 0;

    // Set branch name
    if (strncmp(repo->head_ref, "ref: refs/heads/", 16) == 0)
    {
        strncpy(repo->branch_name, repo->head_ref + 16, VCS_NAME_MAX - 1);
    }
    else
    {
        strncpy(repo->branch_name, "detached", VCS_NAME_MAX - 1);
    }

    // Load latest commit hash
    if (load_latest_commit(repo) != 0)
    {
        fprintf(stderr, "Error loading latest commit\n");
        repository_free(repo);
        return NULL;
    }
    printf("Opened repository at %s\n", repo->vcsdir);
    repo->initialized = 1;
    return repo;
}

void repository_free(repository_t *repo)
{
    if (repo)
    {
        free(repo);
    }
}

static int is_directory(const char *path)
{
    struct stat st;
    if (stat(path, &st) != 0)
    {
        return 0;
    }
    return S_ISDIR(st.st_mode);
}

static int is_file(const char *path)
{
    struct stat st;
    if (stat(path, &st) != 0)
    {
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
    if (!dir)
    {
        fprintf(stderr, "Error: Failed to open directory '%s': %s\n", dirpath, strerror(errno));
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL)
    {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
        {
            continue;
        }

        char path[VCS_PATH_MAX];
        snprintf(path, sizeof(path), "%s/%s", dirpath, entry->d_name);
        if (is_directory(path))
        {
            add_directory(arr, path);
        }
        else if (is_file(path))
        {
            add_file(arr, path);
        }
    }

    closedir(dir);
}

static int parse_file_args(int argc, char **argv, UT_array *arr)
{
    for (int i = 0; i < argc; i++)
    {
        if (is_directory(argv[i]))
        {
            add_directory(arr, argv[i]);
        }
        else if (is_file(argv[i]))
        {
            add_file(arr, argv[i]);
        }
        else
        {
            fprintf(stderr, "Error: '%s' is not a valid file or directory\n", argv[i]);
            return -1;
        }
    }

    return 0;
}

int repository_add(repository_t *repo, int size, char **files)
{
    index_t *index = index_init(repo->index_path);
    UT_array *arr;
    utarray_new(arr, &ut_str_icd);

    if (parse_file_args(size, files, arr) != 0)
    {
        utarray_free(arr);
        return -1;
    }

    if (index_stage_files(index, arr) != 0)
    {
        utarray_free(arr);
        return -1;
    }

    utarray_free(arr);
    index_free(index);
    return 0;
}

static int update_branch_ref(repository_t *repo, const char *branch, const char *commit_hash)
{
    char branch_path[VCS_PATH_MAX];
    snprintf(branch_path, sizeof(branch_path), "%s/refs/heads/%s", repo->vcsdir, branch);

    FILE *fp = fopen(branch_path, "w");
    if (!fp)
    {
        fprintf(stderr, "Error opening branch file: %s\n", strerror(errno));
        return -1;
    }

    if (fprintf(fp, "%s\n", commit_hash) < 0)
    {
        fprintf(stderr, "Error writing to branch file: %s\n", strerror(errno));
        fclose(fp);
        return -1;
    }

    strcpy(repo->branch_name, branch);

    fclose(fp);
    return 0;
}

static int update_head(repository_t *repo, const char *branch)
{
    char head_path[VCS_PATH_MAX];
    snprintf(head_path, sizeof(head_path), "%s/%s", repo->vcsdir, HEAD_FILE);

    FILE *fp = fopen(head_path, "w");
    if (!fp)
    {
        fprintf(stderr, "Error opening HEAD file: %s\n", strerror(errno));
        return -1;
    }

    // Store symbolic ref
    if (fprintf(fp, "ref: refs/heads/%s\n", branch) < 0)
    {
        fprintf(stderr, "Error writing to HEAD file: %s\n", strerror(errno));
        fclose(fp);
        return -1;
    }

    fclose(fp);
    return 0;
}
static int reset_index(repository_t *repo)
{
    if (unlink(repo->index_path) != 0)
    {
        fprintf(stderr, "Error: Failed to delete index file '%s': %s\n", repo->index_path, strerror(errno));
        return -1;
    }
    return 0;
}

static int write_tree(repository_t *repo, index_t *index, char *out_tree_hash)
{
    object_t *tree = object_init(OBJ_TREE);
    if (!tree)
    {
        fprintf(stderr, "Error: Failed to initialize tree object\n");
        return -1;
    }

    object_update_t data = {
        .tree = {.index = index}};

    if (object_update(tree, data) != 0)
    {
        fprintf(stderr, "Error: Failed to update object\n");
        object_free(tree);
        return -1;
    }

    if (object_write(tree, out_tree_hash) != 0)
    {
        fprintf(stderr, "Error: Failed to write object\n");
        object_free(tree);
        return -1;
    }

    return 0;
}

static const char *DEFAULT_AUTHOR_NAME = "Unknown Author";
static const char *DEFAULT_AUTHOR_EMAIL = "unknown@localhost";

static int read_config_value(const char *key, char *out_value, size_t max_len)
{
    char config_path[PATH_MAX];
    snprintf(config_path, sizeof(config_path), ".vcs/config");

    FILE *fp = fopen(config_path, "r");
    if (!fp)
    {
        return -1;
    }

    char line[256];
    char k[128], v[128];
    while (fgets(line, sizeof(line), fp))
    {
        if (sscanf(line, "%[^=]=%s", k, v) == 2)
        {
            if (strcmp(k, key) == 0)
            {
                strncpy(out_value, v, max_len);
                fclose(fp);
                return 0;
            }
        }
    }
    fclose(fp);
    return -1;
}

static int get_user_info(const char *name_var, const char *email_var,
                         char *name_out, char *email_out)
{
    // Try environment variables first
    const char *name = getenv(name_var);
    const char *email = getenv(email_var);

    if (name && email)
    {
        strncpy(name_out, name, 100);
        strncpy(email_out, email, 100);
        return 0;
    }

    // Try config file next
    char config_name[100], config_email[100];
    if (!name && read_config_value(name_var, config_name, sizeof(config_name)) == 0)
    {
        name = config_name;
    }
    if (!email && read_config_value(email_var, config_email, sizeof(config_email)) == 0)
    {
        email = config_email;
    }

    // Use defaults as last resort
    strncpy(name_out, name ? name : DEFAULT_AUTHOR_NAME, 100);
    strncpy(email_out, email ? email : DEFAULT_AUTHOR_EMAIL, 100);

    return 0;
}
static int write_commit(repository_t *repo, const char *tree_hash, const char *message, char *out_commit_hash)
{
    char author_name[100], author_email[100];
    char committer_name[100], committer_email[100];
    time_t now = time(NULL);

    if (get_user_info("VCS_AUTHOR_NAME", "VCS_AUTHOR_EMAIL",
                      author_name, author_email) != 0)
    {
        fprintf(stderr, "Error: Author info not configured\n");
        return -1;
    }

    if (get_user_info("VCS_COMMITTER_NAME", "VCS_COMMITTER_EMAIL",
                      committer_name, committer_email) != 0)
    {
        // Fall back to author
        strcpy(committer_name, author_name);
        strcpy(committer_email, author_email);
    }

    object_update_t data = {
        .commit = {
            .tree_hash = tree_hash,
            .parent_hash = repo->recent_commit,
            .message = message,
            .author_name = author_name,
            .author_email = author_email,
            .author_time = now,
            .committer_name = committer_name,
            .committer_email = committer_email,
            .committer_time = now}};

    object_t *commit = object_init(OBJ_COMMIT);
    if (!commit)
    {
        fprintf(stderr, "Error: Failed to initialize commit object\n");
        return -1;
    }

    if (object_update(commit, data) != 0)
    {
        fprintf(stderr, "Error: Failed to update object\n");
        object_free(commit);
        return -1;
    }

    if (object_write(commit, out_commit_hash) != 0)
    {
        fprintf(stderr, "Error: Failed to write object\n");
        object_free(commit);
        return -1;
    }

    return 0;
}

int repository_commit(repository_t *repo, const char *message)
{

    index_t *index = index_init(repo->index_path);
    if (!index)
    {
        fprintf(stderr, "Error: Failed to load index\n");
        return -1;
    }

    char tree_hash[HEX_SIZE];
    if (write_tree(repo, index, tree_hash) != 0)
    {
        fprintf(stderr, "Error: Failed to write tree object\n");
        return -1;
    }

    char commit_hash[HEX_SIZE];
    if (write_commit(repo, tree_hash, message, commit_hash) != 0)
    {
        fprintf(stderr, "Error: Failed to write commit object\n");
        return -1;
    }

    if (update_branch_ref(repo, repo->branch_name, commit_hash) != 0)
    {
        fprintf(stderr, "Error: Failed to update branch ref\n");
        return -1;
    }

    printf("Committed %d files\n", index->header.entry_count);

    if (reset_index(repo) != 0)
    {
        fprintf(stderr, "Error: Failed to reset index\n");
        return -1;
    }

    index_free(index);

    return 0;
}


int repository_status(repository_t *repo) {

    return 0;
}