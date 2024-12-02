#include "command.h"
#include "cmd_errors.h"
#include "repository.h"

#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>

struct command
{
    const char *name;
    const char *description;
    const char *usage;
    void *ctx;

    int (*validate)(struct command *self, int argc, char **argv);
    int (*run)(struct command *self, int argc, char **argv);
    void (*cleanup)(struct command *self);
};

int command_execute(command_t *cmd, int argc, char **argv)
{

    if (!cmd)
    {
        return CMD_ERROR_INVALID_COMMAND;
    }

    if (cmd->validate)
    {
        if (cmd->validate(cmd, argc, argv) != 0)
        {
            return CMD_ERROR_INVALID_ARGUMENTS;
        }
    }

    if (cmd->run)
    {
        if (cmd->run(cmd, argc, argv) != 0)
        {
            return CMD_ERROR_EXEC_FAILED;
        }
    }

    if (cmd->cleanup)
    {
        cmd->cleanup(cmd);
    }

    return 0;
}

int command_init_validate(command_t *self, int argc, char **argv)
{
    if (argc < 2)
    {
        fprintf(stderr, "Error: Invalid number of arguments\n");
        fprintf(stderr, "Usage: %s\n", self->usage);
        return CMD_ERROR_INVALID_ARGUMENTS;
    }

    return 0;
}

int command_init_run(command_t *self, int argc, char **argv)
{
    repository_t *repo = repository_init();
    if (!repo)
    {
        fprintf(stderr, "Error: Failed to initialize repository\n");
        return CMD_ERROR_EXEC_FAILED;
    }

    repository_free(repo);
    return 0;
}

command_t command_init_obj = {
    .name = "init",
    .description = "Create an empty repository or reinitialize an existing one",
    .usage = "vcs init",
    .ctx = NULL,
    .validate = command_init_validate,
    .run = command_init_run,
    .cleanup = NULL};

int command_add_validate(command_t *self, int argc, char **argv)
{
    if (argc < 3)
    {
        fprintf(stderr, "Error: Invalid number of arguments\n");
        fprintf(stderr, "Usage: %s\n", self->usage);
        return CMD_ERROR_INVALID_ARGUMENTS;
    }

    for (int i = 2; i < argc; i++)
    {
        struct stat st;
        if (stat(argv[i], &st) != 0)
        {
            fprintf(stderr, "Error: '%s' is not a valid file or directory\n", argv[i]);
            return CMD_ERROR_INVALID_ARGUMENTS;
        }
    }

    return 0;
}

int command_add_run(command_t *self, int argc, char **argv)
{


    repository_t *repo = repository_open();
    if (!repo)
    {
        fprintf(stderr, "Error: Failed to open repository\n");
        return CMD_ERROR_EXEC_FAILED;
    }

    int result = repository_add(repo, argc - 2, argv + 2);
    if (result != 0)
    {
        fprintf(stderr, "Error: Failed to add files to repository\n");
    }

    repository_free(repo);
    return result == 0 ? 0 : CMD_ERROR_EXEC_FAILED;
}

command_t command_add_obj = {
    .name = "add",
    .description = "Add file contents to the index",
    .usage = "vcs add <file>",
    .ctx = NULL,
    .validate = command_add_validate,
    .run = command_add_run,
    .cleanup = NULL};


command_t command_commit = {
    .name = "commit",
    .description = "Record changes to the repository",
    .usage = "vcs commit -m <message>",
    .ctx = NULL,
    .validate = NULL,
    .run = NULL,
    .cleanup = NULL};

command_t command_status = {
    .name = "status",
    .description = "Show the working tree status",
    .usage = "vcs status",
    .ctx = NULL,
    .validate = NULL,
    .run = NULL,
    .cleanup = NULL};

command_t command_log = {
    .name = "log",
    .description = "Show commit logs",
    .usage = "vcs log",
    .ctx = NULL,
    .validate = NULL,
    .run = NULL,
    .cleanup = NULL};

command_t *command_init()
{
    return &command_init_obj;
}

command_t *command_add()
{
    return &command_add_obj;
}