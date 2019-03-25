/* TyTools - public domain
   Niels Martignène <niels.martignene@protonmail.com>
   https://koromix.dev/tytools

   This software is in the public domain. Where that dedication is not
   recognized, you are granted a perpetual, irrevocable license to copy,
   distribute, and modify this file as you see fit.

   See the LICENSE file for more details. */

#include "common_priv.h"
#include <sys/types.h>
#include "firmware.h"

#define EI_NIDENT 16

typedef struct Elf32_Ehdr {
    unsigned char e_ident[EI_NIDENT];

    uint16_t e_type;
    uint16_t e_machine;
    uint32_t e_version;
    uint32_t e_entry;
    uint32_t e_phoff;
    uint32_t e_shoff;
    uint32_t e_flags;
    uint16_t e_ehsize;
    uint16_t e_phentsize;
    uint16_t e_phnum;
    uint16_t e_shentsize;
    uint16_t e_shnum;
    uint16_t e_shstrndx;
} Elf32_Ehdr;

#define ELFMAG "\177ELF"
#define SELFMAG 4

#define EI_CLASS 4
#define ELFCLASS32 1

#define EI_DATA 5
#define ELFDATA2LSB 1
#define ELFDATA2MSB 2

typedef struct Elf32_Phdr {
    uint32_t p_type;
    uint32_t p_offset;
    uint32_t p_vaddr;
    uint32_t p_paddr;
    uint32_t p_filesz;
    uint32_t p_memsz;
    uint32_t p_flags;
    uint32_t p_align;
} Elf32_Phdr;

#define PT_NULL 0
#define PT_LOAD 1

struct loader_context {
    ty_firmware *fw;

    const uint8_t *mem;
    size_t len;

    Elf32_Ehdr ehdr;
};

// The compiler will reduce this to a simple conditional branch
static inline bool is_endianness_reversed(struct loader_context *ctx)
{
    union { uint16_t u; uint8_t raw[2]; } u;
    u.raw[0] = 0;
    u.raw[1] = 1;

    return (u.u == 1) == (ctx->ehdr.e_ident[EI_DATA] == ELFDATA2LSB);
}

static inline void reverse_uint16(uint16_t *u)
{
    *u = (uint16_t)(((*u & 0xFF) << 8) | ((*u & 0xFF00) >> 8));
}

static inline void reverse_uint32(uint32_t *u)
{
    *u = ((*u & 0xFF) << 24) | ((*u & 0xFF00) << 8)
            | ((*u & 0xFF0000) >> 8) | ((*u & 0xFF000000) >> 24);
}

static int read_chunk(struct loader_context *ctx, off_t offset, size_t size, void *buf)
{
    if (offset < 0 || (size_t)offset > ctx->len - size)
        return ty_error(TY_ERROR_PARSE, "ELF file '%s' is malformed or truncated",
                        ctx->fw->filename);

    memcpy(buf, ctx->mem + offset, size);
    return 0;
}

static int load_program_header(struct loader_context *ctx, unsigned int i, Elf32_Phdr *rphdr)
{
    int r;

    r = read_chunk(ctx, (off_t)(ctx->ehdr.e_phoff + i * ctx->ehdr.e_phentsize), sizeof(*rphdr), rphdr);
    if (r < 0)
        return r;

    if (is_endianness_reversed(ctx)) {
        reverse_uint32(&rphdr->p_type);
        reverse_uint32(&rphdr->p_offset);
        reverse_uint32(&rphdr->p_vaddr);
        reverse_uint32(&rphdr->p_paddr);
        reverse_uint32(&rphdr->p_filesz);
        reverse_uint32(&rphdr->p_memsz);
        reverse_uint32(&rphdr->p_flags);
        reverse_uint32(&rphdr->p_align);
    }

    return 0;
}

static int load_segment(struct loader_context *ctx, unsigned int i)
{
    Elf32_Phdr phdr;
    ty_firmware_segment *segment;
    int r;

    r = load_program_header(ctx, i ,&phdr);
    if (r < 0)
        return (int)r;

    if (phdr.p_type != PT_LOAD || !phdr.p_filesz)
        return 0;

    r = ty_firmware_add_segment(ctx->fw, phdr.p_paddr, phdr.p_filesz, &segment);
    if (r < 0)
        return r;
    r = read_chunk(ctx, phdr.p_offset, phdr.p_filesz, segment->data);
    if (r < 0)
        return r;

    return 1;
}

int ty_firmware_load_elf(ty_firmware *fw, const uint8_t *mem, size_t len)
{
    assert(fw);
    assert(!fw->segments_count && !fw->total_size);
    assert(mem || !len);

    struct loader_context ctx = {0};
    int r;

    ctx.fw = fw;
    ctx.mem = mem;
    ctx.len = len;

    r = read_chunk(&ctx, 0, sizeof(ctx.ehdr), &ctx.ehdr);
    if (r < 0)
        return r;

    if (memcmp(ctx.ehdr.e_ident, ELFMAG, SELFMAG) != 0)
        return ty_error(TY_ERROR_PARSE, "Missing ELF signature in '%s'", ctx.fw->filename);

    if (ctx.ehdr.e_ident[EI_CLASS] != ELFCLASS32)
        return ty_error(TY_ERROR_UNSUPPORTED, "ELF object '%s' is not supported (not 32-bit)",
                        ctx.fw->filename);

    if (is_endianness_reversed(&ctx)) {
        reverse_uint16(&ctx.ehdr.e_type);
        reverse_uint16(&ctx.ehdr.e_machine);
        reverse_uint32(&ctx.ehdr.e_entry);
        reverse_uint32(&ctx.ehdr.e_phoff);
        reverse_uint32(&ctx.ehdr.e_shoff);
        reverse_uint32(&ctx.ehdr.e_flags);
        reverse_uint16(&ctx.ehdr.e_ehsize);
        reverse_uint16(&ctx.ehdr.e_phentsize);
        reverse_uint16(&ctx.ehdr.e_phnum);
        reverse_uint16(&ctx.ehdr.e_shentsize);
        reverse_uint16(&ctx.ehdr.e_shnum);
        reverse_uint16(&ctx.ehdr.e_shstrndx);
    }

    if (!ctx.ehdr.e_phoff)
        return ty_error(TY_ERROR_PARSE, "ELF file '%s' has no program headers", ctx.fw->filename);

    for (unsigned int i = 0; i < ctx.ehdr.e_phnum; i++) {
        r = load_segment(&ctx, i);
        if (r < 0)
            return r;
    }

    for (unsigned int i = 0; i < fw->segments_count; i++) {
        const ty_firmware_segment *segment = &fw->segments[i];
        fw->total_size += segment->size;
        fw->max_address = TY_MAX(fw->max_address, segment->address + segment->size);
    }

    return 0;
}
