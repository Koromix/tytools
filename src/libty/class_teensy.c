/* TyTools - public domain
   Niels Martignène <niels.martignene@gmail.com>
   https://neodd.com/tytools

   This software is in the public domain. Where that dedication is not
   recognized, you are granted a perpetual, irrevocable license to copy,
   distribute, and modify this file as you see fit.

   See the LICENSE file for more details. */

#include "common_priv.h"
#include "../libhs/device.h"
#include "../libhs/hid.h"
#include "../libhs/serial.h"
#include "board.h"
#include "board_priv.h"
#include "class_priv.h"
#include "firmware.h"
#include "system.h"

#define SEREMU_TX_SIZE 32
#define SEREMU_RX_SIZE 64

enum {
    TEENSY_USAGE_PAGE_BOOTLOADER = 0xFF9C,
    TEENSY_USAGE_PAGE_RAWHID = 0xFFAB,
    TEENSY_USAGE_PAGE_SEREMU = 0xFFC9
};

extern const struct _ty_class_vtable _ty_teensy_class_vtable;

static ty_model identify_model(uint16_t usage)
{
    ty_model model = 0;
    switch (usage) {
        case 0x1A: model = TY_MODEL_TEENSY_PP_10; break;
        case 0x1B: model = TY_MODEL_TEENSY_20; break;
        case 0x1C: model = TY_MODEL_TEENSY_PP_20; break;
        case 0x1D: model = TY_MODEL_TEENSY_30; break;
        case 0x1E: model = TY_MODEL_TEENSY_31; break;
        case 0x20: model = TY_MODEL_TEENSY_LC; break;
        case 0x21: model = TY_MODEL_TEENSY_32; break;
        case 0x1F: model = TY_MODEL_TEENSY_35; break;
        case 0x22: model = TY_MODEL_TEENSY_36; break;
    }

    if (model != 0) {
        ty_log(TY_LOG_DEBUG, "Identified '%s' with usage value 0x%"PRIx16,
               ty_models[model].name, usage);
    } else {
        ty_log(TY_LOG_DEBUG, "Unknown Teensy model with usage value 0x%"PRIx16, usage);
    }

    return model;
}

static uint64_t parse_bootloader_serial_number(const char *s)
{
    uint64_t serial;

    // This happens for AVR Teensy boards (1.0 and 2.0)
    if (!s)
        return 12345;

    /* The bootloader returns the serial number as hexadecimal with prefixed zeros
       (which would suggest octal to stroull). */
    serial = strtoull(s, NULL, 16);

    /* In running modes, a decimal value is used but Teensyduino 1.19 added a workaround for a
       Mac OS X CDC-ADM driver bug: if the number is < 10000000, append a 0.
       See https://github.com/PaulStoffregen/cores/commit/4d8a62cf65624d2dc1d861748a9bb2e90aaf194

       It seems beta K66 boards without a programmed S/N report 00000064 (100) as the S/N and we
       need to ignore this value. */
    if (serial == 100) {
        serial = 0;
    } else if (serial < 10000000) {
        serial *= 10;
    }

    return serial;
}

static int teensy_load_interface(ty_board_interface *iface)
{
    hs_device *dev = iface->dev;

#define MAKE_UINT32(high, low) (((high) << 16) | ((low) & 0xFFFF))

    switch (MAKE_UINT32(dev->vid, dev->pid)) {
    case MAKE_UINT32(0x16C0, 0x0476):
    case MAKE_UINT32(0x16C0, 0x0478):
    case MAKE_UINT32(0x16C0, 0x0482):
    case MAKE_UINT32(0x16C0, 0x0483):
    case MAKE_UINT32(0x16C0, 0x0484):
    case MAKE_UINT32(0x16C0, 0x0485):
    case MAKE_UINT32(0x16C0, 0x0486):
    case MAKE_UINT32(0x16C0, 0x0487):
    case MAKE_UINT32(0x16C0, 0x0488):
    case MAKE_UINT32(0x16C0, 0x0489):
    case MAKE_UINT32(0x16C0, 0x048A):
    case MAKE_UINT32(0x16C0, 0x04D0):
    case MAKE_UINT32(0x16C0, 0x04D1):
    case MAKE_UINT32(0x16C0, 0x04D2):
    case MAKE_UINT32(0x16C0, 0x04D3):
    case MAKE_UINT32(0x16C0, 0x04D4):
    case MAKE_UINT32(0x16C0, 0x04D9):
        break;

    default:
        return 0;
    }

#undef MAKE_UINT32

    switch (dev->type) {
    case HS_DEVICE_TYPE_SERIAL:
        iface->name = "Serial";
        iface->capabilities |= 1 << TY_BOARD_CAPABILITY_RUN;
        iface->capabilities |= 1 << TY_BOARD_CAPABILITY_SERIAL;
        iface->capabilities |= 1 << TY_BOARD_CAPABILITY_REBOOT;
        break;

    case HS_DEVICE_TYPE_HID:
        switch (dev->u.hid.usage_page) {
        case TEENSY_USAGE_PAGE_BOOTLOADER:
            iface->name = "HalfKay";
            iface->model = identify_model(dev->u.hid.usage);
            if (iface->model) {
                iface->capabilities |= 1 << TY_BOARD_CAPABILITY_UPLOAD;
                iface->capabilities |= 1 << TY_BOARD_CAPABILITY_RESET;
            }
            break;

        case TEENSY_USAGE_PAGE_RAWHID:
            iface->name = "RawHID";
            iface->capabilities |= 1 << TY_BOARD_CAPABILITY_RUN;
            break;

        case TEENSY_USAGE_PAGE_SEREMU:
            iface->name = "Seremu";
            iface->capabilities |= 1 << TY_BOARD_CAPABILITY_RUN;
            iface->capabilities |= 1 << TY_BOARD_CAPABILITY_SERIAL;
            iface->capabilities |= 1 << TY_BOARD_CAPABILITY_REBOOT;
            break;

        default:
            return 0;
        }

        break;
    }

    iface->class_vtable = &_ty_teensy_class_vtable;
    if (!iface->model)
        iface->model = TY_MODEL_TEENSY;

    return 1;
}

static int teensy_update_board(ty_board_interface *iface, ty_board *board)
{
    ty_model model = 0;
    char *serial_number = NULL;
    char *description = NULL;
    char *id = NULL;
    int r;

    // Check and update board model
    if (ty_models[iface->model].code_size) {
        model = iface->model;

        if (ty_models[board->model].code_size && board->model != model) {
            r = 0;
            goto error;
        }
    } else if (!board->model) {
        model = iface->model;
    }

    // Check and update board serial number
    if (iface->dev->serial_number_string) {
        uint64_t serial_value;
        if (ty_models[iface->model].code_size) {
            serial_value = parse_bootloader_serial_number(iface->dev->serial_number_string);
        } else {
            serial_value = strtoull(iface->dev->serial_number_string, NULL, 10);
        }

        if (serial_value) {
            /* We cannot uniquely identify AVR Teensy boards because the S/N is always 12345,
               or custom ARM Teensy boards without a valid MAC address. Kind of dirty to
               change iface in this function but it should not be a problem. */
            if (serial_value != 12345)
                iface->capabilities |= 1 << TY_BOARD_CAPABILITY_UNIQUE;

            r = asprintf(&serial_number, "%"PRIu64, serial_value);
            if (r < 0) {
                r = ty_error(TY_ERROR_MEMORY, NULL);
                goto error;
            }

            if (board->serial_number && strcmp(serial_number, board->serial_number) != 0) {
                uint64_t board_serial_value = strtoull(board->serial_number, NULL, 10);

                /* Let boards using an old Teensyduino (before 1.19) firmware pass with a warning
                   because there is no way to interpret the serial number correctly, and the board
                   will show up as a different board if it is first plugged in bootloader mode.
                   The only way to fix this is to use Teensyduino >= 1.19. */
                if (ty_models[iface->model].code_size && serial_value == board_serial_value * 10) {
                    ty_log(TY_LOG_WARNING, "Upgrade board '%s' with recent Teensyduino version",
                           board->tag);
                } else {
                    r = 0;
                    goto error;
                }
            }
        }
    }

    // Update board description
    {
        const char *product_string = NULL;

        if (ty_models[iface->model].code_size) {
            if (!board->description)
                product_string = "Teensy (HalfKay)";
        } else if (iface->dev->product_string) {
            product_string = iface->dev->product_string;
        } else {
            product_string = "Teensy";
        }

        if (product_string && (!board->description ||
                               strcmp(product_string, board->description) != 0)) {
            description = strdup(product_string);
            if (!description) {
                r = ty_error(TY_ERROR_MEMORY, NULL);
                goto error;
            }
        }
    }

    // Update board unique identifier
    if (!board->id || serial_number) {
        r = asprintf(&id, "%s-Teensy", serial_number ? serial_number : "?");
        if (r < 0) {
            r = ty_error(TY_ERROR_MEMORY, NULL);
            goto error;
        }
    }

    // Everything is alright, we can commit changes
    if (model)
        board->model = model;
    if (serial_number) {
        free(board->serial_number);
        board->serial_number = serial_number;
    }
    if (description) {
        free(board->description);
        board->description = description;
    }
    if (id) {
        free(board->id);
        board->id = id;
    }

    return 1;

error:
    free(id);
    free(description);
    free(serial_number);
    return r;
}

static int change_baudrate(hs_port *port, unsigned int baudrate)
{
    hs_serial_config config = {
        .baudrate = baudrate
    };
    return ty_libhs_translate_error(hs_serial_set_config(port, &config));
}

static int teensy_open_interface(ty_board_interface *iface)
{
    int r;

    r = hs_port_open(iface->dev, HS_PORT_MODE_RW, &iface->port);
    if (r < 0)
        return ty_libhs_translate_error(r);

    /* Restore sane baudrate, because some systems (such as Linux) may keep tty settings
       around and reuse them. The device will keep rebooting if 134 is what stays around,
       so try to break the loop here. */
    if (iface->dev->type == HS_DEVICE_TYPE_SERIAL)
        change_baudrate(iface->port, 115200);

    return 0;
}

static void teensy_close_interface(ty_board_interface *iface)
{
    hs_port_close(iface->port);
    iface->port = NULL;
}

static unsigned int teensy_identify_models(const ty_firmware *fw, ty_model *rmodels,
                                           unsigned int max_models)
{
    /* Try ARM models first. We use a few facts to recognize these models:
       - The interrupt vector table (_VectorsFlash[]) is located at 0x0 (at least initially)
       - _VectorsFlash[] has a different length for each model
       - ResetHandler() comes right after _VectorsFlash (this is enforced by the LD script)
       - _VectorsFlash[1] contains the address of ResetHandler
       Knowing all that, we can read the ResetHandler address 4 bytes in, and recognize the model
       from this address.
       To make sure it's really a Teensy firmware, we then check for a magic signature value
       in the .startup section, which is the value assigned to SIM_SCGC5 in ResetHandler(). */
    const size_t teensy3_startup_size = 0x400;
    if (fw->size >= teensy3_startup_size) {
        uint32_t reset_handler_addr, magic_check;
        unsigned int models_count = 0;

        reset_handler_addr = (uint32_t)fw->image[4] | ((uint32_t)fw->image[5] << 8) |
                             ((uint32_t)fw->image[6] << 16) | ((uint32_t)fw->image[7] << 24);
        switch (reset_handler_addr) {
        case 0xF9:
            rmodels[models_count++] = TY_MODEL_TEENSY_30;
            magic_check = 0x00043F82;
            break;
        case 0x1BD:
            rmodels[models_count++] = TY_MODEL_TEENSY_31;
            if (max_models >= 2)
                rmodels[models_count++] = TY_MODEL_TEENSY_32;
            magic_check = 0x00043F82;
            break;
        case 0xC1:
            rmodels[models_count++] = TY_MODEL_TEENSY_LC;
            magic_check = 0x00003F82;
            break;
        case 0x199:
            rmodels[models_count++] = TY_MODEL_TEENSY_35;
            magic_check = 0x00043F82;
            break;
        case 0x1D1:
            rmodels[models_count++] = TY_MODEL_TEENSY_36;
            magic_check = 0x00043F82;
            break;
        }

        if (models_count) {
            for (size_t i = reset_handler_addr; i < teensy3_startup_size - sizeof(uint32_t); i++) {
                uint32_t value4 = (uint32_t)fw->image[i] |
                                  ((uint32_t)fw->image[i + 1] << 8) |
                                  ((uint32_t)fw->image[i + 2] << 16) |
                                  ((uint32_t)fw->image[i + 3] << 24);

                if (value4 == magic_check)
                    return models_count;
            }
        }
    }

    /* Now try AVR Teensies. We search for machine code that matches model-specific code in
       _reboot_Teensyduino_(). Not elegant, but it does the work. */
    if (fw->size > sizeof(uint64_t) && fw->size <= 130048) {
        for (size_t i = 0; i < fw->size - sizeof(uint64_t); i++) {
            uint64_t value8 = (uint64_t)fw->image[i] |
                              ((uint64_t)fw->image[i + 1] << 8) |
                              ((uint64_t)fw->image[i + 2] << 16) |
                              ((uint64_t)fw->image[i + 3] << 24) |
                              ((uint64_t)fw->image[i + 4] << 32) |
                              ((uint64_t)fw->image[i + 5] << 40) |
                              ((uint64_t)fw->image[i + 6] << 48) |
                              ((uint64_t)fw->image[i + 7] << 56);

            switch (value8) {
            case 0x94F8CFFF7E00940C:
                rmodels[0] = TY_MODEL_TEENSY_PP_10;
                return 1;
            case 0x94F8CFFF3F00940C:
                rmodels[0] = TY_MODEL_TEENSY_20;
                return 1;
            case 0x94F8CFFFFE00940C:
                rmodels[0] = TY_MODEL_TEENSY_PP_20;
                return 1;
            }
        }
    }

    return 0;
}

static ssize_t teensy_serial_read(ty_board_interface *iface, char *buf, size_t size, int timeout)
{
    uint8_t hid_buf[SEREMU_RX_SIZE + 1];
    ssize_t r;

    switch (iface->dev->type) {
    case HS_DEVICE_TYPE_SERIAL:
        r = hs_serial_read(iface->port, (uint8_t *)buf, size, timeout);
        if (r < 0)
            return ty_libhs_translate_error((int)r);
        return r;

    case HS_DEVICE_TYPE_HID:
        r = hs_hid_read(iface->port, hid_buf, sizeof(hid_buf), timeout);
        if (r < 0)
            return ty_libhs_translate_error((int)r);
        if (r < 2)
            return 0;

        r = (ssize_t)strnlen((char *)hid_buf + 1, (size_t)(r - 1));
        memcpy(buf, hid_buf + 1, (size_t)r);
        return r;
    }

    assert(false);
    return 0;
}

static ssize_t teensy_serial_write(ty_board_interface *iface, const char *buf, size_t size)
{
    uint8_t report[SEREMU_TX_SIZE + 1];
    size_t total = 0;
    ssize_t r;

    switch (iface->dev->type) {
    case HS_DEVICE_TYPE_SERIAL:
        r = hs_serial_write(iface->port, (uint8_t *)buf, size, 5000);
        if (r < 0)
            return ty_libhs_translate_error((int)r);
        if (!r)
            return ty_error(TY_ERROR_IO, "Timed out while writing to '%s'", iface->dev->path);

        return r;

    case HS_DEVICE_TYPE_HID:
        /* SEREMU expects packets of 32 bytes. The terminating NUL marks the end, so no binary
           transfers. */
        for (size_t i = 0; i < size;) {
            size_t block_size = TY_MIN(SEREMU_TX_SIZE, size - i);

            memset(report, 0, sizeof(report));
            memcpy(report + 1, buf + i, block_size);

            r = hs_hid_write(iface->port, report, sizeof(report));
            if (r < 0)
                return ty_libhs_translate_error((int)r);
            if (!r)
                break;

            i += block_size;
            total += block_size;
        }
        return (ssize_t)total;
    }

    assert(false);
    return 0;
}

static int halfkay_send(hs_port *port, unsigned int halfkay_version, size_t block_size,
                        size_t addr, const void *data, size_t size, unsigned int timeout)
{
    uint8_t buf[2048] = {0};
    uint64_t start;

    ssize_t r;

    // Update if header gets bigger than 64 bytes
    assert(size < sizeof(buf) - 65);

    switch (halfkay_version) {
    case 1:
        buf[1] = addr & 255;
        buf[2] = (addr >> 8) & 255;

        if (size)
            memcpy(buf + 3, data, size);
        size = block_size + 3;
        break;

    case 2:
        buf[1] = (addr >> 8) & 255;
        buf[2] = (addr >> 16) & 255;

        if (size)
            memcpy(buf + 3, data, size);
        size = block_size + 3;
        break;

    case 3:
        buf[1] = addr & 255;
        buf[2] = (addr >> 8) & 255;
        buf[3] = (addr >> 16) & 255;

        if (size)
            memcpy(buf + 65, data, size);
        size = block_size + 65;
        break;

    default:
        assert(false);
        break;
    }

    /* We may get errors along the way (while the bootloader works) so try again
       until timeout expires. */
    start = ty_millis();
    hs_error_mask(HS_ERROR_IO);
restart:
    r = hs_hid_write(port, buf, size);
    if (r == HS_ERROR_IO && ty_millis() - start < timeout) {
        ty_delay(10);
        goto restart;
    }
    hs_error_unmask();
    if (r < 0) {
        if (r == HS_ERROR_IO)
            return ty_error(TY_ERROR_IO, "%s", hs_error_last_message());
        return ty_libhs_translate_error((int)r);
    }

    return 0;
}

static int get_halfkay_settings(ty_model model, unsigned int *rhalfkay_version,
                                size_t *rblock_size)
{
    if ((model == TY_MODEL_TEENSY_PP_10 || model == TY_MODEL_TEENSY_20) &&
            !getenv("TY_EXPERIMENTAL_BOARDS")) {
        return ty_error(TY_ERROR_UNSUPPORTED,
                        "Support for %s boards is experimental, set environment variable"
                        "TY_EXPERIMENTAL_BOARDS to any value to enable support for them",
                        ty_models[model].name);
    }

    *rhalfkay_version = 0;
    switch ((ty_model_teensy)model) {
    case TY_MODEL_TEENSY_PP_10:
        *rhalfkay_version = 1;
        *rblock_size = 256;
        break;
    case TY_MODEL_TEENSY_20:
        *rhalfkay_version = 1;
        *rblock_size = 128;
        break;
    case TY_MODEL_TEENSY_PP_20:
        *rhalfkay_version = 2;
        *rblock_size = 256;
        break;
    case TY_MODEL_TEENSY_30:
    case TY_MODEL_TEENSY_31:
    case TY_MODEL_TEENSY_32:
    case TY_MODEL_TEENSY_35:
    case TY_MODEL_TEENSY_36:
        *rhalfkay_version = 3;
        *rblock_size = 1024;
        break;
    case TY_MODEL_TEENSY_LC:
        *rhalfkay_version = 3;
        *rblock_size = 512;
        break;

    case TY_MODEL_TEENSY:
        assert(false);
        break;
    }
    assert(*rhalfkay_version);

    return 0;
}

static int teensy_upload(ty_board_interface *iface, ty_firmware *fw,
                         ty_board_upload_progress_func *pf, void *udata)
{
    unsigned int halfkay_version = 0;
    size_t block_size = 0;
    int r;

    r = get_halfkay_settings(iface->model, &halfkay_version, &block_size);
    if (r < 0)
        return r;

    if (pf) {
        r = (*pf)(iface->board, fw, 0, udata);
        if (r)
            return r;
    }

    for (size_t addr = 0; addr < fw->size; addr += block_size) {
        size_t write_size = TY_MIN(block_size, (size_t)(fw->size - addr));

        r = halfkay_send(iface->port, halfkay_version, block_size,
                         addr, fw->image + addr, write_size, 3000);
        if (r < 0)
            return r;

        /* HalfKay generates STALL if you go too fast (translates to EPIPE on Linux), and the
           first write takes longer because it triggers a complete erase of all blocks. */
        ty_delay(addr ? 20 : 200);

        if (pf) {
            r = (*pf)(iface->board, fw, addr + write_size, udata);
            if (r)
                return r;
        }
    }

    return 0;
}

static int teensy_reset(ty_board_interface *iface)
{
    unsigned int halfkay_version = 0;
    size_t block_size = 0;

    int r = get_halfkay_settings(iface->model, &halfkay_version, &block_size);
    if (r < 0)
        return r;

    return halfkay_send(iface->port, halfkay_version, block_size, 0xFFFFFF, NULL, 0, 250);
}

static int teensy_reboot(ty_board_interface *iface)
{
    static const unsigned int serial_magic = 134;
    static const unsigned char seremu_magic[] = {0, 0xA9, 0x45, 0xC2, 0x6B};

    int r;

    r = TY_ERROR_UNSUPPORTED;
    switch (iface->dev->type) {
    case HS_DEVICE_TYPE_SERIAL:
        r = change_baudrate(iface->port, serial_magic);
        if (!r) {
            /* Don't keep these settings, some systems (such as Linux) may reuse them and
               the device will keep rebooting when opened. */
            ty_error_mask(TY_ERROR_SYSTEM);
            change_baudrate(iface->port, 115200);
            ty_error_unmask();
        }
        break;

    case HS_DEVICE_TYPE_HID:
        r = (int)hs_hid_send_feature_report(iface->port, seremu_magic, sizeof(seremu_magic));
        if (r < 0) {
            r = ty_libhs_translate_error(r);
        } else {
            assert(r == sizeof(seremu_magic));
            r = 0;
        }
        break;
    }

    return r;
}

const struct _ty_class_vtable _ty_teensy_class_vtable = {
    .load_interface = teensy_load_interface,
    .update_board = teensy_update_board,
    .identify_models = teensy_identify_models,

    .open_interface = teensy_open_interface,
    .close_interface = teensy_close_interface,
    .serial_read = teensy_serial_read,
    .serial_write = teensy_serial_write,
    .upload = teensy_upload,
    .reset = teensy_reset,
    .reboot = teensy_reboot
};
