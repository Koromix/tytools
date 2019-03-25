/* TyTools - public domain
   Niels Martignène <niels.martignene@protonmail.com>
   https://koromix.dev/tytools

   This software is in the public domain. Where that dedication is not
   recognized, you are granted a perpetual, irrevocable license to copy,
   distribute, and modify this file as you see fit.

   See the LICENSE file for more details. */

#ifndef PREFERENCES_DIALOG_HH
#define PREFERENCES_DIALOG_HH

#include "ui_preferences_dialog.h"

class PreferencesDialog: public QDialog, private Ui::PreferencesDialog {
public:
    PreferencesDialog(QWidget *parent = nullptr);

public slots:
    void done(int result) override;
    void apply();
    void reset();

private:
    void refresh();

private slots:
    void browseForSerialLogDir();
};

#endif
