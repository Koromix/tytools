/* TyTools - public domain
   Niels Martignène <niels.martignene@gmail.com>
   https://neodd.com/tytools

   This software is in the public domain. Where that dedication is not
   recognized, you are granted a perpetual, irrevocable license to copy,
   distribute, and modify this file as you see fit.

   See the LICENSE file for more details. */

#ifndef UPDATER_WINDOW_HH
#define UPDATER_WINDOW_HH

#include <QIdentityProxyModel>
#include <QProgressBar>

#include <memory>

#include "../libty/common.h"
#include "ui_updater_window.h"

class Board;
class Monitor;

class UpdaterWindowModelFilter: public QIdentityProxyModel {
    Q_OBJECT

public:
    UpdaterWindowModelFilter(QObject *parent = nullptr)
        : QIdentityProxyModel(parent) {}

    QVariant data(const QModelIndex &index, int role) const override;
};

class UpdaterWindow : public QMainWindow, private Ui::UpdaterWindow {
    Q_OBJECT

    Monitor *monitor_;
    UpdaterWindowModelFilter monitor_model_;

    std::shared_ptr<Board> current_board_;

public:
    UpdaterWindow(QWidget *parent = nullptr);

    bool event(QEvent *ev) override;

public slots:
    void showErrorMessage(const QString &msg);

    void uploadNewToCurrent();
    void resetCurrent();

#ifdef TY_CONFIG_URL_WEBSITE
    void openWebsite();
#endif
#ifdef TY_CONFIG_URL_BUGS
    void openBugReports();
#endif

private:
    void changeCurrentBoard(Board *board);
    void refreshActions();
    void refreshProgress();

    QString browseFirmwareFilter() const;

private slots:
    void currentChanged(int index);
};

#endif
