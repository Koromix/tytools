/* Teensy Tools - public domain
   Niels Martignène <niels.martignene@gmail.com>
   https://neodd.com/teensytools

   This software is in the public domain. Where that dedication is not
   recognized, you are granted a perpetual, irrevocable license to copy,
   distribute, and modify this file as you see fit.

   See the LICENSE file for more details. */

#include <QPushButton>
#include <QSystemTrayIcon>

#include "preferences_dialog.hpp"
#include "teensycommander.hpp"

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
    teensyCommander->setShowTrayIcon(showTrayIconCheck->isChecked());
    teensyCommander->setHideOnStartup(hideOnStartupCheck->isChecked());

    auto monitor = teensyCommander->monitor();
    monitor->setSerialByDefault(serialByDefaultCheck->isChecked());
    monitor->setSerialLogSize(serialLogSizeDefaultSpin->value() * 1000);
    monitor->setMaxTasks(maxTasksSpin->value());
}

void PreferencesDialog::reset()
{
    teensyCommander->clearSettingsAndResetWithConfirmation(this);
    refresh();
}

void PreferencesDialog::refresh()
{
    showTrayIconCheck->setEnabled(QSystemTrayIcon::isSystemTrayAvailable());
    showTrayIconCheck->setChecked(teensyCommander->showTrayIcon());
#ifndef __APPLE__
    if (!showTrayIconCheck->isChecked())
        hideOnStartupCheck->setEnabled(false);
    connect(showTrayIconCheck, &QCheckBox::toggled, hideOnStartupCheck, &QCheckBox::setEnabled);
#endif
    hideOnStartupCheck->setChecked(teensyCommander->hideOnStartup());

    auto monitor = teensyCommander->monitor();
    serialByDefaultCheck->setChecked(monitor->serialByDefault());
    serialLogSizeDefaultSpin->setValue(monitor->serialLogSize() / 1000);
    maxTasksSpin->setValue(monitor->maxTasks());
}
