/* libhs - public domain
   Niels Martignène <niels.martignene@protonmail.com>
   https://koromix.dev/libhs

   This software is in the public domain. Where that dedication is not
   recognized, you are granted a perpetual, irrevocable license to copy,
   distribute, and modify this file as you see fit.

   See the LICENSE file for more details. */

#include <inttypes.h>
#include <stdio.h>
#ifdef _WIN32
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
#else
    #include <termios.h>
    #include <unistd.h>
#endif

/* For single-file use you need a tiny bit more than that, see libhs.h for
   more information. */
#include "../../libhs.h"

static int device_callback(hs_device *dev, void *udata)
{
    (void)(udata);

    const char *event = "?";

    /* Use hs_device_get_status() to differenciate between added and removed devices,
       when called from hs_monitor_list() it is always HS_DEVICE_STATUS_ONLINE. */
    switch (dev->status) {
        case HS_DEVICE_STATUS_DISCONNECTED: { event = "remove"; } break;
        case HS_DEVICE_STATUS_ONLINE: { event = "add"; } break;
    }

    printf("%s %s@%" PRIu8 " %04" PRIx16 ":%04" PRIx16 " (%s)\n",
           event, dev->location, dev->iface_number, dev->vid, dev->pid,
           hs_device_type_strings[dev->type]);
    printf("  - device node:   %s\n", dev->path);
    if (dev->manufacturer_string)
        printf("  - manufacturer:  %s\n", dev->manufacturer_string);
    if (dev->product_string)
        printf("  - product:       %s\n", dev->product_string);
    if (dev->serial_number_string)
        printf("  - serial number: %s\n", dev->serial_number_string);

    /* If you return a non-zero value, the enumeration/refresh is aborted and this value
       is returned from the calling function. */
    return 0;
}

int main(void)
{
    hs_monitor *monitor = NULL;
    hs_poll_source sources[2];
    int r;

    r = hs_monitor_new(NULL, 0, &monitor);
    if (r < 0)
        goto cleanup;

    /* Enumerate devices and start listening to OS notifications. The list is refreshed and the
       callback is called only when hs_monitor_refresh() is called. Use hs_monitor_get_poll_handle()
       to get a pollable descriptor and integrate it to your event loop. */
    r = hs_monitor_start(monitor);
    if (r < 0)
        goto cleanup;

    /* hs_monitor_list() uses a cached device list in the monitor object, which is only updated
       when you call hs_monitor_start() and hs_monitor_refresh(). */
    r = hs_monitor_list(monitor, device_callback, NULL);
    if (r < 0)
        goto cleanup;

    /* Add the waitable descriptor provided by the monitor to the descriptor set, it will
       become ready (POLLIN) when there are pending events. */
    sources[0].desc = hs_monitor_get_poll_handle(monitor);
    /* We also want to poll the terminal/console input buffer, to exit on key presses. */
#ifdef _WIN32
    sources[1].desc = GetStdHandle(STD_INPUT_HANDLE);
#else
    sources[1].desc = STDIN_FILENO;
#endif

    printf("Monitoring devices (press RETURN to end):\n");
    do {
        /* This function is non-blocking, if there are no pending events it does nothing and
           returns immediately. It calls the callback function for each notification (add or
           remove) and updates the device list accessed by hs_monitor_list(). */
        r = hs_monitor_refresh(monitor, device_callback, NULL);
        if (r < 0)
            goto cleanup;

        /* This function returns the number of ready sources, 0 if it times out or a negative
           error code. You can simply check each source's ready field after each call.  */
        r = hs_poll(sources, 2, -1);
    } while (r > 0 && !sources[1].ready);

    if (sources[1].ready) {
#ifndef _WIN32
        /* Clear the terminal input buffer, just to avoid the extra return/characters from
           showing up when this program exits. This has nothing to do with libhs. */
        tcflush(STDIN_FILENO, TCIFLUSH);
#endif
        r = 0;
    }

cleanup:
    hs_monitor_free(monitor);
    return -r;
}
