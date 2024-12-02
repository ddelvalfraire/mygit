#ifndef CMD_ERRORS_H
#define CMD_ERRORS_H

typedef enum
{
    CMD_SUCCESS,
    CMD_ERROR,
    CMD_ERROR_EXEC_FAILED,
    CMD_ERROR_INVALID_ARGUMENTS,
    CMD_ERROR_INVALID_COMMAND,
    CMD_ERROR_INVALID_OPTION,
    CMD_ERROR_INVALID_REPOSITORY,
    CMD_ERROR_INVALID_FILE,
} cmd_error_t;

const char *cmd_error_to_string(int error);

#endif // CMD_ERRORS_H