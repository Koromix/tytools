/* TyTools - public domain
   Niels Martignène <niels.martignene@gmail.com>
   https://neodd.com/tytools

   This software is in the public domain. Where that dedication is not
   recognized, you are granted a perpetual, irrevocable license to copy,
   distribute, and modify this file as you see fit.

   See the LICENSE file for more details. */

#ifndef TY_MODEL_H
#define TY_MODEL_H

#include "common.h"

TY_C_BEGIN

typedef unsigned int ty_model;
typedef struct ty_model_info {
    const char *name;
    const char *mcu;
    size_t code_size;
} ty_model_info;

// Keep these enums and ty_models (below) in sync
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
    TY_MODEL_TEENSY_36
} ty_model_teensy;

static const ty_model_info ty_models[] = {
    {"(unknown)"},

    {"Teensy"},
    {"Teensy++ 1.0", "at90usb646", 64512},
    {"Teensy 2.0", "atmega32u4", 32256},
    {"Teensy++ 2.0", "at90usb1286", 130048},
    {"Teensy 3.0", "mk20dx128", 131072},
    {"Teensy 3.1", "mk20dx256", 262144},
    {"Teensy LC", "mkl26z64", 63488},
    {"Teensy 3.2", "mk20dx256", 262144},
    {"Teensy 3.5", "mk64fx512", 524288},
    {"Teensy 3.6", "mk66fx1m0", 1048576}
};
static const unsigned int ty_models_count = TY_COUNTOF(ty_models);

TY_PUBLIC ty_model ty_model_find(const char *name);

static inline bool ty_model_is_real(ty_model model)
{
    return ty_models[model].code_size;
}

TY_C_END

#endif
