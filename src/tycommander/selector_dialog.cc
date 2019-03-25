/* TyTools - public domain
   Niels Martignène <niels.martignene@protonmail.com>
   https://koromix.dev/tytools

   This software is in the public domain. Where that dedication is not
   recognized, you are granted a perpetual, irrevocable license to copy,
   distribute, and modify this file as you see fit.

   See the LICENSE file for more details. */

#include <QIdentityProxyModel>
#include <QItemDelegate>
#include <QPushButton>

#include "board.hpp"
#include "monitor.hpp"
#include "selector_dialog.hpp"
#include "tycommander.hpp"

using namespace std;

class SelectorDialogModel: public QIdentityProxyModel {
public:
    SelectorDialogModel(QObject *parent = nullptr)
        : QIdentityProxyModel(parent) {}

    int columnCount(const QModelIndex &parent) const override;
    QVariant data(const QModelIndex &index, int role) const override;
};

class SelectorDialogItemDelegate: public QItemDelegate {
public:
    SelectorDialogItemDelegate(QObject *parent = nullptr)
        : QItemDelegate(parent) {}

    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override;
};

int SelectorDialogModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return 3;
}

QVariant SelectorDialogModel::data(const QModelIndex &index, int role) const
{
    if (index.column() == Monitor::COLUMN_BOARD) {
        switch (role) {
        case Qt::TextAlignmentRole:
            return QVariant(Qt::AlignLeft | Qt::AlignVCenter);
        }
    } else if (index.column() == Monitor::COLUMN_MODEL) {
        switch (role) {
        case Qt::TextAlignmentRole:
            return QVariant(Qt::AlignLeft | Qt::AlignVCenter);
        }
    } else if (index.column() == Monitor::COLUMN_STATUS) {
        switch (role) {
        case Qt::TextAlignmentRole:
            return QVariant(Qt::AlignRight | Qt::AlignVCenter);
        case Qt::ForegroundRole:
            return QBrush(Qt::darkGray);
        }
    }

    return QIdentityProxyModel::data(index, role);

}

QSize SelectorDialogItemDelegate::sizeHint(const QStyleOptionViewItem &option,
                                           const QModelIndex &index) const
{
    auto size = QItemDelegate::sizeHint(option, index);
    size.setHeight(24);
    return size;
}

SelectorDialog::SelectorDialog(QWidget *parent)
    : QDialog(parent), monitor_(tyCommander->monitor())
{
    setupUi(this);

    connect(buttonBox, &QDialogButtonBox::accepted, this, &SelectorDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &SelectorDialog::reject);
    connect(tree, &QTreeView::doubleClicked, this, &SelectorDialog::accept);

    monitor_model_ = new SelectorDialogModel(this);
    monitor_model_->setSourceModel(monitor_);
    tree->setModel(monitor_model_);
    tree->setItemDelegate(new SelectorDialogItemDelegate(tree));
    connect(tree->selectionModel(), &QItemSelectionModel::selectionChanged, this,
            &SelectorDialog::updateSelection);
    tree->header()->setStretchLastSection(false);
    tree->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    tree->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    tree->header()->setSectionResizeMode(2, QHeaderView::Stretch);

    auto first_board = Monitor::boardFromModel(monitor_model_, 0);
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
            selected_boards_.push_back(Monitor::boardFromModel(monitor_model_, idx));
    }
    buttonBox->button(QDialogButtonBox::Ok)->setEnabled(!selected_boards_.empty());

    emit selectionChanged();
}
