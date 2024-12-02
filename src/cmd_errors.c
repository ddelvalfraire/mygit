#include "cmd_errors.h"

const char *cmd_error_to_string(int error)
{
    switch (error)
    {
    case CMD_SUCCESS:
        return "Success";
    case CMD_ERROR:
        return "Error";
    case CMD_ERROR_INVALID_ARGUMENTS:
        return "Invalid arguments";
    case CMD_ERROR_INVALID_COMMAND:
        return "Invalid command";
    case CMD_ERROR_INVALID_OPTION:
        return "Invalid option";
    case CMD_ERROR_INVALID_REPOSITORY:
        return "Invalid repository";
    case CMD_ERROR_INVALID_FILE:
        return "Invalid file";
    default:
        return "Unknown error";
    }
}