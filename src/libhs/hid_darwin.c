/* libhs - public domain
   Niels Martignène <niels.martignene@protonmail.com>
   https://koromix.dev/libhs

   This software is in the public domain. Where that dedication is not
   recognized, you are granted a perpetual, irrevocable license to copy,
   distribute, and modify this file as you see fit.

   See the LICENSE file for more details. */

#include "common_priv.h"
#include <CoreFoundation/CFRunLoop.h>
#include <IOKit/IOCFPlugIn.h>
#include <IOKit/hid/IOHIDDevice.h>
#include <poll.h>
#include <pthread.h>
#include <unistd.h>
#include "array.h"
#include "device_priv.h"
#include "hid.h"
#include "platform.h"

struct hid_report {
    size_t size;
    uint8_t *data;
};

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

    _HS_ARRAY(struct hid_report) reports;

    pthread_t read_thread;
    pthread_cond_t cond;
    bool cond_init;

    CFRunLoopRef thread_loop;
    CFRunLoopSourceRef shutdown_source;
    bool device_removed;
};

#define MAX_REPORT_QUEUE_SIZE 128

static void fire_hid_poll_handle(struct _hs_hid_darwin *hid)
{
    char buf = '.';
    write(hid->poll_pipe[1], &buf, 1);
}

static void reset_hid_poll_handle(struct _hs_hid_darwin *hid)
{
    char buf;
    read(hid->poll_pipe[0], &buf, 1);
}

static void hid_removal_callback(void *ctx, IOReturn result, void *sender)
{
    _HS_UNUSED(result);
    _HS_UNUSED(sender);

    hs_port *port = (hs_port *)ctx;
    struct _hs_hid_darwin *hid = port->u.hid;

    pthread_mutex_lock(&hid->mutex);
    hid->device_removed = true;
    CFRunLoopSourceSignal(hid->shutdown_source);
    pthread_mutex_unlock(&hid->mutex);

    fire_hid_poll_handle(hid);
}

static void hid_report_callback(void *ctx, IOReturn result, void *sender,
                                IOHIDReportType report_type, uint32_t report_id,
                                uint8_t *report_data, CFIndex report_size)
{
    _HS_UNUSED(result);
    _HS_UNUSED(sender);

    if (report_type != kIOHIDReportTypeInput)
        return;

    hs_port *port = (hs_port *)ctx;
    struct _hs_hid_darwin *hid = port->u.hid;
    struct hid_report *report;
    bool was_empty;
    int r;

    pthread_mutex_lock(&hid->mutex);

    was_empty = !hid->reports.count;
    if (hid->reports.count == MAX_REPORT_QUEUE_SIZE) {
        r = 0;
        goto cleanup;
    }

    r = _hs_array_grow(&hid->reports, 1);
    if (r < 0)
        goto cleanup;
    report = hid->reports.values + hid->reports.count;
    if (!report->data) {
        // Don't forget the leading report ID
        report->data = (uint8_t *)malloc(hid->read_size + 1);
        if (!report->data) {
            r = hs_error(HS_ERROR_MEMORY, NULL);
            goto cleanup;
        }
    }

    /* You never know, even if hid->red_size is supposed to be the maximum
       input report size. */
    if (report_size > (CFIndex)hid->read_size)
        report_size = (CFIndex)hid->read_size;

    report->data[0] = (uint8_t)report_id;
    memcpy(report->data + 1, report_data, report_size);
    report->size = (size_t)report_size + 1;

    hid->reports.count++;

    r = 0;
cleanup:
    if (r < 0)
        hid->thread_ret = r;
    pthread_mutex_unlock(&hid->mutex);
    if (was_empty)
        fire_hid_poll_handle(hid);
}

static void *hid_read_thread(void *ptr)
{
    hs_port *port = (hs_port *)ptr;
    struct _hs_hid_darwin *hid = port->u.hid;
    CFRunLoopSourceContext shutdown_ctx = {0};
    int r;

    pthread_mutex_lock(&hid->mutex);

    hid->thread_loop = CFRunLoopGetCurrent();

    shutdown_ctx.info = hid->thread_loop;
    shutdown_ctx.perform = (void (*)(void *))CFRunLoopStop;
    /* close_hid_device() could be called before the loop is running, while this thread is between
       pthread_barrier_wait() and CFRunLoopRun(). That's the purpose of the shutdown source. */
    hid->shutdown_source = CFRunLoopSourceCreate(kCFAllocatorDefault, 0, &shutdown_ctx);
    if (!hid->shutdown_source) {
        r = hs_error(HS_ERROR_SYSTEM, "CFRunLoopSourceCreate() failed");
        goto error;
    }

    CFRunLoopAddSource(hid->thread_loop, hid->shutdown_source, kCFRunLoopCommonModes);
    IOHIDDeviceScheduleWithRunLoop(hid->hid_ref, hid->thread_loop, kCFRunLoopCommonModes);

    // This thread is ready, open_hid_device() can carry on
    hid->thread_ret = 1;
    pthread_cond_signal(&hid->cond);
    pthread_mutex_unlock(&hid->mutex);

    CFRunLoopRun();

    IOHIDDeviceUnscheduleFromRunLoop(hid->hid_ref, hid->thread_loop, kCFRunLoopCommonModes);

    pthread_mutex_lock(&hid->mutex);
    hid->thread_loop = NULL;
    pthread_mutex_unlock(&hid->mutex);

    return NULL;

error:
    hid->thread_ret = r;
    pthread_cond_signal(&hid->cond);
    pthread_mutex_unlock(&hid->mutex);
    return NULL;
}

static bool get_hid_device_property_number(IOHIDDeviceRef ref, CFStringRef prop,
                                           CFNumberType type, void *rn)
{
    CFTypeRef data = IOHIDDeviceGetProperty(ref, prop);
    if (!data || CFGetTypeID(data) != CFNumberGetTypeID())
        return false;

    return CFNumberGetValue((CFNumberRef)data, type, rn);
}

int _hs_darwin_open_hid_port(hs_device *dev, hs_port_mode mode, hs_port **rport)
{
    hs_port *port;
    struct _hs_hid_darwin *hid;
    kern_return_t kret;
    int r;

    port = (hs_port *)calloc(1, _HS_ALIGN_SIZE_FOR_TYPE(sizeof(*port), struct _hs_hid_darwin) +
                                sizeof(struct _hs_hid_darwin));
    if (!port) {
        r = hs_error(HS_ERROR_MEMORY, NULL);
        goto error;
    }
    port->type = dev->type;
    port->u.hid = (struct _hs_hid_darwin *)((char *)port +
                                            _HS_ALIGN_SIZE_FOR_TYPE(sizeof(*port), struct _hs_hid_darwin));
    hid = port->u.hid;
    hid->poll_pipe[0] = -1;
    hid->poll_pipe[1] = -1;

    port->mode = mode;
    port->path = dev->path;
    port->dev = hs_device_ref(dev);

    hid->service = IORegistryEntryFromPath(kIOMasterPortDefault, dev->path);
    if (!hid->service) {
        r = hs_error(HS_ERROR_NOT_FOUND, "Device '%s' not found", dev->path);
        goto error;
    }

    hid->hid_ref = IOHIDDeviceCreate(kCFAllocatorDefault, hid->service);
    if (!hid->hid_ref) {
        r = hs_error(HS_ERROR_NOT_FOUND, "Device '%s' not found", dev->path);
        goto error;
    }

    kret = IOHIDDeviceOpen(hid->hid_ref, 0);
    if (kret != kIOReturnSuccess) {
        r = hs_error(HS_ERROR_SYSTEM, "Failed to open HID device '%s'", dev->path);
        goto error;
    }

    IOHIDDeviceRegisterRemovalCallback(hid->hid_ref, hid_removal_callback, port);

    if (mode & HS_PORT_MODE_READ) {
        r = get_hid_device_property_number(hid->hid_ref, CFSTR(kIOHIDMaxInputReportSizeKey),
                                           kCFNumberSInt32Type, &hid->read_size);
        if (!r) {
            r = hs_error(HS_ERROR_SYSTEM, "HID device '%s' has no valid report size key", dev->path);
            goto error;
        }
        hid->read_buf = (uint8_t *)malloc(hid->read_size);
        if (!hid->read_buf) {
            r = hs_error(HS_ERROR_MEMORY, NULL);
            goto error;
        }

        IOHIDDeviceRegisterInputReportCallback(hid->hid_ref, hid->read_buf,
                                               (CFIndex)hid->read_size, hid_report_callback, port);

        r = pipe(hid->poll_pipe);
        if (r < 0) {
            r = hs_error(HS_ERROR_SYSTEM, "pipe() failed: %s", strerror(errno));
            goto error;
        }
        fcntl(hid->poll_pipe[0], F_SETFL, fcntl(hid->poll_pipe[0], F_GETFL, 0) | O_NONBLOCK);
        fcntl(hid->poll_pipe[1], F_SETFL, fcntl(hid->poll_pipe[1], F_GETFL, 0) | O_NONBLOCK);

        r = pthread_mutex_init(&hid->mutex, NULL);
        if (r < 0) {
            r = hs_error(HS_ERROR_SYSTEM, "pthread_mutex_init() failed: %s", strerror(r));
            goto error;
        }
        hid->mutex_init = true;

        r = pthread_cond_init(&hid->cond, NULL);
        if (r < 0) {
            r = hs_error(HS_ERROR_SYSTEM, "pthread_cond_init() failed: %s", strerror(r));
            goto error;
        }
        hid->cond_init = true;

        pthread_mutex_lock(&hid->mutex);

        r = pthread_create(&hid->read_thread, NULL, hid_read_thread, port);
        if (r) {
            r = hs_error(HS_ERROR_SYSTEM, "pthread_create() failed: %s", strerror(r));
            goto error;
        }

        /* Barriers are great for this, but OSX doesn't have those... And since it's the only
           place we would use them, it's probably not worth it to have a custom implementation. */
        while (!hid->thread_ret)
            pthread_cond_wait(&hid->cond, &hid->mutex);
        r = hid->thread_ret;
        hid->thread_ret = 0;
        pthread_mutex_unlock(&hid->mutex);
        if (r < 0)
            goto error;
    }

    *rport = port;
    return 0;

error:
    hs_port_close(port);
    return r;
}

void _hs_darwin_close_hid_port(hs_port *port)
{
    if (port) {
        struct _hs_hid_darwin *hid = port->u.hid;

        if (hid->shutdown_source) {
            pthread_mutex_lock(&hid->mutex);

            if (hid->thread_loop) {
                CFRunLoopSourceSignal(hid->shutdown_source);
                CFRunLoopWakeUp(hid->thread_loop);
            }

            pthread_mutex_unlock(&hid->mutex);
            pthread_join(hid->read_thread, NULL);

            CFRelease(hid->shutdown_source);
        }

        if (hid->cond_init)
            pthread_cond_destroy(&hid->cond);
        if (hid->mutex_init)
            pthread_mutex_destroy(&hid->mutex);

        for (size_t i = 0; i < hid->reports.count; i++) {
            struct hid_report *report = &hid->reports.values[i];
            free(report->data);
        }
        _hs_array_release(&hid->reports);

        close(hid->poll_pipe[0]);
        close(hid->poll_pipe[1]);

        free(hid->read_buf);

        if (hid->hid_ref) {
            IOHIDDeviceClose(hid->hid_ref, 0);
            CFRelease(hid->hid_ref);
        }
        if (hid->service)
            IOObjectRelease(hid->service);

        hs_device_unref(port->dev);
    }

    free(port);
}

hs_handle _hs_darwin_get_hid_port_poll_handle(const hs_port *port)
{
    return port->u.hid->poll_pipe[0];
}

ssize_t hs_hid_read(hs_port *port, uint8_t *buf, size_t size, int timeout)
{
    assert(port);
    assert(port->type == HS_DEVICE_TYPE_HID);
    assert(port->mode & HS_PORT_MODE_READ);
    assert(buf);
    assert(size);

    struct _hs_hid_darwin *hid = port->u.hid;
    struct hid_report *report;
    ssize_t r;

    if (hid->device_removed)
        return hs_error(HS_ERROR_IO, "Device '%s' was removed", port->path);

    if (timeout) {
        struct pollfd pfd;
        uint64_t start;

        pfd.events = POLLIN;
        pfd.fd = hid->poll_pipe[0];

        start = hs_millis();
restart:
        r = poll(&pfd, 1, hs_adjust_timeout(timeout, start));
        if (r < 0) {
            if (errno == EINTR)
                goto restart;

            return hs_error(HS_ERROR_SYSTEM, "poll('%s') failed: %s", port->path, strerror(errno));
        }
        if (!r)
            return 0;
    }

    pthread_mutex_lock(&hid->mutex);

    if (hid->thread_ret < 0) {
        r = hid->thread_ret;
        hid->thread_ret = 0;
        goto cleanup;
    }
    if (!hid->reports.count) {
        r = 0;
        goto cleanup;
    }

    report = &hid->reports.values[0];
    if (size > report->size)
        size = report->size;
    memcpy(buf, report->data, size);
    r = (ssize_t)size;

    // Circular buffer would be more appropriate. Later.
    _hs_array_remove(&hid->reports, 0, 1);

cleanup:
    if (!hid->reports.count)
        reset_hid_poll_handle(hid);
    pthread_mutex_unlock(&hid->mutex);
    return r;
}

static ssize_t send_report(hs_port *port, IOHIDReportType type, const uint8_t *buf, size_t size)
{
    struct _hs_hid_darwin *hid = port->u.hid;
    uint8_t report;
    kern_return_t kret;

    if (hid->device_removed)
        return hs_error(HS_ERROR_IO, "Device '%s' was removed", port->path);

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
    kret = IOHIDDeviceSetReport(hid->hid_ref, type, report, buf, (CFIndex)size);
    if (kret != kIOReturnSuccess)
        return hs_error(HS_ERROR_IO, "I/O error while writing to '%s'", port->path);

    return (ssize_t)size + !report;
}

ssize_t hs_hid_write(hs_port *port, const uint8_t *buf, size_t size)
{
    assert(port);
    assert(port->type == HS_DEVICE_TYPE_HID);
    assert(port->mode & HS_PORT_MODE_WRITE);
    assert(buf);

    return send_report(port, kIOHIDReportTypeOutput, buf, size);
}

ssize_t hs_hid_get_feature_report(hs_port *port, uint8_t report_id, uint8_t *buf, size_t size)
{
    assert(port);
    assert(port->type == HS_DEVICE_TYPE_HID);
    assert(port->mode & HS_PORT_MODE_READ);
    assert(buf);
    assert(size);

    struct _hs_hid_darwin *hid = port->u.hid;
    CFIndex len;
    kern_return_t kret;

    if (hid->device_removed)
        return hs_error(HS_ERROR_IO, "Device '%s' was removed", port->path);

    len = (CFIndex)size - 1;
    kret = IOHIDDeviceGetReport(hid->hid_ref, kIOHIDReportTypeFeature, report_id,
                                buf + 1, &len);
    if (kret != kIOReturnSuccess)
        return hs_error(HS_ERROR_IO, "IOHIDDeviceGetReport() failed on '%s'", port->path);

    buf[0] = report_id;
    return (ssize_t)len;
}

ssize_t hs_hid_send_feature_report(hs_port *port, const uint8_t *buf, size_t size)
{
    assert(port);
    assert(port->type == HS_DEVICE_TYPE_HID);
    assert(port->mode & HS_PORT_MODE_WRITE);
    assert(buf);

    return send_report(port, kIOHIDReportTypeFeature, buf, size);
}
