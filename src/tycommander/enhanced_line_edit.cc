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

void EnhancedLineEdit::setHistoryLimit(int limit)
{
    setMaxCount(limit);
}

void EnhancedLineEdit::appendHistory(const QString &str)
{
    if (count() >= maxCount()) removeItem(0);
    addItem(str);
    setCurrentIndex(-1);
}

QString EnhancedLineEdit::commitAndClearText()
{
    auto str = lineEdit()->text();
    lineEdit()->setText("");

    if (!str.isEmpty()) {
        if (currentIndex() >= 0)
            removeItem(currentIndex());
        if (count() >= maxCount())
            removeItem(0);
        addItem(str);
        setCurrentIndex(-1);
    }
    return str;
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
    int delta = ev->angleDelta().y();
    if (delta && (delta > 0) == (wheel_delta_ < 0))
        wheel_delta_ = 0;
    wheel_delta_ += delta;

    int relative_idx = -(wheel_delta_ / 120);
    wheel_delta_ %= 120;

    if (!relative_idx)
        return;
    int sign = relative_idx > 0 ? 1 : -1;
    while (relative_idx) {
        moveInHistory(sign);
        relative_idx -= sign;
    }
}

void EnhancedLineEdit::moveInHistory(int movement)
{
    if (movement != -1 && movement != 1) return;

    auto str = lineEdit()->text();

    if (movement == -1) {
        if (currentIndex() == -1) {
            if (count() >= maxCount() && !str.isEmpty())
                removeItem(0);
            int go_to_pos = count() >= 1 ? count() - 1 : count();
            if (!str.isEmpty())
                addItem(str);
            setCurrentIndex(go_to_pos);
            lineEdit()->setText(itemText(go_to_pos));
        } else {
            if (!str.isEmpty())
                setItemText(currentIndex(), str);
            auto go_to_pos = currentIndex() > 0 ? currentIndex() - 1 : 0;
            setCurrentIndex(go_to_pos);
            lineEdit()->setText(itemText(go_to_pos));
        }
    }
    if (movement == 1 && !str.isEmpty()) {
        if (currentIndex() == -1) {
            if(count() >= maxCount())
                removeItem(0);
            addItem(str);
            setCurrentIndex(-1);
            lineEdit()->setText("");
        } else {
            setItemText(currentIndex(), str);
            setCurrentIndex(currentIndex() + 1);
        }
    }
    if (movement == 1 && str.isEmpty()) {
        if (currentIndex() == -1) {
            // do nothing
        } else {
            setItemText(currentIndex(), str);
            setCurrentIndex(currentIndex() + 1);
            lineEdit()->setText(itemText(currentIndex()));
        }
    }
}
