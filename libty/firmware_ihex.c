/*
 * ty, a collection of GUI and command-line tools to manage Teensy devices
 *
 * Distributed under the MIT license (see LICENSE.txt or http://opensource.org/licenses/MIT)
 * Copyright (c) 2015 Niels Martignène <niels.martignene@gmail.com>
 */

#include "ty/common.h"
#include "compat.h"
#include "firmware_priv.h"

struct parser_context {
    ty_firmware *fw;

    uint32_t base_offset;

    unsigned int line;

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
        goto parse_error;

    length = parse_hex_byte(ctx, true);
    address = parse_hex_short(ctx);
    type = parse_hex_byte(ctx, true);

    if (ctx->error)
        goto parse_error;

    switch (type) {
    case 0: // data record
        address += ctx->base_offset;
        r = _ty_firmware_expand_image(ctx->fw, address + length);
        if (r < 0)
            return r;
        for (unsigned int i = 0; i < length; i++)
            ctx->fw->image[address + i] = parse_hex_byte(ctx, true);
        break;

    case 1: // EOF record
        if (length > 0)
            goto parse_error;
        return 1;

    case 2: // extended segment address record
        if (length != 2)
            goto parse_error;
        ctx->base_offset = (uint32_t)parse_hex_short(ctx) << 4;
        break;
    case 3: // start segment address record
        break;

    case 4: // extended linear address record
        if (length != 2)
            goto parse_error;
        ctx->base_offset = (uint32_t)parse_hex_short(ctx) << 16;
        break;
    case 5: // start linear address record
        break;

    default:
        goto parse_error;
    }

    // Don't checksum the checksum :)
    checksum = parse_hex_byte(ctx, false);

    if (ctx->error)
        goto parse_error;
    if (((ctx->sum & 0xFF) + (checksum & 0xFF)) & 0xFF)
        goto parse_error;

    // 0 to continue, 1 to stop (EOF record) and negative for errors
    return 0;

parse_error:
    return ty_error(TY_ERROR_PARSE, "Parse error (Intel HEX) on line %u in '%s'\n", ctx->line,
                    ctx->fw->filename);
}

int _ty_firmware_load_ihex(ty_firmware *fw)
{
    assert(fw);

    struct parser_context ctx = {0};
    FILE *fp = NULL;
    char buf[1024];
    int r;

    ctx.fw = fw;

#ifdef _WIN32
    fp = fopen(fw->filename, "r");
#else
    fp = fopen(fw->filename, "re");
#endif
    if (!fp) {
        switch (errno) {
        case EACCES:
            r = ty_error(TY_ERROR_ACCESS, "Permission denied for '%s'", fw->filename);
            break;
        case EIO:
            r = ty_error(TY_ERROR_IO, "I/O error while opening '%s' for reading", fw->filename);
            break;
        case ENOENT:
        case ENOTDIR:
            r = ty_error(TY_ERROR_NOT_FOUND, "File '%s' does not exist", fw->filename);
            break;

        default:
            r = ty_error(TY_ERROR_SYSTEM, "fopen('%s') failed: %s", fw->filename, strerror(errno));
            break;
        }
        goto cleanup;
    }

    while (!feof(fp)) {
        if (!fgets(buf, sizeof(buf), fp))
            break;
        ctx.line++;

        r = parse_line(&ctx, buf);
        if (r < 0)
            goto cleanup;

        /* Either EOF record or real EOF will do, albeit the first is probably better (guarantees
           the file is complete). */
        if (r || feof(fp))
            break;
    }

    r = 0;
cleanup:
    if (fp)
        fclose(fp);
    return r;
}
