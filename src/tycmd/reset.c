/* TyTools - public domain
   Niels Martignène <niels.martignene@protonmail.com>
   https://koromix.dev/tytools

   This software is in the public domain. Where that dedication is not
   recognized, you are granted a perpetual, irrevocable license to copy,
   distribute, and modify this file as you see fit.

   See the LICENSE file for more details. */

#include "../libty/task.h"
#include "main.h"

static bool reset_bootloader = false;

static void print_reset_usage(FILE *f)
{
    fprintf(f, "usage: %s reset\n\n", tycmd_executable_name);

    print_common_options(f);
    fprintf(f, "\n");

    fprintf(f, "Reset options:\n"
               "   -b, --bootloader         Switch board to bootloader\n");
}

int reset(int argc, char *argv[])
{
    ty_optline_context optl;
    char *opt;
    ty_board *board = NULL;
    ty_task *task = NULL;
    int r;

    ty_optline_init_argv(&optl, argc, argv);
    while ((opt = ty_optline_next_option(&optl))) {
        if (strcmp(opt, "--help") == 0) {
            print_reset_usage(stdout);
            return EXIT_SUCCESS;
        } else if (strcmp(opt, "-b") == 0 || strcmp(opt, "--bootloader") == 0) {
            reset_bootloader = true;
        } else if (!parse_common_option(&optl, opt)) {
            print_reset_usage(stderr);
            return EXIT_FAILURE;
        }
    }
    if (ty_optline_consume_non_option(&optl)) {
        ty_log(TY_LOG_ERROR, "No positional argument is allowed");
        print_reset_usage(stderr);
        return EXIT_FAILURE;
    }

    r = get_board(&board);
    if (r < 0)
        goto cleanup;

    if (reset_bootloader) {
        r = ty_reboot(board, &task);
    } else {
        r = ty_reset(board, &task);
    }
    if (r < 0)
        goto cleanup;

    r = ty_task_join(task);

cleanup:
    ty_task_unref(task);
    ty_board_unref(board);
    return r < 0 ? EXIT_FAILURE : EXIT_SUCCESS;
}
