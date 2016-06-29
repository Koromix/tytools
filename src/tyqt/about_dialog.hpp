/*
 * ty, a collection of GUI and command-line tools to manage Teensy devices
 *
 * Distributed under the MIT license (see LICENSE.txt or http://opensource.org/licenses/MIT)
 * Copyright (c) 2015 Niels Martignène <niels.martignene@gmail.com>
 */

#ifndef ABOUT_HH
#define ABOUT_HH

#include "ui_about_dialog.h"

class AboutDialog: public QDialog, private Ui::AboutDialog {
    Q_OBJECT

public:
    AboutDialog(QWidget *parent = nullptr, Qt::WindowFlags f = 0);

public slots:
    static void openWebsite();
    static void openBugReports();
    static void openLicense();

private slots:
    void openLink(const QString &link);
};

#endif
