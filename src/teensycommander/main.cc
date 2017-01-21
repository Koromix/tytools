/* Teensy Tools - public domain
   Niels Martignène <niels.martignene@gmail.com>
   https://neodd.com/teensytools

   This software is in the public domain. Where that dedication is not
   recognized, you are granted a perpetual, irrevocable license to copy,
   distribute, and modify this file as you see fit.

   See the LICENSE file for more details. */

#ifdef _WIN32
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
    #include <fcntl.h>
    #include <io.h>
    #include <stdio.h>
    #include <stdlib.h>
#endif

#include "../libhs/common.h"
#include "teensycommander.hpp"

#ifdef QT_STATIC
    #include <QtPlugin>
    #if defined(_WIN32)
        Q_IMPORT_PLUGIN(QWindowsIntegrationPlugin)
    #elif defined(__APPLE__)
        Q_IMPORT_PLUGIN(QCocoaIntegrationPlugin)
    #endif
#endif

using namespace std;

#ifdef _WIN32

static bool reopen_stream(FILE *fp, const QString &path, const char *mode)
{
    fp = freopen(path.toLocal8Bit().constData(), mode, fp);
    if (!fp)
        return false;
    setvbuf(fp, NULL, _IONBF, 0);

    return true;
}

static bool open_teensycommanderc_bridge()
{
    auto parts = QString(getenv("_TEENSYCOMMANDERC_PIPES")).split(':');
    if (parts.count() != 3)
        return false;
    _putenv("_TEENSYCOMMANDERC_PIPES=");

#define REOPEN_STREAM(fp, path, mode) \
        if (!reopen_stream((fp), (path), (mode))) \
            return false;

    REOPEN_STREAM(stdin, parts[0], "r");
    REOPEN_STREAM(stdout, parts[1], "w");
    REOPEN_STREAM(stderr, parts[2], "w");

#undef REOPEN_STREAM

    return true;
}

#endif

int main(int argc, char *argv[])
{
    hs_log_set_handler(ty_libhs_log_handler, NULL);

    qRegisterMetaType<ty_log_level>("ty_log_level");
    qRegisterMetaType<std::shared_ptr<void>>("std::shared_ptr<void>");
    qRegisterMetaType<ty_descriptor>("ty_descriptor");
    qRegisterMetaType<SessionPeer::CloseReason>("SessionPeer::CloseReason");
    qRegisterMetaType<uint64_t>("uint64_t");

    TeensyCommander app(argc, argv);
#ifdef _WIN32
    app.setClientConsole(open_teensycommanderc_bridge());
#else
    app.setClientConsole(ty_standard_get_modes(TY_STANDARD_OUTPUT) != TY_DESCRIPTOR_MODE_DEVICE);
#endif
    return app.exec();
}
