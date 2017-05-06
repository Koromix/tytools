/* TyTools - public domain
   Niels Martignène <niels.martignene@gmail.com>
   https://neodd.com/tytools

   This software is in the public domain. Where that dedication is not
   recognized, you are granted a perpetual, irrevocable license to copy,
   distribute, and modify this file as you see fit.

   See the LICENSE file for more details. */

#include <QKeyEvent>

#include "enhanced_line_edit.hpp"

EnhancedLineEdit::EnhancedLineEdit(const QString &contents, QWidget *parent)
    : QLineEdit(contents, parent)
{
}

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
        if (history_idx_)
            moveInHistory(history_idx_ - 1);
        break;
    case Qt::Key_Down:
        if (history_idx_ < history_.count())
            moveInHistory(history_idx_ + 1);
        break;

    default:
        QLineEdit::keyPressEvent(ev);
        break;
    }
}

void EnhancedLineEdit::moveInHistory(int idx)
{
    Q_ASSERT(idx >= 0);

    auto str = text();
    if (!str.isEmpty()) {
        if (history_idx_ >= history_.count()) {
            history_.append(str);
        } else {
            history_[history_idx_] = str;
        }
    }

    if (idx < history_.count()) {
        setText(history_[idx]);
    } else {
        setText("");
    }
    history_idx_ = idx;
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
