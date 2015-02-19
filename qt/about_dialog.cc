/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <QDesktopServices>
#include <QUrl>

#include "ty.h"
#include "about_dialog.hh"

AboutDialog::AboutDialog(QWidget *parent, Qt::WindowFlags f)
    : QDialog(parent, f)
{
    setupUi(this);

    versionLabel->setText(QString("Teensy Qt ") + TY_VERSION);
}

void AboutDialog::on_websiteButton_clicked()
{
    QDesktopServices::openUrl(QUrl("https://github.com/Koromix/ty/"));
}

void AboutDialog::on_licenseButton_clicked()
{
    QDesktopServices::openUrl(QUrl("https://www.mozilla.org/MPL/2.0/"));
}

void AboutDialog::on_descriptionText_linkActivated(const QString &link)
{
    QDesktopServices::openUrl(QUrl(link));
}
