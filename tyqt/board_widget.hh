/*
 * ty, a collection of GUI and command-line tools to manage Teensy devices
 *
 * Distributed under the MIT license (see LICENSE.txt or http://opensource.org/licenses/MIT)
 * Copyright (c) 2015 Niels Martignène <niels.martignene@gmail.com>
 */

#ifndef BOARD_WIDGET_HH
#define BOARD_WIDGET_HH

#include <QItemDelegate>

#include "ui_board_widget.h"

class Manager;

class BoardWidget : public QWidget, private Ui::BoardWidget {
    Q_OBJECT

public:
    BoardWidget(QWidget *parent = nullptr);

    void setAvailable(bool available);

    void setIcon(const QPixmap &pixmap);
    void setModel(const QString &model);
    void setTag(const QString &tag);
    void setStatus(const QString &status);

    void setProgress(unsigned int progress, unsigned int total);

    bool available() const;

    const QPixmap *icon() const;
    QString model() const;
    QString tag() const;
    QString status() const;
};

class BoardItemDelegate : public QItemDelegate {
    Q_OBJECT

    Manager *model_;

    mutable BoardWidget widget_;

public:
    BoardItemDelegate(Manager *model);

    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override;
};

#endif
