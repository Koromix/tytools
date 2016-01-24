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

#ifndef HS_MONITOR_H
#define HS_MONITOR_H

#include "common.h"

HS_BEGIN_C

/**
 * @defgroup monitor Device discovery
 * @brief Discover devices and react when devices are added and removed.
 */

struct hs_device;
struct hs_match;

/**
 * @ingroup monitor
 * @brief Opaque structure representing a device monitor.
 */
typedef struct hs_monitor hs_monitor;

/**
 * @ingroup monitor
 * @brief Device enumeration and event callback.
 *
 * When refreshing, use hs_device_get_status() to distinguish between added and removed events.
 *
 * You must return 0 to continue the enumeration or event processing. Non-zero values stop the
 * process and are returned from the enumeration/refresh function. You should probably use
 * negative values for errors (@ref hs_error_code) and positive values otherwise, but this is not
 * enforced.
 *
 * @param dev   Device object.
 * @param udata Pointer to user-defined arbitrary data.
 * @return Return 0 to continue the enumeration/event processing, or any other value to abort.
 *     The enumeration/refresh function will then return this value unchanged.
 *
 * @sa hs_enumerate() for simple device enumeration.
 * @sa hs_monitor_refresh() for hotplug support.
 */
typedef int hs_enumerate_func(struct hs_device *dev, void *udata);

/**
 * @{
 * @name Enumeration Functions
 */

/**
 * @ingroup monitor
 * @brief Enumerate current devices.
 *
 * If you need to support hotplugging you should probably use a monitor instead.
 *
 * @param matches Array of device matches, or NULL to enumerate all supported devices.
 * @param count   Number of elements in @p matches.
 * @param f       Callback called for each enumerated device.
 * @param udata   Pointer to user-defined arbitrary data for the callback.
 * @return This function returns 0 on success, or a negative @ref hs_error_code value. If the
 *     callback returns a non-zero value, the enumeration is interrupted and the value is returned.
 *
 * @sa hs_match Match specific devices.
 * @sa hs_enumerate_func() for more information about the callback.
 */
HS_PUBLIC int hs_enumerate(const struct hs_match *matches, unsigned int count,
                           hs_enumerate_func *f, void *udata);

/**
 * @ingroup monitor
 * @brief Find the first matching device.
 *
 * Don't forget to call hs_device_unref() when you don't need the device object anymore.
 *
 * @param      matches Array of device matches, or NULL to enumerate all supported devices.
 * @param      count   Number of elements in @p matches.
 * @param[out] rdev    Device object, the value is changed only if a device is found.
 * @return This function returns 1 if a device is found, 0 if not or a negative @ref hs_error_code
 *     value.
 */
HS_PUBLIC int hs_find(const struct hs_match *matches, unsigned int count, struct hs_device **rdev);

/**
 * @{
 * @name Monitoring Functions
 */

/**
 * @ingroup monitor
 * @brief Open a new device monitor.
 *
 * @param      matches  Array of device matches, or NULL to enumerate all supported devices.
 *     This array is not copied and must remain valid until hs_monitor_free().
 * @param      count    Number of elements in @p matches.
 * @param[out] rmonitor A pointer to the variable that receives the device monitor, it will stay
 *     unchanged if the function fails.
 * @return This function returns 0 on success, or a negative @ref hs_error_code value.
 *
 * @sa hs_monitor_free()
 */
HS_PUBLIC int hs_monitor_new(const struct hs_match *matches, unsigned int count,
                             hs_monitor **rmonitor);
/**
 * @ingroup monitor
 * @brief Close a device monitor.
 *
 * You should not keep any device object or handles beyond this call. In practice, call this at
 * the end of your program.
 *
 * @param monitor Device monitor.
 *
 * @sa hs_monitor_new()
 */
HS_PUBLIC void hs_monitor_free(hs_monitor *monitor);

/**
 * @ingroup monitor
 * @brief Get a pollable descriptor for device monitor events.
 *
 * @ref hs_descriptor is a typedef to the platform descriptor type: int on POSIX platforms,
 * HANDLE on Windows.
 *
 * You can use this descriptor with select()/poll() on POSIX platforms and the Wait
 * (e.g. WaitForSingleObject()) functions on Windows to know when there are pending device events.
 * Cross-platform facilities are provided to ease this, see @ref hs_poll.
 *
 * Call hs_monitor_refresh() to process events.
 *
 * @param monitor Device monitor.
 * @return This function returns a pollable descriptor, call hs_monitor_refresh() when it
 *     becomes ready.
 *
 * @sa hs_descriptor
 * @sa hs_monitor_refresh()
 */
HS_PUBLIC hs_descriptor hs_monitor_get_descriptor(const hs_monitor *monitor);

/**
 * @ingroup monitor
 * @brief Store a custom pointer in this monitor.
 *
 * @sa hs_monitor_get_userdata() Retrieve this pointer.
 */
HS_PUBLIC void hs_monitor_set_userdata(hs_monitor *monitor, void *udata);
/**
 * @ingroup monitor
 * @brief Retrieve the pointer stored by hs_monitor_set_userdata().
 */
HS_PUBLIC void *hs_monitor_get_userdata(const hs_monitor *monitor);

/**
 * @ingroup monitor
 * @brief Start listening to OS notifications.
 *
 * This function lists current devices and connects to the OS device manager for device change
 * notifications.
 *
 * You can use hs_monitor_get_descriptor() to get a pollable descriptor (int on POSIX, HANDLE
 * on Windows). This descriptors becomes ready (POLLIN) when there are notifications, you can then
 * call hs_monitor_refresh() to process them.
 *
 * @param monitor Device monitor.
 * @return This function returns 0 on success, or a negative @ref hs_error_code value.
 */
HS_PUBLIC int hs_monitor_start(hs_monitor *monitor);
/**
 * @ingroup monitor
 * @brief Stop listening to OS notifications.
 *
 * @param monitor Device monitor.
 */
HS_PUBLIC void hs_monitor_stop(hs_monitor *monitor);

/**
 * @ingroup monitor
 * @brief Refresh the device list and fire device change events.
 *
 * Process all the pending device change events to refresh the device list and call the
 * callback for each event.
 *
 * This function is non-blocking.
 *
 * @param monitor Device monitor.
 * @param f       Callback to process each device event, or NULL.
 * @param udata   Pointer to user-defined arbitrary data for the callback.
 * @return This function returns 0 on success, or a negative @ref hs_error_code value. If the
 *     callback returns a non-zero value, the refresh is interrupted and the value is returned.
 *
 * @sa hs_enumerate_func() for more information about the callback.
 */
HS_PUBLIC int hs_monitor_refresh(hs_monitor *monitor, hs_enumerate_func *f, void *udata);

/**
 * @ingroup monitor
 * @brief List the currently known devices.
 *
 * The device list is refreshed when the monitor is started, and when hs_monitor_refresh() is
 * called. This function simply uses the monitor's internal device list.
 *
 * @param monitor Device monitor.
 * @param f       Device enumeration callback.
 * @param udata   Pointer to user-defined arbitrary data for the callback.
 * @return This function returns 0 on success, or a negative @ref hs_error_code value. If the
 *     callback returns a non-zero value, the listing is interrupted and the value is returned.
 *
 * @sa hs_monitor_refresh()
 * @sa hs_enumerate_func() for more information about the callback.
 */
HS_PUBLIC int hs_monitor_list(hs_monitor *monitor, hs_enumerate_func *f, void *udata);

HS_END_C

#endif
