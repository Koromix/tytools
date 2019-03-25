/* TyTools - public domain
   Niels Martignène <niels.martignene@protonmail.com>
   https://koromix.dev/tytools

   This software is in the public domain. Where that dedication is not
   recognized, you are granted a perpetual, irrevocable license to copy,
   distribute, and modify this file as you see fit.

   See the LICENSE file for more details. */

#ifndef ABOUT_HH
#define ABOUT_HH

#include "../libty/common.h"
#include "ui_about_dialog.h"

class AboutDialog: public QDialog, private Ui::AboutDialog {
    Q_OBJECT

public:
    AboutDialog(QWidget *parent = nullptr, Qt::WindowFlags f = 0);

public slots:
    static void openWebsite();
    static void openBugReports();
    static void openLicense();
};

#endif
