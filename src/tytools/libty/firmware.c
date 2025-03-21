/* TyTools - public domain
   Niels Martignène <niels.martignene@protonmail.com>
   https://koromix.dev/tytools

   This software is in the public domain. Where that dedication is not
   recognized, you are granted a perpetual, irrevocable license to copy,
   distribute, and modify this file as you see fit.

   See the LICENSE file for more details. */

#include "common.h"
#include "class_priv.h"
#include "firmware.h"
#include "system.h"

const ty_firmware_format ty_firmware_formats[] = {
    {"elf",  ".elf", ty_firmware_load_elf},
    {"ihex", ".hex", ty_firmware_load_ihex},
    {"ehex", ".ehex", ty_firmware_load_ehex}
};
const unsigned int ty_firmware_formats_count = _HS_COUNTOF(ty_firmware_formats);

struct read_file_context {
    const char *filename;
    FILE *fp;
    int64_t offset;
};

struct read_memory_context {
    const char *filename;
    const uint8_t *mem;
    size_t len;
    int64_t offset;
};

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

    for (size_t i = 0; i < _HS_COUNTOF(fw->programs); i++)
        fw->programs[i].idx = (int)i;

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

static ssize_t read_file(int64_t offset, uint8_t *buf, size_t len, void *udata)
{
    struct read_file_context *ctx = (struct read_file_context *)udata;
    ssize_t r;

    if (offset < 0) {
        offset = ctx->offset;
    } else if (offset != ctx->offset) {
#if defined(_WIN32)
        r = _fseeki64(ctx->fp, offset, SEEK_SET);
#else
        r = fseeko(ctx->fp, (off_t)offset, SEEK_SET);
#endif
        if (r < 0) {
            switch (errno) {
                case ESPIPE: {
                    return ty_error(TY_ERROR_IO, "Trying to seek in non-seekable file '%s'", ctx->filename);
                } break;
                case EINVAL: {
                    return ty_error(TY_ERROR_RANGE, "Cannot seek beyond end of file '%s'", ctx->filename);
                } break;

                default: {
                    return ty_error(TY_ERROR_SYSTEM, "fseek('%s') failed: %s", ctx->filename, strerror(errno));
                } break;
            }
        }

        ctx->offset = offset;
    }

    r = (ssize_t)fread(buf, 1, len, ctx->fp);
    if (ferror(ctx->fp)) {
        if (errno == EIO)
            return ty_error(TY_ERROR_IO, "I/O error while reading from '%s'", ctx->filename);
        return ty_error(TY_ERROR_SYSTEM, "fread('%s') failed: %s", ctx->filename, strerror(errno));
    }
    ctx->offset += (int64_t)r;

    return r;
}

static ssize_t read_memory(int64_t offset, uint8_t *buf, size_t len, void *udata)
{
    struct read_memory_context *ctx = (struct read_memory_context *)udata;

    if (offset < 0)
        offset = ctx->offset;
    if ((size_t)offset > ctx->len)
        return ty_error(TY_ERROR_RANGE, "Cannot seek beyond end of file '%s'", ctx->filename);

    size_t copy_len = _HS_MIN((size_t)ctx->len - (size_t)offset, (size_t)len);
    memcpy(buf, ctx->mem + offset, copy_len);
    ctx->offset = offset + (int64_t)copy_len;

    return (ssize_t)copy_len;
}

int ty_firmware_load_file(const char *filename, FILE *fp, const char *format_name,
                          ty_firmware **rfw)
{
    assert(filename);
    assert(rfw);

    const ty_firmware_format *format;
    struct read_file_context ctx = {0};
    bool close_fp = false;
    ty_firmware *fw = NULL;
    int r;

    r = find_format(filename, format_name, &format);
    if (r < 0)
        goto cleanup;

    if (!fp) {
restart:
#if defined(_WIN32)
        fp = fopen(filename, "rb");
#else
        fp = fopen(filename, "rbe");
#endif
        if (!fp) {
            switch (errno) {
                case EINTR: {
                    goto restart;
                } break;

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

    r = ty_firmware_new(filename, &fw);
    if (r < 0)
        goto cleanup;

    ctx.filename = filename;
    ctx.fp = fp;
    ctx.offset = 0;

    r = (*format->load)(fw, read_file, &ctx);
    if (r < 0)
        goto cleanup;

    *rfw = fw;
    fw = NULL;

cleanup:
    ty_firmware_unref(fw);
    if (close_fp)
        fclose(fp);
    return r;
}

int ty_firmware_load_mem(const char *filename, const uint8_t *mem, size_t len,
                         const char *format_name, ty_firmware **rfw)
{
    assert(filename);
    assert(mem || !len);
    assert(rfw);

    const ty_firmware_format *format;
    struct read_memory_context ctx = {0};
    ty_firmware *fw = NULL;
    int r;

    r = find_format(filename, format_name, &format);
    if (r < 0)
        goto cleanup;

    r = ty_firmware_new(filename, &fw);
    if (r < 0)
        goto cleanup;

    ctx.filename = filename;
    ctx.mem = mem;
    ctx.len = len;

    r = (*format->load)(fw, read_memory, &ctx);
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

        for (unsigned int i = 0; i < fw->programs_count; i++) {
            ty_firmware_program *program = &fw->programs[i];
            for (unsigned int j = 0; j < program->segments_count; j++)
                free(program->segments[j].data);
        }
        free(fw->name);
        free(fw->filename);
    }

    free(fw);
}

const ty_firmware *ty_firmware_from_program(const ty_firmware_program *program)
{
    size_t offset = offsetof(ty_firmware, programs) + program->idx * sizeof(*program);
    return (const ty_firmware *)((const uint8_t *)program - offset);
}

const ty_firmware_segment *ty_firmware_find_segment(const ty_firmware_program *program, uint32_t address)
{
    assert(program);

    for (unsigned int i = program->segments_count; i-- > 0;) {
        const ty_firmware_segment *segment = &program->segments[i];

        if (address >= segment->address && address < segment->address + segment->size)
            return segment;
    }

    return NULL;
}

size_t ty_firmware_extract(const ty_firmware_program *program, uint32_t address, uint8_t *buf, size_t size)
{
    assert(program);

    size_t total_len = 0;

    for (unsigned int i = 0; i < program->segments_count; i++) {
        const ty_firmware_segment *segment = &program->segments[i];

        if (address >= segment->address && address < segment->address + segment->size) {
            size_t delta = address - segment->address;
            size_t len = _HS_MIN(segment->size - delta, size);

            memcpy(buf, segment->data + delta, len);
            total_len += len;
        } else if (address < segment->address && address + size > segment->address) {
            size_t delta = segment->address - address;
            size_t len = _HS_MIN(segment->size, size - delta);

            memcpy(buf + delta, segment->data, len);
            total_len += len;
        }
    }

    return total_len;
}

int ty_firmware_add_segment(ty_firmware_program *program, uint32_t address, size_t size,
                            ty_firmware_segment **rsegment)
{
    assert(program);

    ty_firmware_segment *segment;
    int r;

    if (program->segments_count >= TY_FIRMWARE_MAX_SEGMENTS)
        return ty_error(TY_ERROR_RANGE, "Firmware '%s' has too many segments",
                        ty_firmware_from_program(program)->filename);

    segment = &program->segments[program->segments_count];
    segment->address = address;

    r = ty_firmware_expand_segment(program, segment, size);
    if (r < 0)
        return r;

    program->segments_count++;

    if (rsegment)
        *rsegment = segment;
    return 0;
}

int ty_firmware_expand_segment(ty_firmware_program *program, ty_firmware_segment *segment, size_t size)
{
    assert(program);

    size_t total_size = program->total_size - segment->size + size;
    const size_t step_size = 65536;

    if (total_size > TY_FIRMWARE_MAX_SIZE)
        return ty_error(TY_ERROR_RANGE, "Firmware '%s' has excessive size (max %u bytes)",
                        ty_firmware_from_program(program)->filename, TY_FIRMWARE_MAX_SIZE);

    if (size > segment->alloc_size) {
        uint8_t *tmp;
        size_t alloc_size;

        alloc_size = (size + (step_size - 1)) / step_size * step_size;
        tmp = realloc(segment->data, alloc_size);
        if (!tmp)
            return ty_error(TY_ERROR_MEMORY, NULL);

        segment->data = tmp;
        segment->alloc_size = alloc_size;
    }

    segment->size = size;
    program->total_size = total_size;

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
                                                                  _HS_COUNTOF(partial_guesses));

        for (unsigned int j = 0; j < partial_count; j++) {
            if (rmodels && guesses_count < max_models)
                rmodels[guesses_count++] = partial_guesses[j];
        }
    }

    return guesses_count;
}
