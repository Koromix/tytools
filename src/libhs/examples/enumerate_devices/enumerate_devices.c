/* libhs - public domain
   Niels Martignène <niels.martignene@protonmail.com>
   https://koromix.dev/libhs

   This software is in the public domain. Where that dedication is not
   recognized, you are granted a perpetual, irrevocable license to copy,
   distribute, and modify this file as you see fit.

   See the LICENSE file for more details. */

#include <inttypes.h>
#include <stdio.h>

/* For single-file use you need a tiny bit more than that, see libhs.h for
   more information. */
#include "../../libhs.h"

static int device_callback(hs_device *dev, void *udata)
{
    (void)(udata);

    printf("+ %s@%" PRIu8 " %04" PRIx16 ":%04" PRIx16 " (%s)\n",
           dev->location, dev->iface_number, dev->vid, dev->pid,
           hs_device_type_strings[dev->type]);
    printf("  - device key:    %s\n", dev->key);
    printf("  - device node:   %s\n", dev->path);
    if (dev->manufacturer_string)
        printf("  - manufacturer:  %s\n", dev->manufacturer_string);
    if (dev->product_string)
        printf("  - product:       %s\n", dev->product_string);
    if (dev->serial_number_string)
        printf("  - serial number: %s\n", dev->serial_number_string);

    if (dev->type == HS_DEVICE_TYPE_HID) {
        printf("  - HID information:\n");
        printf("    - Primary usage page: 0x%" PRIX16 " (%" PRIu16 ")\n", dev->u.hid.usage_page, dev->u.hid.usage_page);
        printf("    - Primary usage: 0x%" PRIX16 " (%" PRIu16 ")\n", dev->u.hid.usage, dev->u.hid.usage);
        printf("    - Maximum input report size: %zu bytes\n", dev->u.hid.max_input_len);
        printf("    - Maximum output report size: %zu bytes\n", dev->u.hid.max_output_len);
        printf("    - Maximum feature report size: %zu bytes\n", dev->u.hid.max_feature_len);
    }

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
