/*
 * ty, a collection of GUI and command-line tools to manage Teensy devices
 *
 * Distributed under the MIT license (see LICENSE.txt or http://opensource.org/licenses/MIT)
 * Copyright (c) 2015 Niels Martignène <niels.martignene@gmail.com>
 */

#ifndef SELECTOR_DIALOG_HH
#define SELECTOR_DIALOG_HH

#include <memory>

#include "ui_selector_dialog.h"

class Board;
class Manager;

class SelectorDialog : public QDialog, private Ui::SelectorDialog {
    Q_OBJECT

    Manager *manager_;
    QString action_;

    std::shared_ptr<Board> current_board_;

public:
    SelectorDialog(Manager *manager, QWidget *parent = nullptr);

    void setAction(const QString &action);
    QString action() const { return action_; }

    void setDescription(const QString &desc) { descriptionLabel->setText(desc); }
    QString description() const { return descriptionLabel->text(); }

    std::shared_ptr<Board> currentBoard() const { return current_board_; }
    std::shared_ptr<Board> selectedBoard() const;

    static std::shared_ptr<Board> getBoard(Manager *manager, QWidget *parent = nullptr);

protected slots:
    void selectionChanged(const QItemSelection &selected, const QItemSelection &previous);
    void doubleClicked(const QModelIndex &index);
    void done(int result) override;

signals:
    void currentChanged(Board *board);
    void boardSelected(Board *board);
};

#endif
