/* TyTools - public domain
   Niels Martignène <niels.martignene@protonmail.com>
   https://koromix.dev/tytools

   This software is in the public domain. Where that dedication is not
   recognized, you are granted a perpetual, irrevocable license to copy,
   distribute, and modify this file as you see fit.

   See the LICENSE file for more details. */

#include "common_priv.h"
#include "../libhs/device.h"
#include "../libhs/serial.h"
#include "board.h"
#include "board_priv.h"
#include "class_priv.h"
#include "system.h"

extern const struct _ty_class_vtable _ty_generic_class_vtable;

static int generic_load_interface(ty_board_interface *iface)
{
    // TODO: Detect devices in DFU mode to show and keep alive during programming
    if (iface->dev->type != HS_DEVICE_TYPE_SERIAL)
        return 0;

    iface->name = "Serial";
    iface->capabilities |= 1 << TY_BOARD_CAPABILITY_SERIAL;
    iface->class_vtable = &_ty_generic_class_vtable;
    iface->model = TY_MODEL_GENERIC;

    return 1;
}

static int generic_update_board(ty_board_interface *iface, ty_board *board, bool new_board)
{
    TY_UNUSED(new_board);

    const char *manufacturer_string;
    const char *product_string;
    const char *serial_number_string;
    char *serial_number = NULL;
    bool unique = false;
    char *description = NULL;
    char *id = NULL;
    int r;

    manufacturer_string = iface->dev->manufacturer_string;
    if (!manufacturer_string)
        manufacturer_string = "Unknown";
    product_string = iface->dev->product_string;
    if (!product_string)
        product_string = "Unknown";
    serial_number_string = iface->dev->serial_number_string;
    if (!serial_number_string)
        serial_number_string = "?";

    // Check and update board model
    if (board->model != TY_MODEL_GENERIC) {
        r = 0;
        goto error;
    }

    // Check and update board serial number
    if (board->serial_number && strcmp(board->serial_number, serial_number_string) != 0) {
        r = 0;
        goto error;
    }
    serial_number = strdup(serial_number_string);
    if (!serial_number) {
        r = ty_error(TY_ERROR_MEMORY, NULL);
        goto error;
    }

    // Does the unique serial number look unique?
    if (iface->dev->serial_number_string)
        unique = iface->dev->serial_number_string[strspn(iface->dev->serial_number_string, "0_ ")];

    // Check and update board description
    if (board->description && strcmp(board->description, product_string) != 0) {
        r = 0;
        goto error;
    }
    description = strdup(product_string);
    if (!description) {
        r = ty_error(TY_ERROR_MEMORY, NULL);
        goto error;
    }

    // Create new board unique identifier
    r = asprintf(&id, "%s-%s", serial_number, manufacturer_string);
    if (r < 0) {
        r = ty_error(TY_ERROR_MEMORY, NULL);
        goto error;
    }
    for (size_t i = 0; id[i]; i++) {
        if (!(id[i] == '-' || id[i] == '_' ||
              id[i] == ':' || id[i] == '.' ||
              (id[i] >= 'a' && id[i] <= 'z') ||
              (id[i] >= 'A' && id[i] <= 'Z') ||
              (id[i] >= '0' && id[i] <= '9')))
            id[i] = '_';
    }

    // Check board unique identifier
    if (board->id && strcmp(id, board->id) != 0) {
        r = 0;
        goto error;
    }

    // Everything is alright, we can commit changes
    if (serial_number) {
        free(board->serial_number);
        board->serial_number = serial_number;
    }
    if (unique)
        iface->capabilities |= 1 << TY_BOARD_CAPABILITY_UNIQUE;
    if (description) {
        free(board->description);
        board->description = description;
    }
    if (!board->id) {
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

static int generic_open_interface(ty_board_interface *iface)
{
    int r = hs_port_open(iface->dev, HS_PORT_MODE_RW, &iface->port);
    return ty_libhs_translate_error(r);
}

static void generic_close_interface(ty_board_interface *iface)
{
    hs_port_close(iface->port);
    iface->port = NULL;
}

static ssize_t generic_serial_read(ty_board_interface *iface, char *buf, size_t size, int timeout)
{
    ssize_t r = hs_serial_read(iface->port, (uint8_t *)buf, size, timeout);
    if (r < 0)
        return ty_libhs_translate_error((int)r);
    return r;
}

static ssize_t generic_serial_write(ty_board_interface *iface, const char *buf, size_t size)
{
    ssize_t r = hs_serial_write(iface->port, (uint8_t *)buf, size, 5000);
    if (r < 0)
        return ty_libhs_translate_error((int)r);
    if (!r)
        return ty_error(TY_ERROR_IO, "Timed out while writing to '%s'", iface->dev->path);
    return r;
}

const struct _ty_class_vtable _ty_generic_class_vtable = {
    .load_interface = generic_load_interface,
    .update_board = generic_update_board,

    .open_interface = generic_open_interface,
    .close_interface = generic_close_interface,
    .serial_read = generic_serial_read,
    .serial_write = generic_serial_write
};
