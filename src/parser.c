#include "parser.h"
#include "util.h"
#include "object_types.h"

#include <stdio.h>

struct parser
{
    FILE *fp;
    char *header;
    char *content;
};

parser_t *parser_init()
{
    parser_t *parser = malloc(sizeof(parser_t));
    if (!parser)
        return NULL;

    parser->fp = NULL;
    parser->header = NULL;
    parser->content = NULL;

    return parser;
}

void parser_free(parser_t *parser)
{
    if (parser->fp)
        fclose(parser->fp);

    if (parser->header)
        free(parser->header);

    if (parser->content)
        free(parser->content);

    free(parser);
}

static parse_result_t parser_open_file(parser_t *parser, const char *hash)
{
    char filepath[OBJECT_PATH_MAX];
    if (filepath_from_hash(hash, filepath) != 0)
    {
        fprintf(stderr, "Error: Failed to get object path\n");
        return PARSE_ERROR_HEADER;
    }

    parser->fp = fopen(filepath, "rb");

    if (!parser->fp)
    {
        fprintf(stderr, "Error: Failed to open object file\n");
        return PARSE_ERROR_HEADER;
    }

    return PARSE_OK;
}

static parse_result_t parser_read_header(parser_t *parser)
{
    parser->header = malloc(OBJECT_HEADER_MAX);
    if (!parser->header)
    {
        fprintf(stderr, "Error: Failed to allocate memory for header\n");
        return PARSE_ERROR_HEADER;
    }

    if (fread(parser->header, 1, OBJECT_HEADER_MAX, parser->fp) != OBJECT_HEADER_MAX)
    {
        fprintf(stderr, "Error: Failed to read header\n");
        return PARSE_ERROR_HEADER;
    }

    return PARSE_OK;
}

static parse_result_t parser_get_type(const char *header, object_type_t *out_type)
{
    char type_str[7];
    if (sscanf(header, "%6s", type_str) != 1)
    {
        fprintf(stderr, "Error: Failed to parse object type\n");
        return PARSE_ERROR_HEADER;
    }

    if (strcmp(type_str, "blob") == 0)
    {
        *out_type = OBJ_BLOB;
    }
    else if (strcmp(type_str, "tree") == 0)
    {
        *out_type = OBJ_TREE;
    }
    else if (strcmp(type_str, "commit") == 0)
    {
        *out_type = OBJ_COMMIT;
    }
    else if (strcmp(type_str, "tag") == 0)
    {
        *out_type = OBJ_TAG;
    }
    else
    {
        fprintf(stderr, "Error: Invalid object type\n");
        return PARSE_ERROR_INVALID_TYPE;
    }

    return PARSE_OK;
}

static parse_result_t parser_parse_header(parser_t *parser, object_t *out_obj)
{
    parse_result_t err;
    err = parser_read_header(parser);
    if (err != PARSE_OK)
        return err;

    char type_str[7];
    size_t size;
    if (sscanf(parser->header, "%6s %lu", type_str, &size) != 2)
    {
        fprintf(stderr, "Error: Failed to parse header\n");
        return PARSE_ERROR_HEADER;
    }

    err = parser_get_type(parser->header, &out_obj->header.type);
    if (err != PARSE_OK)
        return err;

    out_obj->header.content_size = size;

    return PARSE_OK;
}

static parse_result_t parser_read_content(parser_t *parser, object_t *out_obj)
{
    if (!out_obj->header.content_size)
    {
        out_obj->data = NULL;
        return PARSE_ERROR_CONTENT;
    }
    parser->content = malloc(out_obj->header.content_size);

    if (!parser->content)
    {
        fprintf(stderr, "Error: Failed to allocate memory for content\n");
        return PARSE_ERROR_CONTENT;
    }

    if (fread(parser->content, 1, out_obj->header.content_size, parser->fp) != out_obj->header.content_size)
    {
        fprintf(stderr, "Error: Failed to read content\n");
        return PARSE_ERROR_CONTENT;
    }

    return PARSE_OK;
}

static parse_result_t parse_tree_data(parser_t *parser, size_t content_size, tree_data_t *data)
{
    data->entries = NULL;
    data->size = 0;

    char *ptr = parser->content;
    const char *end = ptr + content_size;

    while (ptr < end)
    {
        // Parse "<mode> <name>\0<hash>"
        tree_entry_t *entry = malloc(sizeof(tree_entry_t));

        // Read mode
        int mode;
        if (sscanf(ptr, "%o", &mode) != 1)
        {
            return PARSE_ERROR_CONTENT;
        }
        entry->mode = mode;

        // Skip to name (after space)
        ptr = strchr(ptr, ' ') + 1;

        // Read name until null byte
        size_t name_len = strlen(ptr);
        if (name_len >= PATH_MAX)
        {
            free(entry);
            return PARSE_ERROR_CONTENT;
        }
        strcpy(entry->name, ptr);
        ptr += name_len + 1; // Skip name and null byte

        // Read hash
        memcpy(entry->hash, ptr, HEX_SIZE - 1);
        entry->hash[HEX_SIZE - 1] = '\0';
        ptr += HEX_SIZE - 1;

        // Add to tree
        HASH_ADD_STR(data->entries, name, entry);
        data->size++;
    }

    return PARSE_OK;
}

static int parse_commit_data(FILE *fp, commit_data_t *data) {
    char line[1024];
    char *ptr;

    // Read header lines (tree, parent, author, committer)
    while (fgets(line, sizeof(line), fp)) {
        // Empty line marks start of message
        if (line[0] == '\n') break;
        
        ptr = strchr(line, ' ');
        if (!ptr) return -1;
        ptr++; // Skip space
        
        // Remove newline
        char *nl = strchr(line, '\n');
        if (nl) *nl = '\0';
        
        if (strncmp(line, "tree", 4) == 0) {
            strncpy(data->tree_hash, ptr, HEX_SIZE);
        }
        else if (strncmp(line, "parent", 6) == 0) {
            strncpy(data->parent_hash, ptr, HEX_SIZE); 
        }
        else if (strncmp(line, "author", 6) == 0) {
            sscanf(ptr, "%s <%s> %ld", data->author.name, data->author.email, &data->author.time);
        }
        else if (strncmp(line, "committer", 9) == 0) {
            sscanf(ptr, "%s <%s> %ld", data->committer.name, data->committer.email, &data->committer.time);
        }
    }

    // Read message 
    size_t msg_len = 0;
    while (fgets(line, sizeof(line), fp)) {
        size_t len = strlen(line);
        if (msg_len + len >= COMMIT_MSG_MAX) return -1;
        strcat(data->message + msg_len, line);
        msg_len += len;
    }

    return 0;
}
static parse_result_t parser_parse_content(parser_t *parser, object_t *out_obj)
{

    parse_result_t err = PARSE_OK;
    size_t content_size = out_obj->header.content_size;
    switch (out_obj->header.type)
    {
    case OBJ_TREE:
    {
        tree_data_t *data = (tree_data_t *)out_obj->data;
        err = parse_tree_data(parser, content_size, data);
        break;
    }
    case OBJ_COMMIT:
    {
        commit_data_t *data = (commit_data_t *)out_obj->data;
        err = parse_commit_data(parser, data);
    }
    default:
        fprintf(stderr, "Error: Invalid object type\n");
        err = PARSE_ERROR_INVALID_TYPE;
        break;
    }
    return err;
}

parse_result_t parser_read_object(parser_t *parser, const char *hash, object_t *out_obj)
{
    parse_result_t err = PARSE_OK;

    err = parser_open_file(parser, hash);
    if (err != PARSE_OK)
        return err;

    err = parser_parse_header(parser, out_obj);
    if (err != PARSE_OK)
        return err;

    err = parser_parse_content(parser, out_obj);
    if (err != PARSE_OK)
        return err;

    return PARSE_OK;
}

void print_parse_error(int error)
{
    switch (error)
    {
    case PARSE_ERROR_HEADER:
        fprintf(stderr, "Error: Failed to parse header\n");
        break;
    case PARSE_ERROR_CONTENT:
        fprintf(stderr, "Error: Failed to parse content\n");
        break;
    case PARSE_ERROR_INVALID_TYPE:
        fprintf(stderr, "Error: Invalid object type\n");
        break;
    default:
        fprintf(stderr, "Error: Unknown error\n");
        break;
    }
}