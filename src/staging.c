#include "staging.h"
#include "object.h"

#include <string.h>
#include <stdio.h>
#include <sys/stat.h>

index_t *index_init(const char *filepath)
{
    index_t *index = calloc(1, sizeof(index_t));
    index->filepath = strdup(filepath);
    index->entries = NULL;

    FILE *fp = fopen(filepath, "rb");
    if (!fp)
    {
        index->header.signature = INDEX_SIGNATURE;
        index->header.version = INDEX_VERSION;
        index->header.entry_count = 0;
        return index;
    }

    // Read header
    fread(&index->header, sizeof(index_header_t), 1, fp);

    // Read and hash entries
    for (uint32_t i = 0; i < index->header.entry_count; i++)
    {
        index_hash_entry_t *entry = malloc(sizeof(index_hash_entry_t));
        fread(&entry->entry, sizeof(index_entry_t), 1, fp);
        strcpy(entry->path, entry->entry.path);
        HASH_ADD_STR(index->entries, path, entry);
    }

    fclose(fp);
    return index;
}

void index_free(index_t *index)
{
    index_hash_entry_t *entry, *tmp;
    HASH_ITER(hh, index->entries, entry, tmp)
    {
        HASH_DEL(index->entries, entry);
        free(entry);
    }
    free(index->filepath);
    free(index);
}

int index_write(index_t *index)
{
    char temp_path[strlen(index->filepath) + 5];
    snprintf(temp_path, PATH_MAX, "%s.tmp", index->filepath);

    FILE *fp = fopen(temp_path, "wb");
    if (!fp)
        return -1;

    // Count entries
    index->header.entry_count = HASH_COUNT(index->entries);

    // Write header
    fwrite(&index->header, sizeof(index_header_t), 1, fp);

    // Write entries
    index_hash_entry_t *entry, *tmp;
    HASH_ITER(hh, index->entries, entry, tmp)
    {
        fwrite(&entry->entry, sizeof(index_entry_t), 1, fp);
    }

    fclose(fp);
    rename(temp_path, index->filepath);

    return 0;
}


static void update_index_entry(index_t *index, const char *path, const char *hash, struct stat *st)
{
    index_hash_entry_t *entry;
    HASH_FIND_STR(index->entries, path, entry);

    if (!entry)
    {
        entry = malloc(sizeof(index_hash_entry_t));
        strcpy(entry->path, path);
        HASH_ADD_STR(index->entries, path, entry);
    }

    entry->entry.ctime_sec = st->st_ctimespec.tv_sec;
    entry->entry.ctime_nsec = st->st_ctimespec.tv_nsec;
    entry->entry.mtime_sec = st->st_mtimespec.tv_sec;
    entry->entry.mtime_nsec = st->st_mtimespec.tv_nsec;
    entry->entry.dev = st->st_dev;
    entry->entry.ino = st->st_ino;
    entry->entry.mode = st->st_mode;
    entry->entry.uid = st->st_uid;
    entry->entry.gid = st->st_gid;
    entry->entry.size = st->st_size;
    strcpy(entry->entry.hash, hash);
    strcpy(entry->entry.path, path);
    entry->entry.flags = strlen(path);
}

static int write_staged_file(index_t *index, const char *filepath)
{
    struct stat st;
    if (stat(filepath, &st) != 0)
    {
        fprintf(stderr, "Error: Cannot stat file '%s'\n", filepath);
        return -1;
    }

    // Create and write blob object
    object_t *obj = object_init(OBJ_BLOB);
    if (!obj)
    {
        fprintf(stderr, "Error: Failed to initialize object\n");
        return -1;
    }

    const object_update_t data = {
        .blob = {.filepath = filepath}};

    if (object_update(obj, data) != 0)
    {
        fprintf(stderr, "Error: Failed to update object\n");
        object_free(obj);
        return -1;
    }

    char hash[HEX_SIZE];
    if (object_write(obj, hash) != 0)
    {
        fprintf(stderr, "Error: Failed to write object\n");
        object_free(obj);
        return -1;
    }

    update_index_entry(index, filepath, hash, &st);

    printf("Added '%s'\n", filepath);
    object_free(obj);
    return 0;
}

int index_stage_files(index_t *index, UT_array *files)
{
    if (utarray_len(files) == 0)
    {
        printf("No files to stage\n");
        return 0;
    }

    char **p = NULL;
    while ((p = (char **)utarray_next(files, p)))
    {

        if (write_staged_file(index, *p) != 0)
        {
            printf("Error: Failed to stage file '%s'\n", *p);
            fprintf(stderr, "Error: Failed to stage file '%s'\n", *p);
            return -1;
        }
    }

    index_write(index);

    return 0;
}