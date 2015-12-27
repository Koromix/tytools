/*
 * ty, a collection of GUI and command-line tools to manage Teensy devices
 *
 * Distributed under the MIT license (see LICENSE.txt or http://opensource.org/licenses/MIT)
 * Copyright (c) 2015 Niels Martignène <niels.martignene@gmail.com>
 */

#include <QPainter>

#include "board_widget.hh"

using namespace std;

BoardWidget::BoardWidget(QWidget *parent)
    : QWidget(parent)
{
    setupUi(this);
}

void BoardWidget::setAvailable(bool available)
{
    boardIcon->setEnabled(available);
}

void BoardWidget::setProgress(unsigned int progress, unsigned int total)
{
    if (total) {
        stackedWidget->setCurrentIndex(1);

        taskProgress->setRange(0, total);
        taskProgress->setValue(progress);
    } else {
        stackedWidget->setCurrentIndex(0);
    }
}

void BoardItemDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    if (index.row() >= static_cast<int>(model_->boardCount()))
        return;

    auto board = model_->board(index.row());

    widget_.resize(option.rect.size());

    widget_.setAvailable(board->state() == TYB_BOARD_STATE_ONLINE);

    widget_.setIcon(QPixmap(board->errorOccured() ? ":/board_error" : ":/board"));
    widget_.setModel(board->modelName());
    widget_.setTag(board->tag());
    // FIXME: add better way to detect current board mode
    if (board->isSerialAvailable()) {
        widget_.setStatus(!board->firmwareName().isEmpty() ? board->firmwareName() : tr("(running)"));
    } else if (board->isUploadAvailable()) {
        widget_.setStatus(tr("(bootloader)"));
    } else {
        widget_.setStatus(tr("(missing)"));
    }

    auto task = board->runningTask();
    if (task.status() == TY_TASK_STATUS_RUNNING) {
        widget_.setProgress(task.progress(), task.progressMaximum());
    } else {
        widget_.setProgress(0, 0);
    }

    QPalette pal = option.palette;
    if (option.state & QStyle::State_Selected) {
        pal.setBrush(QPalette::Window, option.palette.brush(QPalette::Highlight));
        pal.setColor(QPalette::WindowText, option.palette.color(QPalette::HighlightedText));
    } else {
        pal.setBrush(QPalette::Window, QBrush(QColor(Qt::transparent)));
    }
    widget_.setPalette(pal);

    painter->save();
    painter->translate(option.rect.topLeft());

    widget_.render(painter);

    painter->restore();
}

QSize BoardItemDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    Q_UNUSED(option);
    Q_UNUSED(index);

    return QSize(widget_.minimumWidth(), widget_.height());
}
