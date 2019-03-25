/* TyTools - public domain
   Niels Martignène <niels.martignene@protonmail.com>
   https://koromix.dev/tytools

   This software is in the public domain. Where that dedication is not
   recognized, you are granted a perpetual, irrevocable license to copy,
   distribute, and modify this file as you see fit.

   See the LICENSE file for more details. */

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

    versionLabel->setText(QString("%1\n%2")
                          .arg(QCoreApplication::applicationName(),
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
    QDesktopServices::openUrl(QUrl("http://unlicense.org/"));
}
