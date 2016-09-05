/*
 * ty, a collection of GUI and command-line tools to manage Teensy devices
 *
 * Distributed under the MIT license (see LICENSE.txt or http://opensource.org/licenses/MIT)
 * Copyright (c) 2015 Niels Martignène <niels.martignene@gmail.com>
 */

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
