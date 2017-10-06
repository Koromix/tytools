/* TyTools - public domain
   Niels Martignène <niels.martignene@protonmail.com>
   https://neodd.com/tytools

   This software is in the public domain. Where that dedication is not
   recognized, you are granted a perpetual, irrevocable license to copy,
   distribute, and modify this file as you see fit.

   See the LICENSE file for more details. */

#include "common_priv.h"
#include "firmware.h"

struct parser_context {
    ty_firmware *fw;
    unsigned int line;

    const char *ptr;
    size_t line_len;
    uint8_t sum;
    bool error;

    uint32_t base_offset;
};

static uint32_t parse_hex_value(struct parser_context *ctx, size_t size)
{
    if (ctx->error)
        return 0;

    uint32_t value = 0;
    while (size--) {
        uint8_t byte;
        int r = sscanf(ctx->ptr, "%02"SCNx8, &byte);
        if (r < 1) {
            ctx->error = true;
            return 0;
        }
        value = (value << 8) | byte;
        ctx->sum = (uint8_t)(ctx->sum + byte);
        ctx->ptr += 2;
    }

    return value;
}

static int ihex_parse_error(struct parser_context *ctx)
{
    return ty_error(TY_ERROR_PARSE, "IHEX parse error on line %u in '%s'", ctx->line,
                    ctx->fw->filename);
}

static int parse_line(struct parser_context *ctx, const char *line)
{
    unsigned int data_len, type;
    uint32_t address;
    uint8_t sum, checksum;
    int r;

    ctx->ptr = line;
    ctx->line_len = strlen(line);
    while (ctx->line_len && strchr("\r\n", ctx->ptr[ctx->line_len - 1]))
        ctx->line_len--;
    ctx->sum = 0;
    ctx->error = false;

    // Empty lines are probably OK
    if (*ctx->ptr++ != ':')
        return 0;

    data_len = parse_hex_value(ctx, 1);
    if (11 + 2 * data_len != ctx->line_len)
        return ihex_parse_error(ctx);
    address = parse_hex_value(ctx, 2);
    type = parse_hex_value(ctx, 1);

    switch (type) {
        case 0: { // data record
            address += ctx->base_offset;
            r = ty_firmware_expand_image(ctx->fw, address + data_len);
            if (r < 0)
                return r;
            for (unsigned int i = 0; i < data_len; i++)
                ctx->fw->image[address + i] = (uint8_t)parse_hex_value(ctx, 1);
        } break;

        case 1: { // EOF record
            if (data_len)
                return ihex_parse_error(ctx);
        } break;

        case 2: { // extended segment address record
            if (data_len != 2)
                return ihex_parse_error(ctx);
            ctx->base_offset = (uint32_t)parse_hex_value(ctx, 2) << 4;
        } break;

        case 4: { // extended linear address record
            if (data_len != 2)
                return ihex_parse_error(ctx);
            ctx->base_offset = (uint32_t)parse_hex_value(ctx, 2) << 16;
        } break;

        case 3:   // start segment address record
        case 5: { // start linear address record
            if (data_len != 4)
                return ihex_parse_error(ctx);
            parse_hex_value(ctx, 4);
        } break;

        default: {
            return ihex_parse_error(ctx);
        } break;
    }

    // Don't checksum the checksum :)
    sum = ctx->sum;
    checksum = (uint8_t)parse_hex_value(ctx, 1);

    if (ctx->error)
        return ihex_parse_error(ctx);
    if ((sum + checksum) & 0xFF)
        return ihex_parse_error(ctx);

    // Return 1 for EOF records, to end the parsing
    return (type == 1);
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
            case EACCES: {
                r = ty_error(TY_ERROR_ACCESS, "Permission denied for '%s'", ctx.fw->filename);
            } break;
            case EIO: {
                r = ty_error(TY_ERROR_IO, "I/O error while opening '%s' for reading",
                             ctx.fw->filename);
            } break;
            case ENOENT:
            case ENOTDIR: {
                r = ty_error(TY_ERROR_NOT_FOUND, "File '%s' does not exist", ctx.fw->filename);
            } break;

            default: {
                r = ty_error(TY_ERROR_SYSTEM, "fopen('%s') failed: %s", ctx.fw->filename,
                             strerror(errno));
            } break;
        }
        goto cleanup;
    }

    do {
        if (!fgets(buf, sizeof(buf), fp)) {
            if (feof(fp)) {
                r = ihex_parse_error(&ctx);
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
