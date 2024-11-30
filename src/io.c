#include <stdlib.h>
#include <sys/stat.h>

#include "io.h"

file_type_t get_file_type(const char *path)
{
    struct stat st;
    if (stat(path, &st) != 0)
        return FILE_TYPE_INVALID;
    if (S_ISREG(st.st_mode))
        return FILE_TYPE_REGULAR;
    if (S_ISDIR(st.st_mode))
        return FILE_TYPE_DIRECTORY;
    return FILE_TYPE_INVALID;
}

bool file_exists(const char *path)
{
    struct stat st;
    return stat(path, &st) == 0;
}

void clear_terminal()
{
// Use system commands to clear the terminal
#ifdef _WIN32
    system("cls"); // Windows
#else
    system("clear"); // Unix/Linux/Mac
#endif
}