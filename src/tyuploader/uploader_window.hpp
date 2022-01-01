/* TyTools - public domain
   Niels Martignène <niels.martignene@protonmail.com>
   https://koromix.dev/tytools

   This software is in the public domain. Where that dedication is not
   recognized, you are granted a perpetual, irrevocable license to copy,
   distribute, and modify this file as you see fit.

   See the LICENSE file for more details. */

#ifndef uploader_window_HH
#define uploader_window_HH

#include <QIdentityProxyModel>
#include <QProgressBar>

#include <memory>

#include "../libty/common.h"
#include "ui_uploader_window.h"

class Board;
class Monitor;

class UploaderWindowModelFilter: public QIdentityProxyModel {
    Q_OBJECT

public:
    UploaderWindowModelFilter(QObject *parent = nullptr)
        : QIdentityProxyModel(parent) {}

    QVariant data(const QModelIndex &index, int role) const override;
};

class UploaderWindow : public QMainWindow, private Ui::UploaderWindow {
    Q_OBJECT

    Monitor *monitor_;
    UploaderWindowModelFilter monitor_model_;

    std::shared_ptr<Board> current_board_;

public:
    UploaderWindow(QWidget *parent = nullptr);

    bool event(QEvent *ev) override;

public slots:
    void showErrorMessage(const QString &msg);

    void uploadNewToCurrent();
    void resetCurrent();

    void openWebsite();
    void openBugReports();

private:
    void changeCurrentBoard(Board *board);
    void refreshActions();
    void refreshProgress();

    QString browseFirmwareFilter() const;

private slots:
    void currentChanged(int index);
};

#endif
