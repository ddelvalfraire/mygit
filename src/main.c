#include "command.h"

#include <string.h>
#include <stdio.h>

int main(int argc, char **argv)
{
    if (argc == 0)
    {
        printf("No arguments\n");
        return 0;
    }

    const char *command = argv[1];
    if (strcmp(command, "init") == 0)
    {
        command_execute(command_init(), argc, argv);
    }
    else if (strcmp(command, "add") == 0)
    {
        command_execute(command_add(), argc, argv);
    }
    else if (strcmp(command, "commit") == 0)
    {
        command_execute(command_commit(), argc, argv);
    }
    else
    {
        printf("Unknown command: %s\n", command);
        return 0;
    }

    return 1;
}