/*
 * ty, a collection of GUI and command-line tools to manage Teensy devices
 *
 * Distributed under the MIT license (see LICENSE.txt or http://opensource.org/licenses/MIT)
 * Copyright (c) 2015 Niels Martignène <niels.martignene@gmail.com>
 */

#ifdef _WIN32
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
    #include <fcntl.h>
    #include <io.h>
    #include <stdio.h>
    #include <stdlib.h>
#endif

#include "hs/common.h"
#include "tyqt.hh"

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

static bool attach_win32_console()
{
    BOOL success;

    success = AttachConsole(ATTACH_PARENT_PROCESS);
    if (!success)
        return false;

    freopen("CONIN$", "r", stdin);
    freopen("CONOUT$", "w", stdout);
    freopen("CONOUT$", "w", stdout);
    ios::sync_with_stdio();

    return true;
}

#endif

int main(int argc, char *argv[])
{
    hs_log_set_handler(ty_libhs_log_handler, NULL);

    qRegisterMetaType<ty_log_level>("ty_log_level");
    qRegisterMetaType<std::shared_ptr<void>>("std::shared_ptr<void>");
    qRegisterMetaType<ty_descriptor>("ty_descriptor");

    TyQt app(argc, argv);
#ifdef _WIN32
    if (getenv("_TYQTC")) {
        _putenv("_TYQTC=");
        app.setClientConsole(attach_win32_console());
    }
#else
    app.setClientConsole(ty_standard_get_modes(TY_STANDARD_OUTPUT) != TY_DESCRIPTOR_MODE_DEVICE);
#endif
    return app.exec();
}
