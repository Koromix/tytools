/*
 * ty, a collection of GUI and command-line tools to manage Teensy devices
 *
 * Distributed under the MIT license (see LICENSE.txt or http://opensource.org/licenses/MIT)
 * Copyright (c) 2015 Niels Martignène <niels.martignene@gmail.com>
 */

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
