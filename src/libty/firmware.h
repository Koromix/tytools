/* TyTools - public domain
   Niels Martignène <niels.martignene@protonmail.com>
   https://koromix.dev/tytools

   This software is in the public domain. Where that dedication is not
   recognized, you are granted a perpetual, irrevocable license to copy,
   distribute, and modify this file as you see fit.

   See the LICENSE file for more details. */

#ifndef TY_FIRMWARE_H
#define TY_FIRMWARE_H

#include "common.h"
#include "class.h"

_HS_BEGIN_C

#define TY_FIRMWARE_MAX_SEGMENTS 16
#define TY_FIRMWARE_MAX_SIZE (32 * 1024 * 1024)

typedef struct ty_firmware_segment {
    uint8_t *data;
    size_t size;
    size_t alloc_size;
    uint32_t address;
} ty_firmware_segment;

typedef struct ty_firmware {
    unsigned int refcount;

    char *name;
    char *filename;

    ty_firmware_segment segments[TY_FIRMWARE_MAX_SEGMENTS];
    unsigned int segments_count;

    size_t max_address;
    size_t total_size;
} ty_firmware;

typedef ssize_t ty_firmware_read_func(int64_t offset, uint8_t *buf, size_t len, void *udata);

typedef struct ty_firmware_format {
    const char *name;
    const char *ext;

    int (*load)(ty_firmware *fw, ty_firmware_read_func *func, void *udata);
} ty_firmware_format;

extern const ty_firmware_format ty_firmware_formats[];
extern const unsigned int ty_firmware_formats_count;

int ty_firmware_new(const char *filename, ty_firmware **rfw);
int ty_firmware_load_file(const char *filename, FILE *fp, const char *format_name,
                          ty_firmware **rfw);
int ty_firmware_load_mem(const char *filename, const uint8_t *mem, size_t len,
                         const char *format_name, ty_firmware **rfw);

int ty_firmware_load_elf(ty_firmware *fw, ty_firmware_read_func *func, void *udata);
int ty_firmware_load_ihex(ty_firmware *fw, ty_firmware_read_func *func, void *udata);

ty_firmware *ty_firmware_ref(ty_firmware *fw);
void ty_firmware_unref(ty_firmware *fw);

const ty_firmware_segment *ty_firmware_find_segment(const ty_firmware *fw, uint32_t address);
size_t ty_firmware_extract(const ty_firmware *fw, uint32_t address, uint8_t *buf, size_t size);

int ty_firmware_add_segment(ty_firmware *fw, uint32_t address, size_t size,
                            ty_firmware_segment **rsegment);
int ty_firmware_expand_segment(ty_firmware *fw, ty_firmware_segment *segment, size_t size);


unsigned int ty_firmware_identify(const ty_firmware *fw, ty_model *rmodels,
                                  unsigned int max_models);

_HS_END_C

#endif
