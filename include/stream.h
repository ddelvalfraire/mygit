#ifndef STREAM_H
#define STREAM_H

#include <stdio.h>
#include <zlib.h>
#include <stdbool.h>
#include <pthread.h>
#include "error.h"
#include "config.h"

#define STREAM_SUCCESS 0
#define STREAM_MAX_HEADER_SIZE 1024
#define STREAM_DEFAULT_CHUNK_SIZE (16 * 1024) // 16KB chunks

typedef enum
{
    STREAM_STATE_UNINITIALIZED = 0,
    STREAM_STATE_READY,
    STREAM_STATE_ERROR,
    STREAM_STATE_EOF
} stream_state_t;

typedef struct
{
    FILE *file;
    z_stream stream;
    unsigned char *in_buffer;
    unsigned char *out_buffer;
    size_t buffer_size;
    size_t out_pos;
    size_t out_size;
    stream_state_t state;
    pthread_mutex_t mutex;
    char *filepath;
    int compression_level;
    bool is_initialized;
} stream_reader_t;

// Main API functions
vcs_error_t stream_reader_create(stream_reader_t **reader);
vcs_error_t stream_reader_init(stream_reader_t *reader, const char *filepath, size_t buffer_size);
vcs_error_t stream_reader_read_until_null(stream_reader_t *reader, char *buffer, size_t max_size, size_t *bytes_read);
vcs_error_t stream_reader_read_chunk(stream_reader_t *reader, void *chunk, size_t chunk_size);
void stream_reader_close(stream_reader_t *reader);
void stream_reader_destroy(stream_reader_t **reader);

#endif // STREAM_H