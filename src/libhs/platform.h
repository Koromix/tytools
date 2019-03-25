/* libhs - public domain
   Niels Martignène <niels.martignene@protonmail.com>
   https://koromix.dev/libhs

   This software is in the public domain. Where that dedication is not
   recognized, you are granted a perpetual, irrevocable license to copy,
   distribute, and modify this file as you see fit.

   See the LICENSE file for more details. */

#ifndef HS_PLATFORM_H
#define HS_PLATFORM_H

#include "common.h"

HS_BEGIN_C

#ifdef _WIN32
/**
 * @ingroup misc
 * @brief Common Windows version numbers.
 */
enum hs_win32_release {
    /** Windows 2000. */
    HS_WIN32_VERSION_2000 = 500,
    /** Windows XP. */
    HS_WIN32_VERSION_XP = 501,
    /** Windows Server 2003 or XP-64. */
    HS_WIN32_VERSION_2003 = 502,
    /** Windows Vista. */
    HS_WIN32_VERSION_VISTA = 600,
    /** Windows 7 */
    HS_WIN32_VERSION_7 = 601,
    /** Windows 8 */
    HS_WIN32_VERSION_8 = 602,
    /** Windows 8.1 */
    HS_WIN32_VERSION_8_1 = 603,
    /** Windows 10 */
    HS_WIN32_VERSION_10 = 1000
};
#endif

/**
 * @ingroup misc
 * @brief Poll descriptor.
 */
typedef struct hs_poll_source {
    /** OS-specific descriptor. */
    hs_handle desc;
    /** Custom pointer. */
    void *udata;

    /** Boolean output member for ready/signaled state. */
    int ready;
} hs_poll_source;

/**
 * @ingroup misc
 * @brief Maximum number of pollable descriptors.
 */
#define HS_POLL_MAX_SOURCES 64

/**
 * @{
 * @name System Functions
 */

/**
 * @ingroup misc
 * @brief Get time from a monotonic clock.
 *
 * You should not rely on the absolute value, whose meaning may differ on various platforms.
 * Use it to calculate periods and durations.
 *
 * While the returned value is in milliseconds, the resolution is not that good on some
 * platforms. On Windows, it is over 10 milliseconds.
 *
 * @return This function returns a mononotic time value in milliseconds.
 */
uint64_t hs_millis(void);

/**
 * @ingroup misc
 * @brief Adjust a timeout over a time period.
 *
 * This function returns -1 if the timeout is negative. Otherwise, it decreases the timeout
 * for each millisecond elapsed since @p start. When @p timeout milliseconds have passed,
 * the function returns 0.
 *
 * hs_millis() is used as the time source, so you must use it for @p start.
 *
 * @code{.c}
 * uint64_t start = hs_millis();
 * do {
 *     r = poll(&pfd, 1, hs_adjust_timeout(timeout, start));
 * } while (r < 0 && errno == EINTR);
 * @endcode
 *
 * This function is mainly used in libhs to restart interrupted system calls with
 * timeouts, such as poll().
 *
 * @param timeout Timeout is milliseconds.
 * @param start Start of the timeout period, from hs_millis().
 *
 * @return This function returns the adjusted value, or -1 if @p timeout is negative.
 */
int hs_adjust_timeout(int timeout, uint64_t start);

#ifdef __linux__
/**
 * @ingroup misc
 * @brief Get the Linux kernel version as a composite decimal number.
 *
 * For pre-3.0 kernels, the value is MMmmrrppp (2.6.34.1 => 020634001). For recent kernels,
 * it is MMmm00ppp (4.1.2 => 040100002).
 *
 * Do not rely on this too much, because kernel versions do not reflect the different kernel
 * flavors. Some distributions use heavily-patched builds, with a lot of backported code. When
 * possible, detect functionalities instead.
 *
 * @return This function returns the version number.
 */
uint32_t hs_linux_version(void);
#endif

#ifdef _WIN32
/**
 * @ingroup misc
 * @brief Format an error string using FormatMessage().
 *
 * The content is only valid until the next call to hs_win32_strerror(), be careful with
 * multi-threaded code.
 *
 * @param err Windows error code, or use 0 to get it from GetLastError().
 * @return This function returns a private buffer containing the error string, valid until the
 *     next call to hs_win32_strerror().
 */
const char *hs_win32_strerror(unsigned long err);
/**
 * @ingroup misc
 * @brief Get the Windows version as a composite decimal number.
 *
 * The value is MMmm, see https://msdn.microsoft.com/en-us/library/windows/desktop/ms724832%28v=vs.85%29.aspx
 * for the operating system numbers. You can use the predefined enum values from
 * @ref hs_win32_release.
 *
 * Use this only when testing for functionality is not possible or impractical.
 *
 * @return This function returns the version number.
 */
uint32_t hs_win32_version(void);
#endif

#ifdef __APPLE__
/**
 * @ingroup misc
 * @brief Get the Darwin version on Apple systems
 *
 * The value is MMmmrr (11.4.2 => 110402), see https://en.wikipedia.org/wiki/Darwin_%28operating_system%29
 * for the corresponding OS X releases.
 *
 * @return This function returns the version number.
 */
uint32_t hs_darwin_version(void);
#endif

/**
 * @ingroup misc
 * @brief Wait for ready/readable descriptors.
 *
 * This function is provided for convenience, but it is neither fast nor powerful. Use more
 * advanced event libraries (libev, libevent, libuv) or OS-specific functions if you need more.
 *
 * The @p ready field of each source is set to 1 for ready/signaled descriptors, and 0 for
 * all others.
 *
 * This function cannot process more than @ref HS_POLL_MAX_SOURCES sources.
 *
 * @param[in,out] sources Array of descriptor sources.
 * @param         count   Number of sources.
 * @param         timeout Timeout in milliseconds, or -1 to block indefinitely.
 * @return This function returns the number of ready descriptors, 0 on timeout, or a negative
 *     @ref hs_error_code value.
 *
 * @sa hs_poll_source
 */
int hs_poll(hs_poll_source *sources, unsigned int count, int timeout);

HS_END_C

#endif
