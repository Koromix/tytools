/**
 * ty, command-line program to manage Teensy devices
 * http://github.com/Koromix/ty
 * Copyright (C) 2014 Niels Martignène
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "ty/common.h"
#include "compat.h"
#include "ty/firmware.h"

struct parser_context {
    ty_firmware *f;

    uint32_t base_offset;

    size_t line;

    const char *ptr;
    uint8_t sum;
    bool error;
};

static uint8_t parse_hex_byte(struct parser_context *ctx, bool checksum)
{
    if (ctx->error)
        return 0;

    uint8_t value;
    int r;

    r = sscanf(ctx->ptr, "%02"SCNx8, &value);
    if (r < 1) {
        ctx->error = true;
        return 0;
    }
    ctx->ptr += 2;

    if (checksum)
        ctx->sum = (uint8_t)(ctx->sum + value);

    return (uint8_t)value;
}

static uint16_t parse_hex_short(struct parser_context *ctx)
{
    return (uint16_t)((parse_hex_byte(ctx, true) << 8) | parse_hex_byte(ctx, true));
}

static int parse_line(struct parser_context *ctx, const char *line)
{
    uint8_t length, type;
    uint32_t address;
    uint8_t checksum;

    ctx->ptr = line;
    ctx->sum = 0;
    ctx->error = false;

    // Empty lines are probably OK
    if (*ctx->ptr++ != ':')
        return 1;
    if (strlen(ctx->ptr) < 11)
        return TY_ERROR_PARSE;

    length = parse_hex_byte(ctx, true);
    address = parse_hex_short(ctx);
    type = parse_hex_byte(ctx, true);

    if (ctx->error)
        return TY_ERROR_PARSE;

    switch (type) {
    case 0: // data record
        address += ctx->base_offset;
        if (address + length > ctx->f->size) {
            ctx->f->size = address + length;

            if (ctx->f->size > ty_firmware_max_size)
                return ty_error(TY_ERROR_RANGE, "Firmware too big (max %zu bytes)",
                                ty_firmware_max_size);
        }

        for (uint8_t i = 0; i < length; i++)
            ctx->f->image[address + i] = parse_hex_byte(ctx, true);
        break;

    case 1: // EOF record
        if (length > 0)
            return TY_ERROR_PARSE;
        return 0;

    case 2: // extended segment address record
        if (length != 2)
            return TY_ERROR_PARSE;
        ctx->base_offset = (uint32_t)parse_hex_short(ctx) << 4;
        break;
    case 3: // start segment address record
        break;

    case 4: // extended linear address record
        if (length != 2)
            return TY_ERROR_PARSE;
        ctx->base_offset = (uint32_t)parse_hex_short(ctx) << 16;
        break;
    case 5: // start linear address record
        break;

    default:
        return TY_ERROR_PARSE;
    }

    // Don't checksum the checksum :)
    checksum = parse_hex_byte(ctx, false);

    if (ctx->error)
        return TY_ERROR_PARSE;

    if (((ctx->sum & 0xFF) + (checksum & 0xFF)) & 0xFF)
        return TY_ERROR_PARSE;

    // 1 to continue, 0 to stop (EOF record) and negative for errors
    return 1;
}

int ty_firmware_load_ihex(const char *filename, ty_firmware **rfirmware)
{
    assert(rfirmware);
    assert(filename);

    struct parser_context ctx = {0};
    FILE *fp = NULL;
    char buf[1024];
    int r;

    ctx.f = malloc(sizeof(ty_firmware) + ty_firmware_max_size);
    if (!ctx.f)
        return ty_error(TY_ERROR_MEMORY, NULL);
    memset(ctx.f, 0, sizeof(*ctx.f));
    memset(ctx.f->image, 0xFF, ty_firmware_max_size);

#ifdef _WIN32
    fp = fopen(filename, "r");
#else
    fp = fopen(filename, "re");
#endif
    if (!fp) {
        switch (errno) {
        case EACCES:
            r = ty_error(TY_ERROR_ACCESS, "Permission denied for '%s'", filename);
            break;
        case EIO:
            r = ty_error(TY_ERROR_IO, "I/O error while opening '%s' for reading", filename);
            break;
        case ENOENT:
        case ENOTDIR:
            r = ty_error(TY_ERROR_NOT_FOUND, "File '%s' does not exist", filename);
            break;

        default:
            r = ty_error(TY_ERROR_SYSTEM, "fopen('%s') failed: %s", filename, strerror(errno));
            break;
        }
        goto cleanup;
    }

    while (!feof(fp)) {
        if (!fgets(buf, sizeof(buf), fp))
            break;
        ctx.line++;

        r = parse_line(&ctx, buf);
        if (r < 0) {
            if (r == TY_ERROR_PARSE)
                ty_error(r, "Parse error (Intel HEX) on line %zu in '%s'\n", ctx.line, filename);
            goto cleanup;
        }

        // Either EOF record or real EOF will do, albeit the first is probably
        // better (guarantees the file is complete)
        if (r == 0 || feof(fp))
            break;
    }

    *rfirmware = ctx.f;
    ctx.f = NULL;

    r = 0;
cleanup:
    if (fp)
        fclose(fp);
    free(ctx.f);
    return r;
}
