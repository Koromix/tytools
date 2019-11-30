/* TyTools - public domain
   Niels Martignène <niels.martignene@protonmail.com>
   https://koromix.dev/tytools

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

static ty_model identify_model_bcd(uint16_t bcd_device)
{
    ty_model model = 0;
    switch (bcd_device) {
        case 0x274: { model = TY_MODEL_TEENSY_30; } break;
        case 0x275: { model = TY_MODEL_TEENSY_31; } break;
        case 0x273: { model = TY_MODEL_TEENSY_LC; } break;
        case 0x276: { model = TY_MODEL_TEENSY_35; } break;
        case 0x277: { model = TY_MODEL_TEENSY_36; } break;
        case 0x278: { model = TY_MODEL_TEENSY_40_BETA1; } break;
        case 0x279: { model = TY_MODEL_TEENSY_40; } break;
    }

    if (model != 0) {
        ty_log(TY_LOG_DEBUG, "Identified '%s' with bcdDevice value 0x%"PRIx16,
               ty_models[model].name, bcd_device);
    } else {
        ty_log(TY_LOG_DEBUG, "Unknown %s model with bcdDevice value 0x%"PRIx16,
               ty_models[TY_MODEL_TEENSY].name, bcd_device);
    }

    return model;
}

static ty_model identify_model_halfkay(uint16_t usage)
{
    ty_model model = 0;
    switch (usage) {
        case 0x1A: { model = TY_MODEL_TEENSY_PP_10; } break;
        case 0x1B: { model = TY_MODEL_TEENSY_20; } break;
        case 0x1C: { model = TY_MODEL_TEENSY_PP_20; } break;
        case 0x1D: { model = TY_MODEL_TEENSY_30; } break;
        case 0x1E: { model = TY_MODEL_TEENSY_31; } break;
        case 0x20: { model = TY_MODEL_TEENSY_LC; } break;
        case 0x21: { model = TY_MODEL_TEENSY_32; } break;
        case 0x1F: { model = TY_MODEL_TEENSY_35; } break;
        case 0x22: { model = TY_MODEL_TEENSY_36; } break;
        case 0x23: { model = TY_MODEL_TEENSY_40_BETA1; } break;
        case 0x24: { model = TY_MODEL_TEENSY_40; } break;
    }

    if (model != 0) {
        ty_log(TY_LOG_DEBUG, "Identified '%s' with usage value 0x%"PRIx16,
               ty_models[model].name, usage);
    } else {
        ty_log(TY_LOG_DEBUG, "Unknown %s model with usage value 0x%"PRIx16,
               ty_models[TY_MODEL_TEENSY].name, usage);
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

    switch (dev->type) {
        case HS_DEVICE_TYPE_SERIAL: {
            iface->name = "Serial";
            iface->capabilities |= 1 << TY_BOARD_CAPABILITY_RUN;
            iface->capabilities |= 1 << TY_BOARD_CAPABILITY_SERIAL;
            iface->capabilities |= 1 << TY_BOARD_CAPABILITY_REBOOT;
        } break;

        case HS_DEVICE_TYPE_HID: {
            switch (dev->u.hid.usage_page) {
                case TEENSY_USAGE_PAGE_BOOTLOADER: {
                    iface->name = "HalfKay";
                    iface->model = identify_model_halfkay(dev->u.hid.usage);
                    if (iface->model) {
                        iface->capabilities |= 1 << TY_BOARD_CAPABILITY_UPLOAD;
                        iface->capabilities |= 1 << TY_BOARD_CAPABILITY_RESET;
                    }
                } break;

                case TEENSY_USAGE_PAGE_RAWHID: {
                    iface->name = "RawHID";
                    iface->capabilities |= 1 << TY_BOARD_CAPABILITY_RUN;
                } break;

                case TEENSY_USAGE_PAGE_SEREMU: {
                    iface->name = "Seremu";
                    iface->capabilities |= 1 << TY_BOARD_CAPABILITY_RUN;
                    iface->capabilities |= 1 << TY_BOARD_CAPABILITY_SERIAL;
                    iface->capabilities |= 1 << TY_BOARD_CAPABILITY_REBOOT;
                } break;

                default: {
                    return 0;
                } break;
            }
        } break;
    }

    if (!iface->model) {
        iface->model = identify_model_bcd(dev->bcd_device);
        if (!iface->model)
            iface->model = TY_MODEL_TEENSY;
    }

    iface->class_vtable = &_ty_teensy_class_vtable;

    return 1;
}

static int teensy_update_board(ty_board_interface *iface, ty_board *board, bool new_board)
{
    ty_model model = 0;
    char *serial_number = NULL;
    char *description = NULL;
    char *id = NULL;
    int r;

    // Check and update board model
    if (iface->model != TY_MODEL_TEENSY) {
        model = iface->model;

        // With the bcdDevice method we detect Teensy 3.2 as Teensy 3.1, tolerate the difference
        if (board->model == TY_MODEL_TEENSY_31 && model == TY_MODEL_TEENSY_32 &&
                iface->capabilities & (1 << TY_BOARD_CAPABILITY_UPLOAD)) {
            // Keep the bootloader info (more accurate)
        } else if (board->model == TY_MODEL_TEENSY_32 && model == TY_MODEL_TEENSY_31 &&
                   !(iface->capabilities & (1 << TY_BOARD_CAPABILITY_UPLOAD))) {
            model = 0;
        } else if (!new_board && board->model != TY_MODEL_TEENSY && board->model != model) {
            r = 0;
            goto error;
        }
    } else if (!board->model) {
        model = iface->model;
    }

    // Check and update board serial number
    if (iface->dev->serial_number_string) {
        uint64_t serial_value;
        if (iface->capabilities & (1 << TY_BOARD_CAPABILITY_UPLOAD)) {
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
                if (iface->capabilities & (1 << TY_BOARD_CAPABILITY_UPLOAD) &&
                        serial_value == board_serial_value * 10) {
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

        if (iface->capabilities & (1 << TY_BOARD_CAPABILITY_UPLOAD)) {
            if (!board->description)
                product_string = "HalfKay";
        } else if (iface->dev->product_string) {
            product_string = iface->dev->product_string;
        } else {
            product_string = ty_models[TY_MODEL_TEENSY].name;
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
        r = asprintf(&id, "%s-%s", serial_number ? serial_number : "?",
                     ty_models[TY_MODEL_TEENSY].name);
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

static uint32_t read_uint32_le(const uint8_t *ptr)
{
    return (uint32_t)ptr[0] |
           ((uint32_t)ptr[1] << 8) |
           ((uint32_t)ptr[2] << 16) |
           ((uint32_t)ptr[3] << 24);
}

static uint64_t read_uint64_le(const uint8_t *ptr)
{
    return (uint64_t)ptr[0] |
           ((uint64_t)ptr[1] << 8) |
           ((uint64_t)ptr[2] << 16) |
           ((uint64_t)ptr[3] << 24) |
           ((uint64_t)ptr[4] << 32) |
           ((uint64_t)ptr[5] << 40) |
           ((uint64_t)ptr[6] << 48) |
           ((uint64_t)ptr[7] << 56);
}

static unsigned int teensy_identify_models(const ty_firmware *fw, ty_model *rmodels,
                                           unsigned int max_models)
{
    const ty_firmware_segment *segment0 = ty_firmware_find_segment(fw, 0);
    const ty_firmware_segment *teensy4_segment = ty_firmware_find_segment(fw, 0x60000000);

    // First, try the Teensy 4.0
    if (teensy4_segment && teensy4_segment->size >= sizeof(uint64_t)) {
        uint64_t flash_config_8 = read_uint64_le(teensy4_segment->data);

        if (flash_config_8 == 0x5601000042464346) {
            unsigned int models_count = 0;

            rmodels[models_count++] = TY_MODEL_TEENSY_40;
            if (max_models >= 2)
                rmodels[models_count++] = TY_MODEL_TEENSY_40_BETA1;

            return models_count;
        }
    }

    /* Now try the Teensy 3.0 stack size method. We use a few facts to recognize these models:
       - The interrupt vector table (_VectorsFlash[]) is located at 0x0 (at least initially)
       - _VectorsFlash[0] is the initial stack pointer, which is the end of the RAM address space
       - _VectorsFlash[1] is the address of ResetHandler(), which follows _VectorsFlash[]
       - _VectorsFlash[] has a different length for each model

       Since Teensyduino 1.38, ResetHandler() can move out of the .startup section (when
       using -mpure-code, LTO), which breaks fact #3. But in that case there will be a lot of
       0xFF bytes after _VectorsFlash[], which we can use to detect the size of _VectorsFlash[].

       We combine the size of _VectorsFlash[] and the initial stack pointer value to
       differenciate models. */
    if (segment0) {
        const uint32_t teensy3_startup_size = 0x400;
        if (segment0->size >= teensy3_startup_size) {
            uint32_t stack_addr;
            uint32_t end_vector_addr;
            unsigned int arm_models_count = 0;

            stack_addr = read_uint32_le(segment0->data);
            end_vector_addr = read_uint32_le(segment0->data + 4) & ~1u;
            if (end_vector_addr >= teensy3_startup_size) {
                for (uint32_t i = 0; i < teensy3_startup_size - sizeof(uint64_t); i += 4) {
                    if (read_uint64_le(segment0->data + i) == 0xFFFFFFFFFFFFFFFF) {
                        end_vector_addr = i;
                        break;
                    }
                }
            }

            switch (((uint64_t)stack_addr << 32) | end_vector_addr) {
                case 0x20002000000000F8: {
                    rmodels[arm_models_count++] = TY_MODEL_TEENSY_30;
                } break;
                case 0x20008000000001BC: {
                    rmodels[arm_models_count++] = TY_MODEL_TEENSY_31;
                    if (max_models >= 2)
                        rmodels[arm_models_count++] = TY_MODEL_TEENSY_32;
                } break;
                case 0x20001800000000C0: {
                    rmodels[arm_models_count++] = TY_MODEL_TEENSY_LC;
                } break;
                case 0x2002000000000198:
                case 0x2002FFFC00000198:
                case 0x2002FFF800000198: {
                    rmodels[arm_models_count++] = TY_MODEL_TEENSY_35;
                } break;
                case 0x20030000000001D0: {
                    rmodels[arm_models_count++] = TY_MODEL_TEENSY_36;
                } break;
            }
            if (arm_models_count)
                return arm_models_count;
        }
    }

    /* Now try AVR Teensies. We search for machine code that matches model-specific code in
       _reboot_Teensyduino_(). Not elegant, but it does the work. */
    if (fw->max_address <= 130048) {
        for (unsigned int i = 0; i < fw->segments_count; i++) {
            const ty_firmware_segment *segment = &fw->segments[i];
            if (segment->size < sizeof(uint64_t))
                continue;

            for (size_t j = 0; j < segment->size - sizeof(uint64_t); j++) {
                uint64_t magic_value = read_uint64_le(segment->data + j);
                switch (magic_value) {
                    case 0x94F8CFFF7E00940C: {
                        rmodels[0] = TY_MODEL_TEENSY_PP_10;
                        return 1;
                    } break;
                    case 0x94F8CFFF3F00940C: {
                        rmodels[0] = TY_MODEL_TEENSY_20;
                        return 1;
                    } break;
                    case 0x94F8CFFFFE00940C: {
                        rmodels[0] = TY_MODEL_TEENSY_PP_20;
                        return 1;
                    } break;
                }
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
        case HS_DEVICE_TYPE_SERIAL: {
            r = hs_serial_read(iface->port, (uint8_t *)buf, size, timeout);
            if (r < 0)
                return ty_libhs_translate_error((int)r);
            return r;
        } break;

        case HS_DEVICE_TYPE_HID: {
            r = hs_hid_read(iface->port, hid_buf, sizeof(hid_buf), timeout);
            if (r < 0)
                return ty_libhs_translate_error((int)r);
            if (r < 2)
                return 0;

            r = (ssize_t)strnlen((char *)hid_buf + 1, (size_t)(r - 1));
            memcpy(buf, hid_buf + 1, (size_t)r);
            return r;
        } break;
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
        case HS_DEVICE_TYPE_SERIAL: {
            r = hs_serial_write(iface->port, (uint8_t *)buf, size, 5000);
            if (r < 0)
                return ty_libhs_translate_error((int)r);
            if (!r)
                return ty_error(TY_ERROR_IO, "Timed out while writing to '%s'", iface->dev->path);
            return r;
        } break;

        case HS_DEVICE_TYPE_HID: {
            /* SEREMU expects packets of 32 bytes. The terminating NUL marks the end, so
               no binary transfers. */
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
        } break;
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
        case 1: {
            buf[1] = addr & 255;
            buf[2] = (addr >> 8) & 255;

            if (size)
                memcpy(buf + 3, data, size);
            size = block_size + 3;
        } break;

        case 2: {
            buf[1] = (addr >> 8) & 255;
            buf[2] = (addr >> 16) & 255;

            if (size)
                memcpy(buf + 3, data, size);
            size = block_size + 3;
        } break;

        case 3: {
            buf[1] = addr & 255;
            buf[2] = (addr >> 8) & 255;
            buf[3] = (addr >> 16) & 255;

            if (size)
                memcpy(buf + 65, data, size);
            size = block_size + 65;
        } break;

        default: {
            assert(false);
        } break;
    }

    /* We may get errors along the way (while the bootloader works) so try again
       until timeout expires. */
    start = ty_millis();
    hs_error_mask(HS_ERROR_IO);
restart:
    r = hs_hid_write(port, buf, size);
    if (r == HS_ERROR_IO && ty_millis() - start < timeout) {
        ty_delay(20);
        goto restart;
    }
    hs_error_unmask();
    if (r < 0) {
        if (r == HS_ERROR_IO)
            return ty_error(TY_ERROR_IO, "%s", hs_error_last_message());
        return ty_libhs_translate_error((int)r);
    }

    /* HalfKay generates STALL if you go too fast (translates to EPIPE on Linux), and the
       first write takes longer because it triggers a complete erase of all blocks. */
    if (!addr)
        ty_delay(200);

    return 0;
}

static int get_halfkay_settings(ty_model model, unsigned int *rhalfkay_version,
                                size_t *rmin_address, size_t *rmax_address, size_t *rblock_size)
{
    if ((model == TY_MODEL_TEENSY_PP_10 || model == TY_MODEL_TEENSY_20) &&
            !getenv("TYTOOLS_EXPERIMENTAL_BOARDS")) {
        return ty_error(TY_ERROR_UNSUPPORTED,
                        "Support for %s boards is experimental, set environment variable"
                        "TYTOOLS_EXPERIMENTAL_BOARDS to any value to enable upload",
                        ty_models[model].name);
    }

    *rhalfkay_version = 0;
    switch ((ty_model_teensy)model) {
        case TY_MODEL_TEENSY_PP_10: {
            *rhalfkay_version = 1;
            *rmin_address = 0;
            *rmax_address = 0xFC00;
            *rblock_size = 256;
        } break;
        case TY_MODEL_TEENSY_20: {
            *rhalfkay_version = 1;
            *rmin_address = 0;
            *rmax_address = 0x7E00;
            *rblock_size = 128;
        } break;
        case TY_MODEL_TEENSY_PP_20: {
            *rhalfkay_version = 2;
            *rmin_address = 0;
            *rmax_address = 0x1FC00;
            *rblock_size = 256;
        } break;
        case TY_MODEL_TEENSY_30: {
            *rhalfkay_version = 3;
            *rmin_address = 0;
            *rmax_address = 0x20000;
            *rblock_size = 1024;
        } break;
        case TY_MODEL_TEENSY_31:
        case TY_MODEL_TEENSY_32: {
            *rhalfkay_version = 3;
            *rmin_address = 0;
            *rmax_address = 0x40000;
            *rblock_size = 1024;
        } break;
        case TY_MODEL_TEENSY_35: {
            *rhalfkay_version = 3;
            *rmin_address = 0;
            *rmax_address = 0x80000;
            *rblock_size = 1024;
        } break;
        case TY_MODEL_TEENSY_36: {
            *rhalfkay_version = 3;
            *rmin_address = 0;
            *rmax_address = 0x100000;
            *rblock_size = 1024;
        } break;
        case TY_MODEL_TEENSY_LC: {
            *rhalfkay_version = 3;
            *rmin_address = 0;
            *rmax_address = 0xF800;
            *rblock_size = 512;
        } break;
        case TY_MODEL_TEENSY_40_BETA1:
        case TY_MODEL_TEENSY_40: {
            *rhalfkay_version = 3;
            *rmin_address = 0x60000000;
            *rmax_address = 0x60180000;
            *rblock_size = 1024;
        } break;

        case TY_MODEL_TEENSY: {
            assert(false);
        } break;
    }
    assert(*rhalfkay_version);

    return 0;
}

static int teensy_upload(ty_board_interface *iface, ty_firmware *fw,
                         ty_board_upload_progress_func *pf, void *udata)
{
    unsigned int halfkay_version;
    size_t min_address, max_address, block_size;
    int r;

    r = get_halfkay_settings(iface->model, &halfkay_version, &min_address, &max_address, &block_size);
    if (r < 0)
        return r;

    if (fw->max_address > max_address)
        return ty_error(TY_ERROR_RANGE, "Firmware is too big for %s",
                        ty_models[iface->model].name);

    if (pf) {
        r = (*pf)(iface->board, fw, 0, max_address - min_address, udata);
        if (r)
            return r;
    }

    size_t uploaded_len = 0;
    for (size_t address = min_address; address < fw->max_address; address += block_size) {
        char buf[8192];
        size_t buf_len;

        memset(buf, 0, sizeof(buf));
        buf_len = ty_firmware_extract(fw, (uint32_t)address, buf, block_size);

        if (buf_len) {
            r = halfkay_send(iface->port, halfkay_version, block_size, address, buf, buf_len, 3000);
            if (r < 0)
                return r;
            uploaded_len += buf_len;

            if (pf) {
                r = (*pf)(iface->board, fw, uploaded_len, max_address - min_address, udata);
                if (r)
                    return r;
            }
        }
    }

    return 0;
}

static int teensy_reset(ty_board_interface *iface)
{
    unsigned int halfkay_version;
    size_t min_address, max_address, block_size;

    int r = get_halfkay_settings(iface->model, &halfkay_version, &min_address, &max_address, &block_size);
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
        case HS_DEVICE_TYPE_SERIAL: {
            r = change_baudrate(iface->port, serial_magic);
            if (!r) {
                /* Don't keep these settings, some systems (such as Linux) may reuse them and
                   the device will keep rebooting when opened. */
                hs_error_mask(HS_ERROR_SYSTEM);
                change_baudrate(iface->port, 115200);
                hs_error_unmask();
            }
        } break;

        case HS_DEVICE_TYPE_HID: {
            r = (int)hs_hid_send_feature_report(iface->port, seremu_magic, sizeof(seremu_magic));
            if (r < 0) {
                r = ty_libhs_translate_error(r);
            } else {
                assert(r == sizeof(seremu_magic));
                r = 0;
            }
        } break;
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
