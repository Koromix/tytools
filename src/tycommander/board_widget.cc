/* TyTools - public domain
   Niels Martignène <niels.martignene@protonmail.com>
   https://koromix.dev/tytools

   This software is in the public domain. Where that dedication is not
   recognized, you are granted a perpetual, irrevocable license to copy,
   distribute, and modify this file as you see fit.

   See the LICENSE file for more details. */

#include <QFontMetrics>
#include <QLineEdit>
#include <QPainter>

#include "board.hpp"
#include "board_widget.hpp"

using namespace std;

BoardWidget::BoardWidget(QWidget *parent)
    : QWidget(parent)
{
    setupUi(this);
}

void BoardWidget::setIcon(const QIcon &icon)
{
    boardIcon->setPixmap(icon.pixmap(boardIcon->size()));
}

void BoardWidget::setStatus(const QString &status)
{
    QFontMetrics metrics(statusLabel->font());
    statusLabel->setText(metrics.elidedText(status, Qt::ElideRight, statusLabel->width()));
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

QRect BoardWidget::tagGeometry() const
{
    auto geometry = tagLabel->geometry();
    geometry.moveTo(tagLabel->mapTo(this, QPoint()));
    return geometry;
}

void BoardItemDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    auto board = Monitor::boardFromModel(model_, index);
    if (!board)
        return;

    widget_.resize(option.rect.size());

    widget_.setIcon(board->statusIcon());
    widget_.setModel(board->modelName());
    widget_.setTag(board->tag());
    widget_.setStatus(board->statusText());

    auto task = board->task();
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

void BoardItemDelegate::updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    Q_UNUSED(index);

    if (!editor)
        return;

    auto geometry = widget_.tagGeometry();
    geometry.moveTopLeft(option.rect.topLeft() + geometry.topLeft());
    editor->setGeometry(geometry);
}
