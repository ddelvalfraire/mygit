#include <string.h>
#include <ctype.h>

#include "utils.h"

void remove_newline(char *str)
{
    if (!str)
    {
        return;
    }

    size_t len = strlen(str);
    if (len > 0 && str[len - 1] == '\n')
    {
        str[len - 1] = '\0';
    }
}

void trim(char *str)
{
    if (!str)
        return;

    // Trim leading space
    char *start = str;
    while (*start && isspace(*start))
        start++;

    if (start != str)
    {
        memmove(str, start, strlen(start) + 1);
    }

    // Trim trailing space
    char *end = str + strlen(str) - 1;
    while (end > str && isspace(*end))
        end--;
    *(end + 1) = '\0';
}
