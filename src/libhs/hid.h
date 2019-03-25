/* libhs - public domain
   Niels Martignène <niels.martignene@protonmail.com>
   https://koromix.dev/libhs

   This software is in the public domain. Where that dedication is not
   recognized, you are granted a perpetual, irrevocable license to copy,
   distribute, and modify this file as you see fit.

   See the LICENSE file for more details. */

#ifndef HS_HID_H
#define HS_HID_H

#include "common.h"

HS_BEGIN_C

/**
 * @defgroup hid HID device I/O
 * @brief Send and receive HID reports (input, output, feature) to and from HID devices.
 */

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
 * @param      port    Device handle.
 * @param[out] buf     Input report buffer.
 * @param      size    Size of the report buffer (make room for the report ID).
 * @param      timeout Timeout in milliseconds, or -1 to block indefinitely.
 *
 * @return This function returns the size of the report in bytes + 1 (report ID). It
 *     returns 0 on timeout, or a negative @ref hs_error_code value.
 */
ssize_t hs_hid_read(hs_port *port, uint8_t *buf, size_t size, int timeout);
/**
 * @ingroup hid
 * @brief Send an output report to the device.
 *
 * The first byte must be the report ID, or 0 if the device does not use report IDs.
 *
 * @param port Device handle.
 * @param buf  Output report data.
 * @param size Output report size (including the report ID byte).
 *
 * @return This function returns the size of the report in bytes + 1 (report ID),
 *     or a negative error code.
 */
ssize_t hs_hid_write(hs_port *port, const uint8_t *buf, size_t size);

/**
 * @ingroup hid
 * @brief Get a feature report from the device.
 *
 * The first byte will contain the report ID, or 0 if the device does not use numbered reports.
 *
 * @param      port      Device handle.
 * @param      report_id Specific report to retrieve, or 0 if the device does not use
 *     numbered reports.
 * @param[out] buf       Feature report buffer (make room for the report ID).
 * @param      size      Size of the report buffer.
 *
 * @return This function returns the size of the report in bytes + 1 (report ID),
 *     or a negative @ref hs_error_code value.
 */
ssize_t hs_hid_get_feature_report(hs_port *port, uint8_t report_id, uint8_t *buf, size_t size);
/**
 * @ingroup hid
 * @brief Send a feature report to the device.
 *
 * The first byte must be the report ID, or 0 if the device does not use numbered reports.
 *
 * @param port Device handle.
 * @param buf  Output report data.
 * @param size Output report size (including the report ID byte).
 *
 * @return This function returns the size of the report in bytes + 1 (report ID),
 *     or a negative @ref hs_error_code value.
 */
ssize_t hs_hid_send_feature_report(hs_port *port, const uint8_t *buf, size_t size);

HS_END_C

#endif
