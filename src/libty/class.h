/* TyTools - public domain
   Niels Martignène <niels.martignene@protonmail.com>
   https://koromix.dev/tytools

   This software is in the public domain. Where that dedication is not
   recognized, you are granted a perpetual, irrevocable license to copy,
   distribute, and modify this file as you see fit.

   See the LICENSE file for more details. */

#ifndef TY_CLASS_H
#define TY_CLASS_H

#include "common.h"

TY_C_BEGIN

typedef unsigned int ty_model;
typedef struct ty_model_info {
    unsigned int priority;
    const char *name;
    const char *mcu;
} ty_model_info;

// Keep these enums and ty_models in sync (in class.c)
typedef enum ty_model_generic {
    TY_MODEL_GENERIC = 0
} ty_model_generic;
typedef enum ty_model_teensy {
    TY_MODEL_TEENSY = 1,
    TY_MODEL_TEENSY_PP_10,
    TY_MODEL_TEENSY_20,
    TY_MODEL_TEENSY_PP_20,
    TY_MODEL_TEENSY_30,
    TY_MODEL_TEENSY_31,
    TY_MODEL_TEENSY_LC,
    TY_MODEL_TEENSY_32,
    TY_MODEL_TEENSY_35,
    TY_MODEL_TEENSY_36,
    TY_MODEL_TEENSY_40_BETA1,
    TY_MODEL_TEENSY_40
} ty_model_teensy;

extern const ty_model_info *ty_models;
extern const unsigned int ty_models_count;

int ty_models_load_patch(const char *filename);

ty_model ty_models_find(const char *name);

TY_C_END

#endif
