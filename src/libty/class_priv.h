/* TyTools - public domain
   Niels Martignène <niels.martignene@protonmail.com>
   https://koromix.dev/tytools

   This software is in the public domain. Where that dedication is not
   recognized, you are granted a perpetual, irrevocable license to copy,
   distribute, and modify this file as you see fit.

   See the LICENSE file for more details. */

#ifndef TY_CLASS_PRIV_H
#define TY_CLASS_PRIV_H

#include "common_priv.h"
#include "board.h"
#include "class.h"
#include "../libhs/match.h"

TY_C_BEGIN

struct _ty_class_vtable {
    int (*load_interface)(ty_board_interface *iface);
    int (*update_board)(ty_board_interface *iface, ty_board *board, bool new_board);
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

struct _ty_class {
    const char *name;
    const struct _ty_class_vtable *vtable;
};

extern const struct _ty_class _ty_classes[];
extern const unsigned int _ty_classes_count;

extern const hs_match_spec *_ty_class_match_specs;
extern unsigned int _ty_class_match_specs_count;

TY_C_END

#endif
