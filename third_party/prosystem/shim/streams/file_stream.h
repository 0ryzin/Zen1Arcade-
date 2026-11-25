/* Zen1 Arcade: the few libretro-common file functions ProSystem's core
 * uses, on plain C stdio (GPL-3.0-or-later, Zen1's own). */
#ifndef ZEN1_PROSYSTEM_FILE_STREAM_H
#define ZEN1_PROSYSTEM_FILE_STREAM_H
#include <stdint.h>
#include <stdio.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct RFILE RFILE;
#define RETRO_VFS_FILE_ACCESS_READ 1
#define RETRO_VFS_FILE_ACCESS_HINT_NONE 0
RFILE* filestream_open(const char* path, unsigned mode, unsigned hints);
int64_t filestream_get_size(RFILE* f);
int filestream_error(RFILE* f);
int filestream_close(RFILE* f);
int64_t rfread(void* buffer, size_t size, size_t count, RFILE* f);
int64_t filestream_read_file(const char* path, void** buf, int64_t* len);
#ifdef __cplusplus
}
#endif
#endif
