/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
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

    void setModel(const QString &model);
    void setCapabilities(const QString &capabilities);
    void setIdentity(const QString &identity);

    void setTask(const QString &msg);
    void setProgress(unsigned int progress, unsigned int total);

    bool available() const;

    QString model() const;
    QString capabilities() const;
    QString identity() const;
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
