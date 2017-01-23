/* TyTools - public domain
   Niels Martignène <niels.martignene@gmail.com>
   https://neodd.com/tytools

   This software is in the public domain. Where that dedication is not
   recognized, you are granted a perpetual, irrevocable license to copy,
   distribute, and modify this file as you see fit.

   See the LICENSE file for more details. */

#ifndef ENHANCED_GROUP_BOX_HH
#define ENHANCED_GROUP_BOX_HH

#include <QGroupBox>
#include <QProxyStyle>

class EnhancedGroupBoxStyle;

class EnhancedGroupBoxStyle: public QProxyStyle {
public:
    EnhancedGroupBoxStyle(QStyle* style = nullptr)
        : QProxyStyle(style) {}
    EnhancedGroupBoxStyle(const QString &key)
        : QProxyStyle(key) {}

    void drawPrimitive(PrimitiveElement pe, const QStyleOption *opt, QPainter *p,
                       const QWidget *widget = nullptr) const override;
};

class EnhancedGroupBox: public QGroupBox {
    Q_OBJECT

    EnhancedGroupBoxStyle style_;

public:
    EnhancedGroupBox(QWidget *parent = nullptr)
        : EnhancedGroupBox(QString(), parent) {}
    EnhancedGroupBox(const QString &text, QWidget *parent = nullptr);

    void paintEvent(QPaintEvent *) override;

    bool isCollapsible() const { return QGroupBox::isCheckable(); }
    bool isExpanded() const { return isChecked(); }

public slots:
    void setCollapsible(bool collapsible);
    void setExpanded(bool expand) { setChecked(expand); }
    void expand() { setExpanded(true); }
    void collapse() { setExpanded(false); }

private:
    using QGroupBox::setCheckable;
    using QGroupBox::isCheckable;

private slots:
    void changeExpanded(bool expand);
};

#endif
