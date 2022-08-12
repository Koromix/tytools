/* TyTools - public domain
   Niels Martignène <niels.martignene@protonmail.com>
   https://koromix.dev/tytools

   This software is in the public domain. Where that dedication is not
   recognized, you are granted a perpetual, irrevocable license to copy,
   distribute, and modify this file as you see fit.

   See the LICENSE file for more details. */

#include <QDir>
#include <QFileDialog>
#include <QPushButton>
#include <QSystemTrayIcon>

#include "preferences_dialog.hpp"
#include "tycommander.hpp"

using namespace std;

PreferencesDialog::PreferencesDialog(QWidget *parent)
    : QDialog(parent)
{
    setupUi(this);
    setWindowTitle(tr("%1 Preferences").arg(QApplication::applicationName()));

    connect(buttonBox, &QDialogButtonBox::accepted, this, &PreferencesDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &PreferencesDialog::reject);
    connect(buttonBox->button(QDialogButtonBox::Apply), &QAbstractButton::clicked, this,
            &PreferencesDialog::apply);
    connect(buttonBox->button(QDialogButtonBox::Reset), &QAbstractButton::clicked,
            this, &PreferencesDialog::reset);

    connect(logSerialCheck, &QCheckBox::clicked, this, &PreferencesDialog::toggleSerialLog);
    connect(serialLogDirButton, &QToolButton::clicked, this,
            &PreferencesDialog::browseForSerialLogDir);

    refresh();
}

void PreferencesDialog::done(int result)
{
    QDialog::done(result);
    if (result)
        apply();
}

void PreferencesDialog::apply()
{
    tyCommander->setShowTrayIcon(showTrayIconCheck->isChecked());
    tyCommander->setHideOnStartup(hideOnStartupCheck->isChecked());

    auto monitor = tyCommander->monitor();
    monitor->setIgnoreGeneric(ignoreGenericCheck->isChecked());
    monitor->setSerialByDefault(serialByDefaultCheck->isChecked());
    monitor->setSerialLogDir(serialLogDir->text());
    if (logSerialCheck->isChecked()) {
        monitor->setSerialLogSize(serialLogSizeDefaultSpin->value() * 1000);
    } else {
        monitor->setSerialLogSize(-1);
    }
    monitor->setMaxTasks(maxTasksSpin->value());
}

void PreferencesDialog::reset()
{
    tyCommander->clearSettingsAndResetWithConfirmation(this);
    refresh();
}

void PreferencesDialog::refresh()
{
    showTrayIconCheck->setEnabled(QSystemTrayIcon::isSystemTrayAvailable());
    showTrayIconCheck->setChecked(tyCommander->showTrayIcon());
#ifndef __APPLE__
    if (!showTrayIconCheck->isChecked())
        hideOnStartupCheck->setEnabled(false);
    connect(showTrayIconCheck, &QCheckBox::toggled, hideOnStartupCheck, &QCheckBox::setEnabled);
#endif
    hideOnStartupCheck->setChecked(tyCommander->hideOnStartup());

    auto monitor = tyCommander->monitor();
    ignoreGenericCheck->setChecked(monitor->ignoreGeneric());
    serialByDefaultCheck->setChecked(monitor->serialByDefault());
    serialLogDir->setText(monitor->serialLogDir());
    if (monitor->serialLogSize() >= 0) {
        logSerialCheck->setChecked(true);
        serialLogSizeDefaultSpin->setEnabled(true);
        serialLogSizeDefaultSpin->setValue(static_cast<int>(monitor->serialLogSize() / 1000));
    } else {
        logSerialCheck->setChecked(false);
        serialLogSizeDefaultSpin->setEnabled(false);
    }
    maxTasksSpin->setValue(monitor->maxTasks());
}

void PreferencesDialog::toggleSerialLog(bool enable)
{
    serialLogSizeDefaultSpin->setEnabled(enable);
}

void PreferencesDialog::browseForSerialLogDir()
{
    auto dir = QFileDialog::getExistingDirectory(this);
    if (dir.isEmpty())
        return;

    dir = QDir::toNativeSeparators(dir);
    serialLogDir->setText(dir);
}
