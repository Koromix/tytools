/*
 * ty, a collection of GUI and command-line tools to manage Teensy devices
 *
 * Distributed under the MIT license (see LICENSE.txt or http://opensource.org/licenses/MIT)
 * Copyright (c) 2015 Niels Martignène <niels.martignene@gmail.com>
 */

#include <QPushButton>
#include <QSystemTrayIcon>

#include "preferences_dialog.hh"
#include "tyqt.hh"

using namespace std;

PreferencesDialog::PreferencesDialog(QWidget *parent)
    : QDialog(parent)
{
    setupUi(this);

    connect(buttonBox, &QDialogButtonBox::accepted, this, &PreferencesDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &PreferencesDialog::reject);
    connect(buttonBox->button(QDialogButtonBox::Apply), &QAbstractButton::clicked, this,
            &PreferencesDialog::apply);

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
