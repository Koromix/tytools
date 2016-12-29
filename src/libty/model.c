/*
 * ty, a collection of GUI and command-line tools to manage Teensy devices
 *
 * Distributed under the MIT license (see LICENSE.txt or http://opensource.org/licenses/MIT)
 * Copyright (c) 2015 Niels Martignène <niels.martignene@gmail.com>
 */

#include "util.h"
#include "ty/firmware.h"
#include "model_priv.h"

struct ty_model {
    TY_MODEL
};

extern const struct _ty_model_vtable _ty_teensy_model_vtable;

const struct _ty_model_vtable *_ty_model_vtables[] = {
    &_ty_teensy_model_vtable
};
const unsigned int _ty_model_vtables_count = TY_COUNTOF(_ty_model_vtables);

extern const ty_model _ty_teensy_pp10_model;
extern const ty_model _ty_teensy_20_model;
extern const ty_model _ty_teensy_pp20_model;
extern const ty_model _ty_teensy_30_model;
extern const ty_model _ty_teensy_31_model;
extern const ty_model _ty_teensy_lc_model;
extern const ty_model _ty_teensy_32_model;
extern const ty_model _ty_teensy_k64_model;
extern const ty_model _ty_teensy_k66_model;

const ty_model *ty_models[] = {
    &_ty_teensy_pp10_model,
    &_ty_teensy_20_model,
    &_ty_teensy_pp20_model,
    &_ty_teensy_30_model,
    &_ty_teensy_31_model,
    &_ty_teensy_lc_model,
    &_ty_teensy_32_model,
    &_ty_teensy_k64_model,
    &_ty_teensy_k66_model
};
const unsigned int ty_models_count = TY_COUNTOF(ty_models);

const ty_model *ty_model_find(const char *name)
{
    assert(name);

    for (unsigned int i = 0; i < ty_models_count; i++) {
        if (strcmp(ty_models[i]->name, name) == 0)
            return ty_models[i];
    }

    return NULL;
}

bool ty_model_is_real(const ty_model *model)
{
    return model && model->code_size;
}

const char *ty_model_get_name(const ty_model *model)
{
    assert(model);
    return model->name;
}

const char *ty_model_get_mcu(const ty_model *model)
{
    assert(model);
    return model->mcu;
}

size_t ty_model_get_code_size(const ty_model *model)
{
    assert(model);
    return model->code_size;
}
