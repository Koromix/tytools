/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "ty/common.h"
#include "compat.h"
#include "ty/board.h"
#include "board_priv.h"
#include "ty/firmware.h"
#include "ty/system.h"

struct tyb_board_model {
    TYB_BOARD_MODEL

    // Identifcation
    uint8_t usage;
    bool experimental;

    // Upload settings
    unsigned int halfkay_version;
    size_t block_size;

    // Firmware signature
    uint8_t signature[8];
};

#define TEENSY_VID 0x16C0

enum {
    TEENSY_USAGE_PAGE_BOOTLOADER = 0xFF9C,
    TEENSY_USAGE_PAGE_SEREMU = 0xFFC9
};

const tyb_board_family _tyb_teensy_family;
static const struct _tyb_board_interface_vtable teensy_vtable;

static const tyb_board_model teensy_unknown_model = {
    .family = &_tyb_teensy_family,
    .name = "Teensy"
};

static const tyb_board_model teensy_pp10_model = {
    .family = &_tyb_teensy_family,
    .name = "Teensy++ 1.0",
    .mcu = "at90usb646",

    .usage = 0x1A,
    .experimental = true,

    .code_size = 64512,
    .halfkay_version = 1,
    .block_size = 256,

    .signature = {0x0C, 0x94, 0x00, 0x7E, 0xFF, 0xCF, 0xF8, 0x94}
};

static const tyb_board_model teensy_20_model = {
    .family = &_tyb_teensy_family,
    .name = "Teensy 2.0",
    .mcu = "atmega32u4",

    .usage = 0x1B,
    .experimental = true,

    .code_size = 32256,
    .halfkay_version = 1,
    .block_size = 128,

    .signature = {0x0C, 0x94, 0x00, 0x3F, 0xFF, 0xCF, 0xF8, 0x94}
};

static const tyb_board_model teensy_pp20_model = {
    .family = &_tyb_teensy_family,
    .name = "Teensy++ 2.0",
    .mcu = "at90usb1286",

    .usage = 0x1C,
    .experimental = true,

    .code_size = 130048,
    .halfkay_version = 2,
    .block_size = 256,

    .signature = {0x0C, 0x94, 0x00, 0xFE, 0xFF, 0xCF, 0xF8, 0x94}
};

static const tyb_board_model teensy_30_model = {
    .family = &_tyb_teensy_family,
    .name = "Teensy 3.0",
    .mcu = "mk20dx128",

    .usage = 0x1D,

    .code_size = 131072,
    .halfkay_version = 3,
    .block_size = 1024,

    .signature = {0x38, 0x80, 0x04, 0x40, 0x82, 0x3F, 0x04, 0x00}
};

static const tyb_board_model teensy_31_model = {
    .family = &_tyb_teensy_family,
    .name = "Teensy 3.1",
    .mcu = "mk20dx256",

    .usage = 0x1E,

    .code_size = 262144,
    .halfkay_version = 3,
    .block_size = 1024,

    .signature = {0x30, 0x80, 0x04, 0x40, 0x82, 0x3F, 0x04, 0x00}
};

static const tyb_board_model teensy_lc_model = {
    .family = &_tyb_teensy_family,
    .name = "Teensy LC",
    .mcu = "mkl26z64",

    .usage = 0x20,

    .code_size = 63488,
    .halfkay_version = 3,
    .block_size = 512,

    .signature = {0x34, 0x80, 0x04, 0x40, 0x82, 0x3F, 0x00, 0x00}
};

static const tyb_board_model *teensy_models[] = {
    &teensy_pp10_model,
    &teensy_20_model,
    &teensy_pp20_model,
    &teensy_30_model,
    &teensy_31_model,
    &teensy_lc_model,
    NULL
};

static const size_t seremu_packet_size = 32;

static const tyb_board_model *identify_model(const tyd_hid_descriptor *desc)
{
    if (desc->usage_page != TEENSY_USAGE_PAGE_BOOTLOADER)
        return NULL;

    for (const tyb_board_model **cur = teensy_models; *cur; cur++) {
        const tyb_board_model *model = *cur;

        if (model->usage == desc->usage)
            return *cur;
    }

    return NULL;
}

/* Two quirks have to be accounted when reading the serial number.

   The bootloader returns the serial number as hexadecimal with prefixed zeros
   (which would suggest octal to stroull).

   In other modes a decimal value is used, but Teensyduino 1.19 added a workaround
   for a Mac OS X CDC-ADM driver bug: if the number is < 10000000, append a 0.
   See https://github.com/PaulStoffregen/cores/commit/4d8a62cf65624d2dc1d861748a9bb2e90aaf194d */
static uint64_t parse_bootloader_serial(const char *s)
{
    uint64_t serial;

    if (!s)
        return 0;

    serial = strtoull(s, NULL, 16);
    if (serial < 10000000)
        serial *= 10;

    return serial;
}

static int teensy_open_interface(tyb_board_interface *iface)
{
    tyd_hid_descriptor desc;
    int r;

    if (tyd_device_get_vid(iface->dev) != TEENSY_VID)
        return 0;

    switch (tyd_device_get_pid(iface->dev)) {
    case 0x478:
    case 0x482:
    case 0x483:
    case 0x484:
    case 0x485:
    case 0x486:
    case 0x487:
    case 0x488:
        break;

    default:
        return 0;
    }

    if (!iface->h) {
        r = tyd_device_open(iface->dev, &iface->h);
        if (r < 0)
            return r;
    }

    switch (tyd_device_get_type(iface->dev)) {
    case TYD_DEVICE_SERIAL:
        /* Restore sane baudrate, because some systems (such as Linux) may keep tty settings
           around and reuse them. The device will keep rebooting if 134 is what stays around,
           so try to break the loop here. */
        tyd_serial_set_attributes(iface->h, 115200, 0);

        iface->desc = "Serial";
        iface->capabilities |= 1 << TYB_BOARD_CAPABILITY_SERIAL;
        iface->capabilities |= 1 << TYB_BOARD_CAPABILITY_REBOOT;
        break;

    case TYD_DEVICE_HID:
        r = tyd_hid_parse_descriptor(iface->h, &desc);
        if (r < 0)
            return r;

        switch (desc.usage_page) {
        case TEENSY_USAGE_PAGE_BOOTLOADER:
            iface->model = identify_model(&desc);
            iface->serial = parse_bootloader_serial(tyd_device_get_serial_number(iface->dev));

            iface->desc = "HalfKay Bootloader";
            if (iface->model) {
                iface->capabilities |= 1 << TYB_BOARD_CAPABILITY_UPLOAD;
                iface->capabilities |= 1 << TYB_BOARD_CAPABILITY_RESET;
            }
            break;

        case TEENSY_USAGE_PAGE_SEREMU:
            iface->desc = "Seremu";
            iface->capabilities |= 1 << TYB_BOARD_CAPABILITY_SERIAL;
            iface->capabilities |= 1 << TYB_BOARD_CAPABILITY_REBOOT;
            break;

        default:
            return 0;
        }

        break;
    }

    if (!iface->model)
        iface->model = &teensy_unknown_model;
    iface->vtable = &teensy_vtable;

    return 1;
}

static unsigned int teensy_guess_models(const tyb_firmware *fw,
                                        const tyb_board_model **rguesses, unsigned int size)
{
    unsigned int count = 0;

    if (fw->size < ty_member_sizeof(tyb_board_model, signature))
        return NULL;

    /* Naive search with each board's signature, not pretty but unless
       thousands of models appear this is good enough. */
    for (size_t i = 0; i < fw->size - ty_member_sizeof(tyb_board_model, signature); i++) {
        for (const tyb_board_model **cur = teensy_models; *cur; cur++) {
            const tyb_board_model *model = *cur;

            if (memcmp(fw->image + i, model->signature, ty_member_sizeof(tyb_board_model, signature)) == 0) {
                rguesses[count++] = model;
                if (count == size)
                    return count;
            }
        }
    }

    return count;
}

static int teensy_serial_set_attributes(tyb_board_interface *iface, uint32_t rate, int flags)
{
    if (tyd_device_get_type(iface->dev) != TYD_DEVICE_SERIAL)
        return 0;

    return tyd_serial_set_attributes(iface->h, rate, flags);
}

static ssize_t teensy_serial_read(tyb_board_interface *iface, char *buf, size_t size, int timeout)
{
    ssize_t r;

    switch (tyd_device_get_type(iface->dev)) {
    case TYD_DEVICE_SERIAL:
        return tyd_serial_read(iface->h, buf, size, timeout);

    case TYD_DEVICE_HID:
        r = tyd_hid_read(iface->h, (uint8_t *)buf, size, timeout);
        if (r < 0)
            return r;
        if (!r)
            return 0;
        return (ssize_t)strnlen(buf, (size_t)r);
    }

    assert(false);
    __builtin_unreachable();
}

static ssize_t teensy_serial_write(tyb_board_interface *iface, const char *buf, size_t size)
{
    uint8_t report[seremu_packet_size + 1];
    size_t total = 0;
    ssize_t r;

    switch (tyd_device_get_type(iface->dev)) {
    case TYD_DEVICE_SERIAL:
        return tyd_serial_write(iface->h, buf, (ssize_t)size);

    case TYD_DEVICE_HID:
        /* SEREMU expects packets of 32 bytes. The terminating NUL marks the end, so no binary
           transfers. */
        for (size_t i = 0; i < size;) {
            memset(report, 0, sizeof(report));
            memcpy(report + 1, buf + i, TY_MIN(seremu_packet_size, size - i));

            r = tyd_hid_write(iface->h, report, sizeof(report));
            if (r < 0)
                return r;
            if (!r)
                break;

            i += (size_t)r - 1;
            total += (size_t)r - 1;
        }
        return (ssize_t)total;
    }

    assert(false);
    __builtin_unreachable();
}

static int halfkay_send(tyb_board_interface *iface, size_t addr, void *data, size_t size, unsigned int timeout)
{
    uint8_t buf[2048] = {0};
    uint64_t start;

    ssize_t r;

    // Update if header gets bigger than 64 bytes
    assert(size < sizeof(buf) - 65);

    switch (iface->model->halfkay_version) {
    case 1:
        buf[1] = addr & 255;
        buf[2] = (addr >> 8) & 255;

        if (size)
            memcpy(buf + 3, data, size);
        size = iface->model->block_size + 3;
        break;

    case 2:
        buf[1] = (addr >> 8) & 255;
        buf[2] = (addr >> 16) & 255;

        if (size)
            memcpy(buf + 3, data, size);
        size = iface->model->block_size + 3;
        break;

    case 3:
        buf[1] = addr & 255;
        buf[2] = (addr >> 8) & 255;
        buf[3] = (addr >> 16) & 255;

        if (size)
            memcpy(buf + 65, data, size);
        size = iface->model->block_size + 65;
        break;

    default:
        assert(false);
    }

    /* We may get errors along the way (while the bootloader works) so try again
       until timeout expires. */
    start = ty_millis();
    do {
        r = tyd_hid_write(iface->h, buf, size);
        if (r >= 0)
            return 0;

        ty_delay(10);
    } while (ty_millis() - start <= timeout);
    if (r < 0)
        return (int)r;

    return 0;
}

static int teensy_upload(tyb_board_interface *iface, tyb_firmware *f, int flags, tyb_board_upload_progress_func *pf, void *udata)
{
    TY_UNUSED(flags);

    if (iface->model->experimental && !ty_config_experimental)
        return ty_error(TY_ERROR_UNSUPPORTED, "Upload to %s is disabled, use --experimental", iface->model->name);

    int r;

    if (pf) {
        r = (*pf)(iface->board, f, 0, udata);
        if (r)
            return r;
    }

    for (size_t addr = 0; addr < f->size; addr += iface->model->block_size) {
        size_t size = TY_MIN(iface->model->block_size, (size_t)(f->size - addr));

        // Writing to the first block triggers flash erasure hence the longer timeout
        r = halfkay_send(iface, addr, f->image + addr, size, addr ? 300 : 3000);
        if (r < 0)
            return r;

        /* HalfKay generates STALL if you go too fast (translates to EPIPE on Linux), and the
           first write takes longer because it triggers a complete erase of all blocks. */
        ty_delay(addr ? 10 : 100);

        if (pf) {
            r = (*pf)(iface->board, f, addr + size, udata);
            if (r)
                return r;
        }
    }

    return 0;
}

static int teensy_reset(tyb_board_interface *iface)
{
    if (iface->model->experimental && !ty_config_experimental)
        return ty_error(TY_ERROR_UNSUPPORTED, "Reset of %s is disabled, use --experimental", iface->model->name);

    return halfkay_send(iface, 0xFFFFFF, NULL, 0, 250);
}

static int teensy_reboot(tyb_board_interface *iface)
{
    static unsigned char seremu_magic[] = {0, 0xA9, 0x45, 0xC2, 0x6B};

    int r;

    r = TY_ERROR_UNSUPPORTED;
    switch (tyd_device_get_type(iface->dev)) {
    case TYD_DEVICE_SERIAL:
        r = tyd_serial_set_attributes(iface->h, 134, 0);
        if (!r) {
            /* Don't keep these settings, some systems (such as Linux) may reuse them and
               the device will keep rebooting when opened. */
            tyd_serial_set_attributes(iface->h, 115200, 0);
        }
        break;

    case TYD_DEVICE_HID:
        r = (int)tyd_hid_send_feature_report(iface->h, seremu_magic, sizeof(seremu_magic));
        if (r >= 0) {
            assert(r == sizeof(seremu_magic));
            r = 0;
        }
        break;

    default:
        assert(false);
    }

    return r;
}

const tyb_board_family _tyb_teensy_family = {
    .name = "Teensy",
    .models = teensy_models,

    .open_interface = teensy_open_interface,
    .guess_models = teensy_guess_models
};

static const struct _tyb_board_interface_vtable teensy_vtable = {
    .serial_set_attributes = teensy_serial_set_attributes,
    .serial_read = teensy_serial_read,
    .serial_write = teensy_serial_write,

    .upload = teensy_upload,
    .reset = teensy_reset,

    .reboot = teensy_reboot
};
