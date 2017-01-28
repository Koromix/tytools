/* TyTools - public domain
   Niels Martignène <niels.martignene@gmail.com>
   https://neodd.com/tytools

   This software is in the public domain. Where that dedication is not
   recognized, you are granted a perpetual, irrevocable license to copy,
   distribute, and modify this file as you see fit.

   See the LICENSE file for more details. */

#ifndef TY_CLASS_PRIV_H
#define TY_CLASS_PRIV_H

#include "common_priv.h"
#include "board.h"
#include "class.h"

TY_C_BEGIN

struct _ty_class_vtable {
    int (*load_interface)(ty_board_interface *iface);
    int (*update_board)(ty_board_interface *iface, ty_board *board);
    unsigned int (*identify_models)(const struct ty_firmware *fw,
                                    ty_model *rmodels, unsigned int max_models);

    int (*open_interface)(ty_board_interface *iface);
    void (*close_interface)(ty_board_interface *iface);
    ssize_t (*serial_read)(ty_board_interface *iface, char *buf, size_t size, int timeout);
    ssize_t (*serial_write)(ty_board_interface *iface, const char *buf, size_t size);
    int (*upload)(ty_board_interface *iface, struct ty_firmware *fw,
                  ty_board_upload_progress_func *pf, void *udata);
    int (*reset)(ty_board_interface *iface);
    int (*reboot)(ty_board_interface *iface);
};

extern const struct _ty_class_vtable *_ty_class_vtables[];
extern const unsigned int _ty_class_vtables_count;

TY_C_END

#endif
