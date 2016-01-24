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

#ifndef HS_SERIAL_H
#define HS_SERIAL_H

#include "common.h"

HS_BEGIN_C

/**
 * @defgroup serial Serial device I/O
 * @brief Send and receive bytes to and from serial devices.
 */

struct hs_handle;

/**
 * @ingroup serial
 * @brief Supported serial baud rates.
 *
 * @sa hs_serial_set_attributes()
 */
enum hs_serial_rate {
    /** 110 bps. */
    HS_SERIAL_RATE_110    = 110,
    /** 134 bps. */
    HS_SERIAL_RATE_134    = 134,
    /** 150 bps. */
    HS_SERIAL_RATE_150    = 150,
    /** 200 bps. */
    HS_SERIAL_RATE_200    = 200,
    /** 300 bps. */
    HS_SERIAL_RATE_300    = 300,
    /** 600 bps. */
    HS_SERIAL_RATE_600    = 600,
    /** 1200 bps. */
    HS_SERIAL_RATE_1200   = 1200,
    /** 1800 bps. */
    HS_SERIAL_RATE_1800   = 1800,
    /** 2400 bps. */
    HS_SERIAL_RATE_2400   = 2400,
    /** 4800 bps. */
    HS_SERIAL_RATE_4800   = 4800,
    /** 9600 bps. */
    HS_SERIAL_RATE_9600   = 9600,
    /** 19200 bps. */
    HS_SERIAL_RATE_19200  = 19200,
    /** 38400 bps. */
    HS_SERIAL_RATE_38400  = 38400,
    /** 57600 bps. */
    HS_SERIAL_RATE_57600  = 57600,
    /** 115200 bps. */
    HS_SERIAL_RATE_115200 = 115200
};

/**
 * @ingroup serial
 * @brief Masks for groups of serial control flags.
 */
enum hs_serial_mask {
    HS_SERIAL_MASK_CSIZE  = 0x3,
    HS_SERIAL_MASK_PARITY = 0xC,
    HS_SERIAL_MASK_STOP   = 0x10,
    HS_SERIAL_MASK_FLOW   = 0x60,
    HS_SERIAL_MASK_CLOSE  = 0x80
};

/**
 * @ingroup serial
 * @brief Supported serial control flags.
 *
 * @sa hs_serial_set_attributes()
 */
enum hs_serial_flag {
    /** Use 7 bits in the bytes transmitted and received, instead of 8 bytes. */
    HS_SERIAL_CSIZE_7BITS  = 0x1,
    /** Use 6 bits in the bytes transmitted and received, instead of 8 bytes. */
    HS_SERIAL_CSIZE_6BITS  = 0x2,
    /** Use 5 bits in the bytes transmitted and received, instead of 8 bytes. */
    HS_SERIAL_CSIZE_5BITS  = 0x3,

    /** Use an odd scheme for parity, instead of no parity. */
    HS_SERIAL_PARITY_ODD   = 0x4,
    /** Use an even scheme for parity, instead of no parity. */
    HS_SERIAL_PARITY_EVEN  = 0x8,

    /** Use two stop bits instead of one. */
    HS_SERIAL_STOP_2BITS   = 0x10,

    /** Enable XON/XOFF (software) flow control on input. */
    HS_SERIAL_FLOW_XONXOFF = 0x20,
    /** Enable RTS/CTS (hardware) flow control. */
    HS_SERIAL_FLOW_RTSCTS  = 0x40,

    /** Keep the DTR line high on close (not supported on Windows). */
    HS_SERIAL_CLOSE_NOHUP  = 0x80
};

/**
 * @ingroup serial
 * @brief Set the parameters associated with a serial device.
 *
 * The change is carried out immediately, before the buffers are emptied.
 *
 * @param h     Open serial device handle.
 * @param rate  Serial baud rate, see @ref hs_serial_rate.
 * @param flags Serial connection settings, see @ref hs_serial_flag.
 * @return This function returns 0 on success, or a negative @ref hs_error_code value.
 *
 * @sa hs_serial_rate for acceptable baud rates.
 * @sa hs_serial_flag for supported control flags.
 */
HS_PUBLIC int hs_serial_set_attributes(struct hs_handle *h, uint32_t rate, int flags);

/**
 * @ingroup serial
 * @brief Read bytes from a serial device.
 *
 * Read up to @p size bytes from the serial device. If no data is available, the function
 * waits for up to @p timeout milliseconds. Use a negative value to wait indefinitely.
 *
 * @param      h       Device handle.
 * @param[out] buf     Data buffer.
 * @param      size    Size of the buffer.
 * @param      timeout Timeout in milliseconds, or -1 to block indefinitely.
 * @return This function returns the number of bytes read, or a negative @ref hs_error_code value.
 */
HS_PUBLIC ssize_t hs_serial_read(struct hs_handle *h, uint8_t *buf, size_t size, int timeout);
/**
 * @ingroup serial
 * @brief Send bytes to a serial device.
 *
 * Write up to @p size bytes to the device. This is a blocking function, but it may not write
 * all the data passed in.
 *
 * @param h    Device handle.
 * @param buf  Data buffer.
 * @param size Size of the buffer.
 * @return This function returns the number of bytes written, or a negative @ref hs_error_code
 *     value.
 */
HS_PUBLIC ssize_t hs_serial_write(struct hs_handle *h, const uint8_t *buf, ssize_t size);

HS_END_C

#endif
