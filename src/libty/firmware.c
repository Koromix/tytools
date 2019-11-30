/* TyTools - public domain
   Niels Martignène <niels.martignene@protonmail.com>
   https://koromix.dev/tytools

   This software is in the public domain. Where that dedication is not
   recognized, you are granted a perpetual, irrevocable license to copy,
   distribute, and modify this file as you see fit.

   See the LICENSE file for more details. */

#include "common_priv.h"
#include "../libhs/array.h"
#include "class_priv.h"
#include "firmware.h"
#include "system.h"

const ty_firmware_format ty_firmware_formats[] = {
    {"elf",  ".elf", ty_firmware_load_elf},
    {"ihex", ".hex", ty_firmware_load_ihex}
};
const unsigned int ty_firmware_formats_count = TY_COUNTOF(ty_firmware_formats);

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

static int find_format(const char *filename, const char *format_name,
                       const ty_firmware_format **rformat)
{
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

    *rformat = format;
    return 0;
}

int ty_firmware_load_file(const char *filename, FILE *fp, const char *format_name,
                          ty_firmware **rfw)
{
    assert(filename);
    assert(rfw);

    const ty_firmware_format *format;
    bool close_fp = false;
    _HS_ARRAY(uint8_t) buf = {0};
    ty_firmware *fw = NULL;
    int r;

    r = find_format(filename, format_name, &format);
    if (r < 0)
        goto cleanup;

    if (!fp) {
#ifdef _WIN32
        fp = fopen(filename, "rb");
#else
        fp = fopen(filename, "rbe");
#endif
        if (!fp) {
            switch (errno) {
                case EACCES: {
                    r = ty_error(TY_ERROR_ACCESS, "Permission denied for '%s'", filename);
                } break;
                case EIO: {
                    r = ty_error(TY_ERROR_IO, "I/O error while opening '%s' for reading", filename);
                } break;
                case ENOENT:
                case ENOTDIR: {
                    r = ty_error(TY_ERROR_NOT_FOUND, "File '%s' does not exist", filename);
                } break;

                default: {
                    r = ty_error(TY_ERROR_SYSTEM, "fopen('%s') failed: %s", filename,
                                 strerror(errno));
                } break;
            }
            goto cleanup;
        }
        close_fp = true;
    }

    // Load file to memory
    while (!feof(fp)) {
        r = _hs_array_grow(&buf, 128 * 1024);
        if (r < 0)
            goto cleanup;

        buf.count += fread(buf.values + buf.count, 1, 131072, fp);
        if (ferror(fp)) {
            if (errno == EIO) {
                r = ty_error(TY_ERROR_IO, "I/O error while reading from '%s'", filename);
            } else {
                r = ty_error(TY_ERROR_SYSTEM, "fread('%s') failed: %s", filename, strerror(errno));
            }
            goto cleanup;
        }
        if (buf.count > 8 * 1024 * 1024) {
            r = ty_error(TY_ERROR_RANGE, "Firmware '%s' is too big to load", filename);
            goto cleanup;
        }
    }
    _hs_array_shrink(&buf);

    r = ty_firmware_new(filename, &fw);
    if (r < 0)
        goto cleanup;

    r = (*format->load)(fw, buf.values, buf.count);
    if (r < 0)
        goto cleanup;

    *rfw = fw;
    fw = NULL;

cleanup:
    ty_firmware_unref(fw);
    if (close_fp)
        fclose(fp);
    _hs_array_release(&buf);
    return r;
}

int ty_firmware_load_mem(const char *filename, const uint8_t *mem, size_t len,
                         const char *format_name, ty_firmware **rfw)
{
    assert(filename);
    assert(mem || !len);
    assert(rfw);

    const ty_firmware_format *format;
    ty_firmware *fw = NULL;
    int r;

    r = find_format(filename, format_name, &format);
    if (r < 0)
        goto cleanup;

    r = ty_firmware_new(filename, &fw);
    if (r < 0)
        goto cleanup;

    r = (*format->load)(fw, mem, len);
    if (r < 0)
        goto cleanup;

    *rfw = fw;
    fw = NULL;

cleanup:
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

        for (unsigned int i = 0; i < fw->segments_count; i++)
            free(fw->segments[i].data);
        free(fw->name);
        free(fw->filename);
    }

    free(fw);
}

const ty_firmware_segment *ty_firmware_find_segment(const ty_firmware *fw, uint32_t address)
{
    assert(fw);

    for (unsigned int i = fw->segments_count; i-- > 0;) {
        const ty_firmware_segment *segment = &fw->segments[i];

        if (address >= segment->address && address < segment->address + segment->size)
            return segment;
    }

    return NULL;
}

size_t ty_firmware_extract(const ty_firmware *fw, uint32_t address, uint8_t *buf, size_t size)
{
    assert(fw);

    size_t total_len = 0;
    for (unsigned int i = 0; i < fw->segments_count; i++) {
        const ty_firmware_segment *segment = &fw->segments[i];

        if (address >= segment->address && address < segment->address + segment->size) {
            size_t delta = address - segment->address;
            size_t len = TY_MIN(segment->size - delta, size);

            memcpy(buf, segment->data + delta, len);
            total_len += len;
        } else if (address < segment->address && address + size > segment->address) {
            size_t delta = segment->address - address;
            size_t len = TY_MIN(segment->size, size - delta);

            memcpy(buf + delta, segment->data, len);
            total_len += len;
        }
    }

    return total_len;
}

int ty_firmware_add_segment(ty_firmware *fw, uint32_t address, size_t size,
                            ty_firmware_segment **rsegment)
{
    assert(fw);

    ty_firmware_segment *segment;
    int r;

    if (fw->segments_count >= TY_FIRMWARE_MAX_SEGMENTS)
        return ty_error(TY_ERROR_RANGE, "Firmware '%s' has too many segments", fw->filename);

    segment = &fw->segments[fw->segments_count];
    segment->address = address;

    r = ty_firmware_expand_segment(fw, segment, size);
    if (r < 0)
        return r;

    fw->segments_count++;

    if (rsegment)
        *rsegment = segment;
    return 0;
}

int ty_firmware_expand_segment(ty_firmware *fw, ty_firmware_segment *segment, size_t size)
{
    const size_t step_size = 65536;

    if (size > segment->alloc_size) {
        uint8_t *tmp;
        size_t alloc_size;

        if (size > TY_FIRMWARE_MAX_SEGMENT_SIZE)
            return ty_error(TY_ERROR_RANGE, "Firmware '%s' has excessive segment size (max %u bytes)",
                            fw->filename, TY_FIRMWARE_MAX_SEGMENT_SIZE);

        alloc_size = (size + (step_size - 1)) / step_size * step_size;
        tmp = realloc(segment->data, alloc_size);
        if (!tmp)
            return ty_error(TY_ERROR_MEMORY, NULL);

        segment->data = tmp;
        segment->alloc_size = alloc_size;
    }
    segment->size = size;

    return 0;
}

unsigned int ty_firmware_identify(const ty_firmware *fw, ty_model *rmodels,
                                  unsigned int max_models)
{
    assert(fw);
    assert(rmodels);
    assert(max_models);

    unsigned int guesses_count = 0;

    for (unsigned int i = 0; i < _ty_classes_count; i++) {
        ty_model partial_guesses[16];
        unsigned int partial_count;

        if (!_ty_classes[i].vtable->identify_models)
            continue;

        partial_count = (*_ty_classes[i].vtable->identify_models)(fw, partial_guesses,
                                                                  TY_COUNTOF(partial_guesses));

        for (unsigned int j = 0; j < partial_count; j++) {
            if (rmodels && guesses_count < max_models)
                rmodels[guesses_count++] = partial_guesses[j];
        }
    }

    return guesses_count;
}
