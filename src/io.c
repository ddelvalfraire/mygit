#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>
#include <zlib.h>

#include "io.h"
#include "config.h"

vcs_error_t compress_file(const char *input_path, const char *output_path) {
    FILE *source = fopen(input_path, "rb");
    if (!source) return VCS_ERROR_FILE_DOES_NOT_EXIST;

    FILE *dest = fopen(output_path, "wb");
    if (!dest) {
        fclose(source);
        return VCS_ERROR_IO;
    }

    z_stream strm = {0};
    if (deflateInit(&strm, Z_DEFAULT_COMPRESSION) != Z_OK) {
        fclose(source);
        fclose(dest);
        return VCS_ERROR_COMPRESSION_FAILED;
    }

    unsigned char in[CHUNK_SIZE];
    unsigned char out[CHUNK_SIZE];
    int ret;

    do {
        strm.avail_in = fread(in, 1, CHUNK_SIZE, source);
        if (ferror(source)) {
            deflateEnd(&strm);
            fclose(source);
            fclose(dest);
            return VCS_ERROR_IO;
        }

        strm.next_in = in;
        do {
            strm.avail_out = CHUNK_SIZE;
            strm.next_out = out;

            ret = deflate(&strm, feof(source) ? Z_FINISH : Z_NO_FLUSH);
            if (ret == Z_STREAM_ERROR) {
                deflateEnd(&strm);
                fclose(source);
                fclose(dest);
                return VCS_ERROR_COMPRESSION_FAILED;
            }

            size_t have = CHUNK_SIZE - strm.avail_out;
            if (fwrite(out, 1, have, dest) != have || ferror(dest)) {
                deflateEnd(&strm);
                fclose(source);
                fclose(dest);
                return VCS_ERROR_IO;
            }
        } while (strm.avail_out == 0);
    } while (!feof(source));

    deflateEnd(&strm);
    fclose(source);
    fclose(dest);
    return VCS_OK;
}

vcs_error_t compress_and_move(const char *src_path, const char *dest_path) {
    vcs_error_t err = compress_file(src_path, dest_path);
    if (err != VCS_OK) return err;
    
    if (remove(src_path) != 0) {
        return VCS_ERROR_IO;
    }
    return VCS_OK;
}
 
file_type_t get_file_type(const char *path)
{
    struct stat st;
    if (stat(path, &st) != 0)
        return FILE_TYPE_INVALID;
    if (S_ISREG(st.st_mode))
        return FILE_TYPE_REGULAR;
    if (S_ISDIR(st.st_mode))
        return FILE_TYPE_DIRECTORY;
    return FILE_TYPE_INVALID;
}

bool file_exists(const char *path)
{
    struct stat st;
    return stat(path, &st) == 0;
}

void clear_terminal()
{
// Use system commands to clear the terminal
#ifdef _WIN32
    system("cls"); // Windows
#else
    system("clear"); // Unix/Linux/Mac
#endif
}

vcs_error_t compress_file_inplace(const char *filepath)
{
    if (!filepath)
        return VCS_ERROR_NULL_INPUT;

    // Create temp file path
    char temp_path[MAX_PATH_LENGTH];
    snprintf(temp_path, sizeof(temp_path), "%s.tmp", filepath);

    // Compress to temp file
    vcs_error_t err = compress_file(filepath, temp_path);
    if (err != VCS_OK)
    {
        remove(temp_path);
        return err;
    }

    // Replace original with compressed
    if (rename(temp_path, filepath) != 0)
    {
        remove(temp_path);
        return VCS_ERROR_IO;
    }

    return VCS_OK;
}

vcs_error_t append_file(const char *src_path, const char *dest_path)
{
    if (!src_path || !dest_path)
        return VCS_ERROR_NULL_INPUT;

    FILE *source = fopen(src_path, "rb");
    if (!source)
        return VCS_ERROR_FILE_OPEN;

    FILE *dest = fopen(dest_path, "ab");
    if (!dest)
    {
        fclose(source);
        return VCS_ERROR_FILE_OPEN;
    }

    unsigned char buffer[CHUNK_SIZE];
    size_t bytes_read;

    while ((bytes_read = fread(buffer, 1, CHUNK_SIZE, source)) > 0)
    {
        if (fwrite(buffer, 1, bytes_read, dest) != bytes_read)
        {
            fclose(source);
            fclose(dest);
            return VCS_ERROR_IO;
        }
    }

    if (ferror(source))
    {
        fclose(source);
        fclose(dest);
        return VCS_ERROR_IO;
    }

    fclose(source);
    fclose(dest);
    return VCS_OK;
}

