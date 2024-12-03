#ifndef CONFIG_H
#define CONFIG_H

#define PATH_MAX 4096
#define INDEX_SIGNATURE 0x44495243 // "DIRC"
#define INDEX_VERSION 2
#define HEX_SIZE (SHA256_SIZE * 2 + 1)
#define SHA256_SIZE 32
#define FILE_MAX 4096
#define COMMIT_MSG_MAX 512
#define OBJECT_HEADER_MAX 27 // type(6) + space(1) + size(20) + null(1)
#define OBJECT_PATH_MAX 78 // ".vcs/objects/"(12) + "xx/"(3) + hash(62) + null(1)

#endif // CONFIG_H