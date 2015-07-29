/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef ABOUT_HH
#define ABOUT_HH

#include "ui_about_dialog.h"

class AboutDialog: public QDialog, private Ui::AboutDialog {
    Q_OBJECT

public:
    AboutDialog(QWidget *parent = nullptr, Qt::WindowFlags f = 0);

private slots:
    void on_websiteButton_clicked();
    void on_licenseButton_clicked();
    void on_descriptionText_linkActivated(const QString &link);
};

#endif
