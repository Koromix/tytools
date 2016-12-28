/*
 * ty, a collection of GUI and command-line tools to manage Teensy devices
 *
 * Distributed under the MIT license (see LICENSE.txt or http://opensource.org/licenses/MIT)
 * Copyright (c) 2015 Niels Martignène <niels.martignene@gmail.com>
 */

#ifndef TY_MODEL_PRIV_H
#define TY_MODEL_PRIV_H

#include "util.h"
#include "ty/board.h"
#include "ty/model.h"

TY_C_BEGIN

struct _ty_model_vtable {
    const ty_model **models;

    int (*load_interface)(ty_board_interface *iface);
    int (*update_board)(ty_board_interface *iface, ty_board *board);

    unsigned int (*guess_models)(const struct ty_firmware *fw,
                                 const ty_model **rmodels, unsigned int max);
};

#define TY_MODEL \
    const struct _ty_model_vtable *vtable; \
    \
    const char *name; \
    const char *mcu; \
    \
    size_t code_size;

extern const struct _ty_model_vtable *_ty_model_vtables[];

TY_C_END

#endif
