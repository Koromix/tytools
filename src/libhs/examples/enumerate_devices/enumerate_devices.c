/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2015 Niels Martignène <niels.martignene@gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <inttypes.h>
#include <stdio.h>
#include "../../libhs.h"

static int device_callback(hs_device *dev, void *udata)
{
    (void)(udata);

    const char *type = "?";

    switch (dev->type) {
    case HS_DEVICE_TYPE_HID:
        type = "hid";
        break;
    case HS_DEVICE_TYPE_SERIAL:
        type = "serial";
        break;
    }

    printf("+ %s@%"PRIu8" %04"PRIx16":%04"PRIx16" (%s)\n",
           dev->location, dev->iface_number, dev->vid, dev->pid, type);

#define PRINT_PROPERTY(name, prop) \
        if (dev->prop) \
            printf("  - " name " %s\n", dev->prop);

    PRINT_PROPERTY("device node:  ", path);
    PRINT_PROPERTY("manufacturer: ", manufacturer_string);
    PRINT_PROPERTY("product:      ", product_string);
    PRINT_PROPERTY("serial number:", serial_number_string);

#undef PRINT_PROPERTY

    /* If you return a non-zero value, the enumeration is aborted and this value is returned
       from the calling function. */
    return 0;
}

int main(void)
{
    int r;

    /* Go through the device tree and call our callback for each device. The callback can abort
       the enumeration by returning a non-zero value. */
    r = hs_enumerate(NULL, 0, device_callback, NULL);
    if (r < 0)
        return -r;

    return 0;
}
