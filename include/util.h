#ifndef UTIL_H
#define UTIL_H

int create_directory(const char *dir);
size_t get_filesize(const char *filepath);
int filepath_from_hash(const char *hash, char *filepath);
int hash_to_hex(const unsigned char *hash, char *hex);

#endif // UTIL_H