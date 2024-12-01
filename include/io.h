#ifndef IO_H
#define IO_H

#include <stdbool.h>

#include "error.h"

typedef enum
{
    FILE_TYPE_REGULAR,
    FILE_TYPE_DIRECTORY,
    FILE_TYPE_INVALID
} file_type_t;


vcs_error_t compress_file(const char *input_path, const char *output_path);
vcs_error_t compress_and_move(const char *src_path, const char *dest_path);
vcs_error_t compress_file_inplace(const char *filepath);
vcs_error_t append_file(const char *src_path, const char *dest_path);
file_type_t get_file_type(const char *path);
void clear_terminal();
bool file_exists(const char *path);

#endif // IO_H