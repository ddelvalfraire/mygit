#include "util.h"
#include "config.h"

#include <stdio.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <errno.h>


int create_directory(const char *path)
{
    struct stat st = {0};
    if (stat(path, &st) == -1)
    {
        if (mkdir(path, 0755) == -1)
        {
            fprintf(stderr, "Error creating directory '%s': %s\n",
                    path, strerror(errno));
            return -1;
        }
    }
    return 0;
}

size_t get_filesize(const char *filepath)
{
    struct stat st;
    if (stat(filepath, &st) == 0)
    {
        return st.st_size;
    }
    return 0;
}

int hash_to_hex(const unsigned char *hash, char *hex)
{
    for (int i = 0; i < SHA256_SIZE; i++)
    {
        sprintf(hex + i * 2, "%02x", hash[i]);
    }
    return 0;
}

static int filepath_from_hash(const char *hash, char *filepath)
{
    const int fp_size = HEX_SIZE + 14; //
    snprintf(filepath, fp_size, ".vcs/objects/%c%c/%s",
             hash[0], hash[1], hash + 2);
    return 0;
}
