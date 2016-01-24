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
typedef struct hs_match {
    /** Device type @ref hs_device_type or 0 to match all types. */
    unsigned int type;

    /** Device vendor ID or 0 to match all. */
    uint16_t vid;
    /** Device product ID or 0 to match all. */
    uint16_t pid;

    /** Device path or NULL to match all. */
    const char *path;
} hs_match;

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
