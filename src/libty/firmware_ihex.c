/* TyTools - public domain
   Niels Martignène <niels.martignene@protonmail.com>
   https://koromix.dev/tytools

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
    const char *end;
    uint8_t sum;
    bool error;

    uint32_t offset2;
    ty_firmware_segment *segment;
};

static uint32_t parse_hex_value(struct parser_context *ctx, size_t size)
{
    if (ctx->error)
        return 0;

    uint32_t value = 0;
    while (size--) {
        char buf[3];
        char *end;
        unsigned long byte;

        if (ctx->ptr > ctx->end - 2) {
            ctx->error = true;
            return 0;
        }
        memcpy(buf, ctx->ptr, 2);
        buf[2] = 0;

        byte = strtoul(buf, &end, 16);
        if (end == buf || end[0]) {
            ctx->error = true;
            return 0;
        }

        value = (value << 8) | (uint8_t)byte;
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

static int parse_line(struct parser_context *ctx, const char *line, size_t line_len)
{
    unsigned int data_len, type;
    uint32_t address;
    uint8_t sum, checksum;
    int r;

    ctx->ptr = line;
    ctx->end = line + line_len;
    ctx->sum = 0;
    ctx->error = false;

    if (!line_len || *ctx->ptr++ != ':')
        return ihex_parse_error(ctx);
    data_len = parse_hex_value(ctx, 1);
    if (11 + 2 * data_len != line_len)
        return ihex_parse_error(ctx);
    address = parse_hex_value(ctx, 2);
    type = parse_hex_value(ctx, 1);

    switch (type) {
        case 0: { // data record
            address += ctx->offset2;

            r = ty_firmware_expand_segment(ctx->fw, ctx->segment, address + data_len);
            if (r < 0)
                return r;

            for (unsigned int i = 0; i < data_len; i++)
                ctx->segment->data[address + i] = (uint8_t)parse_hex_value(ctx, 1);
        } break;

        case 1: { // EOF record
            if (data_len)
                return ihex_parse_error(ctx);
        } break;

        case 2: { // extended segment address record
            if (data_len != 2)
                return ihex_parse_error(ctx);

            ctx->offset2 = (uint32_t)parse_hex_value(ctx, 2) << 4;
        } break;

        case 4: { // extended linear address record
            if (data_len != 2)
                return ihex_parse_error(ctx);

            address = (uint32_t)parse_hex_value(ctx, 2) << 16;
            r = ty_firmware_add_segment(ctx->fw, address, 0, &ctx->segment);
            if (r < 0)
                return r;
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

int ty_firmware_load_ihex(ty_firmware *fw, const uint8_t *mem, size_t len)
{
    assert(fw);
    assert(!fw->segments_count && !fw->total_size);
    assert(mem || !len);

    struct parser_context ctx = {0};
    int r;

    ctx.fw = fw;
    r = ty_firmware_add_segment(fw, 0, 0, &ctx.segment);
    if (r < 0)
        return r;

    size_t start, end = 0;
    do {
        start = end;
        while (start < len && (mem[start] == '\r' || mem[start] == '\n'))
            start++;
        if (start >= len)
            return ty_error(TY_ERROR_PARSE, "Missing EOF record in '%s' (IHEX)", fw->filename);
        end = start;
        while (end < len && mem[end] != '\r' && mem[end] != '\n')
            end++;
        ctx.line++;

        // Returns 1 when EOF record is detected
        r = parse_line(&ctx, (const char *)mem + start, end - start);
        if (r < 0)
            return r;
    } while (!r);

    for (unsigned int i = 0; i < fw->segments_count; i++) {
        const ty_firmware_segment *segment = &fw->segments[i];
        fw->total_size += segment->size;
        fw->max_address = TY_MAX(fw->max_address, segment->address + segment->size);
    }

    return 0;
}
