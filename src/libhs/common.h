/* libhs - public domain
   Niels Martignène <niels.martignene@protonmail.com>
   https://koromix.dev/libhs

   This software is in the public domain. Where that dedication is not
   recognized, you are granted a perpetual, irrevocable license to copy,
   distribute, and modify this file as you see fit.

   See the LICENSE file for more details. */

#ifndef HS_COMMON_H
#define HS_COMMON_H

#include <stddef.h>
#include <sys/types.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
    #define HS_BEGIN_C extern "C" {
    #define HS_END_C }
#else
    #define HS_BEGIN_C
    #define HS_END_C
#endif

HS_BEGIN_C

typedef struct hs_device hs_device;
typedef struct hs_monitor hs_monitor;
typedef struct hs_port hs_port;
typedef struct hs_match_spec hs_match_spec;

/**
 * @defgroup misc Miscellaneous
 * @brief Error management and platform-specific functions.
 */

/**
 * @ingroup misc
 * @brief Compile-time libhs version.
 *
 * The version is represented as a six-digit decimal value respecting **semantic versioning**:
 * MMmmpp (major, minor, patch), e.g. 900 for "0.9.0", 10002 for "1.0.2" or 220023 for "22.0.23".
 *
 * @sa hs_version() for the run-time version.
 * @sa HS_VERSION_STRING for the version string.
 */
#define HS_VERSION 900
/**
 * @ingroup misc
 * @brief Compile-time libhs version string.
 *
 * @sa hs_version_string() for the run-time version.
 * @sa HS_VERSION for the version number.
 */
#define HS_VERSION_STRING "0.9.0"

#if defined(__GNUC__)
    #ifdef __MINGW_PRINTF_FORMAT
        #define HS_PRINTF_FORMAT(fmt, first) __attribute__((__format__(__MINGW_PRINTF_FORMAT, fmt, first)))
    #else
        #define HS_PRINTF_FORMAT(fmt, first) __attribute__((__format__(__printf__, fmt, first)))
    #endif
#elif _MSC_VER >= 1900
    #define HS_PRINTF_FORMAT(fmt, first)

    // HAVE_SSIZE_T is used this way by other projects
    #ifndef HAVE_SSIZE_T
        #define HAVE_SSIZE_T
        #ifdef _WIN64
typedef __int64 ssize_t;
        #else
typedef long ssize_t;
        #endif
    #endif
#else
    #error "This compiler is not supported"
#endif

#define _HS_CONCAT_HELPER(a, b) a ## b
#define _HS_CONCAT(a, b) _HS_CONCAT_HELPER(a, b)

#define _HS_UNIQUE_ID(prefix) _HS_CONCAT(prefix, __LINE__)

#define _hs_container_of(head, type, member) \
    ((type *)((char *)(head) - (size_t)(&((type *)0)->member)))

#if defined(DOXYGEN)
/**
 * @ingroup misc
 * @brief Type representing an OS descriptor/handle.
 *
 * This is used in functions taking or returning an OS descriptor/handle, in order to avoid
 * having different function prototypes on different platforms.
 *
 * The underlying type is:
 * - int on POSIX platforms, including OS X
 * - HANDLE (aka. void *) on Windows
 */
typedef _platform_specific_ hs_handle;
#elif defined(_WIN32)
typedef void *hs_handle; // HANDLE
#else
typedef int hs_handle;
#endif

/**
 * @ingroup misc
 * @brief libhs message log levels.
 */
typedef enum hs_log_level {
    /** Fatal errors. */
    HS_LOG_ERROR,
    /** Non-fatal problem. */
    HS_LOG_WARNING,
    /** Internal debug information. */
    HS_LOG_DEBUG
} hs_log_level;

/**
 * @ingroup misc
 * @brief libhs error codes.
 */
typedef enum hs_error_code {
    /** Memory error. */
    HS_ERROR_MEMORY        = -1,
    /** Missing resource error. */
    HS_ERROR_NOT_FOUND     = -2,
    /** Permission denied. */
    HS_ERROR_ACCESS        = -3,
    /** Input/output error. */
    HS_ERROR_IO            = -4,
    /** Parse error. */
    HS_ERROR_PARSE         = -5,
    /** Generic system error. */
    HS_ERROR_SYSTEM        = -6
} hs_error_code;

typedef void hs_log_handler_func(hs_log_level level, int err, const char *msg, void *udata);

/**
 * @{
 * @name Version Functions
 */

/**
 * @ingroup misc
 * @brief Run-time libhs version.
 *
 * The version is represented as a six-digit decimal value respecting **semantic versioning**:
 * MMmmpp (major, minor, patch), e.g. 900 for "0.9.0", 10002 for "1.0.2" or 220023 for "22.0.23".
 *
 * @return This function returns the run-time version number.
 *
 * @sa HS_VERSION for the compile-time version.
 * @sa hs_version_string() for the version string.
 */
uint32_t hs_version(void);
/**
 * @ingroup misc
 * @brief Run-time libhs version string.
 *
 * @return This function returns the run-time version string.
 *
 * @sa HS_VERSION_STRING for the compile-time version.
 * @sa hs_version() for the version number.
 */
const char *hs_version_string(void);

/** @} */

/**
 * @{
 * @name Log Functions
 */

/**
 * @ingroup misc
 * @brief Default log handler, see hs_log_set_handler() for more information.
 */
void hs_log_default_handler(hs_log_level level, int err, const char *msg, void *udata);
/**
 * @ingroup misc
 * @brief Change the log handler function.
 *
 * The default handler prints the message to stderr. It does not print debug messages unless
 * the environment variable LIBHS_DEBUG is set.
 *
 * @param f     New log handler, or hs_log_default_handler to restore the default one.
 * @param udata Pointer to user-defined data for the handler (use NULL for hs_log_default_handler).
 *
 * @sa hs_log()
 * @sa hs_log_default_handler() is the default log handler.
 */
void hs_log_set_handler(hs_log_handler_func *f, void *udata);
/**
 * @ingroup misc
 * @brief Call the log callback with a printf-formatted message.
 *
 * Format a message and call the log callback with it. The default callback prints it to stderr,
 * see hs_log_set_handler(). This callback does not print debug messages unless the environment
 * variable LIBHS_DEBUG is set.
 *
 * @param level Log level.
 * @param fmt Format string, using printf syntax.
 * @param ...
 *
 * @sa hs_log_set_handler() to use a custom callback function.
 */
void hs_log(hs_log_level level, const char *fmt, ...) HS_PRINTF_FORMAT(2, 3);

/** @} */

/**
 * @{
 * @name Error Functions
 */

/**
 * @ingroup misc
 * @brief Mask an error code.
 *
 * Mask error codes to prevent libhs from calling the log callback (the default one simply prints
 * the string to stderr). It does not change the behavior of the function where the error occurs.
 *
 * For example, if you want to open a device without a missing device message, you can use:
 * @code{.c}
 * hs_error_mask(HS_ERROR_NOT_FOUND);
 * r = hs_port_open(dev, HS_PORT_MODE_RW, &port);
 * hs_error_unmask();
 * if (r < 0)
 *     return r;
 * @endcode
 *
 * The masked codes are kept in a limited stack, you must not forget to unmask codes quickly
 * with @ref hs_error_unmask().
 *
 * @param err Error code to mask.
 *
 * @sa hs_error_unmask()
 * @sa hs_error_is_masked()
 */
void hs_error_mask(hs_error_code err);
/**
 * @ingroup misc
 * @brief Unmask the last masked error code.
 *
 * @sa hs_error_mask()
 */
void hs_error_unmask(void);
/**
 * @ingroup misc
 * @brief Check whether an error code is masked.
 *
 * Returns 1 if error code @p err is currently masked, or 0 otherwise.
 *
 * hs_error() does not call the log handler for masked errors, you only need to use
 * this function if you want to bypass hs_error() and call hs_log() directly.
 *
 * @param err Error code to check.
 *
 * @sa hs_error_mask()
 */
int hs_error_is_masked(int err);

/**
  * @ingroup misc
  * @brief Get the last error message emitted on the current thread.
  */
const char *hs_error_last_message();

/**
 * @ingroup misc
 * @brief Call the log callback with a printf-formatted error message.
 *
 * Format an error message and call the log callback with it. Pass NULL to @p fmt to use a
 * generic error message. The default callback prints it to stderr, see hs_log_set_handler().
 *
 * The error code is simply returned as a convenience, so you can use this function like:
 * @code{.c}
 * // Explicit error message
 * int pipe[2], r;
 * r = pipe(pipe);
 * if (r < 0)
 *     return hs_error(HS_ERROR_SYSTEM, "pipe() failed: %s", strerror(errno));
 *
 * // Generic error message (e.g. "Memory error")
 * char *p = malloc(128);
 * if (!p)
 *     return hs_error(HS_ERROR_MEMORY, NULL);
 * @endcode
 *
 * @param err Error code.
 * @param fmt Format string, using printf syntax.
 * @param ...
 * @return This function returns the error code.
 *
 * @sa hs_error_mask() to mask specific error codes.
 * @sa hs_log_set_handler() to use a custom callback function.
 */
int hs_error(hs_error_code err, const char *fmt, ...) HS_PRINTF_FORMAT(2, 3);

HS_END_C

#endif
