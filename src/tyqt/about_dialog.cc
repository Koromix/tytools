/*
 * ty, a collection of GUI and command-line tools to manage Teensy devices
 *
 * Distributed under the MIT license (see LICENSE.txt or http://opensource.org/licenses/MIT)
 * Copyright (c) 2015 Niels Martignène <niels.martignene@gmail.com>
 */

#include <QCoreApplication>
#include <QDesktopServices>
#include <QUrl>

#include "about_dialog.hpp"

AboutDialog::AboutDialog(QWidget *parent, Qt::WindowFlags f)
    : QDialog(parent, f)
{
    setupUi(this);
    setWindowTitle(tr("About %1").arg(QApplication::applicationName()));

    connect(closeButton, &QPushButton::clicked, this, &AboutDialog::close);
    if (TY_CONFIG_URL_BUGS[0]) {
        connect(reportBugButton, &QPushButton::clicked, &AboutDialog::openBugReports);
    } else {
        reportBugButton->hide();
    }
    connect(licenseButton, &QPushButton::clicked, &AboutDialog::openLicense);
    connect(websiteLabel, &QLabel::linkActivated, this, [](const QString &link) {
        QDesktopServices::openUrl(QUrl(link));
    });

    versionLabel->setText(QString("%1 %2").arg(QCoreApplication::applicationName(),
                                               QCoreApplication::applicationVersion()));
    if (TY_CONFIG_URL_WEBSITE[0])
        websiteLabel->setText(QString("<a href=\"%1\">%1</a>").arg(TY_CONFIG_URL_WEBSITE));
}

void AboutDialog::openWebsite()
{
    QDesktopServices::openUrl(QUrl(TY_CONFIG_URL_WEBSITE));
}

void AboutDialog::openBugReports()
{
    QDesktopServices::openUrl(QUrl(TY_CONFIG_URL_BUGS));
}

void AboutDialog::openLicense()
{
    QDesktopServices::openUrl(QUrl("https://opensource.org/licenses/MIT"));
}
