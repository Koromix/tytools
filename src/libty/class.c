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

extern const struct _ty_class_vtable _ty_teensy_class_vtable;
extern const struct _ty_class_vtable _ty_generic_class_vtable;

const struct _ty_class_vtable *_ty_class_vtables[] = {
    &_ty_teensy_class_vtable,
    &_ty_generic_class_vtable
};
const unsigned int _ty_class_vtables_count = TY_COUNTOF(_ty_class_vtables);

ty_model ty_model_find(const char *name)
{
    assert(name);

    for (unsigned int i = 0; i < ty_models_count; i++) {
        if (strcmp(ty_models[i].name, name) == 0)
            return (ty_model)i;
    }

    return 0;
}
