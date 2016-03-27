/*
 * ty, a collection of GUI and command-line tools to manage Teensy devices
 *
 * Distributed under the MIT license (see LICENSE.txt or http://opensource.org/licenses/MIT)
 * Copyright (c) 2015 Niels Martignène <niels.martignene@gmail.com>
 */

#ifndef PREFERENCES_DIALOG_HH
#define PREFERENCES_DIALOG_HH

#include "ui_preferences_dialog.h"

class PreferencesDialog: public QDialog, private Ui::PreferencesDialog {
public:
    PreferencesDialog(QWidget *parent = nullptr);

public slots:
    void done(int result) override;
    void apply();
};

#endif
