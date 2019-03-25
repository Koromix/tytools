/* TyTools - public domain
   Niels Martignène <niels.martignene@protonmail.com>
   https://koromix.dev/tytools

   This software is in the public domain. Where that dedication is not
   recognized, you are granted a perpetual, irrevocable license to copy,
   distribute, and modify this file as you see fit.

   See the LICENSE file for more details. */

#ifndef ENHANCED_WIDGETS_HH
#define ENHANCED_WIDGETS_HH

#include <QComboBox>
#include <QGroupBox>
#include <QPlainTextEdit>
#include <QProxyStyle>
#include <QStringList>

// --------------------------------------------------------
// EnhancedGroupBox
// --------------------------------------------------------

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

// --------------------------------------------------------
// EnhancedLineEdit
// --------------------------------------------------------

class EnhancedLineInput: public QComboBox {
    Q_OBJECT

public:
    EnhancedLineInput(QWidget *parent = nullptr);

public slots:
    void appendHistory(const QString &text);
    void commit();

signals:
    void textCommitted(const QString &text);

protected:
    void keyPressEvent(QKeyEvent *ev) override;
    void wheelEvent(QWheelEvent *ev) override;

private:
    void moveInHistory(int movement);
};

// --------------------------------------------------------
// EnhancedPlainText
// --------------------------------------------------------

class EnhancedPlainText: public QPlainTextEdit {
    Q_OBJECT

    bool monitor_autoscroll_ = true;
    QTextCursor monitor_cursor_;

public:
    EnhancedPlainText(QWidget *parent = nullptr)
        : EnhancedPlainText(QString(), parent) {}
    EnhancedPlainText(const QString &text, QWidget *parent = nullptr);

protected:
    void showEvent(QShowEvent *e) override;
    void scrollContentsBy(int dx, int dy) override;
    void keyPressEvent(QKeyEvent *e) override;

private slots:
    void fixScrollValue();

private:
    void updateScrollInfo();
};

#endif
