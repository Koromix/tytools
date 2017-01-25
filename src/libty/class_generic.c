/* TyTools - public domain
   Niels Martignène <niels.martignene@gmail.com>
   https://neodd.com/tytools

   This software is in the public domain. Where that dedication is not
   recognized, you are granted a perpetual, irrevocable license to copy,
   distribute, and modify this file as you see fit.

   See the LICENSE file for more details. */

#include "common_priv.h"
#include "../libhs/device.h"
#include "../libhs/serial.h"
#include "board.h"
#include "board_priv.h"
#include "model_priv.h"
#include "system.h"

extern const struct _ty_model_vtable _ty_generic_model_vtable;
static const struct _ty_board_interface_vtable generic_iface_vtable;

static int generic_load_interface(ty_board_interface *iface)
{
    if (iface->dev->type != HS_DEVICE_TYPE_SERIAL)
        return 0;

    iface->name = "Serial";
    iface->capabilities |= 1 << TY_BOARD_CAPABILITY_SERIAL;
    iface->model = TY_MODEL_GENERIC;
    iface->model_vtable = &_ty_generic_model_vtable;
    iface->vtable = &generic_iface_vtable;

    return 1;
}

static int generic_update_board(ty_board_interface *iface, ty_board *board)
{
    const char *manufacturer_string;
    const char *product_string;
    uint64_t new_serial = 0;
    char new_id[256];

    manufacturer_string = iface->dev->manufacturer_string;
    if (!manufacturer_string)
        manufacturer_string = "Generic";
    product_string = iface->dev->product_string;
    if (!product_string)
        product_string = "Unknown";

    if (board->model && board->model != iface->model)
        return 0;
    if (board->description && strcmp(board->description, product_string) != 0)
        return 0;
    if (iface->dev->serial_number_string)
        new_serial = strtoull(iface->dev->serial_number_string, NULL, 10);
    if (board->serial && new_serial != board->serial)
        return 0;
    snprintf(new_id, sizeof(new_id), "%"PRIu64"-%s", new_serial, manufacturer_string);
    for (size_t i = 0; new_id[i]; i++) {
        if (!(new_id[i] == '-' || new_id[i] == '_' ||
              new_id[i] == ':' || new_id[i] == '.' ||
              (new_id[i] >= 'a' && new_id[i] <= 'z') ||
              (new_id[i] >= 'A' && new_id[i] <= 'Z') ||
              (new_id[i] >= '0' && new_id[i] <= '9')))
            new_id[i] = '_';
    }
    if (board->id && strcmp(new_id, board->id))
        return 0;

    board->model = iface->model;
    if (!board->description) {
        board->description = strdup(product_string);
        if (!board->description)
            return ty_error(TY_ERROR_MEMORY, NULL);
    }
    board->serial = new_serial;
    if (!board->id) {
        board->id = strdup(new_id);
        if (!board->id)
            return ty_error(TY_ERROR_MEMORY, NULL);
    }

    return 1;
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

const struct _ty_model_vtable _ty_generic_model_vtable = {
    .load_interface = generic_load_interface,
    .update_board = generic_update_board,
};

static const struct _ty_board_interface_vtable generic_iface_vtable = {
    .open_interface = generic_open_interface,
    .close_interface = generic_close_interface,
    .serial_read = generic_serial_read,
    .serial_write = generic_serial_write,
};
