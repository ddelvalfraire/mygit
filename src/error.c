#include "error.h"

bool is_error(vcs_error_t err)
{
    return err != VCS_OK;
}

const char *vcs_error_string(vcs_error_t err)
{
    switch (err)
    {
    case VCS_OK:
        return "Success";
    case VCS_ERROR_NO_HEAD:
        return "HEAD file not found";
    case VCS_ERROR_INVALID_HEAD:
        return "Invalid HEAD file format";
    case VCS_ERROR_IO:
        return "I/O error";
    case VCS_ERROR_BRANCH_EXISTS:
        return "Branch already exists";
    case VCS_ERROR_INVALID_BRANCH:
        return "Invalid branch name";
    case VCS_ERROR_CREATE_FAILED:
        return "Failed to create branch";
    case VCS_ERROR_BRANCH_DOES_NOT_EXIST:
        return "Branch does not exist";
    case VCS_ERROR_NO_LOGS:
        return "No logs found";
    case VCS_ERROR_NULL_INPUT:
        return "Null input";
    case VCS_ERROR_MEMORY_ALLOCATION_FAILED:
        return "Memory allocation failed";
    case VCS_ERROR_FILE_DOES_NOT_EXIST:
        return "File does not exist";
    case VCS_ERROR_FILE_TOO_LARGE:
        return "File is too large";
    case VCS_ERROR_BLOB_PATH_TOO_LONG:
        return "Blob path is too long";
    case VCS_TOO_MANY_ARGUMENTS:
        return "Too many arguments";
    default:
        return "Unknown error";
    }
}