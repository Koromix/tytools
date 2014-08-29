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

#include "common.h"
#include "board.h"
#include "firmware.h"
#include "system.h"
#include "device.h"

struct firmware_signature {
    const ty_board_model *model;
    uint8_t magic[8];
};

static const uint16_t teensy_vid = 0x16C0;

static const size_t seremu_packet_size = 32;
static const uint64_t probe_fail_delay = 8000;

static const ty_board_model teensypp10 = {
    .name = "teensy++10",
    .mcu = "at90usb646",
    .desc = "Teensy++ 1.0",

    .usage = 0x1A,
    .halfkay_version = 0,
    .code_size = 64512,
    .block_size = 256,

    .toolchain = TY_TOOLCHAIN_AVR,
    .core = "teensy",
    .frequencies = {16000000, 8000000, 4000000, 2000000, 1000000},
    .cflags = "-mmcu=at90usb646",
    .cxxflags = "-mmcu=at90usb646",
    .ldflags = "-mmcu=at90usb646"
};

static const ty_board_model teensy20 = {
    .name = "teensy20",
    .mcu = "atmega32u4",
    .desc = "Teensy 2.0",

    .usage = 0x1B,
    .halfkay_version = 0,
    .code_size = 32256,
    .block_size = 128,

    .toolchain = TY_TOOLCHAIN_AVR,
    .core = "teensy",
    .frequencies = {16000000, 8000000, 4000000, 2000000, 1000000},
    .cflags = "-mmcu=atmega32u4",
    .cxxflags = "-mmcu=atmega32u4",
    .ldflags = "-mmcu=atmega32u4"
};

static const ty_board_model teensypp20 = {
    .name = "teensy++20",
    .mcu = "at90usb1286",
    .desc = "Teensy++ 2.0",

    .usage = 0x1C,
    .halfkay_version = 1,
    .code_size = 130048,
    .block_size = 256,

    .toolchain = TY_TOOLCHAIN_AVR,
    .core = "teensy",
    .frequencies = {16000000, 8000000, 4000000, 2000000, 1000000},
    .cflags = "-mmcu=at90usb1286",
    .cxxflags = "-mmcu=at90usb1286",
    .ldflags = "-mmcu=at90usb1286"
};

static const ty_board_model teensy30 = {
    .name = "teensy30",
    .mcu = "mk20dx128",
    .desc = "Teensy 3.0",

    .usage = 0x1D,
    .halfkay_version = 2,
    .code_size = 131072,
    .block_size = 1024,

    .toolchain = TY_TOOLCHAIN_ARM,
    .core = "teensy3",
    .frequencies = {96000000, 48000000, 24000000},
    .cflags = "-mcpu=cortex-m4 -mthumb -D__MK20DX128__",
    .cxxflags = "-mcpu=cortex-m4 -mthumb -D__MK20DX128__",
    .ldflags = "-mcpu=cortex-m4 -mthumb -T\"$teensyduino/mk20dx128.ld\""
};

static const ty_board_model teensy31 = {
    .name = "teensy31",
    .mcu = "mk20dx256",
    .desc = "Teensy 3.1",

    .usage = 0x1E,
    .halfkay_version = 2,
    .code_size = 262144,
    .block_size = 1024,

    .toolchain = TY_TOOLCHAIN_ARM,
    .core = "teensy3",
    .frequencies = {96000000, 48000000, 24000000},
    .cflags = "-mcpu=cortex-m4 -mthumb -D__MK20DX256__",
    .cxxflags = "-mcpu=cortex-m4 -mthumb -D__MK20DX256__",
    .ldflags = "-mcpu=cortex-m4 -mthumb -T\"$teensyduino/mk20dx256.ld\""
};

static const struct firmware_signature signatures[] = {
    {&teensypp10, {0x0C, 0x94, 0x00, 0x7E, 0xFF, 0xCF, 0xF8, 0x94}},
    {&teensy20,   {0x0C, 0x94, 0x00, 0x3F, 0xFF, 0xCF, 0xF8, 0x94}},
    {&teensypp20, {0x0C, 0x94, 0x00, 0xFE, 0xFF, 0xCF, 0xF8, 0x94}},
    {&teensy30,   {0x38, 0x80, 0x04, 0x40, 0x82, 0x3F, 0x04, 0x00}},
    {&teensy31,   {0x30, 0x80, 0x04, 0x40, 0x82, 0x3F, 0x04, 0x00}},

    {0}
};

static const ty_board_mode bootloader_mode = {
    .name = "bootloader",
    .desc = "HalfKay Bootloader",

    .type = TY_DEVICE_HID,
    .pid = 0x478,
    .iface = 0,
    .capabilities = TY_BOARD_CAPABILITY_IDENTIFY | TY_BOARD_CAPABILITY_UPLOAD |
                    TY_BOARD_CAPABILITY_RESET
};

static const ty_board_mode disk_mode = {
    .name = "disk",
    .desc = "Disk",

    .type = TY_DEVICE_HID,
    .pid = 0x484,
    .iface = 0,
    .capabilities = TY_BOARD_CAPABILITY_SERIAL | TY_BOARD_CAPABILITY_REBOOT,

    .flags = "-DUSB_DISK"
};

static const ty_board_mode flightsim_mode = {
    .name = "flightsim",
    .desc = "FlightSim",

    .type = TY_DEVICE_HID,
    .pid = 0x488,
    .iface = 1,
    .capabilities = TY_BOARD_CAPABILITY_SERIAL | TY_BOARD_CAPABILITY_REBOOT,

    .flags = "-DUSB_FLIGHTSIM"
};

static const ty_board_mode hid_mode = {
    .name = "hid",
    .desc = "HID",

    .type = TY_DEVICE_HID,
    .pid = 0x482,
    .iface = 2,
    .capabilities = TY_BOARD_CAPABILITY_SERIAL | TY_BOARD_CAPABILITY_REBOOT,

    .flags = "-DUSB_HID"
};

static const ty_board_mode midi_mode = {
    .name = "midi",
    .desc = "MIDI",

    .type = TY_DEVICE_HID,
    .pid = 0x485,
    .iface = 0,
    .capabilities = TY_BOARD_CAPABILITY_SERIAL | TY_BOARD_CAPABILITY_REBOOT,

    .flags = "-DUSB_MIDI"
};

static const ty_board_mode rawhid_mode = {
    .name = "rawhid",
    .desc = "Raw HID",

    .type = TY_DEVICE_HID,
    .pid = 0x486,
    .iface = 1,
    .capabilities = TY_BOARD_CAPABILITY_SERIAL | TY_BOARD_CAPABILITY_REBOOT,

    .flags = "-DUSB_RAWHID"
};

static const ty_board_mode serial_mode = {
    .name = "serial",
    .desc = "Serial",

    .type = TY_DEVICE_SERIAL,
    .pid = 0x483,
    .iface = 0,
    .capabilities = TY_BOARD_CAPABILITY_SERIAL | TY_BOARD_CAPABILITY_REBOOT,

    .flags = "-DUSB_SERIAL"
};

static const ty_board_mode serial_hid_mode = {
    .name = "serial_hid",
    .desc = "Serial HID",

    .type = TY_DEVICE_SERIAL,
    .pid = 0x487,
    .iface = 0,
    .capabilities = TY_BOARD_CAPABILITY_SERIAL | TY_BOARD_CAPABILITY_REBOOT,

    .flags = "-DUSB_SERIAL_HID"
};

const ty_board_mode *ty_board_modes[] = {
    &bootloader_mode,
    &flightsim_mode,
    &hid_mode,
    &midi_mode,
    &rawhid_mode,
    &serial_mode,
    &serial_hid_mode,

    NULL
};

const ty_board_model *ty_board_models[] = {
    &teensypp10,
    &teensy20,
    &teensypp20,
    &teensy30,
    &teensy31,

    NULL
};

const ty_board_model *ty_board_find_model(const char *name)
{
    assert(name);

    for (const ty_board_model **cur = ty_board_models; *cur; cur++) {
        const ty_board_model *m = *cur;
        if (strcmp(m->name, name) == 0 || strcmp(m->mcu, name) == 0)
            return m;
    }

    return NULL;
}

const ty_board_mode *ty_board_find_mode(const char *name)
{
    assert(name);

    for (const ty_board_mode **cur = ty_board_modes; *cur; cur++) {
        const ty_board_mode *m = *cur;
        if (strcasecmp(m->name, name) == 0)
            return m;
    }

    return NULL;
}

// Teensy Bootloader shows the serial number as hexadecimal with
// prefixed zeros (which would suggest octal to stroull).
static uint64_t parse_serial_number(const char *s)
{
    if (!s)
        return 0;

    int base = 10;
    if (*s == '0')
        base = 16;

    return strtoull(s, NULL, base);
}

struct list_context {
    ty_board_walker *f;
    void *udata;
};

static int list_walker(ty_device *dev, void *udata)
{
    struct list_context *ctx = udata;

    ty_board *board;
    const ty_board_mode *mode = NULL;
    int r;

    if (dev->vid != teensy_vid)
        return 1;

    for (const ty_board_mode **cur = ty_board_modes; *cur; cur++) {
        mode = *cur;
        if (mode->pid == dev->pid)
            break;
    }
    if (!mode->pid)
        return 1;

    if (dev->iface != mode->iface)
        return 1;

    board = calloc(1, sizeof(*board));
    if (!board)
        return ty_error(TY_ERROR_MEMORY, NULL);
    board->refcount = 1;

    board->dev = ty_device_ref(dev);
    board->serial = parse_serial_number(dev->serial);

    board->mode = mode;

    r = (*ctx->f)(board, ctx->udata);

    ty_board_unref(board);

    return r;
}

int ty_board_list(ty_board_walker *f, void *udata)
{
    assert(f);

    struct list_context ctx;
    int r;

    ctx.f = f;
    ctx.udata = udata;

    r = ty_usb_list_devices(TY_DEVICE_HID, list_walker, &ctx);
    if (r <= 0)
        return r;
    r = ty_usb_list_devices(TY_DEVICE_SERIAL, list_walker, &ctx);
    if (r <= 0)
        return r;

    return 1;
}

struct find_context {
    ty_board *board;

    const char *path;
    uint64_t serial;
};

static int find_walker(ty_board *board, void *udata)
{
    struct find_context *ctx = udata;

    ty_device *dev = board->dev;

    if (ctx->path && strcmp(dev->path, ctx->path) != 0)
        return 1;
    if (ctx->serial && board->serial != ctx->serial)
        return 1;

    ctx->board = ty_board_ref(board);
    return 0;
}

int ty_board_find(ty_board **rboard, const char *path, uint64_t serial)
{
    assert(rboard);

    struct find_context ctx;
    int r;

    ctx.board = NULL;
    ctx.path = path;
    ctx.serial = serial;

    // Returns 1 when the list is completed, but listing is aborted when the
    // walker returns 0 or an error (and ty_board_list returns this value).
    r = ty_board_list(find_walker, &ctx);
    if (r < 0)
        return r;
    else if (!r) {
        *rboard = ctx.board;
        return 1;
    }

    return 0;
}

ty_board *ty_board_ref(ty_board *board)
{
    assert(board);

    board->refcount++;
    return board;
}

void ty_board_unref(ty_board *board)
{
    if (board && !--board->refcount) {
        ty_board_close(board);
        ty_device_unref(board->dev);

        free(board);
    }
}

uint32_t ty_board_get_capabilities(ty_board *board)
{
    assert(board);

    return board->mode->capabilities;
}

static int identify_model(ty_board *board)
{
    const ty_board_mode *mode = board->mode;
    ty_handle *h = board->h;

    // Reset the model, in case we can't identify again
    board->model = NULL;

    if (!(mode->capabilities & TY_BOARD_CAPABILITY_IDENTIFY))
        return 0;

    ty_hid_descriptor desc;
    int r;

    r = ty_hid_parse_descriptor(h, &desc);
    if (r < 0)
        return r;

    for (const ty_board_model **cur = ty_board_models; *cur; cur++) {
        if ((*cur)->usage == desc.usage) {
            board->model = *cur;
            return 1;
        }
    }

    return ty_error(TY_ERROR_UNSUPPORTED, "Unknown board model");
}

int ty_board_probe(ty_board *board, int timeout)
{
    assert(board);

    ty_board *newboard;
    uint64_t end = 0;
    int r;

    ty_board_close(board);

    if (!board->fail_at)
        board->fail_at = ty_millis() + probe_fail_delay;

    if (timeout >= 0)
        end = ty_millis() + (uint64_t)timeout;



    do {
        r = ty_board_find(&newboard, board->dev->path, 0);
        if (r < 0)
            return r;

        ty_delay(500);
    } while (!r && (!end || ty_millis() < end));
    if (!r) {
        if (ty_millis() > board->fail_at)
            return ty_error(TY_ERROR_TIMEOUT, "Board seems to have disappeared");
        return 0;
    }
    board->fail_at = 0;

    ty_error_mask(TY_ERROR_NOT_FOUND);
    r = ty_device_open(&newboard->h, newboard->dev, false);
    ty_error_unmask();
    if (r < 0) {
        if (r == TY_ERROR_NOT_FOUND)
            r = 0;
        goto error;
    }

    r = identify_model(newboard);
    if (r < 0)
        goto error;

    ty_device_unref(board->dev);

    // Steal the board (we're the only one to know about it)
    *board = *newboard;
    free(newboard);

    return 1;

error:
    ty_board_unref(newboard);
    return r;
}

void ty_board_close(ty_board *board)
{
    assert(board);

    if (board->h)
        ty_device_close(board->h);
    board->h = NULL;
}

ssize_t ty_board_read_serial(ty_board *board, char *buf, size_t size)
{
    assert(board);
    assert(board->h);
    assert(ty_board_has_capability(board, TY_BOARD_CAPABILITY_SERIAL));
    assert(buf);
    assert(size);

    ty_handle *h = board->h;

    ssize_t r;

    switch (board->dev->type) {
    case TY_DEVICE_SERIAL:
        return ty_serial_read(h, buf, size);

    case TY_DEVICE_HID:
        r = ty_hid_read(h, (uint8_t *)buf, size);
        if (r < 0)
            return r;
        else if (!r)
            return 0;
        return (ssize_t)strnlen(buf, size);
    }

    assert(false);
    __builtin_unreachable();
}

ssize_t ty_board_write_serial(ty_board *board, const char *buf, size_t size)
{
    assert(board);
    assert(board->h);
    assert(ty_board_has_capability(board, TY_BOARD_CAPABILITY_SERIAL));
    assert(buf);

    if (!size)
        return 0;

    ty_handle *h = board->h;

    uint8_t report[seremu_packet_size + 1];
    size_t total = 0;
    ssize_t r;

    if (!size)
        size = strlen(buf);

    switch (board->dev->type) {
    case TY_DEVICE_SERIAL:
        return ty_serial_write(h, buf, (ssize_t)size);

    case TY_DEVICE_HID:
        // SEREMU expects packets of 32 bytes
        for (size_t i = 0; i < size;) {
            memset(report, 0, sizeof(report));
            memcpy(report + 1, buf + i, TY_MIN(seremu_packet_size, size - i));

            r = ty_hid_write(h, report, sizeof(report));
            if (r < 0)
                return r;
            else if (!r)
                break;

            i += (size_t)r;
            total += (size_t)r - 1;
        }
        return (ssize_t)total;
    }

    assert(false);
    __builtin_unreachable();
}

static int halfkay_send(ty_board *board, size_t addr, void *data, size_t size, unsigned int timeout)
{
    uint8_t buf[2048] = {0};
    uint64_t end;

    const ty_board_model *model = board->model;
    ty_handle *h = board->h;

    ssize_t r = TY_ERROR_OTHER;

    // Update if header gets bigger than 64 bytes
    assert(size < sizeof(buf) - 65);

    switch (model->halfkay_version) {
    case 0:
        buf[1] = addr & 255;
        buf[2] = (addr >> 8) & 255;

        if (size)
            memcpy(buf + 3, data, size);
        size = model->block_size + 3;
        break;

    case 1:
        buf[1] = (addr >> 8) & 255;
        buf[2] = (addr >> 16) & 255;

        if (size)
            memcpy(buf + 3, data, size);
        size = model->block_size + 3;
        break;

    case 2:
        buf[1] = addr & 255;
        buf[2] = (addr >> 8) & 255;
        buf[3] = (addr >> 16) & 255;

        if (size)
            memcpy(buf + 65, data, size);
        size = model->block_size + 65;
        break;

    default:
        assert(false);
    }

    end = ty_millis() + timeout;

    // We may get errors along the way (while the bootloader works)
    // so try again until timeout expires.
    while (ty_millis() < end) {
        r = ty_hid_write(h, buf, size);
        if (r >= 0)
            return 0;

        ty_delay(10);
    }
    if (r < 0)
        return (int)r;

    return 0;
}

int ty_board_upload(ty_board *board, ty_firmware *f)
{
    assert(board);
    assert(board->h);
    assert(ty_board_has_capability(board, TY_BOARD_CAPABILITY_UPLOAD));
    assert(board->model);
    assert(f);

    const ty_board_model *model = board->model;

    if (f->size > model->code_size)
        return ty_error(TY_ERROR_RANGE, "Firmware is too big for %s", model->desc);

    for (size_t addr = 0; addr < f->size; addr += model->block_size) {
        size_t size = TY_MIN(model->block_size, (size_t)(f->size - addr));

        // Writing to the first block triggers flash erasure hence the longer timeout
        int r = halfkay_send(board, addr, f->image + addr, size, addr ? 300 : 3000);
        if (r < 0)
            return r;

        // HalfKay generates STALL if you go too fast (translates to EPIPE on Linux)
        ty_delay(addr ? 30 : 300);
    }

    return 0;
}

int ty_board_reset(ty_board *board)
{
    assert(board);
    assert(board->h);
    assert(ty_board_has_capability(board, TY_BOARD_CAPABILITY_RESET));

    halfkay_send(board, 0xFFFFFF, NULL, 0, 250);
    ty_board_close(board);

    // Give it time
    ty_delay(50);

    return 0;
}

int ty_board_reboot(ty_board *board)
{
    assert(board);
    assert(board->h);
    assert(ty_board_has_capability(board, TY_BOARD_CAPABILITY_REBOOT));

    ty_device *dev = board->dev;
    ty_handle *h = board->h;

    static unsigned char seremu_magic[] = {0, 0xA9, 0x45, 0xC2, 0x6B};
    int r = TY_ERROR_UNSUPPORTED;

    switch (dev->type) {
    case TY_DEVICE_SERIAL:
        r = ty_serial_set_control(h, 134, 0);
        break;

    case TY_DEVICE_HID:
        r = ty_hid_send_feature_report(h, seremu_magic, sizeof(seremu_magic));
        break;

    default:
        assert(false);
    }

    // Teensy waits for 15 SOF tokens (<2ms) before rebooting, but the OS may take its time
    // FIXME: poll for errors to detect when the device is really gone
    ty_delay(1000);

    return r;
}

const ty_board_model *ty_board_test_firmware(ty_firmware *f)
{
    assert(f);

    // Naive search with each board's signature, not pretty but unless
    // thousands of models appear this is good enough.

    size_t magic_size = sizeof(signatures[0].magic);

    if (f->size < magic_size)
        return NULL;

    for (size_t i = 0; i < f->size - magic_size; i++) {
        for (const struct firmware_signature *sig = signatures; sig->model; sig++) {
            if (memcmp(f->image + i, sig->magic, magic_size) == 0)
                return sig->model;
        }
    }

    return NULL;
}
