/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "ty/common.h"
#include "compat.h"
#include "ty/firmware.h"

TY_C_BEGIN

struct tyb_firmware {
    unsigned int refcount;

    char *name;

    size_t size;
    uint8_t image[];
};

TY_C_END
