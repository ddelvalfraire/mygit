#ifndef UTIL_H
#define UTIL_H

#include <stdio.h>
#include <stdlib.h>

int create_directory(const char *dir);
size_t get_filesize_by_filepath(const char *filepath);
size_t get_filesize_by_fp(const FILE *fp);
int filepath_from_hash(const char *hash, char *filepath);
int hash_to_hex(const unsigned char *hash, char *hex);

#endif // UTIL_H