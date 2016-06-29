/*
 * ty, a collection of GUI and command-line tools to manage Teensy devices
 *
 * Distributed under the MIT license (see LICENSE.txt or http://opensource.org/licenses/MIT)
 * Copyright (c) 2015 Niels Martignène <niels.martignene@gmail.com>
 */

#include "util.h"
#include "ty/firmware.h"

TY_C_BEGIN

struct ty_firmware {
    unsigned int refcount;

    char *name;

    size_t alloc_size;
    uint8_t *image;
    size_t size;

    char filename[];
};

int _ty_firmware_expand_image(ty_firmware *fw, size_t size);

TY_C_END
