#include "object.h"
#include "tree.h"
#include "util.h"
#include "object_types.h"
#include "parser.h"
#include "config.h"
#include <stdlib.h>
#include <stdio.h>
#include <openssl/evp.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <libgen.h>
#include <uthash.h>



static void *object_data_allocate(object_type_t type)
{
    switch (type)
    {
    case OBJ_BLOB:
        return malloc(sizeof(blob_data_t));
    case OBJ_TREE:
        return malloc(sizeof(tree_entry_t));
    case OBJ_COMMIT:
        return malloc(sizeof(commit_data_t));
    default:
        return NULL;
    }
}

object_t *object_init(object_type_t type)
{
    object_t *obj = malloc(sizeof(object_t));
    if (!obj)
        return NULL;

    obj->header.type = type;
    obj->header.content_size = 0;
    obj->data = object_data_allocate(type);

    return obj;
}

void object_free(object_t *obj)
{

    if (obj->header.type == OBJ_TREE)
    {
        tree_data_t *data = (tree_data_t *)obj->data;
        tree_entry_t *entry, *tmp;
        HASH_ITER(hh, data->entries, entry, tmp)
        {
            HASH_DEL(data->entries, entry);
            free(entry);
        }
    }

    if (obj->data)
        free(obj->data);

    if (obj)
        free(obj);

    obj = NULL;
}



static const char *object_type_to_string(object_type_t type)
{
    switch (type)
    {
    case OBJ_BLOB:
        return "blob";
    case OBJ_TREE:
        return "tree";
    case OBJ_COMMIT:
        return "commit";
    case OBJ_TAG:
        return "tag";
    default:
        return "unknown";
    }
}

static int object_update_hash_with_header(EVP_MD_CTX *ctx, object_header_t *header, unsigned char *hash)
{
    char header_str[256];
    int header_len = snprintf(header_str, sizeof(header_str), "%s %zu%c",
                              object_type_to_string(header->type),
                              header->content_size,
                              '\0');

    if (header_len < 0)
    {
        return -1;
    }

    if (!EVP_DigestUpdate(ctx, header_str, header_len + 1))
    {
        return -1;
    }

    return 0;
}

static int object_update_hash_with_tree(EVP_MD_CTX *ctx, tree_entry_t *entries)
{
    tree_entry_t *entry, *tmp;

    HASH_ITER(hh, entries, entry, tmp)
    {
        if (!EVP_DigestUpdate(ctx, &entry->mode, sizeof(mode_t)))
        {
            return -1;
        }

        if (!EVP_DigestUpdate(ctx, entry->name, strlen(entry->name)))
        {
            return -1;
        }

        if (!EVP_DigestUpdate(ctx, entry->hash, HEX_SIZE - 1))
        {
            return -1;
        }
    }

    return 0;
}

static int object_update_hash_with_commit(EVP_MD_CTX *ctx, commit_data_t *data)
{
    if (!EVP_DigestUpdate(ctx, data->tree_hash, HEX_SIZE - 1))
    {
        return -1;
    }

    if (!EVP_DigestUpdate(ctx, &data->author, sizeof(signature_t)))
    {
        return -1;
    }

    if (!EVP_DigestUpdate(ctx, &data->committer, sizeof(signature_t)))
    {
        return -1;
    }

    if (!EVP_DigestUpdate(ctx, data->message, strlen(data->message)))
    {
        return -1;
    }

    return 0;
}

static int object_update_hash_with_data(EVP_MD_CTX *ctx, object_t *obj)
{
    switch (obj->header.type)
    {
    case OBJ_BLOB:
    {
        blob_data_t *blob = (blob_data_t *)obj->data;
        if (!EVP_DigestUpdate(ctx, blob->data, blob->size))
        {

            return -1;
        }
        break;
    }
    case OBJ_TREE:
    {
        tree_data_t *tree = (tree_data_t *)obj->data;
        if (object_update_hash_with_tree(ctx, tree->entries) != 0)
        {

            return -1;
        }
        break;
    }
    case OBJ_COMMIT:
    {
        commit_data_t *commit = (commit_data_t *)obj->data;
        if (object_update_hash_with_commit(ctx, commit) != 0)
        {

            return -1;
        }
        break;
    }
    default:

        return -1;
    }
    return 0;
}

static int object_compute_hash(object_t *obj, char *hash)
{
    EVP_MD_CTX *ctx = EVP_MD_CTX_new();
    unsigned char hash_bytes[SHA256_SIZE];
    unsigned int hash_len;

    if (!ctx)
    {
        return -1;
    }

    if (!EVP_DigestInit_ex(ctx, EVP_sha256(), NULL))
    {
        EVP_MD_CTX_free(ctx);
        return -1;
    }

    // Hash header
    if (object_update_hash_with_header(ctx, &obj->header, hash_bytes) != 0)
    {
        EVP_MD_CTX_free(ctx);
        return -1;
    }

    // Hash content based on type
    if (object_update_hash_with_data(ctx, obj) != 0)
    {
        EVP_MD_CTX_free(ctx);
        return -1;
    }

    if (!EVP_DigestFinal_ex(ctx, hash_bytes, &hash_len))
    {
        EVP_MD_CTX_free(ctx);
        return -1;
    }

    EVP_MD_CTX_free(ctx);
    hash_to_hex(hash_bytes, hash);
    return 0;
}


static int update_blob(object_t *obj, object_update_t data)
{
    blob_data_t *blob = (blob_data_t *)obj->data;

    // Check file size first
    size_t size = get_filesize(data.blob.filepath);
    if (size > FILE_MAX)
    {
        fprintf(stderr, "Error: File '%s' is too large (%zu bytes, max is %d)\n",
                data.blob.filepath, size, FILE_MAX);
        return -1;
    }

    FILE *fp = fopen(data.blob.filepath, "rb");
    if (!fp)
    {
        fprintf(stderr, "Error opening file '%s'\n", data.blob.filepath);
        return -1;
    }

    blob->size = fread(blob->data, 1, size, fp);
    if (ferror(fp))
    {
        fprintf(stderr, "Error reading file '%s'\n", data.blob.filepath);
        fclose(fp);
        return -1;
    }

    if (blob->size != size)
    {
        fprintf(stderr, "Error: File '%s' read size mismatch\n", data.blob.filepath);
        fclose(fp);
        return -1;
    }

    obj->header.content_size = blob->size;

    fclose(fp);
    return 0;
}

static tree_entry_t *index_entry_to_tree_entry(index_entry_t *index_entry)
{
    tree_entry_t *tree_entry = malloc(sizeof(tree_entry_t));

    tree_entry->mode = index_entry->mode;
    strcpy(tree_entry->name, index_entry->path);
    strcpy(tree_entry->hash, index_entry->hash);

    return tree_entry;
}

static tree_entry_t *object_tree_to_tree_entry(object_t *obj)
{
    tree_entry_t *tree_entry = malloc(sizeof(tree_entry_t));
    tree_data_t *tree_data = (tree_data_t *)obj->data;
    tree_entry->mode = S_IFDIR;
    strcpy(tree_entry->name, tree_data->dirname);
    strcpy(tree_entry->hash, obj->header.hash);
    return tree_entry;
}

static void process_tree(node_t *node, object_t *curr_tree)
{
    if (!node)
        return;

    tree_data_t *curr_data = (tree_data_t *)curr_tree->data;
    tree_entry_t *tree_entry;

    // Process all children first
    for (int i = 0; i < node->child_count; i++)
    {
        if (node->children[i]->is_file)
        {
            // Files get added directly to current tree
            index_entry_t *index_entry = (index_entry_t *)node->children[i]->data;
            tree_entry = index_entry_to_tree_entry(index_entry);
            HASH_ADD_STR(curr_data->entries, name, tree_entry);
            curr_tree->header.content_size += index_entry->size;
            curr_data->size++;
        }
        else
        {
            // Create new tree object for subdirectory
            object_t *child_tree = object_init(OBJ_TREE);
            process_tree(node->children[i], child_tree);

            // Write child tree and add it to current tree
            object_write(child_tree, NULL);
            tree_entry_t *child_entry = object_tree_to_tree_entry(child_tree);
            HASH_ADD_STR(curr_data->entries, name, child_entry);
            curr_tree->header.content_size += sizeof(mode_t) + strlen(child_entry->name) + HEX_SIZE - 1;
            curr_data->size++;

            object_free(child_tree);
        }
    }
}

static int update_tree(object_t *obj, object_update_t data)
{
    tree_data_t *tree_data = (tree_data_t *)obj->data;
    tree_data->size = 0;
    tree_data->entries = NULL;
    strcpy(tree_data->dirname, ".");

    index_t *index = data.tree.index;
    node_t *root = createNode(".", 0);

    index_hash_entry_t *entry;
    for (entry = index->entries; entry != NULL; entry = entry->hh.next)
    {
        addPath(root, entry->path, &entry->entry);
    }

    process_tree(root, obj);

    return 0;
}

static int update_commit(object_t *obj, object_update_t data)
{
    commit_data_t *commit = (commit_data_t *)obj->data;
    strcpy(commit->tree_hash, data.commit.tree_hash);
    strcpy(commit->message, data.commit.message);
    strcpy(commit->author.name, data.commit.author_name);
    strcpy(commit->author.email, data.commit.author_email);
    commit->author.time = data.commit.author_time;
    strcpy(commit->committer.name, data.commit.committer_name);
    strcpy(commit->committer.email, data.commit.committer_email);
    commit->committer.time = data.commit.committer_time;

    // Calculate total size
    obj->header.content_size = 
        5 + HEX_SIZE + 1 +                              // "tree <hash>\n"
        7 + strlen(commit->author.name) + 2 +           // "author <name> "
        strlen(commit->author.email) + 2 +              // "<email> "
        20 + 1 +                                        // timestamp + \n
        10 + strlen(commit->committer.name) + 2 +       // "committer <name> "
        strlen(commit->committer.email) + 2 +           // "<email> "
        20 + 1 +                                        // timestamp + \n
        1 +                                             // blank line
        strlen(commit->message);                        // message

    return 0;
}
int object_update(object_t *obj, object_update_t data)
{
    switch (obj->header.type)
    {
    case OBJ_BLOB:
        return update_blob(obj, data);
    case OBJ_TREE:
        return update_tree(obj, data);
    case OBJ_COMMIT:
        return update_commit(obj, data);
    default:
        fprintf(stderr, "Error: Unsupported type\n");
        return -1;
    }
}


static int write_tree_data(FILE *fp, tree_data_t *data)
{
    tree_entry_t *entry;
    for (entry = data->entries; entry != NULL; entry = entry->hh.next)
    {
        // Write "<mode> <name>\0"
        char mode_str[8];
        snprintf(mode_str, sizeof(mode_str), "%o", entry->mode);
        if (fprintf(fp, "%s %s%c", mode_str, entry->name, '\0') < 0)
        {
            return -1;
        }

        // Write hash
        if (fprintf(fp, "%s", entry->hash) < 0)
        {
            return -1;
        }
    }
    return 0;
}

static int write_commit_data(FILE *fp, commit_data_t *data)
{
    if (fprintf(fp, "tree %s\n", data->tree_hash) < 0)
    {
        return -1;
    }

    if (fprintf(fp, "author %s <%s> %ld\n", data->author.name, data->author.email, data->author.time) < 0)
    {
        return -1;
    }

    if (fprintf(fp, "committer %s <%s> %ld\n", data->committer.name, data->committer.email, data->committer.time) < 0)
    {
        return -1;
    }

    if (fprintf(fp, "\n%s\n", data->message) < 0)
    {
        return -1;
    }

    return 0;
}

static int object_write_header(FILE *fp, object_t *obj)
{
    int written = fprintf(fp, "%s %zu%c",
                          object_type_to_string(obj->header.type),
                          obj->header.content_size,
                          '\0');
    return written < 0 ? -1 : 0;
}

static int object_data_write(FILE *fp, object_t *obj)
{
    switch (obj->header.type)
    {
    case OBJ_BLOB:
    {
        blob_data_t *blob = (blob_data_t *)obj->data;
        if (fwrite(blob->data, 1, blob->size, fp) != blob->size)
        {
            fprintf(stderr, "Error: Failed to write data\n");
            fclose(fp);
            return -1;
        }
        break;
    }
    case OBJ_TREE:
    {
        tree_data_t *tree = (tree_data_t *)obj->data;
        if (write_tree_data(fp, tree) < 0)
        {
            fprintf(stderr, "Error: Failed to write tree data\n");
            fclose(fp);
            return -1;
        }
        break;
    }
    case OBJ_COMMIT:
    {
        commit_data_t *commit = (commit_data_t *)obj->data;
        if (write_commit_data(fp, commit) < 0)
        {
            fprintf(stderr, "Error: Failed to write commit data\n");
            fclose(fp);
            return -1;
        }
        break;
    }
    default:
        fprintf(stderr, "Error: Unsupported type\n");
        fclose(fp);
        return -1;
    }
    return 0;
}

int object_write(object_t *obj, char *out_hash)
{
    char hash[HEX_SIZE];
    if (object_compute_hash(obj, hash) != 0)
    {
        fprintf(stderr, "Error: Failed to compute hash\n");
        return -1;
    }

    // Create hash prefix directory
    char prefix_dir[PATH_MAX];
    snprintf(prefix_dir, sizeof(prefix_dir), ".vcs/objects/%c%c",
             hash[0], hash[1]);
    if (create_directory(prefix_dir) < 0)
    {
        return -1;
    }

    // Full path with hash suffix
    char obj_path[PATH_MAX];
    snprintf(obj_path, sizeof(obj_path), "%s/%s", prefix_dir, hash + 2);

    FILE *fp = fopen(obj_path, "wb"); // Binary mode
    if (!fp)
    {
        fprintf(stderr, "Error: Failed to open '%s'\n", obj_path);
        return -1;
    }

    if (object_write_header(fp, obj) < 0)
    {
        fprintf(stderr, "Error: Failed to write header\n");
        fclose(fp);
        return -1;
    }

    if (object_data_write(fp, obj) < 0)
    {
        fprintf(stderr, "Error: Failed to write data\n");
        fclose(fp);
        return -1;
    }

    fclose(fp);

    if (out_hash)
    {
        strcpy(out_hash, hash);
    }
    return 0;
}

static int object_read_headerr(FILE *fp, char *header)
{
    char c;
    int i = 0;
    while (fread(&c, 1, 1, fp) == 1)
    {
        if (c == '\0')
        {
            header[i] = '\0';
            return 0;
        }
        header[i++] = c;
    }
    return -1;
}

static int object_parse_header(char *header, object_t *obj)
{
    char type[16];
    size_t size;
    if (sscanf(header, "%s %zu", type, &size) != 2)
    {
        fprintf(stderr, "Error: Failed to parse header\n");
        return -1;
    }

    if (strcmp(type, "blob") == 0)
    {
        obj->header.type = OBJ_BLOB;
    }
    else if (strcmp(type, "tree") == 0)
    {
        obj->header.type = OBJ_TREE;
    }
    else if (strcmp(type, "commit") == 0)
    {
        obj->header.type = OBJ_COMMIT;
    }
    else
    {
        fprintf(stderr, "Error: Unknown object type '%s'\n", type);
        return -1;
    }

    obj->header.content_size = size;
    return 0;
}

static int object_read_content(FILE *fp, char* content)
{
    size_t file_size = get_filesize(fp);

    if (file_size > FILE_MAX)
    {
        fprintf(stderr, "Error: File is too large (%zu bytes, max is %d)\n",
                file_size, FILE_MAX);
        return -1;
    }


    if (fread(content, 1, file_size, fp) != file_size)
    {
        fprintf(stderr, "Error: Failed to read content\n");
        return -1;
    }
    return 0;
}


int object_read(object_t *obj, const char *hash)
{
    parser_t *parser = parser_init();

    parse_result_t err = parser_read_object(parser, hash, obj);
    
    if (err != PARSE_OK) print_parser_error(err);

    parser_free(parser);

    return err == PARSE_OK ? 0 : -1;
}