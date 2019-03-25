/* libhs - public domain
   Niels Martignène <niels.martignene@protonmail.com>
   https://koromix.dev/libhs

   This software is in the public domain. Where that dedication is not
   recognized, you are granted a perpetual, irrevocable license to copy,
   distribute, and modify this file as you see fit.

   See the LICENSE file for more details. */

#ifndef HS_SERIAL_H
#define HS_SERIAL_H

#include "common.h"

HS_BEGIN_C

/**
 * @defgroup serial Serial device I/O
 * @brief Send and receive bytes to and from serial devices.
 */

/**
 * @ingroup serial
 * @brief Supported serial baud rates.
 *
 * @sa hs_serial_config
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
    HS_SERIAL_RATE_115200 = 115200,
    /** 230400 bps. */
    HS_SERIAL_RATE_230400 = 230400
};

/**
 * @ingroup serial
 * @brief Supported serial parity modes.
 *
 * @sa hs_serial_config
 */
typedef enum hs_serial_config_parity {
    /** Leave this setting unchanged. */
    HS_SERIAL_CONFIG_PARITY_INVALID = 0,

    /** No parity. */
    HS_SERIAL_CONFIG_PARITY_OFF,
    /** Even parity. */
    HS_SERIAL_CONFIG_PARITY_EVEN,
    /** Odd parity. */
    HS_SERIAL_CONFIG_PARITY_ODD,
    /** Mark parity. */
    HS_SERIAL_CONFIG_PARITY_MARK,
    /** Space parity. */
    HS_SERIAL_CONFIG_PARITY_SPACE
} hs_serial_config_parity;

/**
 * @ingroup serial
 * @brief Supported RTS pin modes and RTS/CTS flow control.
 *
 * @sa hs_serial_config
 */
typedef enum hs_serial_config_rts {
    /** Leave this setting unchanged. */
    HS_SERIAL_CONFIG_RTS_INVALID = 0,

    /** Disable RTS pin. */
    HS_SERIAL_CONFIG_RTS_OFF,
    /** Enable RTS pin. */
    HS_SERIAL_CONFIG_RTS_ON,
    /** Use RTS/CTS pins for flow control. */
    HS_SERIAL_CONFIG_RTS_FLOW
} hs_serial_config_rts;

/**
 * @ingroup serial
 * @brief Supported DTR pin modes.
 *
 * @sa hs_serial_config
 */
typedef enum hs_serial_config_dtr {
    /** Leave this setting unchanged. */
    HS_SERIAL_CONFIG_DTR_INVALID = 0,

    /** Disable DTR pin. */
    HS_SERIAL_CONFIG_DTR_OFF,
    /** Enable DTR pin. This is done by default when a device is opened. */
    HS_SERIAL_CONFIG_DTR_ON
} hs_serial_config_dtr;

/**
 * @ingroup serial
 * @brief Supported serial XON/XOFF (software) flow control modes.
 *
 * @sa hs_serial_config
 */
typedef enum hs_serial_config_xonxoff {
    /** Leave this setting unchanged. */
    HS_SERIAL_CONFIG_XONXOFF_INVALID = 0,

    /** Disable XON/XOFF flow control. */
    HS_SERIAL_CONFIG_XONXOFF_OFF,
    /** Enable XON/XOFF flow control for input only. */
    HS_SERIAL_CONFIG_XONXOFF_IN,
    /** Enable XON/XOFF flow control for output only. */
    HS_SERIAL_CONFIG_XONXOFF_OUT,
    /** Enable XON/XOFF flow control for input and output. */
    HS_SERIAL_CONFIG_XONXOFF_INOUT
} hs_serial_config_xonxoff;

/**
 * @ingroup serial
 * @brief Serial device configuration.
 *
 * Some OS settings have no equivalent in libhs, and will be set to 0 by hs_serial_get_config().
 * Parameters set to 0 are ignored by hs_serial_set_config().
 *
 * @sa hs_serial_set_config() to change device settings
 * @sa hs_serial_get_config() to get current settings
 */
typedef struct hs_serial_config {
    /** Device baud rate, see @ref hs_serial_rate for accepted values. */
    unsigned int baudrate;

    /** Number of data bits, can be 5, 6, 7 or 8 (or 0 to ignore). */
    unsigned int databits;
    /** Number of stop bits, can be 1 or 2 (or 0 to ignore). */
    unsigned int stopbits;
    /** Serial parity mode. */
    hs_serial_config_parity parity;

    /** RTS pin mode and RTS/CTS flow control. */
    hs_serial_config_rts rts;
    /** DTR pin mode. */
    hs_serial_config_dtr dtr;
    /** Serial XON/XOFF (software) flow control. */
    hs_serial_config_xonxoff xonxoff;
} hs_serial_config;

/**
 * @ingroup serial
 * @brief Set the serial settings associated with a serial device.
 *
 * Each parameter set to 0 will be ignored, and left as is for this device. The following
 * example code will only modify the parity and baudrate settings.
 *
 * @code{.c}
 * hs_serial_config config = {
 *     .baudrate = 115200,
 *     .parity = HS_SERIAL_CONFIG_PARITY_OFF
 * };
 * // This is an example, but you should check for errors
 * hs_serial_set_config(h, &config);
 * @endcode
 *
 * The change is carried out immediately, before the buffers are emptied.
 *
 * @param port   Open serial device handle.
 * @param config Serial settings, see @ref hs_serial_config.
 * @return This function returns 0 on success, or a negative @ref hs_error_code value.
 *
 * @sa hs_serial_config for available serial settings.
 */
int hs_serial_set_config(hs_port *port, const hs_serial_config *config);

/**
 * @ingroup serial
 * @brief Get the serial settings associated with a serial device.
 *
 * Only a subset of parameters available on each OS is recognized. Some hs_serial_config
 * values may be left to 0 if there is no valid libhs equivalent value, such that
 * subsequent hs_serial_set_config() calls should not lose these parameters.
 *
 * You do not need to call hs_serial_get_config() to change only a few settings, see
 * hs_serial_set_config() for more details.
 *
 * @param      port   Open serial device handle.
 * @param[out] config Serial settings, see @ref hs_serial_config.
 * @return This function returns 0 on success, or a negative @ref hs_error_code value.
 *
 * @sa hs_serial_config for available serial settings.
 */
int hs_serial_get_config(hs_port *port, hs_serial_config *config);

/**
 * @ingroup serial
 * @brief Read bytes from a serial device.
 *
 * Read up to @p size bytes from the serial device. If no data is available, the function
 * waits for up to @p timeout milliseconds. Use a negative value to wait indefinitely.
 *
 * @param      port    Device handle.
 * @param[out] buf     Data buffer.
 * @param      size    Size of the buffer.
 * @param      timeout Timeout in milliseconds, or -1 to block indefinitely.
 * @return This function returns the number of bytes read, or a negative @ref hs_error_code value.
 */
ssize_t hs_serial_read(hs_port *port, uint8_t *buf, size_t size, int timeout);
/**
 * @ingroup serial
 * @brief Send bytes to a serial device.
 *
 * Write up to @p size bytes to the device. This is a blocking function, but it may not write
 * all the data passed in.
 *
 * @param port    Device handle.
 * @param buf     Data buffer.
 * @param size    Size of the buffer.
 * @param timeout Timeout in milliseconds, or -1 to block indefinitely.
 * @return This function returns the number of bytes written, or a negative @ref hs_error_code
 *     value.
 */
ssize_t hs_serial_write(hs_port *port, const uint8_t *buf, size_t size, int timeout);

HS_END_C

#endif
