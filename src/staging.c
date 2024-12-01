#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>

#include "staging.h"
#include "object.h"
#include "hash.h"
#include "io.h"

#define INDEX_HEADER_SIGNATURE "DIRC"
#define INDEX_VERSION 2

static vcs_error_t get_file_metadata(const char *path, index_entry_t *entry)
{
    struct stat st;
    if (stat(path, &st) != 0)
    {
        return VCS_ERROR_FILE_DOES_NOT_EXIST;
    }

    entry->ctime_seconds = st.st_ctime;
    entry->ctime_nanoseconds = st.st_ctimespec.tv_nsec;
    entry->mtime_seconds = st.st_mtime;
    entry->mtime_nanoseconds = st.st_mtimespec.tv_nsec;
    entry->dev = st.st_dev;
    entry->ino = st.st_ino;
    entry->mode = st.st_mode;
    entry->uid = st.st_uid;
    entry->gid = st.st_gid;
    entry->file_size = st.st_size;

    return VCS_OK;
}

vcs_error_t staging_init(staging_area_t *staging, const char *index_path)
{
    if (!staging || !index_path)
        return VCS_ERROR_NULL_INPUT;

    memset(staging, 0, sizeof(staging_area_t));
    return staging_load(staging);
}

vcs_error_t staging_load(staging_area_t *staging)
{
    if (!staging)
        return VCS_ERROR_NULL_INPUT;

    FILE *fp = fopen(STAGED_PATH, "rb");
    if (!fp)
        return VCS_OK; // Empty index is valid

    char signature[5] = {0};
    if (fread(signature, 1, 4, fp) != 4 || strcmp(signature, INDEX_HEADER_SIGNATURE) != 0)
    {
        fclose(fp);
        return VCS_ERROR_INDEX_HEADER;
    }

    uint32_t version;
    if (fread(&version, sizeof(version), 1, fp) != 1 || version != INDEX_VERSION)
    {
        fclose(fp);
        return VCS_ERROR_INDEX_HEADER;
    }

    uint32_t entry_count;
    if (fread(&entry_count, sizeof(entry_count), 1, fp) != 1)
    {
        fclose(fp);
        return VCS_ERROR_IO;
    }

    for (uint32_t i = 0; i < entry_count; i++)
    {
        index_entry_t *entry = calloc(1, sizeof(index_entry_t));
        if (!entry)
        {
            fclose(fp);
            return VCS_ERROR_MEMORY_ALLOCATION_FAILED;
        }

        if (fread(entry, sizeof(index_entry_t), 1, fp) != 1)
        {
            free(entry);
            fclose(fp);
            return VCS_ERROR_IO;
        }

        HASH_ADD_STR(staging->entries, filepath, entry);
        staging->count++;
    }

    fclose(fp);
    return VCS_OK;
}

vcs_error_t staging_save(staging_area_t *staging)
{
    if (!staging)
        return VCS_ERROR_NULL_INPUT;
    if (!staging->is_dirty)
        return VCS_OK;

    FILE *fp = fopen(STAGED_PATH, "wb");
    if (!fp)
        return VCS_ERROR_IO;

    // Write header
    fwrite(INDEX_HEADER_SIGNATURE, 1, 4, fp);
    uint32_t version = INDEX_VERSION;
    fwrite(&version, sizeof(version), 1, fp);
    fwrite(&staging->count, sizeof(staging->count), 1, fp);

    // Write entries
    index_entry_t *entry, *tmp;
    HASH_ITER(hh, staging->entries, entry, tmp)
    {
        if (fwrite(entry, sizeof(index_entry_t), 1, fp) != 1)
        {
            fclose(fp);
            return VCS_ERROR_IO;
        }
    }

    fclose(fp);
    staging->is_dirty = false;
    return VCS_OK;
}

vcs_error_t staging_add_entry(staging_area_t *staging, const char *path, const  char *hash)
{
    if (!staging || !path || !hash)
        return VCS_ERROR_NULL_INPUT;

    index_entry_t *entry = staging_find_entry(staging, path);
    if (!entry)
    {
        entry = calloc(1, sizeof(index_entry_t));
        if (!entry)
            return VCS_ERROR_MEMORY_ALLOCATION_FAILED;

        strncpy(entry->filepath, path, MAX_PATH_LENGTH - 1);
        HASH_ADD_STR(staging->entries, filepath, entry);
        staging->count++;
    }

    memcpy(entry->hash, hash, HEX_STR_SIZE);

    vcs_error_t err = get_file_metadata(path, entry);
    if (err != VCS_OK)
    {
        if (!staging_find_entry(staging, path))
        {
            HASH_DEL(staging->entries, entry);
            free(entry);
            staging->count--;
        }
        return err;
    }

    staging->is_dirty = true;
    return VCS_OK;
}

index_entry_t *staging_find_entry(staging_area_t *staging, const char *path)
{
    if (!staging || !path)
        return NULL;

    index_entry_t *entry;
    HASH_FIND_STR(staging->entries, path, entry);
    return entry;
}

vcs_error_t staging_remove_entry(staging_area_t *staging, const char *path)
{
    if (!staging || !path)
        return VCS_ERROR_NULL_INPUT;

    index_entry_t *entry = staging_find_entry(staging, path);
    if (entry)
    {
        HASH_DEL(staging->entries, entry);
        free(entry);
        staging->count--;
        staging->is_dirty = true;
    }
    return VCS_OK;
}

void staging_clear(staging_area_t *staging)
{
    if (!staging)
        return;

    index_entry_t *entry, *tmp;
    HASH_ITER(hh, staging->entries, entry, tmp)
    {
        HASH_DEL(staging->entries, entry);
        free(entry);
    }
    staging->entries = NULL;
    staging->count = 0;
    staging->is_dirty = true;
}

vcs_error_t staging_compute_blob_hash(const char *path, char *hash)
{
    if (!path || !hash)
        return VCS_ERROR_NULL_INPUT;
    return sha256_hash_file(path, hash);
}

bool staging_is_tracked(staging_area_t *staging, const char *path)
{
    return staging_find_entry(staging, path) != NULL;
}

index_status_t staging_get_status(staging_area_t *staging, const char *path)
{
    if (!staging || !path)
        return INDEX_UNTRACKED;

    index_entry_t *entry = staging_find_entry(staging, path);
    if (!entry)
        return INDEX_UNTRACKED;

    struct stat st;
    if (stat(path, &st) != 0)
        return INDEX_DELETED;

    // Check if file has been modified
    if (st.st_mtime != entry->mtime_seconds ||
        st.st_size != entry->file_size ||
        st.st_mode != entry->mode)
    {
        return INDEX_MODIFIED;
    }

    return INDEX_UNMODIFIED;
}

const char *index_status_to_string(index_status_t status)
{
    switch (status)
    {
    case INDEX_UNMODIFIED:
        return "unmodified";
    case INDEX_MODIFIED:
        return "modified";
    case INDEX_ADDED:
        return "added";
    case INDEX_DELETED:
        return "deleted";
    case INDEX_UNTRACKED:
        return "untracked";
    case INDEX_CONFLICT:
        return "conflict";
    default:
        return "unknown";
    }
}

vcs_error_t staging_get_modified_entries(staging_area_t *staging, index_entry_t **changes, size_t *count)
{
    if (!staging || !changes || !count)
        return VCS_ERROR_NULL_INPUT;

    *changes = NULL;
    *count = 0;

    index_entry_t *entry, *tmp;
    HASH_ITER(hh, staging->entries, entry, tmp)
    {
        index_status_t status = staging_get_status(staging, entry->filepath);
        if (status != INDEX_UNMODIFIED)
        {
            (*count)++;
        }
    }

    if (*count == 0)
        return VCS_OK;

    *changes = calloc(*count, sizeof(index_entry_t));
    if (!*changes)
        return VCS_ERROR_MEMORY_ALLOCATION_FAILED;

    size_t i = 0;
    HASH_ITER(hh, staging->entries, entry, tmp)
    {
        index_status_t status = staging_get_status(staging, entry->filepath);
        if (status != INDEX_UNMODIFIED)
        {
            memcpy(&(*changes)[i++], entry, sizeof(index_entry_t));
        }
    }

    return VCS_OK;
}