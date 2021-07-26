#include "core_types.h"

#include "log.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef WIN32
#define WN_OS_SEP "\\"
#else
#define WN_OS_SEP "/"
#endif

typedef struct wn_file_src_t
{
    size_t size;
    char* data;
} wn_file_src_t;

wn_file_src_t wn_file_read(const char* filename)
{
    FILE* file = fopen(filename, "rb");
    char* buffer = NULL;
    wn_file_src_t ret = { 0 };
    if (file)
    {
        fseek(file, 0, SEEK_END);
        size_t file_size = ftell(file);
        rewind(file);
        buffer = (char*)malloc(sizeof(char) * file_size);
        // FIXME
        assert(buffer);
        size_t read_size = fread(buffer, sizeof(char), file_size, file);
        // FIXME
        if (file_size != read_size)
        {
            log_fatal("Error reading shader file %s", filename);
            exit(EXIT_FAILURE);
        }

        ret.data = buffer;
        ret.size = file_size;

        fclose(file);
    }
    else
    {
        log_fatal("Couldn't load file");
        exit(EXIT_FAILURE);
    }

    return ret;
}