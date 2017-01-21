/* Teensy Tools - public domain
   Niels Martignène <niels.martignene@gmail.com>
   https://neodd.com/teensytools

   This software is in the public domain. Where that dedication is not
   recognized, you are granted a perpetual, irrevocable license to copy,
   distribute, and modify this file as you see fit.

   See the LICENSE file for more details. */

#ifndef TY_MODEL_PRIV_H
#define TY_MODEL_PRIV_H

#include "common_priv.h"
#include "board.h"
#include "model.h"

TY_C_BEGIN

struct _ty_model_vtable {
    const ty_model **models;

    int (*load_interface)(ty_board_interface *iface);
    int (*update_board)(ty_board_interface *iface, ty_board *board);

    unsigned int (*identify_models)(const struct ty_firmware *fw,
                                    ty_model *rmodels, unsigned int max_models);
};

extern const struct _ty_model_vtable *_ty_model_vtables[];
extern const unsigned int _ty_model_vtables_count;

TY_C_END

#endif
