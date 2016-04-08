/*
 * ty, a collection of GUI and command-line tools to manage Teensy devices
 *
 * Distributed under the MIT license (see LICENSE.txt or http://opensource.org/licenses/MIT)
 * Copyright (c) 2015 Niels Martignène <niels.martignene@gmail.com>
 */

#include <QPushButton>

#include "board.hh"
#include "monitor.hh"
#include "selector_dialog.hh"
#include "tyqt.hh"

using namespace std;

SelectorDialog::SelectorDialog(QWidget *parent)
    : QDialog(parent), monitor_(tyQt->monitor())
{
    setupUi(this);

    connect(buttonBox, &QDialogButtonBox::accepted, this, &SelectorDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &SelectorDialog::reject);
    connect(tree, &QTreeView::doubleClicked, this, &SelectorDialog::doubleClicked);

    tree->setModel(monitor_);
    connect(tree->selectionModel(), &QItemSelectionModel::selectionChanged, this,
            &SelectorDialog::selectionChanged);
    tree->header()->setStretchLastSection(false);
    tree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    tree->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);

    current_board_ = monitor_->board(0);
    if (current_board_) {
        tree->setCurrentIndex(monitor_->index(0, 0));
    } else {
        buttonBox->button(QDialogButtonBox::Ok)->setEnabled(false);
    }
}

void SelectorDialog::setAction(const QString &action)
{
    action_ = action;
    setWindowTitle(QString("%1 | %2").arg(action, QCoreApplication::applicationName()));
}

shared_ptr<Board> SelectorDialog::selectedBoard() const
{
    return result() ? current_board_ : nullptr;
}

void SelectorDialog::selectionChanged(const QItemSelection &selected, const QItemSelection &previous)
{
    Q_UNUSED(previous);

    if (!selected.indexes().isEmpty()) {
        current_board_ = monitor_->board(selected.indexes().front().row());
        buttonBox->button(QDialogButtonBox::Ok)->setEnabled(true);
    } else {
        current_board_ = nullptr;
        buttonBox->button(QDialogButtonBox::Ok)->setEnabled(false);
    }

    emit currentChanged(current_board_.get());
}

void SelectorDialog::doubleClicked(const QModelIndex &index)
{
    Q_UNUSED(index);
    accept();
}

void SelectorDialog::done(int result)
{
    QDialog::done(result);
    emit boardSelected(result ? current_board_.get() : nullptr);
}

shared_ptr<Board> SelectorDialog::getBoard(QWidget *parent)
{
    SelectorDialog dialog(parent);

    dialog.exec();

    return dialog.selectedBoard();
}
