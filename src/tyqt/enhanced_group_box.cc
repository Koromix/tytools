/*
 * ty, a collection of GUI and command-line tools to manage Teensy devices
 *
 * Distributed under the MIT license (see LICENSE.txt or http://opensource.org/licenses/MIT)
 * Copyright (c) 2015 Niels Martignène <niels.martignene@gmail.com>
 */

#include <QLayout>
#include <QProxyStyle>
#include <QStylePainter>
#include <QStyleOptionGroupBox>

#include "enhanced_group_box.hpp"

using namespace std;

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
