/*
 * ty, a collection of GUI and command-line tools to manage Teensy devices
 *
 * Distributed under the MIT license (see LICENSE.txt or http://opensource.org/licenses/MIT)
 * Copyright (c) 2015 Niels Martignène <niels.martignene@gmail.com>
 */

#include "ty/common.h"
#include "compat.h"
#include "firmware_priv.h"
#include "ty/system.h"

int _tyb_firmware_load_elf(tyb_firmware *fw);
int _tyb_firmware_load_ihex(tyb_firmware *fw);

const tyb_firmware_format tyb_firmware_formats[] = {
    {"elf",  ".elf", _tyb_firmware_load_elf},
    {"ihex", ".hex", _tyb_firmware_load_ihex},
    {0}
};

#define FIRMWARE_STEP_SIZE 32768

static const char *get_basename(const char *filename)
{
    const char *basename = strrpbrk(filename, TY_PATH_SEPARATORS);
    if (!basename)
        return filename;

    return basename + 1;
}

int tyb_firmware_load(const char *filename, const char *format_name, tyb_firmware **rfw)
{
    assert(filename);
    assert(rfw);

    const tyb_firmware_format *format;
    tyb_firmware *fw = NULL;
    int r;

    if (format_name) {
        for (format = tyb_firmware_formats; format->name; format++) {
            if (strcasecmp(format->name, format_name) == 0)
                break;
        }
        if (!format->name) {
            r = ty_error(TY_ERROR_UNSUPPORTED, "Firmware file format '%s' unknown", format_name);
            goto error;
        }
    } else {
        const char *ext = strrchr(filename, '.');

        for (format = tyb_firmware_formats; format->name; format++) {
            if (strcmp(format->ext, ext) == 0)
                break;
        }
        if (!format->name) {
            r = ty_error(TY_ERROR_UNSUPPORTED, "Firmware '%s' uses unrecognized file format", filename);
            goto error;
        }
    }

    fw = malloc(sizeof(tyb_firmware) + strlen(filename) + 1);
    if (!fw) {
        r = ty_error(TY_ERROR_MEMORY, NULL);
        goto error;
    }
    memset(fw, 0, sizeof(*fw));
    fw->refcount = 1;
    strcpy(fw->filename, filename);

    r = (*format->load)(fw);
    if (r < 0)
        goto error;

    if (!fw->name) {
        fw->name = strdup(get_basename(filename));
        if (!fw->name) {
            r = ty_error(TY_ERROR_MEMORY, NULL);
            goto error;
        }
    }

    *rfw = fw;
    return 0;

error:
    tyb_firmware_unref(fw);
    return r;
}

tyb_firmware *tyb_firmware_ref(tyb_firmware *fw)
{
    assert(fw);

    __atomic_fetch_add(&fw->refcount, 1, __ATOMIC_RELAXED);
    return fw;
}

void tyb_firmware_unref(tyb_firmware *fw)
{
    if (fw) {
        if (__atomic_fetch_sub(&fw->refcount, 1, __ATOMIC_RELEASE) > 1)
            return;
        __atomic_thread_fence(__ATOMIC_ACQUIRE);

        free(fw->image);
        free(fw->name);
    }

    free(fw);
}

const char *tyb_firmware_get_filename(const tyb_firmware *fw)
{
    assert(fw);
    return fw->filename;
}

const char *tyb_firmware_get_name(const tyb_firmware *fw)
{
    assert(fw);
    return fw->name;
}

size_t tyb_firmware_get_size(const tyb_firmware *fw)
{
    assert(fw);
    return fw->size;
}

const uint8_t *tyb_firmware_get_image(const tyb_firmware *fw)
{
    assert(fw);
    return fw->image;
}

int _tyb_firmware_expand_image(tyb_firmware *fw, size_t size)
{
    if (size > fw->alloc_size) {
        uint8_t *tmp;
        size_t alloc_size;

        if (size > TYB_FIRMWARE_MAX_SIZE)
            return ty_error(TY_ERROR_RANGE, "Firmware too big (max %u bytes) in '%s'",
                            TYB_FIRMWARE_MAX_SIZE, fw->filename);

        alloc_size = (size + (FIRMWARE_STEP_SIZE - 1)) / FIRMWARE_STEP_SIZE * FIRMWARE_STEP_SIZE;
        tmp = realloc(fw->image, alloc_size);
        if (!tmp)
            return ty_error(TY_ERROR_MEMORY, NULL);
        fw->image = tmp;

        fw->alloc_size = size;
        fw->size = size;
    }

    return 0;
}
