/* TyTools - public domain
   Niels Martignène <niels.martignene@protonmail.com>
   https://koromix.dev/tytools

   This software is in the public domain. Where that dedication is not
   recognized, you are granted a perpetual, irrevocable license to copy,
   distribute, and modify this file as you see fit.

   See the LICENSE file for more details. */

#include "common.h"
#include "class_priv.h"
#include "../libhs/array.h"
#include "../libhs/device.h"
#include "ini.h"
#include "system.h"

static const ty_model_info default_models[] = {
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
    {1, "Teensy 3.6", "mk66fx1m0"},
    {1, "Teensy 4.0 (beta 1)", "imxrt_b1"},
    {1, "Teensy 4.0", "imxrt"},
    {1, "Teensy 4.1", "imxrt_t41"},
};
const ty_model_info *ty_models = default_models;
const unsigned int ty_models_count = _HS_COUNTOF(default_models);

extern const struct _ty_class_vtable _ty_teensy_class_vtable;
extern const struct _ty_class_vtable _ty_generic_class_vtable;
const struct _ty_class _ty_classes[] = {
    {"Generic", &_ty_generic_class_vtable},
    {"Teensy", &_ty_teensy_class_vtable}
};
const unsigned int _ty_classes_count = _HS_COUNTOF(_ty_classes);

static bool data_is_patched = false;

static void free_models(ty_model_info *models)
{
    if (!models)
        return;

    for (unsigned int i = 0; i < ty_models_count; i++) {
        if (models[i].name != default_models[i].name)
            free((void *)models[i].name);
    }
    free(models);
}

static void free_patch_data(void)
{
    free_models((ty_model_info *)ty_models);
    free((hs_match_spec *)_ty_class_match_specs);
}

struct patch_ini_context {
    ty_model_info *new_models;
    _HS_ARRAY(hs_match_spec) new_matches;
};

static int patch_ini_callback(const char *section, char *key, char *value, void *udata)
{
    struct patch_ini_context *ctx = udata;

    if (!strcmp(section, "Models")) {
        for (unsigned int i = 0; i < ty_models_count; i++) {
            if (!strcmp(ty_models[i].name, key)) {
                char *new_name;

                new_name = strdup(value);
                if (!new_name)
                    return ty_error(TY_ERROR_MEMORY, NULL);
                if (ctx->new_models[i].name != default_models[i].name)
                    free((void *)ctx->new_models[i].name);
                ctx->new_models[i].name = new_name;

                return 0;
            }
        }
    } else if (!strcmp(section, "Devices")) {
        hs_match_spec new_spec;

        if (hs_match_parse(key, &new_spec) < 0)
            return 0;
        if (value[0]) {
            for (unsigned int i = 0; i < _ty_classes_count; i++) {
                if (!strcmp(_ty_classes[i].name, value)) {
                    new_spec.udata = (void *)_ty_classes[i].vtable;
                    break;
                }
            }
            if (!new_spec.udata) {
                ty_log(TY_LOG_WARNING, "Cannot find device class '%s' for match '%s'",
                       value, key);
                return 0;
            }
        }

        return ty_libhs_translate_error(_hs_array_push(&ctx->new_matches, new_spec));
    }

    if (section) {
        ty_log(TY_LOG_WARNING, "Unknown TyTools setting '%s.%s'", section, key);
    } else {
        ty_log(TY_LOG_WARNING, "Unknown TyTools setting '%s'", key);
    }
    return 0;
}

int ty_models_load_patch(const char *filename)
{
    static const char *default_names[] = {
        "tytools.ini",
#ifndef _WIN32
        "TyTools.ini"
#endif
    };
    struct patch_ini_context ctx = {0};
    int r;

    ctx.new_models = malloc(sizeof(default_models));
    if (!ctx.new_models) {
        r = ty_error(TY_ERROR_MEMORY, NULL);
        goto error;
    }
    memcpy(ctx.new_models, default_models, sizeof(default_models));

    if (filename) {
        r = ty_ini_walk(filename, patch_ini_callback, &ctx);
        if (r < 0)
            goto error;
    } else {
        char dirs[16][TY_PATH_MAX_SIZE];
        unsigned int dirs_count = 0;
        unsigned int patches_count = 0;

        dirs_count += ty_standard_get_paths(TY_PATH_CONFIG_DIRECTORY, "TyTools", dirs + dirs_count,
                                            (unsigned int)(_HS_COUNTOF(dirs) - dirs_count));
        dirs_count += ty_standard_get_paths(TY_PATH_EXECUTABLE_DIRECTORY, NULL, dirs + dirs_count,
                                            (unsigned int)(_HS_COUNTOF(dirs) - dirs_count));

        for (unsigned int i = dirs_count; i-- > 0;) {
            size_t directory_len = strlen(dirs[i]);

            r = TY_ERROR_NOT_FOUND;
            ty_error_mask(TY_ERROR_NOT_FOUND);
            for (unsigned int j = 0; r == TY_ERROR_NOT_FOUND && j < _HS_COUNTOF(default_names); j++) {
                snprintf(dirs[i] + directory_len, sizeof(dirs[i]) - directory_len,
                         "/%s", default_names[j]);
                r = ty_ini_walk(dirs[i], patch_ini_callback, &ctx);
            }
            dirs[i][directory_len] = 0;
            ty_error_unmask();
            if (r < 0) {
                if (r != TY_ERROR_NOT_FOUND)
                    goto error;
            } else {
                patches_count++;
            }
        }

        if (!patches_count) {
            r = 0;
            goto error;
        }
    }

    r = _hs_array_grow(&ctx.new_matches, _ty_class_match_specs_count);
    if (r < 0) {
        r = ty_libhs_translate_error(r);
        goto error;
    }
    memcpy(ctx.new_matches.values + ctx.new_matches.count, _ty_class_match_specs,
           sizeof(*_ty_class_match_specs) * _ty_class_match_specs_count);

    if (data_is_patched) {
        free_patch_data();
    } else {
        data_is_patched = true;
        atexit(free_patch_data);
    }
    ty_models = ctx.new_models;
    _ty_class_match_specs = ctx.new_matches.values;
    _ty_class_match_specs_count = (unsigned int)ctx.new_matches.count + _ty_class_match_specs_count;

    return 0;

error:
    _hs_array_release(&ctx.new_matches);
    free_models(ctx.new_models);
    return (int)r;
}

ty_model ty_models_find(const char *name)
{
    assert(name);

    for (unsigned int i = 0; i < ty_models_count; i++) {
        if (strcmp(ty_models[i].name, name) == 0)
            return (ty_model)i;
    }

    return 0;
}
