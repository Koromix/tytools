/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <QPushButton>

#include "selector_dialog.hh"

using namespace std;

SelectorDialog::SelectorDialog(Manager *manager, QWidget *parent)
    : QDialog(parent), manager_(manager)
{
    setupUi(this);

    tree->setModel(manager);
    connect(tree->selectionModel(), &QItemSelectionModel::selectionChanged, this, &SelectorDialog::selectionChanged);

    tree->header()->setStretchLastSection(false);
    tree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    tree->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);

    current_board_ = manager->board(0);
    if (current_board_) {
        tree->setCurrentIndex(manager->index(0, 0));
    } else {
        buttonBox->button(QDialogButtonBox::Ok)->setEnabled(false);
    }
}

std::shared_ptr<Board> SelectorDialog::currentBoard() const
{
    return current_board_;
}

std::shared_ptr<Board> SelectorDialog::selectedBoard() const
{
    return result() ? current_board_ : nullptr;
}

void SelectorDialog::selectionChanged(const QItemSelection &selected, const QItemSelection &previous)
{
    TY_UNUSED(previous);

    if (!selected.indexes().isEmpty()) {
        current_board_ = manager_->board(selected.indexes().front().row());
        buttonBox->button(QDialogButtonBox::Ok)->setEnabled(true);
    } else {
        current_board_ = nullptr;
        buttonBox->button(QDialogButtonBox::Ok)->setEnabled(false);
    }

    emit currentChanged(current_board_);
}

void SelectorDialog::done(int result)
{
    QDialog::done(result);
    emit boardSelected(result ? currentBoard() : nullptr);
}

shared_ptr<Board> SelectorDialog::getBoard(Manager *manager, QWidget *parent)
{
    SelectorDialog dialog(manager, parent);

    dialog.exec();

    return dialog.selectedBoard();
}
