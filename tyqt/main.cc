/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "tyqt.hh"

#ifdef _WIN32
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
    #include <fcntl.h>
    #include <io.h>
    #include <stdio.h>
    #include <stdlib.h>
#endif

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
static void set_standard_handle(DWORD n, HANDLE h, FILE *f, const char *mode)
{
    SetStdHandle(n, h);

    *f = *_fdopen(_open_osfhandle(reinterpret_cast<uintptr_t>(h), _O_TEXT), mode);
    setvbuf(f, NULL, _IONBF, 0);
}

static bool setup_tyqtc_bridge()
{
    QStringList parts = QString(getenv("_TYQT_BRIDGE")).split(':');
    if (parts.count() != 3)
        return false;
    _putenv("_TYQT_BRIDGE=");

#define SET_STANDARD_HANDLE(nstd, n, f, mode) \
        set_standard_handle((nstd), reinterpret_cast<HANDLE>(parts[n].toULong(nullptr, 16)), (f), (mode))

    SET_STANDARD_HANDLE(STD_INPUT_HANDLE, 0, stdin, "r");
    SET_STANDARD_HANDLE(STD_OUTPUT_HANDLE, 1, stdout, "w");
    SET_STANDARD_HANDLE(STD_ERROR_HANDLE, 2, stderr, "w");

#undef SET_STANDARD_HANDLE

    return true;
}
#endif

int main(int argc, char *argv[])
{
    TyQt app(argc, argv);

#ifdef _WIN32
    if (setup_tyqtc_bridge())
        app.setClientConsole(true);
#endif

    return app.exec();
}
