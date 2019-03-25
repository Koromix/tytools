/* TyTools - public domain
   Niels Martignène <niels.martignene@protonmail.com>
   https://koromix.dev/tytools

   This software is in the public domain. Where that dedication is not
   recognized, you are granted a perpetual, irrevocable license to copy,
   distribute, and modify this file as you see fit.

   See the LICENSE file for more details. */

#ifndef SELECTOR_DIALOG_HH
#define SELECTOR_DIALOG_HH

#include <memory>
#include <vector>

#include "ui_selector_dialog.h"

class Board;
class Monitor;
class SelectorDialogModel;

class SelectorDialog : public QDialog, private Ui::SelectorDialog {
    Q_OBJECT

    Monitor *monitor_;
    SelectorDialogModel *monitor_model_;
    QString action_;

    std::vector<std::shared_ptr<Board>> selected_boards_;

public:
    SelectorDialog(QWidget *parent = nullptr);

    void setExtendedSelection(bool extended);
    bool setExtendedSelection() const
        { return tree->selectionMode() == QAbstractItemView::ExtendedSelection; }

    void setAction(const QString &action);
    QString action() const { return action_; }

    void setDescription(const QString &desc) { descriptionLabel->setText(desc); }
    QString description() const { return descriptionLabel->text(); }

    std::vector<std::shared_ptr<Board>> selectedBoards() const { return selected_boards_; }

protected slots:
    void updateSelection();

signals:
    void selectionChanged();
};

#endif
