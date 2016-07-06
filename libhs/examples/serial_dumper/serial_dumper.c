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
#include <stdlib.h>
#ifdef _WIN32
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
#else
    #include <termios.h>
    #include <unistd.h>
#endif
#include "hs.h"

static hs_poll_source sources[HS_POLL_MAX_SOURCES];
static unsigned int sources_count;

static size_t read_total, read_rate;

struct serial_source {
    hs_device *dev;
    hs_handle *in;
    FILE *out;

    char out_name[40];
};

static void free_serial_source(struct serial_source *src)
{
    if (src) {
        if (src->out)
            fclose(src->out);

        hs_handle_close(src->in);
        hs_device_unref(src->dev);
    }

    free(src);
}

static int add_serial_source(hs_device *dev)
{
    static unsigned int dump_count;
    struct serial_source *src;
    int r;

    if (sources_count == sizeof(sources) / sizeof(*sources)) {
        hs_log(HS_LOG_WARNING, "Cannot monitor more than %zu descriptors, ignoring %s",
               sizeof(sources) / sizeof(*sources), hs_device_get_path(dev));
        return 0;
    }

    src = calloc(1, sizeof(*src));
    if (!src) {
        r = hs_error(HS_ERROR_MEMORY, NULL);
        goto error;
    }

    src->dev = hs_device_ref(dev);
    r = hs_handle_open(dev, HS_HANDLE_MODE_READ, &src->in);
    if (r < 0) {
        // If something goes wrong, ignore this device and continue monitoring anyway
        r = 0;
        goto error;
    }

    sprintf(src->out_name, "dump%u.txt", dump_count++);
    src->out = fopen(src->out_name, "wb");
    if (!src->out) {
        r = hs_error(HS_ERROR_SYSTEM, "Failed to open '%s'", src->out_name);
        goto error;
    }

    sources[sources_count].desc = hs_handle_get_descriptor(src->in);
    sources[sources_count].udata = src;
    sources_count++;

    printf("Dumping '%s' to %s\n", hs_device_get_path(dev), src->out_name);
    return 0;

error:
    free_serial_source(src);
    return r;
}

static void remove_serial_source(hs_device *dev)
{
    for (unsigned int i = 2; i < sources_count; i++) {
        struct serial_source *src = sources[i].udata;

        if (src->dev == dev) {
            free_serial_source(src);

            // We need a contiguous source array, move the last source to this spot
            sources[i].desc = sources[sources_count - 1].desc;
            sources[i].udata = sources[sources_count - 1].udata;
            sources_count--;

            printf("Closed file %s for device '%s'\n", src->out_name, hs_device_get_path(dev));
            break;
        }
    }
}

static int device_callback(hs_device *dev, void *udata)
{
    (void)(udata);

    switch (hs_device_get_status(dev)) {
    case HS_DEVICE_STATUS_ONLINE:
        return add_serial_source(dev);
    case HS_DEVICE_STATUS_DISCONNECTED:
        remove_serial_source(dev);
        return 0;
    }

    return 0;
}

static void echo_serial(struct serial_source *src)
{
    uint8_t buf[8192];
    ssize_t r;

    r = hs_serial_read(src->in, buf, sizeof(buf), 0);
    if (r <= 0)
        return;

    fwrite(buf, 1, (size_t)r, src->out);
    fflush(src->out);

    read_total += (size_t)r;
}

static int refresh_read_rate(void)
{
    static uint64_t last_refresh;
    uint64_t now;

    now = hs_millis();
    if (now - last_refresh < 1000)
        return 0;

    read_rate = read_total * 1000 / (size_t)(now - last_refresh);
    read_total = 0;

    last_refresh = now;
    return 1;
}

int main(void)
{
    // We want serial devices only, you can match multiple devices with an array of matches
    static const hs_match match = HS_MATCH_TYPE(HS_DEVICE_TYPE_SERIAL);
    hs_monitor *monitor = NULL;
    int r;

    r = hs_monitor_new(&match, 1, &monitor);
    if (r < 0)
        goto cleanup;

    /* Look at the monitor_devices example for comments about the monitor API. */
    r = hs_monitor_start(monitor);
    if (r < 0)
        goto cleanup;

    sources[0].desc = hs_monitor_get_descriptor(monitor);
#ifdef _WIN32
    sources[1].desc = GetStdHandle(STD_INPUT_HANDLE);
#else
    sources[1].desc = STDIN_FILENO;
#endif
    sources_count = 2;

    r = hs_monitor_list(monitor, device_callback, NULL);
    if (r < 0)
        goto cleanup;

    printf("---- Press RETURN to end ----\n");

    do {
        int rate_changed;

        for (unsigned int i = 2; i < sources_count; i++) {
            if (sources[i].ready)
                echo_serial(sources[i].udata);
        }
        rate_changed = refresh_read_rate();

        if (sources[0].ready) {
            r = hs_monitor_refresh(monitor, device_callback, NULL);
            if (r < 0)
                goto cleanup;

            // Notifications may have clobbered the transfer rate line, write it again
            rate_changed = 1;
        }

        if (rate_changed) {
            if (read_rate >= 1024) {
                printf("Read Rate: %zu kiB/sec           \r", read_rate / 1024);
            } else {
                printf("Read Rate: %zu bytes/sec         \r", read_rate);
            }
            fflush(stdout);
        }

        /* Timeout to recompute the transfer rate unless it is already 0, so that we don't
           keep showing a non-zero transfer rate if nothing happens. */
        r = hs_poll(sources, sources_count, read_rate ? 1000 : -1);
    } while (r >= 0 && !sources[1].ready);

    if (sources[1].ready) {
#ifndef _WIN32
        tcflush(STDIN_FILENO, TCIFLUSH);
#endif
        r = 0;
    }

cleanup:
    for (unsigned int i = 2; i < sources_count; i++)
        free_serial_source(sources[i].udata);
    hs_monitor_free(monitor);
    return -r;
}
