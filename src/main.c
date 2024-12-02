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

    return 1;
}