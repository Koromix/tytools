/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <QApplication>
#include <QMessageBox>
#include <QTimer>

#include "board_proxy.hh"
#include "main_window.hh"

#ifdef QT_STATIC
    #include <QtPlugin>
    #if defined(_WIN32)
        Q_IMPORT_PLUGIN(QWindowsIntegrationPlugin)
    #elif defined(__APPLE__)
        Q_IMPORT_PLUGIN(QCocoaIntegrationPlugin)
    #endif
#endif

using namespace std;

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    ty_error_redirect([](ty_err err, const char *msg, void *udata) {
        TY_UNUSED(err);
        TY_UNUSED(udata);

        QMessageBox::critical(nullptr, MainWindow::tr("Teensy Qt (critical error)"), msg, QMessageBox::Close);
        exit(1);
    }, nullptr);

    BoardManagerProxy manager;
    manager.start();

    MainWindow window(&manager);
    window.show();

    return app.exec();
}
