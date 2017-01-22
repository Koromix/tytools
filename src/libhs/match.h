/* libhs - public domain
   Niels Martignène <niels.martignene@gmail.com>
   https://neodd.com/libraries

   This software is in the public domain. Where that dedication is not
   recognized, you are granted a perpetual, irrevocable license to copy,
   distribute, and modify this file as you see fit.

   See the LICENSE file for more details. */

#ifndef HS_MATCH_H
#define HS_MATCH_H

#include "common.h"

HS_BEGIN_C

/**
 * @defgroup match Device matching
 * @brief Match specific devices on enumeration and hotplug events.
 */

/**
 * @ingroup match
 * @brief Device match, use the @ref HS_MATCH_TYPE "dedicated macros" for convenience.
 *
 * Here is a short example to enumerate all serial devices and HID devices with a specific
 * VID:PID pair.
 *
 * @code{.c}
 * const hs_match matches[] = {
 *     HS_MATCH_TYPE(HS_DEVICE_TYPE_SERIAL),
 *     HS_MATCH_TYPE_VID_PID(HS_DEVICE_TYPE_HID, 0x16C0, 0x478)
 * };
 *
 * r = hs_enumerate(matches, sizeof(matches) / sizeof(*matches), device_callback, NULL);
 * @endcode
 */
struct hs_match {
    /** Device type @ref hs_device_type or 0 to match all types. */
    unsigned int type;

    /** Device vendor ID or 0 to match all. */
    uint16_t vid;
    /** Device product ID or 0 to match all. */
    uint16_t pid;

    /** Device path or NULL to match all. */
    const char *path;
};

/**
 * @ingroup match
 * @brief Match a specific device type, see @ref hs_device_type.
 */
#define HS_MATCH_TYPE(type) {(type), 0, 0, NULL}
/**
 * @ingroup match
 * @brief Match devices with corresponding VID:PID pair.
 */
#define HS_MATCH_VID_PID(vid, pid) {0, (vid), (pid), NULL}
/**
 * @ingroup match
 * @brief Match devices with corresponding @ref hs_device_type and VID:PID pair.
 */
#define HS_MATCH_TYPE_VID_PID(type, vid, pid) {(type), (vid), (pid), NULL}
/**
 * @ingroup match
 * @brief Match device with corresponding @ref hs_device_type and device path (e.g. COM1).
 */
#define HS_MATCH_TYPE_PATH(type, path) {(type), 0, 0, (path)}

HS_END_C

#endif
