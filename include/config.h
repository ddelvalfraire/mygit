#ifndef CONFIG_H
#define CONFIG_H

#define MAX_BRANCH_NAME 100
#define NULL_COMMIT "0000000000000000000000000000000000000000"
#define BRANCH_FMT ".vcs/refs/heads/%s"
#define HEAD_PATH ".vcs/HEAD"
#define STAGED_PATH ".vcs/index"
#define BLOB_PATH_FMT ".vcs/objects/%s/%s"
#define HASH_SIZE 20
#define HASH_STR_SIZE 41
#define MAX_PATH_LENGTH 4096
#define MAX_FILE_SIZE (2L * 1024 * 1024 * 1024)    // 2GB
#define MAX_STAGING_SIZE (2L * 1024 * 1024 * 1024) // 2GB

// temp
#define MAX_DEPTH 10
#define MAX_PATHS 256

#endif // CONFIG_H