/* TyTools - public domain
   Niels Martignène <niels.martignene@gmail.com>
   https://neodd.com/tytools

   This software is in the public domain. Where that dedication is not
   recognized, you are granted a perpetual, irrevocable license to copy,
   distribute, and modify this file as you see fit.

   See the LICENSE file for more details. */

#include <QKeyEvent>
#include <QWheelEvent>

#include "enhanced_line_edit.hpp"

void EnhancedLineEdit::setHistoryLimit(int limit)
{
    history_limit_ = limit;
    clearOldHistory();
}

void EnhancedLineEdit::setHistory(const QStringList &history)
{
    history_ = history;
    clearOldHistory();
    history_idx_ = history.count();
}

void EnhancedLineEdit::appendHistory(const QString &str)
{
    history_.append(str);
    if (history_idx_ == history_.count() - 1)
        history_idx_++;
}

QString EnhancedLineEdit::commitAndClearText()
{
    auto str = text();
    setText("");

    if (history_limit_ != 0 && !str.isEmpty() &&
            (history_.isEmpty() || history_idx_ != history_.count() - 1 ||
             str != history_.last())) {
        clearOldHistory();
        history_.append(str);
    }
    history_idx_ = history_.count();

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
        QLineEdit::keyPressEvent(ev);
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
    moveInHistory(relative_idx);
}

void EnhancedLineEdit::moveInHistory(int relative_idx)
{
    int new_idx = history_idx_ + relative_idx;
    if (new_idx < 0) {
        new_idx = 0;
    } else if (new_idx > history_.count()) {
        new_idx = history_.count();
    }

    auto str = text();
    if (!str.isEmpty()) {
        if (history_idx_ >= history_.count()) {
            history_.append(str);
        } else {
            history_[history_idx_] = str;
        }
    }

    if (new_idx < history_.count()) {
        setText(history_[new_idx]);
    } else {
        setText("");
    }
    history_idx_ = new_idx;
}

void EnhancedLineEdit::clearOldHistory()
{
    if (history_limit_ > 0) {
        while (history_.count() >= history_limit_) {
            history_.removeFirst();
            history_idx_--;
        }
        if (history_idx_ < 0)
            history_idx_ = history_.count();
    } else if (!history_limit_) {
        history_.clear();
        history_idx_ = 0;
    }
}
