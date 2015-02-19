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

    BoardManagerProxy manager;
    MainWindow window(&manager);

    if (!manager.start()) {
        QMessageBox::critical(nullptr, MainWindow::tr("Teensy Qt (critical error)"), window.lastError(), QMessageBox::Close);
        return 1;
    }

    window.show();

    return app.exec();
}
