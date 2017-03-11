/* TyTools - public domain
   Niels Martignène <niels.martignene@gmail.com>
   https://neodd.com/tytools

   This software is in the public domain. Where that dedication is not
   recognized, you are granted a perpetual, irrevocable license to copy,
   distribute, and modify this file as you see fit.

   See the LICENSE file for more details. */

#include "common_priv.h"
#include "class_priv.h"
#include "firmware.h"
#include "system.h"

const ty_firmware_format ty_firmware_formats[] = {
    {"elf",  ".elf", ty_firmware_load_elf},
    {"ihex", ".hex", ty_firmware_load_ihex}
};
const unsigned int ty_firmware_formats_count = TY_COUNTOF(ty_firmware_formats);

#define FIRMWARE_STEP_SIZE 32768

static const char *get_basename(const char *filename)
{
    const char *basename;

    basename = filename + strlen(filename);
    // Skip the separators at the end, if any
    while(basename > filename && strchr(TY_PATH_SEPARATORS, basename[-1]))
        basename--;
    // Find the last path part
    while (basename > filename && !strchr(TY_PATH_SEPARATORS, basename[-1]))
        basename--;

    return basename;
}

int ty_firmware_new(const char *filename, ty_firmware **rfw)
{
    assert(filename);
    assert(rfw);

    ty_firmware *fw;
    int r;

    fw = calloc(1, sizeof(ty_firmware));
    if (!fw) {
        r = ty_error(TY_ERROR_MEMORY, NULL);
        goto error;
    }
    fw->refcount = 1;

    fw->filename = strdup(filename);
    if (!fw->filename) {
        r = ty_error(TY_ERROR_MEMORY, NULL);
        goto error;
    }

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

int ty_firmware_load(const char *filename, const char *format_name, ty_firmware **rfw)
{
    assert(filename);
    assert(rfw);

    const ty_firmware_format *format = NULL;

    if (format_name) {
        for (unsigned int i = 0; i < ty_firmware_formats_count; i++) {
            if (strcasecmp(ty_firmware_formats[i].name, format_name) == 0) {
                format = &ty_firmware_formats[i];
                break;
            }
        }
        if (!format)
            return ty_error(TY_ERROR_UNSUPPORTED, "Firmware file format '%s' unknown", format_name);
    } else {
        const char *ext = strrchr(filename, '.');
        if (!ext)
            return ty_error(TY_ERROR_UNSUPPORTED, "Firmware '%s' has no file extension", filename);

        for (unsigned int i = 0; i < ty_firmware_formats_count; i++) {
            if (strcasecmp(ty_firmware_formats[i].ext, ext) == 0) {
                format = &ty_firmware_formats[i];
                break;
            }
        }
        if (!format)
            return ty_error(TY_ERROR_UNSUPPORTED, "Firmware '%s' uses unrecognized extension",
                            filename);
    }

    return (*format->load)(filename, rfw);
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
        free(fw->filename);
    }

    free(fw);
}

int ty_firmware_expand_image(ty_firmware *fw, size_t size)
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
        fw->alloc_size = alloc_size;
    }
    fw->size = size;

    return 0;
}

unsigned int ty_firmware_identify(const ty_firmware *fw, ty_model *rmodels,
                                  unsigned int max_models)
{
    assert(fw);
    assert(rmodels);
    assert(max_models);

    unsigned int guesses_count = 0;

    for (unsigned int i = 0; i < _ty_class_vtables_count; i++) {
        ty_model partial_guesses[16];
        unsigned int partial_count;

        if (!_ty_class_vtables[i]->identify_models)
            continue;

        partial_count = (*_ty_class_vtables[i]->identify_models)(fw, partial_guesses,
                                                                 TY_COUNTOF(partial_guesses));

        for (unsigned int j = 0; j < partial_count; j++) {
            if (rmodels && guesses_count < max_models)
                rmodels[guesses_count++] = partial_guesses[j];
        }
    }

    return guesses_count;
}
