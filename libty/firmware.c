/*
 * ty, a collection of GUI and command-line tools to manage Teensy devices
 *
 * Distributed under the MIT license (see LICENSE.txt or http://opensource.org/licenses/MIT)
 * Copyright (c) 2015 Niels Martignène <niels.martignene@gmail.com>
 */

#include "util.h"
#include "firmware_priv.h"
#include "ty/system.h"

int _ty_firmware_load_elf(ty_firmware *fw);
int _ty_firmware_load_ihex(ty_firmware *fw);

const ty_firmware_format ty_firmware_formats[] = {
    {"elf",  ".elf", _ty_firmware_load_elf},
    {"ihex", ".hex", _ty_firmware_load_ihex},
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

int ty_firmware_load(const char *filename, const char *format_name, ty_firmware **rfw)
{
    assert(filename);
    assert(rfw);

    const ty_firmware_format *format;
    ty_firmware *fw = NULL;
    int r;

    if (format_name) {
        for (format = ty_firmware_formats; format->name; format++) {
            if (strcasecmp(format->name, format_name) == 0)
                break;
        }
        if (!format->name) {
            r = ty_error(TY_ERROR_UNSUPPORTED, "Firmware file format '%s' unknown", format_name);
            goto error;
        }
    } else {
        const char *ext = strrchr(filename, '.');
        if (!ext) {
            r = ty_error(TY_ERROR_UNSUPPORTED, "Firmware '%s' has no file extension", filename);
            goto error;
        }

        for (format = ty_firmware_formats; format->name; format++) {
            if (strcmp(format->ext, ext) == 0)
                break;
        }
        if (!format->name) {
            r = ty_error(TY_ERROR_UNSUPPORTED, "Firmware '%s' uses unrecognized file format", filename);
            goto error;
        }
    }

    fw = malloc(sizeof(ty_firmware) + strlen(filename) + 1);
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
    ty_firmware_unref(fw);
    return r;
}

ty_firmware *ty_firmware_ref(ty_firmware *fw)
{
    assert(fw);

    _ty_refcount_increase(&fw->refcount);
    return fw;
}

void ty_firmware_unref(ty_firmware *fw)
{
    if (fw) {
        if (_ty_refcount_decrease(&fw->refcount))
            return;

        free(fw->image);
        free(fw->name);
    }

    free(fw);
}

const char *ty_firmware_get_filename(const ty_firmware *fw)
{
    assert(fw);
    return fw->filename;
}

const char *ty_firmware_get_name(const ty_firmware *fw)
{
    assert(fw);
    return fw->name;
}

size_t ty_firmware_get_size(const ty_firmware *fw)
{
    assert(fw);
    return fw->size;
}

const uint8_t *ty_firmware_get_image(const ty_firmware *fw)
{
    assert(fw);
    return fw->image;
}

int _ty_firmware_expand_image(ty_firmware *fw, size_t size)
{
    if (size > fw->alloc_size) {
        uint8_t *tmp;
        size_t alloc_size;

        if (size > TY_FIRMWARE_MAX_SIZE)
            return ty_error(TY_ERROR_RANGE, "Firmware too big (max %u bytes) in '%s'",
                            TY_FIRMWARE_MAX_SIZE, fw->filename);

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
