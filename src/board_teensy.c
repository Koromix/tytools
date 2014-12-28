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
#include "ty/board.h"
#include "board_priv.h"
#include "ty/firmware.h"
#include "ty/system.h"

struct ty_board_model {
    struct ty_board_model_;

    // Upload settings
    uint8_t usage;
    uint8_t halfkay_version;
    size_t block_size;

    // Build settings
    const char *toolchain;
    const char *core;
    uint32_t frequency;
    const char *flags;
    const char *ldflags;
};

struct ty_board_mode {
    struct ty_board_mode_;

    // Build settings
    const char *flags;
};

static const struct _ty_board_mode_vtable teensy_mode_vtable;

const ty_board_mode _ty_teensy_bootloader_mode = {
    .name = "bootloader",
    .desc = "HalfKay Bootloader",

    .vtable = &teensy_mode_vtable,

    .type = TY_DEVICE_HID,
    .vid = 0x16C0,
    .pid = 0x478,
    .iface = 0,
    .capabilities = TY_BOARD_CAPABILITY_IDENTIFY | TY_BOARD_CAPABILITY_UPLOAD | TY_BOARD_CAPABILITY_RESET
};

const ty_board_mode _ty_teensy_flightsim_mode = {
    .name = "flightsim",
    .desc = "FlightSim",

    .vtable = &teensy_mode_vtable,

    .type = TY_DEVICE_HID,
    .vid = 0x16C0,
    .pid = 0x488,
    .iface = 1,
    .capabilities = TY_BOARD_CAPABILITY_SERIAL | TY_BOARD_CAPABILITY_REBOOT,

    // FIXME: build capability?
    .flags = "-DUSB_FLIGHTSIM -DLAYOUT_US_ENGLISH"
};

const ty_board_mode _ty_teensy_hid_mode = {
    .name = "hid",
    .desc = "HID",

    .vtable = &teensy_mode_vtable,

    .type = TY_DEVICE_HID,
    .vid = 0x16C0,
    .pid = 0x482,
    .iface = 2,
    .capabilities = TY_BOARD_CAPABILITY_SERIAL | TY_BOARD_CAPABILITY_REBOOT,

    .flags = "-DUSB_HID -DLAYOUT_US_ENGLISH"
};

const ty_board_mode _ty_teensy_midi_mode = {
    .name = "midi",
    .desc = "MIDI",

    .vtable = &teensy_mode_vtable,

    .type = TY_DEVICE_HID,
    .vid = 0x16C0,
    .pid = 0x485,
    .iface = 1,
    .capabilities = TY_BOARD_CAPABILITY_SERIAL | TY_BOARD_CAPABILITY_REBOOT,

    .flags = "-DUSB_MIDI -DLAYOUT_US_ENGLISH"
};

const ty_board_mode _ty_teensy_rawhid_mode = {
    .name = "rawhid",
    .desc = "Raw HID",

    .vtable = &teensy_mode_vtable,

    .type = TY_DEVICE_HID,
    .vid = 0x16C0,
    .pid = 0x486,
    .iface = 1,
    .capabilities = TY_BOARD_CAPABILITY_SERIAL | TY_BOARD_CAPABILITY_REBOOT,

    .flags = "-DUSB_RAWHID -DLAYOUT_US_ENGLISH"
};

const ty_board_mode _ty_teensy_serial_mode = {
    .name = "serial",
    .desc = "Serial",

    .vtable = &teensy_mode_vtable,

    .type = TY_DEVICE_SERIAL,
    .vid = 0x16C0,
    .pid = 0x483,
    .iface = 0,
    .capabilities = TY_BOARD_CAPABILITY_SERIAL | TY_BOARD_CAPABILITY_REBOOT,

    .flags = "-DUSB_SERIAL -DLAYOUT_US_ENGLISH"
};

const ty_board_mode _ty_teensy_serial_hid_mode = {
    .name = "serial_hid",
    .desc = "Serial HID",

    .vtable = &teensy_mode_vtable,

    .type = TY_DEVICE_SERIAL,
    .vid = 0x16C0,
    .pid = 0x487,
    .iface = 0,
    .capabilities = TY_BOARD_CAPABILITY_SERIAL | TY_BOARD_CAPABILITY_REBOOT,

    .flags = "-DUSB_SERIAL_HID -DLAYOUT_US_ENGLISH"
};

static const struct _ty_board_model_vtable teensy_model_vtable;

const ty_board_model _ty_teensy_pp10_model = {
    .name = "teensy++10",
    .mcu = "at90usb646",
    .desc = "Teensy++ 1.0",

    .vtable = &teensy_model_vtable,

    .usage = 0x1A,
    .halfkay_version = 0,
    .code_size = 64512,
    .block_size = 256,

    .toolchain = "avr",
    .core = "teensy/cores/teensy",
    .frequency = 16000000,
    .flags = "-mmcu=at90usb646",
    .ldflags = "-mmcu=at90usb646"
};

const ty_board_model _ty_teensy_20_model = {
    .name = "teensy20",
    .mcu = "atmega32u4",
    .desc = "Teensy 2.0",

    .vtable = &teensy_model_vtable,

    .usage = 0x1B,
    .halfkay_version = 0,
    .code_size = 32256,
    .block_size = 128,

    .toolchain = "avr",
    .core = "teensy/cores/teensy",
    .frequency = 16000000,
    .flags = "-mmcu=atmega32u4",
    .ldflags = "-mmcu=atmega32u4"
};

const ty_board_model _ty_teensy_pp20_model = {
    .name = "teensy++20",
    .mcu = "at90usb1286",
    .desc = "Teensy++ 2.0",

    .vtable = &teensy_model_vtable,

    .usage = 0x1C,
    .halfkay_version = 1,
    .code_size = 130048,
    .block_size = 256,

    .toolchain = "avr",
    .core = "teensy/cores/teensy",
    .frequency = 16000000,
    .flags = "-mmcu=at90usb1286",
    .ldflags = "-mmcu=at90usb1286"
};

const ty_board_model _ty_teensy_30_model = {
    .name = "teensy30",
    .mcu = "mk20dx128",
    .desc = "Teensy 3.0",

    .vtable = &teensy_model_vtable,

    .usage = 0x1D,
    .halfkay_version = 2,
    .code_size = 131072,
    .block_size = 1024,

    .toolchain = "arm-none-eabi",
    .core = "teensy/cores/teensy3",
    .frequency = 96000000,
    .flags = "-mcpu=cortex-m4 -mthumb -D__MK20DX128__",
    .ldflags = "-mcpu=cortex-m4 -mthumb -T\"$arduino/hardware/teensy/cores/teensy3/mk20dx128.ld\""
};

const ty_board_model _ty_teensy_31_model = {
    .name = "teensy31",
    .mcu = "mk20dx256",
    .desc = "Teensy 3.1",

    .vtable = &teensy_model_vtable,

    .usage = 0x1E,
    .halfkay_version = 2,
    .code_size = 262144,
    .block_size = 1024,

    .toolchain = "arm-none-eabi",
    .core = "teensy/cores/teensy3",
    .frequency = 96000000,
    .flags = "-mcpu=cortex-m4 -mthumb -D__MK20DX256__",
    .ldflags = "-mcpu=cortex-m4 -mthumb -T\"$arduino/hardware/teensy/cores/teensy3/mk20dx256.ld\""
};

static const size_t seremu_packet_size = 32;

static int teensy_identify(ty_board *board)
{
    ty_hid_descriptor desc;
    int r;

    r = ty_hid_parse_descriptor(board->h, &desc);
    if (r < 0)
        return r;

    board->model = NULL;
    for (const ty_board_model **cur = ty_board_models; *cur; cur++) {
        const ty_board_model *model = *cur;

        if (model->vtable != &teensy_model_vtable)
            continue;

        if (model->usage == desc.usage) {
            board->model = *cur;
            break;
        }
    }
    if (!board->model)
        return ty_error(TY_ERROR_UNSUPPORTED, "Unknown board model");

    return 0;
}

static ssize_t teensy_read_serial(ty_board *board, char *buf, size_t size)
{
    ssize_t r;

    switch (ty_device_get_type(board->dev)) {
    case TY_DEVICE_SERIAL:
        return ty_serial_read(board->h, buf, size);

    case TY_DEVICE_HID:
        r = ty_hid_read(board->h, (uint8_t *)buf, size);
        if (r < 0)
            return r;
        else if (!r)
            return 0;
        return (ssize_t)strnlen(buf, size);
    }

    assert(false);
    __builtin_unreachable();
}

static ssize_t teensy_write_serial(ty_board *board, const char *buf, size_t size)
{
    uint8_t report[seremu_packet_size + 1];
    size_t total = 0;
    ssize_t r;

    switch (ty_device_get_type(board->dev)) {
    case TY_DEVICE_SERIAL:
        return ty_serial_write(board->h, buf, (ssize_t)size);

    case TY_DEVICE_HID:
        // SEREMU expects packets of 32 bytes
        for (size_t i = 0; i < size;) {
            memset(report, 0, sizeof(report));
            memcpy(report + 1, buf + i, TY_MIN(seremu_packet_size, size - i));

            r = ty_hid_write(board->h, report, sizeof(report));
            if (r < 0)
                return r;
            else if (!r)
                break;

            i += (size_t)r - 1;
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
    uint64_t start;

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

    start = ty_millis();

    // We may get errors along the way (while the bootloader works)
    // so try again until timeout expires.
    do {
        r = ty_hid_write(h, buf, size);
        if (r >= 0)
            return 0;

        ty_delay(10);
    } while (ty_millis() - start <= timeout);
    if (r < 0)
        return (int)r;

    return 0;
}

static int teensy_upload(ty_board *board, ty_firmware *f, uint16_t flags)
{
    TY_UNUSED(flags);

    for (size_t addr = 0; addr < f->size; addr += board->model->block_size) {
        size_t size;
        int r;

        size = TY_MIN(board->model->block_size, (size_t)(f->size - addr));

        // Writing to the first block triggers flash erasure hence the longer timeout
        r = halfkay_send(board, addr, f->image + addr, size, addr ? 300 : 3000);
        if (r < 0)
            return r;

        // HalfKay generates STALL if you go too fast (translates to EPIPE on Linux)
        ty_delay(addr ? 30 : 300);
    }

    return 0;
}

static int teensy_reset(ty_board *board)
{
    return halfkay_send(board, 0xFFFFFF, NULL, 0, 250);
}

static int teensy_reboot(ty_board *board)
{
    static unsigned char seremu_magic[] = {0, 0xA9, 0x45, 0xC2, 0x6B};
    int r;

    r = TY_ERROR_UNSUPPORTED;
    switch (ty_device_get_type(board->dev)) {
    case TY_DEVICE_SERIAL:
        r = ty_serial_set_control(board->h, 134, 0);
        break;

    case TY_DEVICE_HID:
        r = ty_hid_send_feature_report(board->h, seremu_magic, sizeof(seremu_magic));
        break;

    default:
        assert(false);
    }

    return r;
}

static const struct _ty_board_mode_vtable teensy_mode_vtable = {
    .identify = teensy_identify,
    .read_serial = teensy_read_serial,
    .write_serial = teensy_write_serial,
    .upload = teensy_upload,
    .reset = teensy_reset,
    .reboot = teensy_reboot
};

static const struct _ty_board_model_vtable teensy_model_vtable = {
};
