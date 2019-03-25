/* libhs - public domain
   Niels Martignène <niels.martignene@protonmail.com>
   https://koromix.dev/libhs

   This software is in the public domain. Where that dedication is not
   recognized, you are granted a perpetual, irrevocable license to copy,
   distribute, and modify this file as you see fit.

   See the LICENSE file for more details. */

#ifndef HS_DEVICE_H
#define HS_DEVICE_H

#include "common.h"
#include "htable.h"

HS_BEGIN_C

/**
 * @defgroup device Device handling
 * @brief Access device information and open device handles.
 */

/**
 * @ingroup device
 * @brief Current device status.
 *
 * The device status can only change when hs_monitor_refresh() is called.
 *
 * @sa hs_device
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
 * @sa hs_device
 * @sa hs_device_type_strings
 */
typedef enum hs_device_type {
    /** HID device. */
    HS_DEVICE_TYPE_HID = 1,
    /** Serial device. */
    HS_DEVICE_TYPE_SERIAL
} hs_device_type;

/**
 * @ingroup device
 * @brief Device type strings
 *
 * Use hs_device_type_strings[dev->type] to get a string representation:
 * - HS_DEVICE_TYPE_HID = "hid"
 * - HS_DEVICE_TYPE_SERIAL = "serial"
 *
 * @sa hs_device_type
 */
static const char *const hs_device_type_strings[] = {
    NULL,
    "hid",
    "serial"
};

/**
 * @ingroup device
 * @brief The hs_device struct
 *
 * Members omitted from the list below are reserved for internal use.
 */
struct hs_device {
    /** @cond */
    unsigned int refcount;
    _hs_htable_head hnode;
    char *key;
    /** @endcond */

    /** Device type, see @ref hs_device_type. */
    hs_device_type type;
    /** Current device status, see @ref hs_device_status. */
    hs_device_status status;
    /**
     * @brief Device location.
     *
     * The location is bus-specific:
     * - **USB** = "usb-<root_hub_id>[-<port_id>]+" (e.g. "usb-2-5-4")
     */
    char *location;
    /**
     * @brief Get the device node path.
     *
     * This may not always be a real filesystem path, for example on OS X HID devices cannot be
     * used through a device node.
     */
    char *path;
    /** Device vendor identifier. */
    uint16_t vid;
    /** Device product identifier. */
    uint16_t pid;
    /** Device bcd. */
    uint16_t bcd_device;
    /** Device manufacturer string, or NULL if not available. */
    char *manufacturer_string;
    /** Device product string, or NULL if not available. */
    char *product_string;
    /** Device serial number string, or NULL if not available. */
    char *serial_number_string;
    /** Device interface number. */
    uint8_t iface_number;

    /** Match pointer, copied from udata in @ref hs_match_spec. */
    void *match_udata;

    /** Contains type-specific information, see below. */
    union {
        /** Only valid when type == HS_DEVICE_TYPE_HID. */
        struct {
            /** Primary usage page value read from the HID report descriptor. */
            uint16_t usage_page;
            /** Primary usage value read from the HID report descriptor. */
            uint16_t usage;

#if defined(WIN32)
            /** @cond */
            size_t input_report_len;
            /** @endcond */
#elif defined(__linux__)
            /** @cond */
            // Needed to work around a bug on old Linux kernels
            bool numbered_reports;
            /** @endcond */
#endif
        } hid;
    } u;
};

/**
 * @ingroup device
 * @brief Device open mode.
 *
 * @sa hs_port_open()
 */
typedef enum hs_port_mode {
    /** Open device for reading. */
    HS_PORT_MODE_READ  = 1,
    /** Open device for writing. */
    HS_PORT_MODE_WRITE = 2,
    /** Open device for read/write operations. */
    HS_PORT_MODE_RW    = 3
} hs_port_mode;

/**
 * @ingroup device
 * @typedef hs_port
 * @brief Opaque structure representing a device I/O port.
 *
 */
struct hs_port;

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
hs_device *hs_device_ref(hs_device *dev);
/**
 * @ingroup device
 * @brief Decrease the device reference count.
 *
 * When the reference count reaches 0, the device object is freed. This function is thread-safe.
 *
 * @param dev Device object.
 */
void hs_device_unref(hs_device *dev);

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
int hs_port_open(hs_device *dev, hs_port_mode mode, hs_port **rport);
/**
 * @ingroup device
 * @brief Close a device, and free all used resources.
 *
 * @param port Device handle.
 */
void hs_port_close(hs_port *port);

/**
 * @ingroup device
 * @brief Get the device object from which this handle was opened.
 *
 * @param port Device handle.
 * @return Device object.
 */
hs_device *hs_port_get_device(const hs_port *port);
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
hs_handle hs_port_get_poll_handle(const hs_port *port);

HS_END_C

#endif
