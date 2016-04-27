/*
 * ty, a collection of GUI and command-line tools to manage Teensy devices
 *
 * Distributed under the MIT license (see LICENSE.txt or http://opensource.org/licenses/MIT)
 * Copyright (c) 2015 Niels Martignène <niels.martignene@gmail.com>
 */

#include <QCoreApplication>
#include <QDesktopServices>
#include <QUrl>

#include "about_dialog.hh"

AboutDialog::AboutDialog(QWidget *parent, Qt::WindowFlags f)
    : QDialog(parent, f)
{
    setupUi(this);

    connect(closeButton, &QPushButton::clicked, this, &AboutDialog::close);
    connect(reportBugButton, &QPushButton::clicked, &AboutDialog::openBugReports);
    connect(licenseButton, &QPushButton::clicked, &AboutDialog::openLicense);
    connect(descriptionText, &QLabel::linkActivated, this, &AboutDialog::openLink);

    versionLabel->setText(QString("%1 %2").arg(QCoreApplication::applicationName(),
                                               QCoreApplication::applicationVersion()));
}

void AboutDialog::openWebsite()
{
    QDesktopServices::openUrl(QUrl("https://github.com/Koromix/ty"));
}

void AboutDialog::openBugReports()
{
    QDesktopServices::openUrl(QUrl("https://github.com/Koromix/ty/issues"));
}

void AboutDialog::openLicense()
{
    QDesktopServices::openUrl(QUrl("https://opensource.org/licenses/MIT"));
}

void AboutDialog::openLink(const QString &link)
{
    QDesktopServices::openUrl(QUrl(link));
}
