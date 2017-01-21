/* Teensy Tools - public domain
   Niels Martignène <niels.martignene@gmail.com>
   https://neodd.com/teensytools

   This software is in the public domain. Where that dedication is not
   recognized, you are granted a perpetual, irrevocable license to copy,
   distribute, and modify this file as you see fit.

   See the LICENSE file for more details. */

#ifndef TY_FIRMWARE_H
#define TY_FIRMWARE_H

#include "common.h"
#include "model.h"

TY_C_BEGIN

typedef struct ty_firmware {
    unsigned int refcount;

    char *name;
    char *filename;
    uint8_t *image;
    size_t size;

    size_t alloc_size;
} ty_firmware;

typedef struct ty_firmware_format {
    const char *name;
    const char *ext;

    int (*load)(const char *filename, ty_firmware **rfw);
} ty_firmware_format;

TY_PUBLIC extern const ty_firmware_format ty_firmware_formats[];
TY_PUBLIC extern const unsigned int ty_firmware_formats_count;

#define TY_FIRMWARE_MAX_SIZE (1024 * 1024)

TY_PUBLIC int ty_firmware_new(const char *filename, ty_firmware **rfw);

TY_PUBLIC int ty_firmware_load(const char *filename, const char *format_name, ty_firmware **rfw);
TY_PUBLIC int ty_firmware_load_elf(const char *filename, ty_firmware **rfw);
TY_PUBLIC int ty_firmware_load_ihex(const char *filename, ty_firmware **rfw);

TY_PUBLIC ty_firmware *ty_firmware_ref(ty_firmware *fw);
TY_PUBLIC void ty_firmware_unref(ty_firmware *fw);

TY_PUBLIC int ty_firmware_expand_image(ty_firmware *fw, size_t size);

TY_PUBLIC unsigned int ty_firmware_identify(const ty_firmware *fw, ty_model *rmodels,
                                            unsigned int max_models);

TY_C_END

#endif
