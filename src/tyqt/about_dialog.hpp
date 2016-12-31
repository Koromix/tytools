/*
 * ty, a collection of GUI and command-line tools to manage Teensy devices
 *
 * Distributed under the MIT license (see LICENSE.txt or http://opensource.org/licenses/MIT)
 * Copyright (c) 2015 Niels Martignène <niels.martignene@gmail.com>
 */

#ifndef ABOUT_HH
#define ABOUT_HH

#include "../libty/common.h"
#include "ui_about_dialog.h"

class AboutDialog: public QDialog, private Ui::AboutDialog {
    Q_OBJECT

public:
    AboutDialog(QWidget *parent = nullptr, Qt::WindowFlags f = 0);

public slots:
#ifdef TY_CONFIG_URL_WEBSITE
    static void openWebsite();
#endif
#ifdef TY_CONFIG_URL_BUGS
    static void openBugReports();
#endif
    static void openLicense();
};

#endif
