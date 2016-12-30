/*
 * ty, a collection of GUI and command-line tools to manage Teensy devices
 *
 * Distributed under the MIT license (see LICENSE.txt or http://opensource.org/licenses/MIT)
 * Copyright (c) 2015 Niels Martignène <niels.martignene@gmail.com>
 */

#include "util.h"
#include "ty/firmware.h"
#include "model_priv.h"

extern const struct _ty_model_vtable _ty_teensy_model_vtable;

const struct _ty_model_vtable *_ty_model_vtables[] = {
    &_ty_teensy_model_vtable
};
const unsigned int _ty_model_vtables_count = TY_COUNTOF(_ty_model_vtables);

ty_model ty_model_find(const char *name)
{
    assert(name);

    for (unsigned int i = 0; i < ty_models_count; i++) {
        if (strcmp(ty_models[i].name, name) == 0)
            return (ty_model)i;
    }

    return 0;
}
