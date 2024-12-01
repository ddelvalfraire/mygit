#ifndef CONFIG_H
#define CONFIG_H

#define MAX_BRANCH_NAME 100
#define NULL_COMMIT "0000000000000000000000000000000000000000"
#define BRANCH_FMT ".vcs/refs/heads/%s"
#define HEAD_PATH ".vcs/HEAD"
#define STAGED_PATH ".vcs/index"
#define BLOB_PATH_FMT ".vcs/objects/%s/%s"
#define OBJECT_DIR_FMT ".vcs/objects/%s"
#define OBJECT_DIR_LEN 15
#define OBJECT_FILENAME_LEN 63
#define OBJECT_PATH_FMT ".vcs/objects/%s/%s"
#define CHUNK_SIZE 4096 // 4KB
#define HASH_SIZE 32
#define OBJECT_DIR_SIZE 3 
#define OBJECT_FILENAME_SIZE 63
#define HASH_STR_SIZE 33
#define HEX_SIZE 2 * HASH_SIZE
#define HEX_STR_SIZE 2 * HASH_SIZE + 1
#define MAX_PATH_LENGTH 4096
#define MAX_FILE_SIZE (2L * 1024 * 1024 * 1024)    // 2GB
#define MAX_STAGING_SIZE (2L * 1024 * 1024 * 1024) // 2GB
#define MAX_AUTHOR_NAME_SIZE 20
#define MAX_AUTHOR_EMAIL_SIZE 20
#define MAX_HEADER_SIZE 27

// temp
#define MAX_DEPTH 10
#define MAX_PATHS 256

#endif // CONFIG_H