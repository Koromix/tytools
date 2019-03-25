/* TyTools - public domain
   Niels Martignène <niels.martignene@protonmail.com>
   https://koromix.dev/tytools

   This software is in the public domain. Where that dedication is not
   recognized, you are granted a perpetual, irrevocable license to copy,
   distribute, and modify this file as you see fit.

   See the LICENSE file for more details. */

#ifndef BOARD_WIDGET_HH
#define BOARD_WIDGET_HH

#include <QItemDelegate>

#include "board.hpp"
#include "monitor.hpp"
#include "ui_board_widget.h"

class BoardWidget : public QWidget, private Ui::BoardWidget {
    Q_OBJECT

public:
    BoardWidget(QWidget *parent = nullptr);

    void setIcon(const QIcon &icon);
    void setModel(const QString &model) { modelLabel->setText(model); }
    void setTag(const QString &tag) { tagLabel->setText(tag); }
    void setStatus(const QString &status);

    void setProgress(unsigned int progress, unsigned int total);

    QRect tagGeometry() const;
};

class BoardItemDelegate : public QItemDelegate {
    Q_OBJECT

    Monitor *model_;

    mutable BoardWidget widget_;

public:
    BoardItemDelegate(Monitor *model)
        : QItemDelegate(model), model_(model) {}

    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override;

    void updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
};

#endif
