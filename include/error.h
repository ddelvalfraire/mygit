#ifndef ERROR_H
#define ERROR_H

#include <stdbool.h>

typedef enum
{
    VCS_OK,
    VCS_ERROR_NO_HEAD,
    VCS_ERROR_INVALID_HEAD,
    VCS_ERROR_IO,
    VCS_ERROR_BRANCH_EXISTS,
    VCS_ERROR_INVALID_BRANCH,
    VCS_ERROR_CREATE_FAILED,
    VCS_ERROR_BRANCH_DOES_NOT_EXIST,
    VCS_ERROR_NO_LOGS,
    VCS_ERROR_NULL_INPUT,
    VCS_ERROR_MEMORY_ALLOCATION_FAILED,
    VCS_ERROR_FILE_DOES_NOT_EXIST,
    VCS_ERROR_FILE_TOO_LARGE,
    VCS_ERROR_BLOB_PATH_TOO_LONG,
    VCS_TOO_MANY_ARGUMENTS
} vcs_error_t;

const char *vcs_error_string(vcs_error_t err);
bool is_error(vcs_error_t err);

#endif // ERROR_H