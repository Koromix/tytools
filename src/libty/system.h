/* TyTools - public domain
   Niels Martignène <niels.martignene@protonmail.com>
   https://koromix.dev/tytools

   This software is in the public domain. Where that dedication is not
   recognized, you are granted a perpetual, irrevocable license to copy,
   distribute, and modify this file as you see fit.

   See the LICENSE file for more details. */

#ifndef TY_SYSTEM_H
#define TY_SYSTEM_H

#include "common.h"
#include <sys/types.h>

TY_C_BEGIN

#ifdef _WIN32
    #define TY_PATH_SEPARATORS "\\/"
#else
    #define TY_PATH_SEPARATORS "/"
#endif
#define TY_PATH_MAX_SIZE 4096

#ifdef _WIN32
typedef void *ty_descriptor; // HANDLE
#else
typedef int ty_descriptor;
#endif

typedef enum ty_standard_stream {
    TY_STREAM_INPUT = 0,
    TY_STREAM_OUTPUT = 1,
    TY_STREAM_ERROR = 2
} ty_standard_stream;

typedef enum ty_standard_path {
    TY_PATH_EXECUTABLE_DIRECTORY = 0,
    TY_PATH_CONFIG_DIRECTORY
} ty_standard_path;

enum {
    TY_DESCRIPTOR_MODE_FIFO = 1,
    TY_DESCRIPTOR_MODE_DEVICE = 2,
    TY_DESCRIPTOR_MODE_TERMINAL = 4,
    TY_DESCRIPTOR_MODE_FILE = 8
};

typedef struct ty_descriptor_set {
    unsigned int count;
    ty_descriptor desc[64];
    int id[64];
} ty_descriptor_set;

enum {
    TY_TERMINAL_RAW = 0x1,
    TY_TERMINAL_SILENT = 0x2
};

#ifdef _WIN32
char *ty_win32_strerror(unsigned long err);
#endif

uint64_t ty_millis(void);
void ty_delay(unsigned int ms);

int ty_adjust_timeout(int timeout, uint64_t start);

void ty_descriptor_set_clear(ty_descriptor_set *set);
void ty_descriptor_set_add(ty_descriptor_set *set, ty_descriptor desc, int id);
void ty_descriptor_set_remove(ty_descriptor_set *set, int id);

unsigned int ty_descriptor_get_modes(ty_descriptor desc);

ty_descriptor ty_standard_get_descriptor(ty_standard_stream std_stream);
static inline unsigned int ty_standard_get_modes(ty_standard_stream std_stream)
{
    return ty_descriptor_get_modes(ty_standard_get_descriptor(std_stream));
}
unsigned int ty_standard_get_paths(ty_standard_path std_path, const char *suffix,
                                   char (*rpaths)[TY_PATH_MAX_SIZE], unsigned int max_paths);

int ty_poll(const ty_descriptor_set *set, int timeout);

bool ty_compare_paths(const char *path1, const char *path2);

int ty_terminal_setup(int flags);
void ty_terminal_restore(void);

TY_C_END

#endif
