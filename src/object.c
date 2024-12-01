#include <memory.h>
#include <sys/stat.h>
#include <stdio.h>

#include "object.h"
#include "hash.h"
#include "io.h"

vcs_error_t object_init(vcs_object_t *obj, object_type_t type)
{
    if (!obj)
        return VCS_ERROR_NULL_INPUT;

    memset(obj, 0, sizeof(vcs_object_t));
    obj->type = type;

    switch (type)
    {
    case OBJECT_TYPE_BLOB:
    {
        blob_object_t *blob = malloc(sizeof(blob_object_t));
        if (!blob)
            return VCS_ERROR_MEMORY_ALLOCATION_FAILED;
        memset(blob, 0, sizeof(blob_object_t));
        blob->header = *obj;
        obj->data = blob;
        break;
    }
    case OBJECT_TYPE_TREE:
    {
        tree_object_t *tree = malloc(sizeof(tree_object_t));
        if (!tree)
            return VCS_ERROR_MEMORY_ALLOCATION_FAILED;
        memset(tree, 0, sizeof(tree_object_t));
        tree->header = *obj;
        obj->data = tree;
        break;
    }
    case OBJECT_TYPE_COMMIT:
    {
        commit_object_t *commit = malloc(sizeof(commit_object_t));
        if (!commit)
            return VCS_ERROR_MEMORY_ALLOCATION_FAILED;
        memset(commit, 0, sizeof(commit_object_t));
        commit->header = *obj;
        obj->data = commit;
        break;
    }
    case OBJECT_TYPE_TAG:
    {
        tag_object_t *tag = malloc(sizeof(tag_object_t));
        if (!tag)
            return VCS_ERROR_MEMORY_ALLOCATION_FAILED;
        memset(tag, 0, sizeof(tag_object_t));
        tag->header = *obj;
        obj->data = tag;
        break;
    }
    default:
        return VCS_ERROR_INVALID_OBJECT_TYPE;
    }

    return VCS_OK;
}

vcs_error_t create_object_path(const unsigned char *hash, object_path_t *path)
{
    if (!hash || !path)
    {
        return VCS_ERROR_NULL_INPUT;
    }
    size_t hash_len = strlen((const char *)hash);
    if (hash_len < HASH_SIZE)
    {
        return VCS_ERROR_INVALID_HASH;
    }

    char hex_hash[HEX_STR_SIZE];
    vcs_error_t err = sha256_hash_to_hex(hash, hex_hash);

    // Copy first 2 chars for directory
    strncpy(path->dir, hex_hash, OBJECT_DIR_SIZE - 1);
    path->dir[OBJECT_DIR_SIZE - 1] = '\0';

    // Copy remaining hash for filename
    strncpy(path->filename, (hex_hash + 2), OBJECT_FILENAME_SIZE);
    path->filename[OBJECT_FILENAME_SIZE - 1] = '\0';

    // Create full path
    int written = snprintf(path->path, MAX_PATH_LENGTH,
                           ".vcs/objects/%s/%s",
                           path->dir, path->filename);

    if (written < 0)
    {
        return VCS_ERROR_IO;
    }

    return VCS_OK;
}

vcs_error_t object_read(const unsigned char *hash, vcs_object_t *obj)
{
    if (!hash || !obj)
        return VCS_ERROR_NULL_INPUT;

    vcs_error_t err = create_object_path(hash, &obj->path);

    if (is_error(err))
        return err;

    return VCS_OK;
}

const char *object_type_to_string(object_type_t type)
{
    switch (type)
    {
    case OBJECT_TYPE_BLOB:
        return "blob";
    case OBJECT_TYPE_TREE:
        return "tree";
    case OBJECT_TYPE_COMMIT:
        return "commit";
    case OBJECT_TYPE_TAG:
        return "tag";
    default:
        return "unknown";
    }
}

vcs_error_t serialize_header(const vcs_object_t *obj, char **header)
{
    if (!obj)
        return VCS_ERROR_NULL_INPUT;

    const char *type_str = object_type_to_string(obj->type);

    size_t header_size = snprintf(NULL, 0, "%s %zu", type_str, obj->size);

    *header = malloc(header_size + 1);

    if (!header)
        return VCS_ERROR_MEMORY_ALLOCATION_FAILED;

    snprintf(*header, header_size + 1, "%s %zu", type_str, obj->size);

    return VCS_OK;
}

static vcs_error_t serialize_tree(const tree_object_t *tree, char **content)
{
    size_t total_size = 0;
    for (size_t i = 0; i < tree->entry_count; i++)
    {
        total_size += strlen(tree->entries[i].mode) + 1 +
                      strlen(tree->entries[i].hash) + 1 +
                      strlen(tree->entries[i].name) + 1;
    }

    *content = malloc(total_size + 1);
    if (!*content)
        return VCS_ERROR_MEMORY_ALLOCATION_FAILED;

    char *ptr = *content;
    for (size_t i = 0; i < tree->entry_count; i++)
    {
        int written = snprintf(ptr, total_size, "%s %s %s\n",
                               tree->entries[i].mode,
                               tree->entries[i].hash,
                               tree->entries[i].name);
        if (written < 0)
        {
            free(*content);
            return VCS_ERROR_IO;
        }
        ptr += written;
    }
    return VCS_OK;
}

static vcs_error_t serialize_commit(const commit_object_t *commit, char **content)
{
    size_t size = strlen(commit->tree) + strlen(commit->parent) +
                  strlen(commit->author.name) + strlen(commit->author.email) +
                  strlen(commit->committer.name) + strlen(commit->committer.email) +
                  strlen(commit->message) + 100; // Extra for formatting

    *content = malloc(size);
    if (!*content)
        return VCS_ERROR_MEMORY_ALLOCATION_FAILED;

    int written = snprintf(*content, size,
                           "tree %s\n"
                           "parent %s\n"
                           "author %s <%s>\n"
                           "committer %s <%s>\n"
                           "\n%s",
                           commit->tree,
                           commit->parent,
                           commit->author.name, commit->author.email,
                           commit->committer.name, commit->committer.email,
                           commit->message);

    if (written < 0)
    {
        free(*content);
        return VCS_ERROR_IO;
    }
    return VCS_OK;
}

static vcs_error_t serialize_tag(const tag_object_t *tag, char **content)
{
    size_t size = strlen(tag->ref) + strlen(tag->tag) +
                  strlen(tag->tagger.name) + strlen(tag->tagger.email) +
                  strlen(tag->message) + 100; // Extra for formatting

    *content = malloc(size);
    if (!*content)
        return VCS_ERROR_MEMORY_ALLOCATION_FAILED;

    int written = snprintf(*content, size,
                           "object %s\n"
                           "type %s\n"
                           "tag %s\n"
                           "tagger %s <%s>\n"
                           "\n%s",
                           tag->ref,
                           object_type_to_string(tag->type),
                           tag->tag,
                           tag->tagger.name,
                           tag->tagger.email,
                           tag->message);

    if (written < 0)
    {
        free(*content);
        return VCS_ERROR_IO;
    }
    return VCS_OK;
}

vcs_error_t serialize_content(const vcs_object_t *obj, char **content)
{
    if (!obj || !content)
        return VCS_ERROR_NULL_INPUT;

    switch (obj->type)
    {
    case OBJECT_TYPE_TREE:
        return serialize_tree((tree_object_t *)obj->data, content);
    case OBJECT_TYPE_COMMIT:
        return serialize_commit((commit_object_t *)obj->data, content);
    case OBJECT_TYPE_TAG:
        return serialize_tag((tag_object_t *)obj->data, content);
    default:
        return VCS_ERROR_INVALID_OBJECT_TYPE;
    }
}

vcs_error_t create_dir_if_not_exists(const char *path)
{
    if (!path)
        return VCS_ERROR_NULL_INPUT;

    struct stat st;
    if (stat(path, &st) == 0)
    {
        return VCS_OK;
    }

    if (mkdir(path, 0755) != 0)
    {
        return VCS_ERROR_IO;
    }

    return VCS_OK;
}

vcs_error_t object_write(vcs_object_t *obj)
{
    char dir_path[OBJECT_DIR_LEN + 1];
    snprintf(dir_path, OBJECT_DIR_LEN + 1, OBJECT_DIR_FMT, obj->path.dir);

    vcs_error_t err = create_dir_if_not_exists(dir_path);
    if (is_error(err))
        return err;

    FILE *file = fopen(obj->path.path, "wb");
    if (!file)
        return VCS_ERROR_IO;

    char *header;
    err = serialize_header(obj, &header);
    if (is_error(err))
        return err;
    size_t header_len = strlen(header);
    size_t write = fwrite(header, 1, header_len + 1, file);
    if (write != header_len + 1)
        return VCS_ERROR_IO;

    free(header);

    if (obj->type == OBJECT_TYPE_BLOB)
    {
        if (fclose(file) != 0)
        {
            return VCS_ERROR_IO;
        }

        blob_object_t *blob = (blob_object_t *)obj->data;
        err = append_file(blob->src_path, obj->path.path);
        if (is_error(err))
            return err;
    }
    else
    {
        char *content;
        err = serialize_content(obj, &content);
        if (is_error(err))
            return err;

        size_t content_len = strlen(content);

        if (fwrite(content, 1, content_len, file) != content_len)
            return VCS_ERROR_IO;

        if (fclose(file) != 0)

            return VCS_ERROR_IO;

        free(content);
    }
    
    err = compress_file_inplace(obj->path.path);
    if (is_error(err))
        return err;

    return VCS_OK;
}

void object_free(vcs_object_t *obj)
{
    if (!obj)
        return;

    switch (obj->type)
    {
    case OBJECT_TYPE_BLOB:
    {
        blob_object_t *blob = (blob_object_t *)obj->data;
        free(blob);
        break;
    }
    case OBJECT_TYPE_TREE:
    {
        tree_object_t *tree = (tree_object_t *)obj->data;
        if (tree->entries)
        {
            free(tree->entries);
            tree->entries = NULL;
        }
        free(tree);
        break;
    }
    case OBJECT_TYPE_COMMIT:
    {
        commit_object_t *commit = (commit_object_t *)obj->data;
        if (commit->message)
        {
            free(commit->message);
            commit->message = NULL;
        }
        free(commit);
        break;
    }
    case OBJECT_TYPE_TAG:
    {
        tag_object_t *tag = (tag_object_t *)obj->data;
        if (tag->message)
        {
            free(tag->message);
            tag->message = NULL;
        }
        free(tag);
        break;
    }
    }
    obj->data = NULL;
}