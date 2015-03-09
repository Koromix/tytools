/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "ty/common.h"
#include "compat.h"
#include <CoreFoundation/CFRunLoop.h>
#include <IOKit/IOCFPlugIn.h>
#include <IOKit/usb/IOUSBLib.h>
#include <IOKit/hid/IOHIDDevice.h>
#include <mach/mach.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/event.h>
#include <sys/select.h>
#include <sys/time.h>
#include <unistd.h>
#include "ty/device.h"
#include "list.h"
#include "ty/system.h"
#include "device_priv.h"

struct ty_device_monitor {
    TY_DEVICE_MONITOR

    IONotificationPortRef notify_port;
    io_iterator_t attach_it;
    io_iterator_t detach_it;
    int notify_ret;

    int kqfd;
    mach_port_t port_set;

    ty_list_head controllers;
};

struct ty_handle {
    TY_HANDLE

    io_service_t service;
    union {
        IOHIDDeviceRef hid;
    };

    uint8_t *buf;
    size_t size;

    pthread_mutex_t mutex;
    bool mutex_init;
    int pipe[2];
    int thread_ret;

    ty_list_head reports;
    unsigned int allocated_reports;
    ty_list_head free_reports;

    pthread_t thread;
    pthread_cond_t cond;
    bool cond_init;

    CFRunLoopRef loop;
    CFRunLoopSourceRef shutdown;
};

extern const struct _ty_device_vtable _ty_posix_device_vtable;
static const struct _ty_device_vtable hid_device_vtable;

static int get_ioregistry_value_string(io_service_t service, CFStringRef prop, char **rs)
{
    CFTypeRef data;
    CFIndex size;
    char *s;
    int r;

    data = IORegistryEntryCreateCFProperty(service, prop, kCFAllocatorDefault, 0);
    if (!data || CFGetTypeID(data) != CFStringGetTypeID()) {
        r = 0;
        goto cleanup;
    }

    size = CFStringGetMaximumSizeForEncoding(CFStringGetLength(data), kCFStringEncodingUTF8) + 1;

    s = malloc((size_t)size);
    if (!s) {
        r = ty_error(TY_ERROR_MEMORY, NULL);
        goto cleanup;
    }

    r = CFStringGetCString(data, s, size, kCFStringEncodingUTF8);
    if (!r) {
        r = 0;
        goto cleanup;
    }

    *rs = s;
    r = 1;
cleanup:
    if (data)
        CFRelease(data);
    return r;
}

static bool get_ioregistry_value_number(io_service_t service, CFStringRef prop, CFNumberType type,
                                        void *rn)
{
    CFTypeRef data;
    bool r;

    data = IORegistryEntryCreateCFProperty(service, prop, kCFAllocatorDefault, 0);
    if (!data || CFGetTypeID(data) != CFNumberGetTypeID()) {
        r = false;
        goto cleanup;
    }

    r = CFNumberGetValue(data, type, rn);
cleanup:
    if (data)
        CFRelease(data);
    return r;
}

static int get_object_interface(io_service_t service, CFUUIDRef uuid, IUnknownVTbl **robject)
{
    IOCFPlugInInterface **plugin = NULL;
    int32_t score;
    IUnknownVTbl *object = NULL;
    kern_return_t kret;
    int r;

    kret = IOCreatePlugInInterfaceForService(service, kIOUSBDeviceUserClientTypeID,
                                             kIOCFPlugInInterfaceID, &plugin,
                                             &score);
    if (kret != kIOReturnSuccess || !plugin) {
        r = 0;
        goto cleanup;
    }

    kret = (*plugin)->QueryInterface(plugin, CFUUIDGetUUIDBytes(uuid), (void **)&object);
    if (kret != kIOReturnSuccess || !object) {
        r = 0;
        goto cleanup;
    }

    *robject = object;
    object = NULL;

    r = 1;
cleanup:
    if (object)
        object->Release(object);
    if (plugin)
        (*plugin)->Release(plugin);
    return r;
}

static void clear_iterator(io_iterator_t it)
{
    io_object_t object;
    while ((object = IOIteratorNext(it)))
        IOObjectRelease(object);
}

struct iokit_device {
    io_service_t service;
    IOUSBDeviceInterface **iface;
};

static int find_serial_device_node(io_service_t service, char **rpath)
{
    io_service_t stream = 0, client = 0;
    char *path;
    kern_return_t kret;
    int r;

    kret = IORegistryEntryGetChildEntry(service, kIOServicePlane, &stream);
    if (kret != kIOReturnSuccess || !IOObjectConformsTo(stream, "IOSerialStreamSync")) {
        ty_error(TY_ERROR_SYSTEM, "Serial device interface does not have IOSerialStreamSync child");
        r = 0;
        goto cleanup;
    }

    kret = IORegistryEntryGetChildEntry(stream, kIOServicePlane, &client);
    if (kret != kIOReturnSuccess || !IOObjectConformsTo(client, "IOSerialBSDClient")) {
        ty_error(TY_ERROR_SYSTEM, "Serial device interface does not have IOSerialBSDClient child");
        r = 0;
        goto cleanup;
    }

    r = get_ioregistry_value_string(client, CFSTR("IOCalloutDevice"), &path);
    if (r <= 0) {
        if (!r)
            ty_error(TY_ERROR_SYSTEM, "Serial device does not have property IOCalloutDevice");
        goto cleanup;
    }

    *rpath = path;
    r = 1;
cleanup:
    if (client)
        IOObjectRelease(client);
    if (stream)
        IOObjectRelease(stream);
    return r;
}

static int find_hid_device_node(io_service_t service, char **rpath)
{
    io_string_t buf;
    char *path;
    kern_return_t kret;

    kret = IORegistryEntryGetPath(service, kIOServicePlane, buf);
    if (kret != kIOReturnSuccess) {
        ty_error(TY_ERROR_SYSTEM, "IORegistryEntryGetPath() failed");
        return 0;
    }

    path = strdup(buf);
    if (!path)
        return ty_error(TY_ERROR_MEMORY, NULL);

    *rpath = path;
    return 1;
}

static int find_device_node(ty_device *dev, io_service_t service)
{
    io_service_t spec_service;
    kern_return_t kret;
    int r;

    kret = IORegistryEntryGetChildEntry(service, kIOServicePlane, &spec_service);
    if (kret != kIOReturnSuccess)
        return 0;

    if (IOObjectConformsTo(spec_service, "IOSerialDriverSync")) {
        dev->type = TY_DEVICE_SERIAL;
        dev->vtable = &_ty_posix_device_vtable;

        r = find_serial_device_node(spec_service, &dev->path);
    } else if (IOObjectConformsTo(spec_service, "IOHIDDevice")) {
        dev->type = TY_DEVICE_HID;
        dev->vtable = &hid_device_vtable;

        r = find_hid_device_node(spec_service, &dev->path);
    } else {
        r = 0;
    }

    IOObjectRelease(spec_service);
    return r;
}

struct usb_controller {
    ty_list_head list;

    uint8_t index;
    uint64_t session;
};

static int build_location_string(uint8_t ports[], unsigned int depth, char **rpath)
{
    char buf[256];
    char *ptr;
    size_t size;
    char *path;
    int r;

    ptr = buf;
    size = sizeof(buf);

    strcpy(buf, "usb");
    ptr += strlen(buf);
    size -= (size_t)(ptr - buf);

    for (unsigned int i = 0; i < depth; i++) {
        r = snprintf(ptr, size, "-%hhu", ports[i]);
        assert(r >= 2 && (size_t)r < size);

        ptr += r;
        size -= (size_t)r;
    }

    path = strdup(buf);
    if (!path)
        return ty_error(TY_ERROR_MEMORY, NULL);

    *rpath = path;
    return 0;
}

static int resolve_device_location(struct iokit_device *iodev, ty_list_head *controllers,
                                   char **rlocation)
{
    uint8_t ports[16];
    unsigned int depth;
    uint64_t session;
    kern_return_t kret;
    int r;

    r = get_ioregistry_value_number(iodev->service, CFSTR("PortNum"), kCFNumberSInt8Type, &ports[0]);
    if (!r) {
        ty_error(TY_ERROR_SYSTEM, "Missing property 'PortNum' for USB device");
        return 0;
    }
    depth = 1;

    IOObjectRetain(iodev->service);

    io_service_t parent = iodev->service;
    while (depth < TY_COUNTOF(ports)) {
        io_service_t tmp = parent;

        kret = IORegistryEntryGetParentEntry(tmp, kIOUSBPlane, &parent);
        IOObjectRelease(tmp);
        if (kret != kIOReturnSuccess) {
            ty_error(TY_ERROR_SYSTEM, "IORegistryEntryGetParentEntry() failed");
            return 0;
        }

        r = get_ioregistry_value_number(parent, CFSTR("PortNum"), kCFNumberSInt8Type, &ports[depth]);
        if (!r)
            break;
        depth++;
    }
    if (depth == TY_COUNTOF(ports)) {
        ty_error(TY_ERROR_SYSTEM, "Excessive USB location depth");
        return 0;
    }

    r = get_ioregistry_value_number(parent, CFSTR("sessionID"), kCFNumberSInt64Type, &session);
    IOObjectRelease(parent);
    if (!r) {
        ty_error(TY_ERROR_SYSTEM, "Missing property 'sessionID' for USB device");
        return 0;
    }

    ty_list_foreach(cur, controllers) {
        struct usb_controller *controller = ty_container_of(cur, struct usb_controller, list);

        if (controller->session == session) {
            ports[depth++] = controller->index;
            break;
        }
    }

    for (unsigned int i = 0; i < depth / 2; i++) {
        uint8_t tmp = ports[i];

        ports[i] = ports[depth - i - 1];
        ports[depth - i - 1] = tmp;
    }

    r = build_location_string(ports, depth, rlocation);
    if (r < 0)
        return r;

    return 1;
}

static int make_device_for_interface(ty_device_monitor *monitor, struct iokit_device *iodev,
                                     io_service_t iface_service)
{
    ty_device *dev;
    uint64_t session;
    int r;

    dev = calloc(1, sizeof(*dev));
    if (!dev) {
        r = ty_error(TY_ERROR_MEMORY, NULL);
        goto cleanup;
    }
    dev->refcount = 1;

    r = get_ioregistry_value_number(iodev->service, CFSTR("sessionID"), kCFNumberSInt64Type, &session);
    if (!r) {
        ty_error(TY_ERROR_SYSTEM, "Missing property 'sessionID' for USB device interface");
        goto cleanup;
    }

    r = get_ioregistry_value_number(iface_service, CFSTR("bInterfaceNumber"), kCFNumberSInt8Type,
                                    &dev->iface);
    if (!r) {
        ty_error(TY_ERROR_SYSTEM, "Missing property 'bInterfaceNumber' for USB device interface");
        goto cleanup;
    }

    (*iodev->iface)->GetDeviceVendor(iodev->iface, &dev->vid);
    (*iodev->iface)->GetDeviceProduct(iodev->iface, &dev->pid);

    r = asprintf(&dev->key, "%"PRIx64, session);
    if (r < 0) {
        r = ty_error(TY_ERROR_MEMORY, NULL);
        goto cleanup;
    }

    r = get_ioregistry_value_string(iodev->service, CFSTR("USB Serial Number"), &dev->serial);
    if (r < 0)
        goto cleanup;

    r = resolve_device_location(iodev, &monitor->controllers, &dev->location);
    if (r <= 0)
        goto cleanup;

    r = find_device_node(dev, iface_service);
    if (r <= 0)
        goto cleanup;

    r = _ty_device_monitor_add(monitor, dev);
cleanup:
    ty_device_unref(dev);
    return r;
}

static int process_darwin_device(ty_device_monitor *monitor, io_service_t device_service)
{
    io_name_t cls;
    struct iokit_device iodev = {0};
    IOUSBFindInterfaceRequest request;
    io_iterator_t interfaces = 0;
    io_service_t iface;
    kern_return_t kret;
    int r;

    IOObjectGetClass(device_service, cls);
    if (strcmp(cls, "IOUSBDevice") != 0)
        return 0;

    iodev.service = device_service;

    r = get_object_interface(device_service, kIOUSBDeviceInterfaceID, (IUnknownVTbl **)&iodev.iface);
    if (r <= 0)
        goto cleanup;

    request.bInterfaceClass    = kIOUSBFindInterfaceDontCare;
    request.bInterfaceSubClass = kIOUSBFindInterfaceDontCare;
    request.bInterfaceProtocol = kIOUSBFindInterfaceDontCare;
    request.bAlternateSetting  = kIOUSBFindInterfaceDontCare;

    kret = (*iodev.iface)->CreateInterfaceIterator(iodev.iface, &request, &interfaces);
    if (kret != kIOReturnSuccess) {
        ty_error(TY_ERROR_SYSTEM, "IOUSBDevice::CreateInterfaceIterator() failed");
        r = 0;
        goto cleanup;
    }

    while ((iface = IOIteratorNext(interfaces))) {
        r = make_device_for_interface(monitor, &iodev, iface);
        if (r < 0)
            goto cleanup;

        IOObjectRelease(iface);
    }

    r = 1;
cleanup:
    if (interfaces) {
        clear_iterator(interfaces);
        IOObjectRelease(interfaces);
    }
    if (iodev.iface)
        (*iodev.iface)->Release(iodev.iface);
    return r;
}

static int list_devices(ty_device_monitor *monitor)
{
    io_service_t service;
    int r;

    while ((service = IOIteratorNext(monitor->attach_it))) {
        r = process_darwin_device(monitor, service);
        if (r < 0)
            goto error;

        IOObjectRelease(service);
    }

    return 0;

error:
    clear_iterator(monitor->attach_it);
    return r;
}

static void darwin_devices_attached(void *ptr, io_iterator_t devices)
{
    // devices == h->attach_t
    TY_UNUSED(devices);

    ty_device_monitor *monitor = ptr;

    int r;

    r = list_devices(monitor);
    if (r < 0)
        monitor->notify_ret = r;
}

static void remove_device(ty_device_monitor *monitor, io_service_t device_service)
{
    uint64_t session;
    char key[16];
    int r;

    r = get_ioregistry_value_number(device_service, CFSTR("sessionID"), kCFNumberSInt64Type, &session);
    if (!r)
        return;

    snprintf(key, sizeof(key), "%"PRIx64, session);
    _ty_device_monitor_remove(monitor, key);
}

static void darwin_devices_detached(void *ptr, io_iterator_t devices)
{
    ty_device_monitor *monitor = ptr;

    io_service_t service;
    while ((service = IOIteratorNext(devices))) {
        remove_device(monitor, service);
        IOObjectRelease(service);
    }
}

static int add_controller(ty_device_monitor *monitor, uint8_t i, io_service_t service)
{
    struct usb_controller *controller;
    int r;

    controller = calloc(1, sizeof(*controller));
    if (!controller) {
        r = ty_error(TY_ERROR_MEMORY, NULL);
        goto error;
    }

    controller->index = i;
    r = get_ioregistry_value_number(service, CFSTR("sessionID"), kCFNumberSInt64Type,
                                    &controller->session);
    if (!r)
        goto error;

    ty_list_add(&monitor->controllers, &controller->list);

    return 0;

error:
    free(controller);
    return r;
}

static int list_controllers(ty_device_monitor *monitor)
{
    io_iterator_t controllers = 0;
    io_service_t service;
    kern_return_t kret;
    int r;

    kret = IOServiceGetMatchingServices(kIOMasterPortDefault, IOServiceMatching("IOUSBRootHubDevice"),
                                        &controllers);
    if (kret != kIOReturnSuccess) {
        r = ty_error(TY_ERROR_SYSTEM, "IOServiceGetMatchingServices() failed");
        goto cleanup;
    }

    uint8_t i = 0;
    while ((service = IOIteratorNext(controllers))) {
        r = add_controller(monitor, ++i, service);
        if (r < 0)
            goto cleanup;
        IOObjectRelease(service);
    }

    r = 0;
cleanup:
    if (controllers) {
        clear_iterator(controllers);
        IOObjectRelease(controllers);
    }
    return r;
}

int ty_device_monitor_new(ty_device_monitor **rmonitor)
{
    assert(rmonitor);

    ty_device_monitor *monitor;
    struct kevent kev;
    const struct timespec ts = {0};
    kern_return_t kret;
    int r;

    monitor = calloc(1, sizeof(*monitor));
    if (!monitor) {
        r = ty_error(TY_ERROR_MEMORY, NULL);
        goto error;
    }
    monitor->kqfd = -1;

    ty_list_init(&monitor->controllers);

    monitor->notify_port = IONotificationPortCreate(kIOMasterPortDefault);
    if (!monitor->notify_port) {
        r = ty_error(TY_ERROR_SYSTEM, "IONotificationPortCreate() failed");
        goto error;
    }

    kret = IOServiceAddMatchingNotification(monitor->notify_port, kIOFirstMatchNotification,
                                            IOServiceMatching(kIOUSBDeviceClassName),
                                            darwin_devices_attached,
                                            monitor, &monitor->attach_it);
    if  (kret != kIOReturnSuccess) {
        r = ty_error(TY_ERROR_SYSTEM, "IOServiceAddMatchingNotification() failed");
        goto error;
    }

    kret = IOServiceAddMatchingNotification(monitor->notify_port, kIOTerminatedNotification,
                                            IOServiceMatching(kIOUSBDeviceClassName),
                                            darwin_devices_detached,
                                            monitor, &monitor->detach_it);
    if  (kret != kIOReturnSuccess) {
        r = ty_error(TY_ERROR_SYSTEM, "IOServiceAddMatchingNotification() failed");
        goto error;
    }

    monitor->kqfd = kqueue();
    if (monitor->kqfd < 0) {
        r = ty_error(TY_ERROR_SYSTEM, "kqueue() failed: %s", strerror(errno));
        goto error;
    }

    kret = mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_PORT_SET, &monitor->port_set);
    if (kret != KERN_SUCCESS) {
        r = ty_error(TY_ERROR_SYSTEM, "mach_port_allocate() failed");
        goto error;
    }

    kret = mach_port_insert_member(mach_task_self(), IONotificationPortGetMachPort(monitor->notify_port),
                                   monitor->port_set);
    if (kret != KERN_SUCCESS) {
        r = ty_error(TY_ERROR_SYSTEM, "mach_port_insert_member() failed");
        goto error;
    }

    EV_SET(&kev, monitor->port_set, EVFILT_MACHPORT, EV_ADD, 0, 0, NULL);

    r = kevent(monitor->kqfd, &kev, 1, NULL, 0, &ts);
    if (r < 0) {
        r = ty_error(TY_ERROR_SYSTEM, "kevent() failed: %d", errno);
        goto error;
    }

    r = _ty_device_monitor_init(monitor);
    if (r < 0)
        goto error;

    r = list_controllers(monitor);
    if (r < 0)
        goto error;

    r = list_devices(monitor);
    if (r < 0)
        goto error;
    clear_iterator(monitor->detach_it);

    *rmonitor = monitor;
    return 0;

error:
    ty_device_monitor_free(monitor);
    return r;
}

void ty_device_monitor_free(ty_device_monitor *monitor)
{
    if (monitor) {
        _ty_device_monitor_release(monitor);

        ty_list_foreach(cur, &monitor->controllers) {
            struct usb_controller *controller = ty_container_of(cur, struct usb_controller, list);
            free(controller);
        }

        close(monitor->kqfd);
        if (monitor->port_set)
            mach_port_deallocate(mach_task_self(), monitor->port_set);

        // I don't know how these functions are supposed to treat NULL
        if (monitor->attach_it)
            IOObjectRelease(monitor->attach_it);
        if (monitor->detach_it)
            IOObjectRelease(monitor->detach_it);
        if (monitor->notify_port)
            IONotificationPortDestroy(monitor->notify_port);
    }

    free(monitor);
}

void ty_device_monitor_get_descriptors(const ty_device_monitor *monitor, struct ty_descriptor_set *set,
                                       int id)
{
    assert(monitor);
    assert(set);

    ty_descriptor_set_add(set, monitor->kqfd, id);
}

int ty_device_monitor_refresh(ty_device_monitor *monitor)
{
    assert(monitor);

    struct kevent kev;
    const struct timespec ts = {0};
    int r;

    r = kevent(monitor->kqfd, NULL, 0, &kev, 1, &ts);
    if (r < 0)
        return ty_error(TY_ERROR_SYSTEM, "kevent() failed: %s", strerror(errno));
    if (!r)
        return 0;
    assert(kev.filter == EVFILT_MACHPORT);

    r = 0;
    while (true) {
        struct {
            mach_msg_header_t header;
            uint8_t body[128];
        } msg;
        mach_msg_return_t mret;

        mret = mach_msg(&msg.header, MACH_RCV_MSG | MACH_RCV_TIMEOUT, 0, sizeof(msg),
                        monitor->port_set, 0, MACH_PORT_NULL);
        if (mret != MACH_MSG_SUCCESS) {
            if (mret == MACH_RCV_TIMED_OUT)
                break;

            r = ty_error(TY_ERROR_SYSTEM, "mach_msg() failed");
            break;
        }

        IODispatchCalloutFromMessage(NULL, &msg.header, monitor->notify_port);

        if (monitor->notify_ret < 0) {
            r = monitor->notify_ret;
            monitor->notify_ret = 0;

            break;
        }
    }

    return r;
}

static void fire_device_event(ty_handle *h)
{
    char buf = '.';
    write(h->pipe[1], &buf, 1);
}

static void reset_device_event(ty_handle *h)
{
    char buf;
    read(h->pipe[0], &buf, 1);
}

static void hid_removal_callback(void *ctx, IOReturn result, void *sender)
{
    TY_UNUSED(result);
    TY_UNUSED(sender);

    ty_handle *h = ctx;

    pthread_mutex_lock(&h->mutex);

    CFRelease(h->hid);
    h->hid = NULL;

    CFRunLoopSourceSignal(h->shutdown);
    h->loop = NULL;

    pthread_mutex_unlock(&h->mutex);

    fire_device_event(h);
}

struct hid_report {
    ty_list_head list;

    size_t size;
    uint8_t data[];
};

static void hid_report_callback(void *ctx, IOReturn result, void *sender,
                                IOHIDReportType report_type, uint32_t report_id,
                                uint8_t *report_data, CFIndex report_size)
{
    TY_UNUSED(result);
    TY_UNUSED(sender);

    if (report_type != kIOHIDReportTypeInput)
        return;

    ty_handle *h = ctx;

    struct hid_report *report;
    bool fire;
    int r;

    pthread_mutex_lock(&h->mutex);

    fire = ty_list_is_empty(&h->reports);

    report = ty_list_get_first(&h->free_reports, struct hid_report, list);
    if (!report) {
        if (h->allocated_reports < 64) {
            // Don't forget the potential leading report ID
            report = calloc(1, sizeof(struct hid_report) + h->size + 1);
            if (!report) {
                r = ty_error(TY_ERROR_MEMORY, NULL);
                goto cleanup;
            }
            h->allocated_reports++;
        } else {
            // Drop oldest report, too bad for the user
            report = ty_list_get_first(&h->reports, struct hid_report, list);
        }
    }
    if (report->list.prev)
        ty_list_remove(&report->list);

    // You never know, even if h->size is supposed to be the maximum input report size
    if (report_size > (CFIndex)h->size)
        report_size = (CFIndex)h->size;

    if (report_id) {
        report->data[0] = (uint8_t)report_id;
        memcpy(report->data + 1, report_data, report_size);

        report->size = (size_t)report_size + 1;
    } else {
        memcpy(report->data, report_data, report_size);

        report->size = (size_t)report_size;
    }

    ty_list_add_tail(&h->reports, &report->list);

    r = 0;
cleanup:
    if (r < 0)
        h->thread_ret = r;
    pthread_mutex_unlock(&h->mutex);
    if (fire)
        fire_device_event(h);
}

static void *device_thread(void *ptr)
{
    ty_handle *h = ptr;

    CFRunLoopSourceContext shutdown_ctx = {0};
    int r;

    pthread_mutex_lock(&h->mutex);

    h->loop = CFRunLoopGetCurrent();

    shutdown_ctx.info = h->loop;
    shutdown_ctx.perform = (void (*)(void *))CFRunLoopStop;

    /* close_hid_device() could be called before the loop is running, while this thread is between
       pthread_barrier_wait() and CFRunLoopRun(). That's the purpose of the shutdown source. */
    h->shutdown = CFRunLoopSourceCreate(kCFAllocatorDefault, 0, &shutdown_ctx);
    if (!h->shutdown) {
        r = ty_error(TY_ERROR_SYSTEM, "CFRunLoopSourceCreate() failed");
        goto error;
    }

    CFRunLoopAddSource(h->loop, h->shutdown, kCFRunLoopCommonModes);
    IOHIDDeviceScheduleWithRunLoop(h->hid, h->loop, kCFRunLoopCommonModes);

    // This thread is ready, open_hid_device() can carry on
    h->thread_ret = 1;
    pthread_cond_signal(&h->cond);
    pthread_mutex_unlock(&h->mutex);

    CFRunLoopRun();

    if (h->hid)
        IOHIDDeviceUnscheduleFromRunLoop(h->hid, h->loop, kCFRunLoopCommonModes);

    pthread_mutex_lock(&h->mutex);
    h->loop = NULL;
    pthread_mutex_unlock(&h->mutex);

    return NULL;

error:
    h->thread_ret = r;
    pthread_cond_signal(&h->cond);
    pthread_mutex_unlock(&h->mutex);
    return NULL;
}

static bool get_hid_device_property_number(IOHIDDeviceRef dev, CFStringRef prop, CFNumberType type,
                                           void *rn)
{
    CFTypeRef data = IOHIDDeviceGetProperty(dev, prop);
    if (!data || CFGetTypeID(data) != CFNumberGetTypeID())
        return false;

    return CFNumberGetValue(data, type, rn);
}

static int open_hid_device(ty_device *dev, ty_handle **rh)
{
    ty_handle *h;
    kern_return_t kret;
    int r;

    h = calloc(1, sizeof(*h));
    if (!h) {
        r = ty_error(TY_ERROR_MEMORY, NULL);
        goto error;
    }
    h->dev = ty_device_ref(dev);

    h->pipe[0] = -1;
    h->pipe[1] = -1;

    h->service = IORegistryEntryFromPath(kIOMasterPortDefault, dev->path);
    if (!h->service) {
        r = ty_error(TY_ERROR_NOT_FOUND, "Device '%s' not found", dev->path);
        goto error;
    }

    h->hid = IOHIDDeviceCreate(kCFAllocatorDefault, h->service);
    if (!h->hid) {
        r = ty_error(TY_ERROR_NOT_FOUND, "Device '%s' not found", dev->path);
        goto error;
    }

    kret = IOHIDDeviceOpen(h->hid, 0);
    if (kret != kIOReturnSuccess) {
        r = ty_error(TY_ERROR_SYSTEM, "Failed to open HID device '%s'", dev->path);
        goto error;
    }

    r = get_hid_device_property_number(h->hid, CFSTR(kIOHIDMaxInputReportSizeKey), kCFNumberSInt32Type,
                                       &h->size);
    if (!r) {
        r = ty_error(TY_ERROR_SYSTEM, "HID device '%s' has no valid report size key", dev->path);
        goto error;
    }
    h->buf = malloc(h->size);
    if (!h->buf) {
        r = ty_error(TY_ERROR_MEMORY, NULL);
        goto error;
    }

    IOHIDDeviceRegisterRemovalCallback(h->hid, hid_removal_callback, h);
    IOHIDDeviceRegisterInputReportCallback(h->hid, h->buf, (CFIndex)h->size, hid_report_callback, h);

    r = pipe(h->pipe);
    if (r < 0) {
        r = ty_error(TY_ERROR_SYSTEM, "pipe() failed: %s", strerror(errno));
        goto error;
    }
    fcntl(h->pipe[0], F_SETFL, fcntl(h->pipe[0], F_GETFL, 0) | O_NONBLOCK);
    fcntl(h->pipe[1], F_SETFL, fcntl(h->pipe[1], F_GETFL, 0) | O_NONBLOCK);

    ty_list_init(&h->reports);
    ty_list_init(&h->free_reports);

    r = pthread_mutex_init(&h->mutex, NULL);
    if (r) {
        r = ty_error(TY_ERROR_SYSTEM, "pthread_mutex_init() failed: %s", strerror(r));
        goto error;
    }
    h->mutex_init = true;

    r = pthread_cond_init(&h->cond, NULL);
    if (r) {
        r = ty_error(TY_ERROR_SYSTEM, "pthread_cond_init() failed: %s", strerror(r));
        goto error;
    }
    h->cond_init = true;

    pthread_mutex_lock(&h->mutex);

    r = pthread_create(&h->thread, NULL, device_thread, h);
    if (r) {
        r = ty_error(TY_ERROR_SYSTEM, "pthread_create() failed: %s", strerror(r));
        goto error;
    }

    /* Barriers are great for this, but OSX doesn't have those... And since it's the only place
       we would use them, it's probably not worth it to have a custom implementation. */
    while (!h->thread_ret)
        pthread_cond_wait(&h->cond, &h->mutex);
    r = h->thread_ret;
    h->thread_ret = 0;
    pthread_mutex_unlock(&h->mutex);
    if (r < 0)
        goto error;

    *rh = h;
    return 0;

error:
    ty_device_close(h);
    return r;
}

static void close_hid_device(ty_handle *h)
{
    if (h) {
        if (h->shutdown) {
            pthread_mutex_lock(&h->mutex);

            if (h->loop) {
                CFRunLoopSourceSignal(h->shutdown);
                CFRunLoopWakeUp(h->loop);
            }

            pthread_mutex_unlock(&h->mutex);
            pthread_join(h->thread, NULL);

            CFRelease(h->shutdown);
        }

        if (h->cond_init)
            pthread_cond_destroy(&h->cond);
        if (h->mutex_init)
            pthread_mutex_destroy(&h->mutex);

        ty_list_splice(&h->free_reports, &h->reports);
        ty_list_foreach(cur, &h->free_reports) {
            struct hid_report *report = ty_container_of(cur, struct hid_report, list);
            free(report);
        }

        close(h->pipe[0]);
        close(h->pipe[1]);

        free(h->buf);

        if (h->hid) {
            IOHIDDeviceClose(h->hid, 0);
            CFRelease(h->hid);
        }
        if (h->service)
            IOObjectRelease(h->service);

        ty_device_unref(h->dev);
    }

    free(h);
}

static void get_hid_descriptors(const ty_handle *h, ty_descriptor_set *set, int id)
{
    ty_descriptor_set_add(set, h->pipe[0], id);
}

static const struct _ty_device_vtable hid_device_vtable = {
    .open = open_hid_device,
    .close = close_hid_device,

    .get_descriptors = get_hid_descriptors
};

int ty_hid_parse_descriptor(ty_handle *h, ty_hid_descriptor *desc)
{
    if (!h->hid)
        return ty_error(TY_ERROR_IO, "Device '%s' was removed", h->dev->path);

    memset(desc, 0, sizeof(*desc));

    get_hid_device_property_number(h->hid, CFSTR(kIOHIDPrimaryUsagePageKey), kCFNumberSInt16Type,
                                   &desc->usage_page);
    get_hid_device_property_number(h->hid, CFSTR(kIOHIDPrimaryUsageKey), kCFNumberSInt16Type,
                                   &desc->usage);

    return 0;
}

ssize_t ty_hid_read(ty_handle *h, uint8_t *buf, size_t size, int timeout)
{
    assert(h);
    assert(h->dev->type == TY_DEVICE_HID);
    assert(buf);
    assert(size);

    fd_set fds;
    struct hid_report *report;
    ssize_t r;

    if (!h->hid)
        return ty_error(TY_ERROR_IO, "Device '%s' was removed", h->dev->path);

    FD_ZERO(&fds);
    FD_SET(h->pipe[0], &fds);

    if (timeout >= 0) {
        uint64_t start;
        int adjusted_timeout;
        struct timeval tv;

        start = ty_millis();
restart:
        adjusted_timeout = ty_adjust_timeout(timeout, start);
        tv.tv_sec = adjusted_timeout / 1000;
        tv.tv_usec = (adjusted_timeout % 1000) * 1000;

        r = select(h->pipe[0] + 1, &fds, NULL, NULL, &tv);
    } else {
        r = select(h->pipe[0] + 1, &fds, NULL, NULL, NULL);
    }
    if (r < 0) {
        if (errno == EINTR)
            goto restart;

        return ty_error(TY_ERROR_SYSTEM, "select() failed: %s", strerror(errno));
    }
    if (!r)
        return 0;

    pthread_mutex_lock(&h->mutex);

    if (h->thread_ret < 0) {
        r = h->thread_ret;
        h->thread_ret = 0;

        goto cleanup;
    }

    report = ty_list_get_first(&h->reports, struct hid_report, list);
    assert(report);

    if (size > report->size)
        size = report->size;

    memcpy(buf, report->data, size);

    ty_list_remove(&report->list);
    ty_list_add(&h->free_reports, &report->list);

    r = (ssize_t)size;
cleanup:
    if (ty_list_is_empty(&h->reports))
        reset_device_event(h);
    pthread_mutex_unlock(&h->mutex);
    return r;
}

static ssize_t send_report(ty_handle *h, IOHIDReportType type, const uint8_t *buf, size_t size)
{
    uint8_t report;
    kern_return_t kret;

    if (!h->hid)
        return ty_error(TY_ERROR_IO, "Device '%s' was removed", h->dev->path);

    if (size < 2)
        return 0;

    report = buf[0];
    if (!report) {
        buf++;
        size--;
    }

    // FIXME: detect various errors, here and elsewhere for common kIOReturn values
    kret = IOHIDDeviceSetReport(h->hid, type, report, buf, (CFIndex)size);
    if (kret != kIOReturnSuccess)
        return ty_error(TY_ERROR_SYSTEM, "IOHIDDeviceSetReport() failed");

    return (ssize_t)size + !report;
}

ssize_t ty_hid_write(ty_handle *h, const uint8_t *buf, size_t size)
{
    assert(h);
    assert(h->dev->type == TY_DEVICE_HID);
    assert(buf);

    return send_report(h, kIOHIDReportTypeOutput, buf, size);
}

ssize_t ty_hid_send_feature_report(ty_handle *h, const uint8_t *buf, size_t size)
{
    assert(h);
    assert(h->dev->type == TY_DEVICE_HID);
    assert(buf);

    return send_report(h, kIOHIDReportTypeFeature, buf, size);
}
