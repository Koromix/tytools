/* TyTools - public domain
   Niels Martignène <niels.martignene@gmail.com>
   https://neodd.com/tytools

   This software is in the public domain. Where that dedication is not
   recognized, you are granted a perpetual, irrevocable license to copy,
   distribute, and modify this file as you see fit.

   See the LICENSE file for more details. */

#include "common_priv.h"
#include "class_priv.h"
#include "firmware.h"

static ty_model_info default_models[] = {
    {0, "Generic"},

    {1, "Teensy"},
    {1, "Teensy++ 1.0", "at90usb646"},
    {1, "Teensy 2.0", "atmega32u4"},
    {1, "Teensy++ 2.0", "at90usb1286"},
    {1, "Teensy 3.0", "mk20dx128"},
    {1, "Teensy 3.1", "mk20dx256"},
    {1, "Teensy LC", "mkl26z64"},
    {1, "Teensy 3.2", "mk20dx256"},
    {1, "Teensy 3.5", "mk64fx512"},
    {1, "Teensy 3.6", "mk66fx1m0"}
};
const ty_model_info *ty_models = default_models;
const unsigned int ty_models_count = TY_COUNTOF(default_models);

extern const struct _ty_class_vtable _ty_teensy_class_vtable;
extern const struct _ty_class_vtable _ty_generic_class_vtable;

const struct _ty_class_vtable *_ty_class_vtables[] = {
    &_ty_teensy_class_vtable,
    &_ty_generic_class_vtable
};
const unsigned int _ty_class_vtables_count = TY_COUNTOF(_ty_class_vtables);

ty_model ty_models_find(const char *name)
{
    assert(name);

    for (unsigned int i = 0; i < ty_models_count; i++) {
        if (strcmp(ty_models[i].name, name) == 0)
            return (ty_model)i;
    }

    return 0;
}
