#ifndef IO_H
#define IO_H

#include <stdbool.h>

typedef enum
{
    FILE_TYPE_REGULAR,
    FILE_TYPE_DIRECTORY,
    FILE_TYPE_INVALID
} file_type_t;

file_type_t get_file_type(const char *path);
void clear_terminal();
bool file_exists(const char *path);

#endif // IO_H