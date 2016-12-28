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
    &_ty_teensy_model_vtable,
    NULL
};

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
    &_ty_teensy_k66_model,
    NULL
};

const ty_model *ty_model_find(const char *name)
{
    assert(name);

    for (const ty_model **cur = ty_models; *cur; cur++) {
        const ty_model *model = *cur;

        if (strcmp(model->name, name) == 0)
            return model;
    }

    return NULL;
}

bool ty_model_is_real(const ty_model *model)
{
    return model && model->code_size;
}

bool ty_model_test_firmware(const ty_model *model, const ty_firmware *fw,
                            const ty_model **rguesses, unsigned int *rcount)
{
    assert(fw);
    assert(!!rguesses == !!rcount);
    if (rguesses)
        assert(*rcount);

    bool compatible = false;
    unsigned int count = 0;

    for (const struct _ty_model_vtable **cur = _ty_model_vtables; *cur; cur++) {
        const struct _ty_model_vtable *model_vtable = *cur;

        const ty_model *partial_guesses[8];
        unsigned int partial_count;

        partial_count = (*model_vtable->guess_models)(fw, partial_guesses, TY_COUNTOF(partial_guesses));

        for (unsigned int i = 0; i < partial_count; i++) {
            if (partial_guesses[i] == model)
                compatible = true;
            if (rguesses && count < *rcount)
                rguesses[count++] = partial_guesses[i];
        }
    }

    if (rcount)
        *rcount = count;
    return compatible;
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
