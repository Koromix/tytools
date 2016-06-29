/*
 * ty, a collection of GUI and command-line tools to manage Teensy devices
 *
 * Distributed under the MIT license (see LICENSE.txt or http://opensource.org/licenses/MIT)
 * Copyright (c) 2015 Niels Martignène <niels.martignene@gmail.com>
 */

#include <QPushButton>
#include <QSystemTrayIcon>

#include "preferences_dialog.hpp"
#include "tyqt.hpp"

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
    tyQt->setShowTrayIcon(showTrayIconCheck->isChecked());
    tyQt->setHideOnStartup(hideOnStartupCheck->isChecked());

    auto monitor = tyQt->monitor();
    monitor->setMaxTasks(maxTasksSpin->value());
}

void PreferencesDialog::reset()
{
    tyQt->clearSettingsAndResetWithConfirmation(this);
    refresh();
}

void PreferencesDialog::refresh()
{
    showTrayIconCheck->setEnabled(QSystemTrayIcon::isSystemTrayAvailable());
    showTrayIconCheck->setChecked(tyQt->showTrayIcon());
#ifndef __APPLE__
    if (!showTrayIconCheck->isChecked())
        hideOnStartupCheck->setEnabled(false);
    connect(showTrayIconCheck, &QCheckBox::toggled, hideOnStartupCheck, &QCheckBox::setEnabled);
#endif
    hideOnStartupCheck->setChecked(tyQt->hideOnStartup());

    auto monitor = tyQt->monitor();
    maxTasksSpin->setValue(monitor->maxTasks());
}
