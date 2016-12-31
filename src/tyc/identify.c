/*
 * ty, a collection of GUI and command-line tools to manage Teensy devices
 *
 * Distributed under the MIT license (see LICENSE.txt or http://opensource.org/licenses/MIT)
 * Copyright (c) 2015 Niels Martignène <niels.martignene@gmail.com>
 */

#include <stdarg.h>
#include "main.h"
#include "../libty/firmware.h"

static const char *firmware_format = NULL;
static bool output_json = false;

static void print_identify_usage(FILE *f)
{
    fprintf(f, "usage: %s identify [options] <firmwares>\n\n", executable_name);

    print_common_options(f);
    fprintf(f, "\n");

    fprintf(f, "Identify options:\n"
               "   -f, --format <format>    Firmware file format (autodetected by default)\n"
               "   -j, --json               Output data in JSON format\n");
}

int identify(int argc, char *argv[])
{
    ty_optline_context optl;
    char *opt;

    ty_optline_init_argv(&optl, argc, argv);
    while ((opt = ty_optline_next_option(&optl))) {
        if (strcmp(opt, "--help") == 0) {
            print_identify_usage(stdout);
            return EXIT_SUCCESS;
        } else if (strcmp(opt, "--format") == 0 || strcmp(opt, "-f") == 0) {
            firmware_format = ty_optline_get_value(&optl);
            if (!firmware_format) {
                ty_log(TY_LOG_ERROR, "Option '--format' takes an argument");
                print_identify_usage(stderr);
                return EXIT_FAILURE;
            }
        } else if (strcmp(opt, "--json") == 0 || strcmp(opt, "-j") == 0) {
            output_json = true;
        } else if (!parse_common_option(&optl, opt)) {
            print_identify_usage(stderr);
            return EXIT_FAILURE;
        }
    }

    opt = ty_optline_consume_non_option(&optl);
    if (!opt) {
        ty_log(TY_LOG_ERROR, "Missing firmware filename");
        print_identify_usage(stderr);
        return EXIT_FAILURE;
    }
    do {
        ty_firmware *fw = NULL;
        ty_model fw_models[64];
        unsigned int fw_models_count = 0;
        int r;

        r = ty_firmware_load(opt, firmware_format, &fw);
        if (!r)
            fw_models_count = ty_firmware_identify(fw, fw_models, TY_COUNTOF(fw_models));
        ty_firmware_unref(fw);

        if (output_json) {
            printf("{\"file\": \"%s\", \"models\": [", opt);
            if (fw_models_count) {
                printf("\"%s\"", ty_models[fw_models[0]].name);
                for (unsigned int i = 1; i < fw_models_count; i++)
                    printf(", \"%s\"", ty_models[fw_models[i]].name);
            }
            printf("]");
            if (r < 0)
                printf(", \"error\": \"%s\"", ty_error_last_message());
            printf("}\n");
        } else {
            printf("%s: ", opt);
            if (fw_models_count) {
                printf("%s", ty_models[fw_models[0]].name);
                for (unsigned int i = 1; i < fw_models_count; i++)
                    printf("%s%s", (i + 1 < fw_models_count) ? ", " : " and ",
                           ty_models[fw_models[i]].name);
            } else {
                printf("Unknown");
            }
            printf("\n");
        }
    } while ((opt = ty_optline_consume_non_option(&optl)));

    return EXIT_SUCCESS;
}
