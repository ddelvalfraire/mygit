#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "stream.h"
#include "util.h"

#define SAFE_FREE(ptr)  \
    do                  \
    {                   \
        if (ptr)        \
        {               \
            free(ptr);  \
            ptr = NULL; \
        }               \
    } while (0)

static vcs_error_t initialize_zlib_stream(stream_reader_t *reader)
{
    reader->stream.zalloc = Z_NULL;
    reader->stream.zfree = Z_NULL;
    reader->stream.opaque = Z_NULL;
    reader->stream.avail_in = 0;
    reader->stream.next_in = Z_NULL;

    int ret = inflateInit(&reader->stream);
    if (ret != Z_OK)
    {
        return VCS_ERROR_COMPRESSION_FAILED;
    }
    return VCS_OK;
}

vcs_error_t stream_reader_create(stream_reader_t **reader)
{
    if (!reader)
        return VCS_ERROR_NULL_INPUT;

    *reader = calloc(1, sizeof(stream_reader_t));
    if (!*reader)
        return VCS_ERROR_MEMORY_ALLOCATION_FAILED;

    pthread_mutex_init(&(*reader)->mutex, NULL);
    return VCS_OK;
}

vcs_error_t stream_reader_init(stream_reader_t *reader, const char *filepath, size_t buffer_size)
{
    if (!reader || !filepath)
        return VCS_ERROR_NULL_INPUT;
    if (reader->is_initialized)
        return VCS_STREAM_READER_NOT_INITIALIZED;

    pthread_mutex_lock(&reader->mutex);

    reader->buffer_size = buffer_size ? buffer_size : STREAM_DEFAULT_CHUNK_SIZE;
    reader->in_buffer = malloc(reader->buffer_size);
    reader->out_buffer = malloc(reader->buffer_size);
    reader->filepath = strdup(filepath);

    if (!reader->in_buffer || !reader->out_buffer || !reader->filepath)
    {
        stream_reader_close(reader);
        pthread_mutex_unlock(&reader->mutex);
        return VCS_ERROR_MEMORY_ALLOCATION_FAILED;
    }

    reader->file = fopen(filepath, "rb");
    if (!reader->file)
    {
        stream_reader_close(reader);
        pthread_mutex_unlock(&reader->mutex);
        return VCS_ERROR_FILE_OPEN;
    }

    vcs_error_t err = initialize_zlib_stream(reader);
    if (err != VCS_OK)
    {
        stream_reader_close(reader);
        pthread_mutex_unlock(&reader->mutex);
        return err;
    }

    reader->state = STREAM_STATE_READY;
    reader->is_initialized = true;
    pthread_mutex_unlock(&reader->mutex);

    return VCS_OK;
}

vcs_error_t stream_reader_read_chunk(stream_reader_t *reader, void *chunk, size_t chunk_size)
{
    if (!reader || !chunk || !chunk_size)
        return VCS_ERROR_NULL_INPUT;
    if (!reader->is_initialized)
        return VCS_STREAM_READER_NOT_INITIALIZED;
    if (reader->state == STREAM_STATE_ERROR)
        return VCS_ERROR_STREAM_CORRUPT;
    if (reader->state == STREAM_STATE_EOF)
        return VCS_ERROR_EOF;

    pthread_mutex_lock(&reader->mutex);
    unsigned char *dest = (unsigned char *)chunk;
    size_t bytes_written = 0;

    while (bytes_written < chunk_size)
    {
        if (reader->out_pos >= reader->out_size)
        {
            reader->stream.next_out = reader->out_buffer;
            reader->stream.avail_out = reader->buffer_size;
            reader->out_pos = 0;
            reader->out_size = 0;

            if (reader->stream.avail_in == 0)
            {
                size_t bytes_read = fread(reader->in_buffer, 1, reader->buffer_size, reader->file);
                if (bytes_read == 0)
                {
                    if (feof(reader->file))
                    {
                        reader->state = STREAM_STATE_EOF;
                        pthread_mutex_unlock(&reader->mutex);
                        return bytes_written ? VCS_OK : VCS_ERROR_EOF;
                    }
                    reader->state = STREAM_STATE_ERROR;
                    pthread_mutex_unlock(&reader->mutex);
                    return VCS_ERROR_FILE_READ;
                }
                reader->stream.next_in = reader->in_buffer;
                reader->stream.avail_in = bytes_read;
            }

            int ret = inflate(&reader->stream, Z_NO_FLUSH);
            if (ret != Z_OK && ret != Z_STREAM_END)
            {
                reader->state = STREAM_STATE_ERROR;
                pthread_mutex_unlock(&reader->mutex);
                return VCS_ERROR_COMPRESSION;
            }

            reader->out_size = reader->buffer_size - reader->stream.avail_out;
            if (reader->out_size == 0)
            {
                reader->state = STREAM_STATE_EOF;
                pthread_mutex_unlock(&reader->mutex);
                return bytes_written ? VCS_OK : VCS_ERROR_EOF;
            }
        }

        size_t available = reader->out_size - reader->out_pos;
        size_t to_copy = (chunk_size - bytes_written) < available ? (chunk_size - bytes_written) : available;

        memcpy(dest + bytes_written,
               reader->out_buffer + reader->out_pos,
               to_copy);

        reader->out_pos += to_copy;
        bytes_written += to_copy;
    }

    pthread_mutex_unlock(&reader->mutex);
    return VCS_OK;
}

vcs_error_t stream_reader_read_until_null(stream_reader_t *reader, char *buffer, size_t max_size, size_t *bytes_read)
{
    if (!reader || !buffer || !bytes_read)
        return VCS_ERROR_NULL_INPUT;

    *bytes_read = 0;
    char byte;
    vcs_error_t err;

    while (*bytes_read < max_size - 1)
    {
        // Read one byte at a time
        err = stream_reader_read_chunk(reader, &byte, 1);
        if (err != VCS_OK)
            return err;

        if (byte == '\0')
        {
            buffer[*bytes_read] = '\0';
            return VCS_OK;
        }

        buffer[*bytes_read] = byte;
        (*bytes_read)++;
    }

    buffer[*bytes_read] = '\0';
    return VCS_ERROR_BUFFER_OVERFLOW;
}

void stream_reader_close(stream_reader_t *reader)
{
    if (!reader)
        return;

    pthread_mutex_lock(&reader->mutex);

    if (reader->is_initialized)
    {
        inflateEnd(&reader->stream);
    }

    if (reader->file)
    {
        fclose(reader->file);
        reader->file = NULL;
    }

    SAFE_FREE(reader->in_buffer);
    SAFE_FREE(reader->out_buffer);
    SAFE_FREE(reader->filepath);

    reader->state = STREAM_STATE_UNINITIALIZED;
    reader->is_initialized = false;

    pthread_mutex_unlock(&reader->mutex);
}

void stream_reader_destroy(stream_reader_t **reader)
{
    if (!reader || !*reader)
        return;

    stream_reader_close(*reader);
    pthread_mutex_destroy(&(*reader)->mutex);
    free(*reader);
    *reader = NULL;
}