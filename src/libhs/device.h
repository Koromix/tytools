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

#ifndef HS_DEVICE_H
#define HS_DEVICE_H

#include "common.h"

HS_BEGIN_C

/**
 * @defgroup device Device handling
 * @brief Access device information and open device handles.
 */

struct hs_monitor;

typedef struct hs_device hs_device;
typedef struct hs_port hs_port;

/**
 * @ingroup device
 * @brief Current device status.
 *
 * The device status can only change when hs_monitor_refresh() is called.
 *
 * @sa hs_device_get_status()
 */
typedef enum hs_device_status {
    /** Device is connected and ready. */
    HS_DEVICE_STATUS_ONLINE = 1,
    /** Device has been disconnected. */
    HS_DEVICE_STATUS_DISCONNECTED
} hs_device_status;

/**
 * @ingroup device
 * @brief Device type.
 *
 * @sa hs_device_get_type()
 */
typedef enum hs_device_type {
    /** HID device. */
    HS_DEVICE_TYPE_HID = 1,
    /** Serial device. */
    HS_DEVICE_TYPE_SERIAL
} hs_device_type;

/**
 * @ingroup device
 * @brief Device open mode.
 *
 * @sa hs_port_open()
 */
typedef enum hs_port_mode {
    HS_PORT_MODE_READ  = 1,
    HS_PORT_MODE_WRITE = 2,
    HS_PORT_MODE_RW    = 3
} hs_port_mode;

/**
 * @{
 * @name Resource management
 */

/**
 * @ingroup device
 * @brief Increase the device reference count.
 *
 * This function is thread-safe.
 *
 * @param dev Device object.
 * @return This function returns the device object, for convenience.
 */
HS_PUBLIC hs_device *hs_device_ref(hs_device *dev);
/**
 * @ingroup device
 * @brief Decrease the device reference count.
 *
 * When the reference count reaches 0, the device object is freed. This function is thread-safe.
 *
 * @param dev Device object.
 */
HS_PUBLIC void hs_device_unref(hs_device *dev);

/**
  * @{
  * @name Device information
  */

/**
 * @ingroup device
 * @brief Get the current device status.
 *
 * @param dev Device object.
 * @return This function returns the current device status.
 *
 * @sa hs_device_status
 */
HS_PUBLIC hs_device_status hs_device_get_status(const hs_device *dev);
/**
 * @ingroup device
 * @brief Get the device type.
 *
 * @param dev Device object.
 * @return This function returns the device type.
 *
 * @sa hs_device_type
 */
HS_PUBLIC hs_device_type hs_device_get_type(const hs_device *dev);
/**
 * @ingroup device
 * @brief Get the device location.
 *
 * The location is bus-specific:
 * - **USB** = "usb-<root_hub_id>[-<port_id>]+" (e.g. "usb-2-5-4")
 *
 * @param dev Device object.
 * @return This function returns the cached location, you must not change or free the it.
 */
HS_PUBLIC const char *hs_device_get_location(const hs_device *dev);
/**
 * @ingroup device
 * @brief Get the device interface number.
 *
 * @param dev Device object.
 * @return This function returns the device interface number.
 */
HS_PUBLIC uint8_t hs_device_get_interface_number(const hs_device *dev);
/**
 * @ingroup device
 * @brief Get the device node path.
 *
 * This may not always be a real filesystem path, for example on OS X HID devices cannot be used
 * through a device node.
 *
 * @param dev Device object.
 * @return This function returns the cached node path, you must not change or free the it.
 */
HS_PUBLIC const char *hs_device_get_path(const hs_device *dev);
/**
 * @ingroup device
 * @brief Get the device vendor identifier.
 *
 * @param dev Device object.
 * @return This function returns the vendor ID.
 */
HS_PUBLIC uint16_t hs_device_get_vid(const hs_device *dev);
/**
 * @ingroup device
 * @brief Get the device product identifier.
 *
 * @param dev Device object.
 * @return This function returns the product ID.
 */
HS_PUBLIC uint16_t hs_device_get_pid(const hs_device *dev);
/**
 * @ingroup device
 * @brief Get the device manufacturer string.
 *
 * This string is internal to the device object, you must not change or free it. NULL means
 * the device did not report a manufacturer string.
 *
 * @param dev Device object.
 * @return This function returns the manufacturer string, or NULL if the device did not report one.
 */
HS_PUBLIC const char *hs_device_get_manufacturer_string(const hs_device *dev);
/**
 * @ingroup device
 * @brief Get the device product string.
 *
 * This string is internal to the device object, you must not change or free it. NULL means
 * the device did not report a product string.
 *
 * @param dev Device object.
 * @return This function returns the product string, or NULL if the device did not report one.
 */
HS_PUBLIC const char *hs_device_get_product_string(const hs_device *dev);
/**
 * @ingroup device
 * @brief Get the device serial number string.
 *
 * This string is internal to the device object, you must not change or free it. NULL means
 * the device did not report a serial number.
 *
 * @param dev Device object.
 * @return This function returns the serial number string, or NULL if the device did not report one.
 */
HS_PUBLIC const char *hs_device_get_serial_number_string(const hs_device *dev);

/**
 * @ingroup device
 * @brief Get the primary usage page value from the HID report descriptor.
 *
 * It is invalid to call this function for non-HID devices, and will result in an assert
 * on debug builds.
 *
 * @param dev Device object.
 * @return This function returns the primary HID usage page value.
 */
HS_PUBLIC uint16_t hs_device_get_hid_usage_page(const hs_device *dev);
/**
 * @ingroup device
 * @brief Get the primary usage value from the HID report descriptor.
 *
 * It is invalid to call this function for non-HID devices, and will result in an assert
 * on debug builds.
 *
 * @param dev Device object.
 * @return This function returns the primary HID usage value.
 */
HS_PUBLIC uint16_t hs_device_get_hid_usage(const hs_device *dev);

/**
  * @{
  * @name Handle Functions
  */

/**
 * @ingroup device
 * @brief Open a device.
 *
 * The handle object keeps a refcounted reference to the device object, you are free to drop
 * your own reference.
 *
 * @param      dev   Device object to open.
 * @param      mode  Open device for read / write or both.
 * @param[out] rport Device handle, the value is changed only if the function succeeds.
 * @return This function returns 0 on success, or a negative @ref hs_error_code value.
 */
HS_PUBLIC int hs_port_open(hs_device *dev, hs_port_mode mode, hs_port **rport);
/**
 * @ingroup device
 * @brief Close a device, and free all used resources.
 *
 * @param port Device handle.
 */
HS_PUBLIC void hs_port_close(hs_port *port);

/**
 * @ingroup device
 * @brief Get the device object from which this handle was opened.
 *
 * @param port Device handle.
 * @return Device object.
 */
HS_PUBLIC hs_device *hs_port_get_device(const hs_port *port);
/**
 * @ingroup device
 * @brief Get a pollable device handle.
 *
 * @ref hs_handle is a typedef to the platform descriptor type: int on POSIX platforms,
 * HANDLE on Windows.
 *
 * You can use this descriptor with select()/poll() on POSIX platforms and the Wait
 * (e.g. WaitForSingleObject()) functions on Windows to know when the device input buffer contains
 * data.
 *
 * Note that this descriptor may not be the real device descriptor on some platforms. For
 * HID devices on OSX, this is actually a pipe that gets signalled when IOHIDDevice gives
 * libhs a report on the background thread.
 *
 * @param port Device handle.
 * @return This function returns a pollable handle.
 *
 * @sa hs_handle
 */
HS_PUBLIC hs_handle hs_port_get_poll_handle(const hs_port *port);

HS_END_C

#endif
