#ifndef STAGING_H
#define STAGING_H

#include <stdbool.h>
#include <stdint.h>
#include <uthash.h>

#include "error.h"
#include "config.h"

// Enum representing possible file states in the index
typedef enum
{
    INDEX_UNMODIFIED, // File content and metadata match HEAD
    INDEX_MODIFIED,   // File content has changed
    INDEX_ADDED,      // File is new and staged
    INDEX_DELETED,    // File is staged for deletion
    INDEX_UNTRACKED,  // File is not in the index
    INDEX_CONFLICT    // File has a merge conflict
} index_status_t;

// Metadata for each entry in the staging area (index)
typedef struct
{
    uint32_t ctime_seconds;         // Last status change time (seconds)
    uint32_t ctime_nanoseconds;     // Last status change time (nanoseconds)
    uint32_t mtime_seconds;         // Last modification time (seconds)
    uint32_t mtime_nanoseconds;     // Last modification time (nanoseconds)
    uint32_t dev;                   // Device ID
    uint32_t ino;                   // Inode number
    uint32_t mode;                  // File mode (e.g., regular file, executable)
    uint32_t uid;                   // User ID of the file owner
    uint32_t gid;                   // Group ID of the file owner
    uint32_t file_size;             // Size of the file in bytes
    unsigned char hash[HASH_SIZE];               // SHA-1 hash (or adjust for SHA-256 if required)
    char filepath[MAX_PATH_LENGTH]; // Path to the file (relative to repository root)
    UT_hash_handle hh;              // Hash table handle
} index_entry_t;

// Staging area (index) structure
typedef struct
{
    index_entry_t *entries; // Hash table of index entries
    size_t count;           // Number of entries in the index
    bool is_dirty;          // Whether the index has pending changes
} staging_area_t;

// Core functions
vcs_error_t staging_init(staging_area_t *staging, const char *index_path); // Initialize the index
vcs_error_t staging_load(staging_area_t *staging);                         // Load index from disk
vcs_error_t staging_save(staging_area_t *staging);                         // Save index to disk
void staging_clear(staging_area_t *staging);                               // Clear all index entries

// Entry management
vcs_error_t staging_add_entry(staging_area_t *staging, const char *path, const unsigned char *hash);
vcs_error_t staging_remove_entry(staging_area_t *staging, const char *path);
index_entry_t *staging_find_entry(staging_area_t *staging, const char *path);

// Status checking
bool staging_is_tracked(staging_area_t *staging, const char *path);
index_status_t staging_get_status(staging_area_t *staging, const char *path);
vcs_error_t staging_get_modified_entries(staging_area_t *staging, index_entry_t **changes, size_t *count);

// Utility functions
const char *index_status_to_string(index_status_t status);              // Convert status to string
vcs_error_t staging_compute_blob_hash(const char *path, uint8_t *hash); // Compute hash for a file

#endif // STAGING_H
