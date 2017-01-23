/* TyTools - public domain
   Niels Martignène <niels.martignene@gmail.com>
   https://neodd.com/tytools

   This software is in the public domain. Where that dedication is not
   recognized, you are granted a perpetual, irrevocable license to copy,
   distribute, and modify this file as you see fit.

   See the LICENSE file for more details. */

#include <QScrollBar>
#include <QTextBlock>

#include "enhanced_plain_text.hpp"

EnhancedPlainText::EnhancedPlainText(const QString &text, QWidget *parent)
    : QPlainTextEdit(text, parent)
{
    connect(this, &EnhancedPlainText::textChanged, this, &EnhancedPlainText::fixScrollValue);
}

void EnhancedPlainText::showEvent(QShowEvent *e)
{
    QPlainTextEdit::showEvent(e);

    /* This is a hacky way to call QPlainTextEditPrivate::_q_adjustScrollbars() without
       private headers, we need that to work around a Qt bug where the scrollbar is not
       updated correctly on text insertions while the widget is hidden. */
    QResizeEvent re(QSize(1, 1), QSize(1, 1));
    resizeEvent(&re);
}

void EnhancedPlainText::scrollContentsBy(int dx, int dy)
{
    QPlainTextEdit::scrollContentsBy(dx, dy);
    updateScrollInfo();
}

void EnhancedPlainText::keyPressEvent(QKeyEvent *e)
{
    QPlainTextEdit::keyPressEvent(e);
    /* For some reason, neither scrollContentsBy() nor the scrollbar signals are triggered
       by keyboard navigation. Is that normal, or a Qt bug? */
    updateScrollInfo();
}

void EnhancedPlainText::fixScrollValue()
{
    auto vbar = verticalScrollBar();

    if (monitor_autoscroll_) {
        vbar->setValue(vbar->maximum());
    } else {
        /* QPlainTextEdit does a good job of keeping the text steady when we append to the
           end... until maximumBlockCount kicks in. We use our own cursor monitor_cursor_,
           updated in scrollContentsBy to fix that. */
        vbar->setValue(monitor_cursor_.block().firstLineNumber());
    }
}

void EnhancedPlainText::updateScrollInfo()
{
    auto vbar = verticalScrollBar();
    auto cursor = cursorForPosition(QPoint(0, 0));

    monitor_autoscroll_ = vbar->value() >= vbar->maximum() - 1;

    /* Some functions, such as QTextDocument::clear(), really don't like when you delete
       a cursor while they run because they keep a copy of the list of cursors which
       is not updated by the cursor destructor. */
    if (!monitor_cursor_.isNull() && monitor_cursor_.document() == document()) {
        monitor_cursor_.setPosition(cursor.position());
    } else {
        monitor_cursor_ = cursor;
    }
}
