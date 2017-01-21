/* Teensy Tools - public domain
   Niels Martignène <niels.martignene@gmail.com>
   https://neodd.com/teensytools

   This software is in the public domain. Where that dedication is not
   recognized, you are granted a perpetual, irrevocable license to copy,
   distribute, and modify this file as you see fit.

   See the LICENSE file for more details. */

#ifndef ENHANCED_PLAIN_TEXT_HH
#define ENHANCED_PLAIN_TEXT_HH

#include <QPlainTextEdit>

class EnhancedPlainText: public QPlainTextEdit {
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
