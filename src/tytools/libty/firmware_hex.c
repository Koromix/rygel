/* TyTools - public domain
   Niels Martignène <niels.martignene@protonmail.com>
   https://koromix.dev/tytools

   This software is in the public domain. Where that dedication is not
   recognized, you are granted a perpetual, irrevocable license to copy,
   distribute, and modify this file as you see fit.

   See the LICENSE file for more details. */

#include "common.h"
#include "src/tytools/libhs/array.h"
#include "firmware.h"

typedef _HS_ARRAY(uint8_t) read_buf;

struct parser_context {
    ty_firmware *fw;

    ty_firmware_read_func *func;
    void *udata;

    ty_firmware_program *program;

    read_buf buf;
    unsigned int line;

    const char *ptr;
    const char *end;
    uint8_t sum;
    bool error;

    uint32_t offset1;
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
            address += ctx->offset1 + ctx->offset2;

            if (!ctx->segment || address + data_len > ctx->segment->address + 1048576) {
                r = ty_firmware_add_segment(ctx->program, address, data_len, &ctx->segment);
                if (r < 0)
                    return r;
                address = 0;
            } else {
                address -= ctx->segment->address;
                r = ty_firmware_expand_segment(ctx->program, ctx->segment, address + data_len);
                if (r < 0)
                    return r;
            }

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

            ctx->offset1 = (uint32_t)parse_hex_value(ctx, 2) << 16;
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

static int load_hex(struct parser_context *ctx, size_t pgm)
{
    assert(pgm < ctx->fw->programs_count);

    ty_firmware *fw = ctx->fw;
    ty_firmware_program *program = &fw->programs[pgm];
    read_buf *buf = &ctx->buf;
    size_t start, end = 0;
    bool eof = false;
    ssize_t r;

    ctx->program = program;
    ctx->segment = NULL;

#define SKIP_LINE(c) \
        (ctx->line += (c == '\n'), c == '\r' || c == '\n')

    do {
        if (eof) {
            r = ty_error(TY_ERROR_PARSE, "Missing EOF record in '%s' (IHEX)", fw->filename);
            goto cleanup;
        }

        // Find line limits
        start = end;
        while (start < buf->count && SKIP_LINE(buf->values[start]))
            start++;
        end = start;
        while (end < buf->count && (buf->values[end] != '\r' && buf->values[end] != '\n'))
            end++;

        // Could not find end of line, need more data
        if (end >= buf->count) {
            if (buf->count > 2 * 1024 * 1024) {
                r = ty_error(TY_ERROR_PARSE, "Excessive IHEX line length in '%s' (%d)", fw->filename, ctx->line + 1);
                goto cleanup;
            }

            r = _hs_array_grow(&ctx->buf, 4096);
            if (r < 0)
                goto cleanup;

            r = (*ctx->func)(-1, buf->values + buf->count, buf->allocated - buf->count, ctx->udata);
            if (r < 0)
                goto cleanup;
            if (!r) {
                buf->values[buf->count++] = '\n';
                eof = true;
            }
            buf->count += (size_t)r;

            end = 0;
            continue;
        }

        // Returns 1 when EOF record is detected
        r = parse_line(ctx, (const char *)buf->values + start, end - start);
        if (r < 0)
            goto cleanup;

        while (end < buf->count && SKIP_LINE(buf->values[end]))
            end++;

        memmove(buf->values, buf->values + end, buf->count - end);
        buf->count -= end;
        end = 0;
    } while (r != 1);

#undef SKIP_LINE

    program->min_address = SIZE_MAX;
    for (unsigned int i = 0; i < program->segments_count; i++) {
        const ty_firmware_segment *segment = &program->segments[i];

        program->min_address = _HS_MIN(program->min_address, segment->address);
        program->max_address = _HS_MAX(program->max_address, segment->address + segment->size);
    }

    r = 0;
cleanup:
    return (int)r;
}

static void init_context(struct parser_context *ctx, ty_firmware *fw, ty_firmware_read_func *func, void *udata)
{
    memset(ctx, 0, sizeof(*ctx));

    ctx->fw = fw;
    ctx->func = func;
    ctx->udata = udata;
    ctx->line = 1;
}

static void release_context(struct parser_context *ctx)
{
    _hs_array_release(&ctx->buf);
}

int ty_firmware_load_ihex(ty_firmware *fw, ty_firmware_read_func *func, void *udata)
{
    assert(fw);
    assert(!fw->programs_count);
    assert(func);

    struct parser_context ctx;
    int r;

    fw->type = TY_FIRMWARE_TYPE_IHEX;
    fw->programs_count = 1;

    init_context(&ctx, fw, func, udata);

    r = load_hex(&ctx, 0);
    if (r < 0)
        goto cleanup;

    r = 0;
cleanup:
    release_context(&ctx);
    return r;
}

int ty_firmware_load_ehex(ty_firmware *fw, ty_firmware_read_func *func, void *udata)
{
    assert(fw);
    assert(!fw->programs_count);
    assert(func);

    struct parser_context ctx;
    int r;

    fw->type = TY_FIRMWARE_TYPE_EHEX;
    fw->programs_count = 2;

    init_context(&ctx, fw, func, udata);

    r = load_hex(&ctx, 0);
    if (r < 0)
        goto cleanup;
    r = load_hex(&ctx, 1);
    if (r < 0)
        goto cleanup;

    r = 0;
cleanup:
    release_context(&ctx);
    return r;
}
