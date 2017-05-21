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
#include "ini.h"
#include "system.h"

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

static void free_models(ty_model_info *models)
{
    for (unsigned int i = 0; i < ty_models_count; i++) {
        if (models[i].name != default_models[i].name)
            free((void *)models[i].name);
    }
    free(models);
}
static void free_current_models(void)
{
    if (ty_models != default_models)
        free_models((ty_model_info *)ty_models);
}

static int models_ini_callback(const char *section, char *key, char *value, void *udata)
{
    ty_model_info *new_models = udata;

    if (!strcmp(section, "Models")) {
        for (unsigned int i = 0; i < ty_models_count; i++) {
            if (!strcmp(ty_models[i].name, key)) {
                char *new_name;

                new_name = strdup(value);
                if (!new_name)
                    return ty_error(TY_ERROR_MEMORY, NULL);
                if (new_models[i].name != default_models[i].name)
                    free((void *)new_models[i].name);
                new_models[i].name = new_name;

                return 0;
            }
        }
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
    ty_model_info *new_models = NULL;
    int r;

    if (ty_models == default_models) {
        new_models = malloc(sizeof(default_models));
        if (!new_models) {
            r = ty_error(TY_ERROR_MEMORY, NULL);
            goto error;
        }
        memcpy(new_models, default_models, sizeof(default_models));
    } else {
        new_models = (ty_model_info *)ty_models;
    }

    if (filename) {
        r = ty_ini_walk(filename, models_ini_callback, new_models);
        if (r < 0)
            goto error;
    } else {
        char dirs[16][TY_PATH_MAX_SIZE];
        unsigned int dirs_count = 0;
        unsigned int patches_count = 0;

        dirs_count += ty_standard_get_paths(TY_PATH_CONFIG_DIRECTORY, "TyTools", dirs + dirs_count,
                                            (unsigned int)(TY_COUNTOF(dirs) - dirs_count));
        dirs_count += ty_standard_get_paths(TY_PATH_EXECUTABLE_DIRECTORY, NULL, dirs + dirs_count,
                                            (unsigned int)(TY_COUNTOF(dirs) - dirs_count));

        for (unsigned int i = dirs_count; i-- > 0;) {
            size_t directory_len = strlen(dirs[i]);

            r = TY_ERROR_NOT_FOUND;
            ty_error_mask(TY_ERROR_NOT_FOUND);
            for (unsigned int j = 0; r == TY_ERROR_NOT_FOUND && j < TY_COUNTOF(default_names); j++) {
                snprintf(dirs[i] + directory_len, sizeof(dirs[i]) - directory_len,
                         "/%s", default_names[j]);
                r = ty_ini_walk(dirs[i], models_ini_callback, new_models);
            }
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

    if (ty_models == default_models) {
        ty_models = new_models;
        atexit(free_current_models);
    }
    return 0;

error:
    if (new_models != ty_models)
        free_models(new_models);
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
