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
    case VCS_ERROR_HASH_FAILED:
        return "Hash computation failed";
    case VCS_ERROR_COMPRESSION_FAILED:
        return "Compression operation failed";
    case VCS_ERROR_INVALID_OBJECT_TYPE:
        return "Invalid object type";
    case VCS_ERROR_INVALID_HASH:
        return "Invalid hash value";
    case VCS_STREAM_READER_NOT_INITIALIZED:
        return "Stream reader not initialized";
    case VCS_ERROR_STREAM_CORRUPT:
        return "Stream data is corrupt";
    case VCS_ERROR_EOF:
        return "End of file reached";
    case VCS_ERROR_FILE_OPEN:
        return "Failed to open file";
    case VCS_ERROR_FILE_READ:
        return "Failed to read file";
    case VCS_ERROR_COMPRESSION:
        return "Compression error";
    case VCS_ERROR_BUFFER_OVERFLOW:
        return "Buffer overflow";
    case VCS_ERROR_INDEX_HEADER:
        return "Invalid index header";
    default:
        return "Unknown error";
    }
}