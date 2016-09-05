/*
 * ty, a collection of GUI and command-line tools to manage Teensy devices
 *
 * Distributed under the MIT license (see LICENSE.txt or http://opensource.org/licenses/MIT)
 * Copyright (c) 2015 Niels Martignène <niels.martignene@gmail.com>
 */

#include <QPushButton>

#include "tyqt/board.hpp"
#include "tyqt/monitor.hpp"
#include "selector_dialog.hpp"
#include "tyqt.hpp"

using namespace std;

int SelectorDialogModelFilter::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return 2;
}

QVariant SelectorDialogModelFilter::data(const QModelIndex &index, int role) const
{
    if (index.column() == Monitor::COLUMN_STATUS) {
        switch (role) {
        case Qt::ForegroundRole:
            return QBrush(Qt::darkGray);
        case Qt::TextAlignmentRole:
            return QVariant(Qt::AlignRight | Qt::AlignVCenter);
        }
    }

    return QIdentityProxyModel::data(index, role);

}

SelectorDialog::SelectorDialog(QWidget *parent)
    : QDialog(parent), monitor_(tyQt->monitor())
{
    setupUi(this);

    connect(buttonBox, &QDialogButtonBox::accepted, this, &SelectorDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &SelectorDialog::reject);
    connect(tree, &QTreeView::doubleClicked, this, &SelectorDialog::accept);

    monitor_model_.setSourceModel(monitor_);
    tree->setModel(&monitor_model_);
    connect(tree->selectionModel(), &QItemSelectionModel::selectionChanged, this,
            &SelectorDialog::updateSelection);
    tree->header()->setStretchLastSection(false);
    tree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    tree->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);

    auto first_board = Monitor::boardFromModel(&monitor_model_, 0);
    if (first_board) {
        tree->setCurrentIndex(monitor_->index(0, 0));
    } else {
        buttonBox->button(QDialogButtonBox::Ok)->setEnabled(false);
    }
}

void SelectorDialog::setExtendedSelection(bool extended)
{
    tree->setSelectionMode(extended
                           ? QAbstractItemView::ExtendedSelection
                           : QAbstractItemView::SingleSelection);
}

void SelectorDialog::setAction(const QString &action)
{
    action_ = action;
    setWindowTitle(QString("%1 | %2").arg(action, QCoreApplication::applicationName()));
}

void SelectorDialog::updateSelection()
{
    selected_boards_.clear();
    auto indexes = tree->selectionModel()->selectedIndexes();
    qSort(indexes);
    for (auto &idx: indexes) {
        if (idx.column() == 0)
            selected_boards_.push_back(Monitor::boardFromModel(&monitor_model_, idx));
    }
    buttonBox->button(QDialogButtonBox::Ok)->setEnabled(!selected_boards_.empty());

    emit selectionChanged();
}
