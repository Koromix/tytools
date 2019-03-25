/* libhs - public domain
   Niels Martignène <niels.martignene@protonmail.com>
   https://koromix.dev/libhs

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
 * @brief Device match specifier, use the @ref HS_MATCH_TYPE "dedicated macros" for convenience.
 *
 * Here is a short example to enumerate all serial devices and HID devices with a specific
 * VID:PID pair.
 *
 * @code{.c}
 * const hs_match_spec specs[] = {
 *     HS_MATCH_TYPE(HS_DEVICE_TYPE_SERIAL, NULL),
 *     HS_MATCH_TYPE_VID_PID(HS_DEVICE_TYPE_HID, 0x16C0, 0x478, NULL)
 * };
 *
 * r = hs_enumerate(specs, sizeof(specs) / sizeof(*specs), device_callback, NULL);
 * @endcode
 */
struct hs_match_spec {
    /** Device type @ref hs_device_type or 0 to match all types. */
    unsigned int type;

    /** Device vendor ID or 0 to match all. */
    uint16_t vid;
    /** Device product ID or 0 to match all. */
    uint16_t pid;

    /** This value will be copied to dev->match_udata, see @ref hs_device. */
    void *udata;
};

/**
 * @ingroup match
 * @brief Match a specific device type, see @ref hs_device_type.
 */
#define HS_MATCH_TYPE(type, udata) {(type), 0, 0, (udata)}
/**
 * @ingroup match
 * @brief Match devices with corresponding VID:PID pair.
 */
#define HS_MATCH_VID_PID(vid, pid, udata) {0, (vid), (pid), (udata)}
/**
 * @ingroup match
 * @brief Match devices with corresponding @ref hs_device_type and VID:PID pair.
 */
#define HS_MATCH_TYPE_VID_PID(type, vid, pid, udata) {(type), (vid), (pid), (udata)}

/**
 * @ingroup match
 * @brief Create device match from human-readable string.
 *
 * Match string  | Details
 * ------------- | -------------------------------------------------
 * 0:0           | Match all devices
 * 0:0/serial    | Match all serial devices
 * abcd:0123/hid | Match HID devices with VID:PID pair 0xABCD:0x0123
 * 0123:abcd     | Match devices with VID:PID pair 0x0123:0xABCD
 *
 * @param      str    Human-readable match string.
 * @param[out] rmatch A pointer to the variable that receives the device match specifier,
 *     it will stay unchanged if the function fails.
 * @return This function returns 0 on success, or a negative @ref hs_error_code value.
 */
int hs_match_parse(const char *str, hs_match_spec *rspec);

HS_END_C

#endif
