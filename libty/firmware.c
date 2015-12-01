/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "ty/common.h"
#include "compat.h"
#include "firmware_priv.h"
#include "ty/system.h"

int _tyb_firmware_load_elf(tyb_firmware *firmware, const char *filename);
int _tyb_firmware_load_ihex(tyb_firmware *firmware, const char *filename);

const tyb_firmware_format tyb_firmware_formats[] = {
    {"elf",  ".elf", _tyb_firmware_load_elf},
    {"ihex", ".hex", _tyb_firmware_load_ihex},
    {0}
};

static const char *get_basename(const char *filename)
{
    const char *basename = strrpbrk(filename, TY_PATH_SEPARATORS);
    if (!basename)
        return filename;

    return basename + 1;
}

int tyb_firmware_load(const char *filename, const char *format_name, tyb_firmware **rfirmware)
{
    assert(filename);
    assert(rfirmware);

    const tyb_firmware_format *format;
    tyb_firmware *firmware = NULL;
    int r;

    format = tyb_firmware_formats;
    if (format_name) {
        for (; format->name; format++) {
            if (strcasecmp(format->name, format_name) == 0)
                break;
        }
        if (!format->name) {
            r = ty_error(TY_ERROR_UNSUPPORTED, "Firmware file format '%s' unknown", format_name);
            goto error;
        }
    } else {
        const char *ext = strrchr(filename, '.');

        for (; format->name; format++) {
            if (strcmp(format->ext, ext) == 0)
                break;
        }
        if (!format->name) {
            r = ty_error(TY_ERROR_UNSUPPORTED, "Firmware '%s' uses unrecognized file format", filename);
            goto error;
        }
    }

    firmware = malloc(sizeof(tyb_firmware) + TYB_FIRMWARE_MAX_SIZE);
    if (!firmware) {
        r = ty_error(TY_ERROR_MEMORY, NULL);
        goto error;
    }
    memset(firmware, 0, sizeof(*firmware));
    memset(firmware->image, 0xFF, TYB_FIRMWARE_MAX_SIZE);
    firmware->refcount = 1;

    r = (*format->load)(firmware, filename);
    if (r < 0)
        goto error;

    if (!firmware->name) {
        firmware->name = strdup(get_basename(filename));
        if (!firmware->name) {
            r = ty_error(TY_ERROR_MEMORY, NULL);
            goto error;
        }
    }

    *rfirmware = firmware;
    return 0;

error:
    tyb_firmware_unref(firmware);
    return r;
}

tyb_firmware *tyb_firmware_ref(tyb_firmware *firmware)
{
    assert(firmware);

    __atomic_fetch_add(&firmware->refcount, 1, __ATOMIC_RELAXED);
    return firmware;
}

void tyb_firmware_unref(tyb_firmware *firmware)
{
    if (firmware) {
        if (__atomic_fetch_sub(&firmware->refcount, 1, __ATOMIC_RELEASE) > 1)
            return;
        __atomic_thread_fence(__ATOMIC_ACQUIRE);

        free(firmware->name);
    }

    free(firmware);
}

const char *tyb_firmware_get_name(const tyb_firmware *firmware)
{
    assert(firmware);
    return firmware->name;
}

size_t tyb_firmware_get_size(const tyb_firmware *firmware)
{
    assert(firmware);
    return firmware->size;
}

const uint8_t *tyb_firmware_get_image(const tyb_firmware *firmware)
{
    assert(firmware);
    return firmware->image;
}
