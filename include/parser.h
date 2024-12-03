#ifndef PARSER_H
#define PARSER_H

#include "object_types.h"

typedef enum
{
    PARSE_OK = 0,
    PARSE_ERROR_HEADER = -1,
    PARSE_ERROR_CONTENT = -2,
    PARSE_ERROR_INVALID_TYPE = -3
} parse_result_t;

typedef struct parser parser_t;

parser_t *parser_init();
void parser_free(parser_t *parser);

parse_result_t parser_read_object(parser_t *parser, const char *hash, object_t *out_obj);

void print_parse_error(int error);
#endif // PARSER_H