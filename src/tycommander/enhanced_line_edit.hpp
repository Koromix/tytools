/* TyTools - public domain
   Niels Martignène <niels.martignene@gmail.com>
   https://neodd.com/tytools

   This software is in the public domain. Where that dedication is not
   recognized, you are granted a perpetual, irrevocable license to copy,
   distribute, and modify this file as you see fit.

   See the LICENSE file for more details. */

#ifndef ENHANCED_LINE_EDIT_HH
#define ENHANCED_LINE_EDIT_HH

#include <QStringList>
#include <QLineEdit>

class EnhancedLineEdit: public QLineEdit {
    Q_OBJECT

    int history_limit_ = 100;

    QStringList history_;
    int history_idx_ = 0;

    int wheel_delta_ = 0;

public:
    EnhancedLineEdit(QWidget *parent = nullptr)
        : QLineEdit(parent) {}
    EnhancedLineEdit(const QString &contents, QWidget *parent = nullptr)
        : QLineEdit(contents, parent) {}

public:
    int historyLimit() const { return history_limit_; }
    QStringList history() const { return history_; }

public slots:
    void setHistoryLimit(int limit);
    void setHistory(const QStringList &history);
    void appendHistory(const QString &str);

    QString commitAndClearText();

protected:
    void keyPressEvent(QKeyEvent *ev) override;
    void wheelEvent(QWheelEvent *ev) override;

private:
    void moveInHistory(int relative_idx);
    void clearOldHistory();
};

#endif
