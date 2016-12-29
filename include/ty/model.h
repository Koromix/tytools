/*
 * ty, a collection of GUI and command-line tools to manage Teensy devices
 *
 * Distributed under the MIT license (see LICENSE.txt or http://opensource.org/licenses/MIT)
 * Copyright (c) 2015 Niels Martignène <niels.martignene@gmail.com>
 */

#ifndef TY_MODEL_H
#define TY_MODEL_H

#include "common.h"

TY_C_BEGIN

struct ty_firmware;

typedef struct ty_model ty_model;

TY_PUBLIC extern const ty_model *ty_models[];
TY_PUBLIC extern const unsigned int ty_models_count;

TY_PUBLIC const ty_model *ty_model_find(const char *name);

TY_PUBLIC bool ty_model_is_real(const ty_model *model);
TY_PUBLIC const char *ty_model_get_name(const ty_model *model);
TY_PUBLIC const char *ty_model_get_mcu(const ty_model *model);
TY_PUBLIC size_t ty_model_get_code_size(const ty_model *model);

TY_C_END

#endif
