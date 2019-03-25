/* TyTools - public domain
   Niels Martignène <niels.martignene@protonmail.com>
   https://koromix.dev/tytools

   This software is in the public domain. Where that dedication is not
   recognized, you are granted a perpetual, irrevocable license to copy,
   distribute, and modify this file as you see fit.

   See the LICENSE file for more details. */

#include <QKeyEvent>
#include <QLayout>
#include <QLineEdit>
#include <QProxyStyle>
#include <QScrollBar>
#include <QStylePainter>
#include <QStyleOptionGroupBox>
#include <QTextBlock>

#include "enhanced_widgets.hpp"

using namespace std;

// --------------------------------------------------------
// EnhancedGroupBox
// --------------------------------------------------------

void EnhancedGroupBoxStyle::drawPrimitive(PrimitiveElement pe, const QStyleOption *opt,
                                          QPainter *p, const QWidget *widget) const
{
    if (pe == QStyle::PE_IndicatorCheckBox) {
        auto group_box = qobject_cast<const EnhancedGroupBox *>(widget);
        if (group_box) {
            auto arrow = group_box->isChecked() ? QStyle::PE_IndicatorArrowDown
                                                : QStyle::PE_IndicatorArrowRight;
            QProxyStyle::drawPrimitive(arrow, opt, p, widget);
            return;
        }
    }

    QProxyStyle::drawPrimitive(pe, opt, p, widget);
}

EnhancedGroupBox::EnhancedGroupBox(const QString &text, QWidget *parent)
    : QGroupBox(text, parent)
{
    setStyle(&style_);
    connect(this, &QGroupBox::toggled, this, &EnhancedGroupBox::changeExpanded);
}

void EnhancedGroupBox::paintEvent(QPaintEvent *)
{
    QStylePainter paint(this);
    QStyleOptionGroupBox option;

    initStyleOption(&option);
    if (isCheckable() && !isChecked())
        option.subControls &= ~QStyle::SC_GroupBoxFrame;
    paint.drawComplexControl(QStyle::CC_GroupBox, option);
}

void EnhancedGroupBox::setCollapsible(bool collapsible)
{
    if (!collapsible)
        setChecked(true);
    setCheckable(collapsible);
}

void EnhancedGroupBox::changeExpanded(bool checked)
{
    if (checked) {
        setMaximumHeight(16777215);
    } else {
        QStyleOptionGroupBox option;
        initStyleOption(&option);
        auto label_rect = style()->subControlRect(QStyle::CC_GroupBox, &option,
                                                  QStyle::SC_GroupBoxLabel, this);
        setMaximumHeight(label_rect.bottom());
    }
}

// --------------------------------------------------------
// EnhancedLineInput
// --------------------------------------------------------

EnhancedLineInput::EnhancedLineInput(QWidget *parent)
    : QComboBox(parent)
{
    setFocusPolicy(Qt::StrongFocus);
    setInsertPolicy(QComboBox::NoInsert);
    setEditable(true);
    setMaxCount(10000);

    connect(lineEdit(), &QLineEdit::returnPressed, this, &EnhancedLineInput::commit);
}

void EnhancedLineInput::appendHistory(const QString &text)
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

void EnhancedLineInput::commit()
{
    auto text = currentText();
    appendHistory(text);
    setCurrentIndex(-1);
    emit textCommitted(text);
}

void EnhancedLineInput::keyPressEvent(QKeyEvent *ev)
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

void EnhancedLineInput::wheelEvent(QWheelEvent *ev)
{
    if (ev->delta() > 0) {
        moveInHistory(-1);
    } else if (ev->delta() < 0) {
        moveInHistory(1);
    }
}

void EnhancedLineInput::moveInHistory(int movement)
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

// --------------------------------------------------------
// EnhancedPlainText
// --------------------------------------------------------

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
