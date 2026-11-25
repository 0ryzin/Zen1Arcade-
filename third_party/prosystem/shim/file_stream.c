/* SPDX-License-Identifier: GPL-3.0-or-later
 * Zen1 Arcade: stdio versions of the libretro-common file functions used
 * by ProSystem's core (see streams/file_stream.h). */
#include "streams/file_stream.h"
#include <stdlib.h>

struct RFILE { FILE* fp; };

RFILE* filestream_open(const char* path, unsigned mode, unsigned hints) {
    (void)mode; (void)hints;
    FILE* fp = fopen(path, "rb");
    if (!fp) return NULL;
    RFILE* f = (RFILE*)malloc(sizeof *f);
    f->fp = fp;
    return f;
}
int64_t filestream_get_size(RFILE* f) {
    long at = ftell(f->fp), size;
    fseek(f->fp, 0, SEEK_END);
    size = ftell(f->fp);
    fseek(f->fp, at, SEEK_SET);
    return size;
}
int filestream_error(RFILE* f) { return ferror(f->fp); }
int filestream_close(RFILE* f) {
    if (!f) return 0;
    fclose(f->fp);
    free(f);
    return 0;
}
int64_t rfread(void* buffer, size_t size, size_t count, RFILE* f) { return (int64_t)fread(buffer, size, count, f->fp); }
int64_t filestream_read_file(const char* path, void** buf, int64_t* len) {
    RFILE* f = filestream_open(path, 0, 0);
    *buf = NULL;
    *len = 0;
    if (!f) return 0;
    int64_t size = filestream_get_size(f);
    unsigned char* data = (unsigned char*)malloc((size_t)size + 1);
    int64_t got = rfread(data, 1, (size_t)size, f);
    filestream_close(f);
    data[got > 0 ? got : 0] = 0;
    *buf = data;
    *len = got;
    return 1;
}
