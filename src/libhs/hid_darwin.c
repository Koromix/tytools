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

#include "common_priv.h"
#include <CoreFoundation/CFRunLoop.h>
#include <IOKit/IOCFPlugIn.h>
#include <IOKit/hid/IOHIDDevice.h>
#include <poll.h>
#include <pthread.h>
#include <unistd.h>
#include "device_priv.h"
#include "hid.h"
#include "list.h"
#include "platform.h"

// Used for HID devices, see serial_posix.c for serial devices
struct _hs_hid_darwin {
    io_service_t service;
    union {
        IOHIDDeviceRef hid_ref;
    };

    uint8_t *read_buf;
    size_t read_size;

    pthread_mutex_t mutex;
    bool mutex_init;
    int poll_pipe[2];
    int thread_ret;

    _hs_list_head reports;
    unsigned int allocated_reports;
    _hs_list_head free_reports;

    pthread_t read_thread;
    pthread_cond_t cond;
    bool cond_init;

    CFRunLoopRef thread_loop;
    CFRunLoopSourceRef shutdown_source;
    bool device_removed;
};

static void fire_device_event(hs_handle *h)
{
    char buf = '.';
    write(h->u.hid->poll_pipe[1], &buf, 1);
}

static void reset_device_event(hs_handle *h)
{
    char buf;
    read(h->u.hid->poll_pipe[0], &buf, 1);
}

static void hid_removal_callback(void *ctx, IOReturn result, void *sender)
{
    _HS_UNUSED(result);
    _HS_UNUSED(sender);

    hs_handle *h = ctx;

    pthread_mutex_lock(&h->u.hid->mutex);
    h->u.hid->device_removed = true;
    CFRunLoopSourceSignal(h->u.hid->shutdown_source);
    pthread_mutex_unlock(&h->u.hid->mutex);

    fire_device_event(h);
}

struct hid_report {
    _hs_list_head list;

    size_t size;
    uint8_t data[];
};

static void hid_report_callback(void *ctx, IOReturn result, void *sender,
                                IOHIDReportType report_type, uint32_t report_id,
                                uint8_t *report_data, CFIndex report_size)
{
    _HS_UNUSED(result);
    _HS_UNUSED(sender);

    if (report_type != kIOHIDReportTypeInput)
        return;

    hs_handle *h = ctx;

    struct hid_report *report;
    bool fire;
    int r;

    pthread_mutex_lock(&h->u.hid->mutex);

    fire = _hs_list_is_empty(&h->u.hid->reports);

    report = _hs_list_get_first(&h->u.hid->free_reports, struct hid_report, list);
    if (report) {
        _hs_list_remove(&report->list);
    } else {
        if (h->u.hid->allocated_reports == 64) {
            r = 0;
            goto cleanup;
        }

        // Don't forget the leading report ID
        report = calloc(1, sizeof(struct hid_report) + h->u.hid->read_size + 1);
        if (!report) {
            r = hs_error(HS_ERROR_MEMORY, NULL);
            goto cleanup;
        }
        h->u.hid->allocated_reports++;
    }

    // You never know, even if h->u.hid->red_size is supposed to be the maximum input report size
    if (report_size > (CFIndex)h->u.hid->read_size)
        report_size = (CFIndex)h->u.hid->read_size;

    report->data[0] = (uint8_t)report_id;
    memcpy(report->data + 1, report_data, report_size);
    report->size = (size_t)report_size + 1;

    _hs_list_add_tail(&h->u.hid->reports, &report->list);

    r = 0;
cleanup:
    if (r < 0)
        h->u.hid->thread_ret = r;
    pthread_mutex_unlock(&h->u.hid->mutex);
    if (fire)
        fire_device_event(h);
}

static void *hid_read_thread(void *ptr)
{
    hs_handle *h = ptr;
    CFRunLoopSourceContext shutdown_ctx = {0};
    int r;

    pthread_mutex_lock(&h->u.hid->mutex);

    h->u.hid->thread_loop = CFRunLoopGetCurrent();

    shutdown_ctx.info = h->u.hid->thread_loop;
    shutdown_ctx.perform = (void (*)(void *))CFRunLoopStop;
    /* close_hid_device() could be called before the loop is running, while this thread is between
       pthread_barrier_wait() and CFRunLoopRun(). That's the purpose of the shutdown source. */
    h->u.hid->shutdown_source = CFRunLoopSourceCreate(kCFAllocatorDefault, 0, &shutdown_ctx);
    if (!h->u.hid->shutdown_source) {
        r = hs_error(HS_ERROR_SYSTEM, "CFRunLoopSourceCreate() failed");
        goto error;
    }

    CFRunLoopAddSource(h->u.hid->thread_loop, h->u.hid->shutdown_source, kCFRunLoopCommonModes);
    IOHIDDeviceScheduleWithRunLoop(h->u.hid->hid_ref, h->u.hid->thread_loop, kCFRunLoopCommonModes);

    // This thread is ready, open_hid_device() can carry on
    h->u.hid->thread_ret = 1;
    pthread_cond_signal(&h->u.hid->cond);
    pthread_mutex_unlock(&h->u.hid->mutex);

    CFRunLoopRun();

    IOHIDDeviceUnscheduleFromRunLoop(h->u.hid->hid_ref, h->u.hid->thread_loop,
                                     kCFRunLoopCommonModes);

    pthread_mutex_lock(&h->u.hid->mutex);
    h->u.hid->thread_loop = NULL;
    pthread_mutex_unlock(&h->u.hid->mutex);

    return NULL;

error:
    h->u.hid->thread_ret = r;
    pthread_cond_signal(&h->u.hid->cond);
    pthread_mutex_unlock(&h->u.hid->mutex);
    return NULL;
}

static bool get_hid_device_property_number(IOHIDDeviceRef ref, CFStringRef prop,
                                           CFNumberType type, void *rn)
{
    CFTypeRef data = IOHIDDeviceGetProperty(ref, prop);
    if (!data || CFGetTypeID(data) != CFNumberGetTypeID())
        return false;

    return CFNumberGetValue(data, type, rn);
}

static int open_hid_device(hs_device *dev, hs_handle_mode mode, hs_handle **rh)
{
    hs_handle *h;
    kern_return_t kret;
    int r;

    h = calloc(1, _HS_ALIGN_SIZE_FOR_TYPE(sizeof(*h), struct _hs_hid_darwin) +
                  sizeof(struct _hs_hid_darwin));
    if (!h) {
        r = hs_error(HS_ERROR_MEMORY, NULL);
        goto error;
    }
    h->dev = hs_device_ref(dev);
    h->mode = mode;
    h->u.hid = (struct _hs_hid_darwin *)((char *)h +
                                         _HS_ALIGN_SIZE_FOR_TYPE(sizeof(*h), struct _hs_hid_darwin));

    h->u.hid->poll_pipe[0] = -1;
    h->u.hid->poll_pipe[1] = -1;

    _hs_list_init(&h->u.hid->reports);
    _hs_list_init(&h->u.hid->free_reports);

    h->u.hid->service = IORegistryEntryFromPath(kIOMasterPortDefault, dev->path);
    if (!h->u.hid->service) {
        r = hs_error(HS_ERROR_NOT_FOUND, "Device '%s' not found", dev->path);
        goto error;
    }

    h->u.hid->hid_ref = IOHIDDeviceCreate(kCFAllocatorDefault, h->u.hid->service);
    if (!h->u.hid->hid_ref) {
        r = hs_error(HS_ERROR_NOT_FOUND, "Device '%s' not found", dev->path);
        goto error;
    }

    kret = IOHIDDeviceOpen(h->u.hid->hid_ref, 0);
    if (kret != kIOReturnSuccess) {
        r = hs_error(HS_ERROR_SYSTEM, "Failed to open HID device '%s'", dev->path);
        goto error;
    }

    IOHIDDeviceRegisterRemovalCallback(h->u.hid->hid_ref, hid_removal_callback, h);

    if (mode & HS_HANDLE_MODE_READ) {
        r = get_hid_device_property_number(h->u.hid->hid_ref, CFSTR(kIOHIDMaxInputReportSizeKey),
                                           kCFNumberSInt32Type, &h->u.hid->read_size);
        if (!r) {
            r = hs_error(HS_ERROR_SYSTEM, "HID device '%s' has no valid report size key", dev->path);
            goto error;
        }
        h->u.hid->read_buf = malloc(h->u.hid->read_size);
        if (!h->u.hid->read_buf) {
            r = hs_error(HS_ERROR_MEMORY, NULL);
            goto error;
        }

        IOHIDDeviceRegisterInputReportCallback(h->u.hid->hid_ref, h->u.hid->read_buf,
                                               (CFIndex)h->u.hid->read_size, hid_report_callback, h);

        r = pipe(h->u.hid->poll_pipe);
        if (r < 0) {
            r = hs_error(HS_ERROR_SYSTEM, "pipe() failed: %s", strerror(errno));
            goto error;
        }
        fcntl(h->u.hid->poll_pipe[0], F_SETFL,
                fcntl(h->u.hid->poll_pipe[0], F_GETFL, 0) | O_NONBLOCK);
        fcntl(h->u.hid->poll_pipe[1], F_SETFL,
                fcntl(h->u.hid->poll_pipe[1], F_GETFL, 0) | O_NONBLOCK);

        r = pthread_mutex_init(&h->u.hid->mutex, NULL);
        if (r < 0) {
            r = hs_error(HS_ERROR_SYSTEM, "pthread_mutex_init() failed: %s", strerror(r));
            goto error;
        }
        h->u.hid->mutex_init = true;

        r = pthread_cond_init(&h->u.hid->cond, NULL);
        if (r < 0) {
            r = hs_error(HS_ERROR_SYSTEM, "pthread_cond_init() failed: %s", strerror(r));
            goto error;
        }
        h->u.hid->cond_init = true;

        pthread_mutex_lock(&h->u.hid->mutex);

        r = pthread_create(&h->u.hid->read_thread, NULL, hid_read_thread, h);
        if (r) {
            r = hs_error(HS_ERROR_SYSTEM, "pthread_create() failed: %s", strerror(r));
            goto error;
        }

        /* Barriers are great for this, but OSX doesn't have those... And since it's the only
           place we would use them, it's probably not worth it to have a custom implementation. */
        while (!h->u.hid->thread_ret)
            pthread_cond_wait(&h->u.hid->cond, &h->u.hid->mutex);
        r = h->u.hid->thread_ret;
        h->u.hid->thread_ret = 0;
        pthread_mutex_unlock(&h->u.hid->mutex);
        if (r < 0)
            goto error;
    }

    *rh = h;
    return 0;

error:
    hs_handle_close(h);
    return r;
}

static void close_hid_device(hs_handle *h)
{
    if (h) {
        if (h->u.hid->shutdown_source) {
            pthread_mutex_lock(&h->u.hid->mutex);

            if (h->u.hid->thread_loop) {
                CFRunLoopSourceSignal(h->u.hid->shutdown_source);
                CFRunLoopWakeUp(h->u.hid->thread_loop);
            }

            pthread_mutex_unlock(&h->u.hid->mutex);
            pthread_join(h->u.hid->read_thread, NULL);

            CFRelease(h->u.hid->shutdown_source);
        }

        if (h->u.hid->cond_init)
            pthread_cond_destroy(&h->u.hid->cond);
        if (h->u.hid->mutex_init)
            pthread_mutex_destroy(&h->u.hid->mutex);

        _hs_list_splice(&h->u.hid->free_reports, &h->u.hid->reports);
        _hs_list_foreach(cur, &h->u.hid->free_reports) {
            struct hid_report *report = _hs_container_of(cur, struct hid_report, list);
            free(report);
        }

        close(h->u.hid->poll_pipe[0]);
        close(h->u.hid->poll_pipe[1]);

        free(h->u.hid->read_buf);

        if (h->u.hid->hid_ref) {
            IOHIDDeviceClose(h->u.hid->hid_ref, 0);
            CFRelease(h->u.hid->hid_ref);
        }
        if (h->u.hid->service)
            IOObjectRelease(h->u.hid->service);

        hs_device_unref(h->dev);
    }

    free(h);
}

static hs_descriptor get_hid_descriptor(const hs_handle *h)
{
    return h->u.hid->poll_pipe[0];
}

const struct _hs_device_vtable _hs_darwin_hid_vtable = {
    .open = open_hid_device,
    .close = close_hid_device,

    .get_descriptor = get_hid_descriptor
};

ssize_t hs_hid_read(hs_handle *h, uint8_t *buf, size_t size, int timeout)
{
    assert(h);
    assert(h->dev->type == HS_DEVICE_TYPE_HID);
    assert(h->mode & HS_HANDLE_MODE_READ);
    assert(buf);
    assert(size);

    struct hid_report *report;
    ssize_t r;

    if (h->u.hid->device_removed)
        return hs_error(HS_ERROR_IO, "Device '%s' was removed", h->dev->path);

    if (timeout) {
        struct pollfd pfd;
        uint64_t start;

        pfd.events = POLLIN;
        pfd.fd = h->u.hid->poll_pipe[0];

        start = hs_millis();
restart:
        r = poll(&pfd, 1, hs_adjust_timeout(timeout, start));
        if (r < 0) {
            if (errno == EINTR)
                goto restart;

            return hs_error(HS_ERROR_SYSTEM, "poll('%s') failed: %s", h->dev->path, strerror(errno));
        }
        if (!r)
            return 0;
    }

    pthread_mutex_lock(&h->u.hid->mutex);

    if (h->u.hid->thread_ret < 0) {
        r = h->u.hid->thread_ret;
        h->u.hid->thread_ret = 0;

        goto reset;
    }

    report = _hs_list_get_first(&h->u.hid->reports, struct hid_report, list);
    if (!report) {
        r = 0;
        goto cleanup;
    }

    if (size > report->size)
        size = report->size;
    memcpy(buf, report->data, size);
    r = (ssize_t)size;

    _hs_list_remove(&report->list);
    _hs_list_add(&h->u.hid->free_reports, &report->list);

reset:
    if (_hs_list_is_empty(&h->u.hid->reports))
        reset_device_event(h);

cleanup:
    pthread_mutex_unlock(&h->u.hid->mutex);
    return r;
}

static ssize_t send_report(hs_handle *h, IOHIDReportType type, const uint8_t *buf, size_t size)
{
    uint8_t report;
    kern_return_t kret;

    if (h->u.hid->device_removed)
        return hs_error(HS_ERROR_IO, "Device '%s' was removed", h->dev->path);

    if (size < 2)
        return 0;

    report = buf[0];
    if (!report) {
        buf++;
        size--;
    }

    /* FIXME: find a way drop out of IOHIDDeviceSetReport() after a reasonable time, because
       IOHIDDeviceSetReportWithCallback() is broken. Perhaps we can open the device twice and
       close the write side to drop out of IOHIDDeviceSetReport() after a few seconds? Or maybe
       we can call IOHIDDeviceSetReport() in another thread and kill it, but I don't trust OSX
       to behave well in that case. The HID API does like to crash OSX for no reason. */
    kret = IOHIDDeviceSetReport(h->u.hid->hid_ref, type, report, buf, (CFIndex)size);
    if (kret != kIOReturnSuccess)
        return hs_error(HS_ERROR_IO, "I/O error while writing to '%s'", h->dev->path);

    return (ssize_t)size + !report;
}

ssize_t hs_hid_write(hs_handle *h, const uint8_t *buf, size_t size)
{
    assert(h);
    assert(h->dev->type == HS_DEVICE_TYPE_HID);
    assert(h->mode & HS_HANDLE_MODE_WRITE);
    assert(buf);

    return send_report(h, kIOHIDReportTypeOutput, buf, size);
}

ssize_t hs_hid_get_feature_report(hs_handle *h, uint8_t report_id, uint8_t *buf, size_t size)
{
    assert(h);
    assert(h->dev->type == HS_DEVICE_TYPE_HID);
    assert(h->mode & HS_HANDLE_MODE_READ);
    assert(buf);
    assert(size);

    CFIndex len;
    kern_return_t kret;

    if (h->u.hid->device_removed)
        return hs_error(HS_ERROR_IO, "Device '%s' was removed", h->dev->path);

    len = (CFIndex)size - 1;
    kret = IOHIDDeviceGetReport(h->u.hid->hid_ref, kIOHIDReportTypeFeature, report_id,
                                buf + 1, &len);
    if (kret != kIOReturnSuccess)
        return hs_error(HS_ERROR_IO, "IOHIDDeviceGetReport() failed on '%s'", h->dev->path);

    buf[0] = report_id;
    return (ssize_t)len;
}

ssize_t hs_hid_send_feature_report(hs_handle *h, const uint8_t *buf, size_t size)
{
    assert(h);
    assert(h->dev->type == HS_DEVICE_TYPE_HID);
    assert(h->mode & HS_HANDLE_MODE_WRITE);
    assert(buf);

    return send_report(h, kIOHIDReportTypeFeature, buf, size);
}
