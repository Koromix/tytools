/*
 * ty, a collection of GUI and command-line tools to manage Teensy devices
 *
 * Distributed under the MIT license (see LICENSE.txt or http://opensource.org/licenses/MIT)
 * Copyright (c) 2015 Niels Martignène <niels.martignene@gmail.com>
 */

#include <QDesktopServices>
#include <QUrl>

#include "ty.h"
#include "about_dialog.hh"

AboutDialog::AboutDialog(QWidget *parent, Qt::WindowFlags f)
    : QDialog(parent, f)
{
    setupUi(this);

    versionLabel->setText(QString("TyQt ") + TY_VERSION);
}

void AboutDialog::on_websiteButton_clicked()
{
    QDesktopServices::openUrl(QUrl("https://github.com/Koromix/ty/"));
}

void AboutDialog::on_licenseButton_clicked()
{
    QDesktopServices::openUrl(QUrl("https://opensource.org/licenses/MIT"));
}

void AboutDialog::on_descriptionText_linkActivated(const QString &link)
{
    QDesktopServices::openUrl(QUrl(link));
}
