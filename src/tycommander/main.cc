/* TyTools - public domain
   Niels Martignène <niels.martignene@protonmail.com>
   https://koromix.dev/tytools

   This software is in the public domain. Where that dedication is not
   recognized, you are granted a perpetual, irrevocable license to copy,
   distribute, and modify this file as you see fit.

   See the LICENSE file for more details. */

#ifdef _WIN32
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
    #include <dbghelp.h>
    #include <shlobj.h>
    #include <fcntl.h>
    #include <io.h>
    #include <stdio.h>
    #include <stdlib.h>
#endif

#include "../libhs/common.h"
#include "../libty/class.h"
#include "tycommander.hpp"

#ifdef QT_STATIC
    #include <QtPlugin>
    #if defined(_WIN32)
        Q_IMPORT_PLUGIN(QWindowsIntegrationPlugin)
        Q_IMPORT_PLUGIN(QWindowsVistaStylePlugin)
    #elif defined(__APPLE__)
        Q_IMPORT_PLUGIN(QCocoaIntegrationPlugin)
    #endif
#endif

using namespace std;

#ifdef _WIN32

static void make_minidump(EXCEPTION_POINTERS *ex)
{
    auto MiniDumpWriteDump_ =
        reinterpret_cast<decltype(MiniDumpWriteDump) *>(GetProcAddress(LoadLibrary("dbghelp"),
                                                                       "MiniDumpWriteDump"));
    auto SHGetFolderPath_ =
        reinterpret_cast<decltype(SHGetFolderPath) *>(GetProcAddress(LoadLibrary("shell32"),
                                                                     "SHGetFolderPathA"));
    if (!MiniDumpWriteDump_ || !SHGetFolderPath_)
        return;

    char dmp_path[MAX_PATH + 256];
    {
        // Crash dump directory
        if (SHGetFolderPath_(0, CSIDL_LOCAL_APPDATA, nullptr, SHGFP_TYPE_CURRENT, dmp_path) != S_OK)
            return;
        strcat(dmp_path, "\\CrashDumps");
        CreateDirectory(dmp_path, nullptr);

        // Executable name
        char module_path[MAX_PATH];
        DWORD module_len = GetModuleFileNameA(GetModuleHandle(nullptr), module_path,
                                              sizeof(module_path));
        if (!module_len)
            return;
        char *module_name = module_path + module_len;
        while (module_name > module_path && module_name[-1] != '\\' && module_name[-1] != '/')
            module_name--;

        SYSTEMTIME st;
        GetSystemTime(&st);

        size_t path_len = strlen(dmp_path);
        snprintf(dmp_path + path_len, sizeof(dmp_path) - path_len,
                 "\\%s_%4d%02d%02d_%02d%02d%02d.dmp",
                 module_name, st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
    }

    HANDLE h = CreateFile(dmp_path, GENERIC_WRITE, FILE_SHARE_READ, 0, CREATE_ALWAYS,
                          FILE_ATTRIBUTE_NORMAL, 0);
    if(h == INVALID_HANDLE_VALUE)
        return;

    MINIDUMP_EXCEPTION_INFORMATION ex_info;
    ex_info.ThreadId = GetCurrentThreadId();
    ex_info.ExceptionPointers = ex;
    ex_info.ClientPointers = FALSE;

    MiniDumpWriteDump_(GetCurrentProcess(), GetCurrentProcessId(), h,
                       MINIDUMP_TYPE(MiniDumpWithIndirectlyReferencedMemory | MiniDumpScanMemory),
                       ex ? &ex_info : nullptr, nullptr, nullptr);

    CloseHandle(h);
}

static LONG CALLBACK unhandled_exception_handler(EXCEPTION_POINTERS *ex)
{
    make_minidump(ex);
    return EXCEPTION_CONTINUE_SEARCH;
}

static bool reopen_stream(FILE *fp, const QString &path, const char *mode)
{
    fp = freopen(path.toLocal8Bit().constData(), mode, fp);
    if (!fp)
        return false;
    setvbuf(fp, NULL, _IONBF, 0);

    return true;
}

static bool open_tycommanderc_bridge()
{
    auto parts = QString(getenv("_TYCOMMANDERC_PIPES")).split(':');
    if (parts.count() != 3)
        return false;
    _putenv("_TYCOMMANDERC_PIPES=");

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
#ifdef _WIN32
    SetUnhandledExceptionFilter(unhandled_exception_handler);
#endif

    hs_log_set_handler(ty_libhs_log_handler, NULL);
    if (ty_models_load_patch(nullptr) == TY_ERROR_MEMORY)
        return 1;

    qRegisterMetaType<ty_log_level>("ty_log_level");
    qRegisterMetaType<std::shared_ptr<void>>("std::shared_ptr<void>");
    qRegisterMetaType<ty_descriptor>("ty_descriptor");
    qRegisterMetaType<SessionPeer::CloseReason>("SessionPeer::CloseReason");
    qRegisterMetaType<uint64_t>("uint64_t");

    TyCommander app(argc, argv);
#ifdef _WIN32
    app.setClientConsole(open_tycommanderc_bridge());
#else
    app.setClientConsole(ty_standard_get_modes(TY_STREAM_OUTPUT) != TY_DESCRIPTOR_MODE_DEVICE);
#endif
    return app.exec();
}
