/* TyTools - public domain
   Niels Martignène <niels.martignene@gmail.com>
   https://neodd.com/tytools

   This software is in the public domain. Where that dedication is not
   recognized, you are granted a perpetual, irrevocable license to copy,
   distribute, and modify this file as you see fit.

   See the LICENSE file for more details. */

#include <QKeyEvent>
#include <QLineEdit>

#include "enhanced_line_edit.hpp"

EnhancedLineEdit::EnhancedLineEdit(QWidget *parent)
    : QComboBox(parent)
{
    setFocusPolicy(Qt::StrongFocus);
    setInsertPolicy(QComboBox::NoInsert);
    setEditable(true);
    setMaxCount(10000);

    connect(lineEdit(), &QLineEdit::returnPressed, this, &EnhancedLineEdit::commit);
}

void EnhancedLineEdit::appendHistory(const QString &text)
{
    if (text.isEmpty())
        return;
    if (count() && text == itemText(count() - 1))
        return;

    if (count() >= maxCount()) {
        if (currentIndex() == 0) {
            auto text = currentText();
            setCurrentIndex(-1);
            setCurrentText(text);
        }
        removeItem(0);
    }
    addItem(text);
}

void EnhancedLineEdit::commit()
{
    auto text = currentText();
    appendHistory(text);
    setCurrentIndex(-1);
    emit textCommitted(text);
}

void EnhancedLineEdit::keyPressEvent(QKeyEvent *ev)
{
    switch (ev->key()) {
    case Qt::Key_Up:
        moveInHistory(-1);
        break;
    case Qt::Key_Down:
        moveInHistory(1);
        break;

    default:
        QComboBox::keyPressEvent(ev);
        break;
    }
}

void EnhancedLineEdit::wheelEvent(QWheelEvent *ev)
{
    if (ev->delta() > 0) {
        moveInHistory(-1);
    } else if (ev->delta() < 0) {
        moveInHistory(1);
    }
}

void EnhancedLineEdit::moveInHistory(int movement)
{
    int current_idx = currentIndex();
    int new_idx = current_idx;
    if (movement < 0) {
        if (current_idx == -1) {
            new_idx = count() + movement;
        } else {
            new_idx = current_idx + movement;
        }
        if (new_idx < 0)
            new_idx = 0;
    } else if (movement > 0) {
        if (current_idx == -1) {
            new_idx = -1;
        } else {
            new_idx = current_idx + movement;
            if (new_idx >= count())
                new_idx = -1;
        }
    }

    auto text = currentText();
    setCurrentIndex(new_idx);
    if (current_idx == -1) {
        appendHistory(text);
    } else {
        setItemText(current_idx, text);
    }
}
