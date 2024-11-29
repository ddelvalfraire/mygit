#include <stdio.h>
#include <string.h>
#include <stdlib.h>

void clear_terminal();

int main(void)
{
    char command[256];
    printf("vcs started\n");
    while (1)
    {
        printf("vcs> ");
        scanf("%s", command);

        if (strcmp(command, "exit") == 0)
        {
            printf("vcs ended\n");
            break;
        }
        else if (strcmp(command, "init") == 0)
        {
            printf("vcs initialized\n");
        }
        else if (strcmp(command, "add") == 0)
        {
            printf("added\n");
        }
        else if (strcmp(command, "commit") == 0)
        {
            printf("committed\n");
        }
        else if (strcmp(command, "status") == 0)
        {
            printf("status\n");
        }
        else if (strcmp(command, "log") == 0)
        {
            printf("log\n");
        }
        else if (strcmp(command, "checkout") == 0)
        {
            printf("checkout\n");
        }
        else if (strcmp(command, "clear") == 0)
        {
            clear_terminal();
        }
        else
        {
            printf("unknown command\n");
        }
    }
    return 0;
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