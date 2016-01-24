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

#ifndef HS_HID_H
#define HS_HID_H

#include "common.h"

HS_BEGIN_C

/**
 * @defgroup hid HID device I/O
 * @brief Send and receive HID reports (input, output, feature) to and from HID devices.
 */

struct hs_handle;

/**
 * @ingroup hid
 * @brief Structure representing a parsed HID descriptor.
 *
 * @sa hs_hid_parse_descriptor()
 */
typedef struct hs_hid_descriptor {
    /** Primary usage page value. */
    uint16_t usage_page;
    /** Primary usage value. */
    uint16_t usage;
} hs_hid_descriptor;

/**
 * @ingroup hid
 * @brief Parse the report descriptor from the device.
 *
 * This parser is very incomplete at the moment, only a few values are extracted from the
 * descriptor. See @ref hs_hid_descriptor.
 *
 * @param      h    Device handle.
 * @param[out] desc A pointer to a hs_hid_descriptor structure that receives the parsed
 *     descriptor information.
 * @return This function returns 0 on success, or a negative @ref hs_error_code value.
 *
 * @sa hs_hid_descriptor
 */
HS_PUBLIC int hs_hid_parse_descriptor(struct hs_handle *h, hs_hid_descriptor *desc);

/**
 * @ingroup hid
 * @brief Read an input report from the device.
 *
 * The first byte will contain the report ID, or 0 if the device does not use numbered reports.
 * HID is message-oriented, if the buffer is too small the extra bytes will be discarded.
 *
 * If no report is available, the function waits for up to @p timeout milliseconds. Use a
 * negative value to wait indefinitely.
 *
 * @param      h       Device handle.
 * @param[out] buf     Input report buffer.
 * @param      size    Size of the report buffer (make room for the report ID).
 * @param      timeout Timeout in milliseconds, or -1 to block indefinitely.
 *
 * @return This function returns the size of the report in bytes + 1 (report ID). It
 *     returns 0 on timeout, or a negative @ref hs_error_code value.
 */
HS_PUBLIC ssize_t hs_hid_read(struct hs_handle *h, uint8_t *buf, size_t size, int timeout);
/**
 * @ingroup hid
 * @brief Send an output report to the device.
 *
 * The first byte must be the report ID, or 0 if the device does not use report IDs.
 *
 * @param h    Device handle.
 * @param buf  Output report data.
 * @param size Output report size (including the report ID byte).
 *
 * @return This function returns the size of the report in bytes + 1 (report ID),
 *     or a negative error code.
 */
HS_PUBLIC ssize_t hs_hid_write(struct hs_handle *h, const uint8_t *buf, size_t size);

/**
 * @ingroup hid
 * @brief Get a feature report from the device.
 *
 * The first byte will contain the report ID, or 0 if the device does not use numbered reports.
 *
 * @param      h         Device handle.
 * @param      report_id Specific report to retrieve, or 0 if the device does not use
 *     numbered reports.
 * @param[out] buf       Feature report buffer (make room for the report ID).
 * @param      size      Size of the report buffer.
 *
 * @return This function returns the size of the report in bytes + 1 (report ID),
 *     or a negative @ref hs_error_code value.
 */
HS_PUBLIC ssize_t hs_hid_get_feature_report(hs_handle *h, uint8_t report_id, uint8_t *buf, size_t size);
/**
 * @ingroup hid
 * @brief Send a feature report to the device.
 *
 * The first byte must be the report ID, or 0 if the device does not use numbered reports.
 *
 * @param h    Device handle.
 * @param buf  Output report data.
 * @param size Output report size (including the report ID byte).
 *
 * @return This function returns the size of the report in bytes + 1 (report ID),
 *     or a negative @ref hs_error_code value.
 */
HS_PUBLIC ssize_t hs_hid_send_feature_report(struct hs_handle *h, const uint8_t *buf, size_t size);

HS_END_C

#endif
