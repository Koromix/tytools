/*
 * ty, a collection of GUI and command-line tools to manage Teensy devices
 *
 * Distributed under the MIT license (see LICENSE.txt or http://opensource.org/licenses/MIT)
 * Copyright (c) 2015 Niels Martignène <niels.martignene@gmail.com>
 */

#include "util.h"
#include "ty/firmware.h"

struct parser_context {
    ty_firmware *fw;
    unsigned int line;

    const char *ptr;
    uint8_t sum;
    bool error;

    uint32_t base_offset;
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

static int parse_error(struct parser_context *ctx)
{
    return ty_error(TY_ERROR_PARSE, "IHEX parse error on line %u in '%s'", ctx->line,
                    ctx->fw->filename);
}

static int parse_line(struct parser_context *ctx, const char *line)
{
    unsigned int length, type;
    uint32_t address;
    uint8_t checksum;
    int r;

    ctx->ptr = line;
    ctx->sum = 0;
    ctx->error = false;

    // Empty lines are probably OK
    if (*ctx->ptr++ != ':')
        return 0;
    if (strlen(ctx->ptr) < 11)
        return parse_error(ctx);

    length = parse_hex_byte(ctx, true);
    address = parse_hex_short(ctx);
    type = parse_hex_byte(ctx, true);

    if (ctx->error)
        return parse_error(ctx);

    switch (type) {
    case 0: // data record
        address += ctx->base_offset;
        r = ty_firmware_expand_image(ctx->fw, address + length);
        if (r < 0)
            return r;
        for (unsigned int i = 0; i < length; i++)
            ctx->fw->image[address + i] = parse_hex_byte(ctx, true);
        break;

    case 1: // EOF record
        if (length > 0)
            return parse_error(ctx);
        break;

    case 2: // extended segment address record
        if (length != 2)
            return parse_error(ctx);
        ctx->base_offset = (uint32_t)parse_hex_short(ctx) << 4;
        break;
    case 3: // start segment address record
        break;

    case 4: // extended linear address record
        if (length != 2)
            return parse_error(ctx);
        ctx->base_offset = (uint32_t)parse_hex_short(ctx) << 16;
        break;
    case 5: // start linear address record
        break;

    default:
        return parse_error(ctx);
    }

    // Don't checksum the checksum :)
    checksum = parse_hex_byte(ctx, false);

    if (ctx->error)
        return parse_error(ctx);
    if (*ctx->ptr != '\r' && *ctx->ptr != '\n' && *ctx->ptr)
        return parse_error(ctx);
    if (((ctx->sum & 0xFF) + (checksum & 0xFF)) & 0xFF)
        return parse_error(ctx);

    // Return 1 for EOF records, to end the parsing
    return type == 1;
}

int ty_firmware_load_ihex(const char *filename, ty_firmware **rfw)
{
    assert(filename);
    assert(rfw);

    struct parser_context ctx = {0};
    FILE *fp = NULL;
    char buf[1024];
    int r;

    r = ty_firmware_new(filename, &ctx.fw);
    if (r < 0)
        goto cleanup;

#ifdef _WIN32
    fp = fopen(ctx.fw->filename, "r");
#else
    fp = fopen(ctx.fw->filename, "re");
#endif
    if (!fp) {
        switch (errno) {
        case EACCES:
            r = ty_error(TY_ERROR_ACCESS, "Permission denied for '%s'", ctx.fw->filename);
            break;
        case EIO:
            r = ty_error(TY_ERROR_IO, "I/O error while opening '%s' for reading", ctx.fw->filename);
            break;
        case ENOENT:
        case ENOTDIR:
            r = ty_error(TY_ERROR_NOT_FOUND, "File '%s' does not exist", ctx.fw->filename);
            break;

        default:
            r = ty_error(TY_ERROR_SYSTEM, "fopen('%s') failed: %s", ctx.fw->filename,
                         strerror(errno));
            break;
        }
        goto cleanup;
    }

    do {
        if (!fgets(buf, sizeof(buf), fp)) {
            if (feof(fp)) {
                r = parse_error(&ctx);
            } else {
                r = ty_error(TY_ERROR_IO, "I/O error while reading '%s'", ctx.fw->filename);
            }
            goto cleanup;
        }
        ctx.line++;

        // Returns 1 when EOF record is detected
        r = parse_line(&ctx, buf);
        if (r < 0)
            goto cleanup;
    } while (!r);

    *rfw = ctx.fw;
    ctx.fw = NULL;

    r = 0;
cleanup:
    if (fp)
        fclose(fp);
    ty_firmware_unref(ctx.fw);
    return r;
}
