/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef BOARD_HH
#define BOARD_HH

#include <QAbstractListModel>
#include <QTextDocument>
#include <QThread>

#include <memory>
#include <vector>

#include "ty.h"
#include "descriptor_set_notifier.hh"

class Manager;
class Board;

struct BoardInterfaceInfo {
    QString desc;
    QString path;

    uint16_t capabilities;
    uint8_t number;
};

class BoardCommand;
class BoardWorker : public QObject
{
    Q_OBJECT

    BoardCommand *running_task_ = nullptr;

private:
    BoardWorker(QObject *parent = nullptr)
        : QObject(parent) {}

    void reportTaskProgress(unsigned int progress, unsigned int total);
    void reportTaskProgress() { reportTaskProgress(0, 0); }

signals:
    void taskProgress(const QString &msg, unsigned int progress, unsigned int total);

protected:
    virtual void customEvent(QEvent *ev) override;

    friend class Board;
};

class Board : public QObject {
    Q_OBJECT

    QThread *thread_;
    BoardWorker *worker_;

    tyb_board *board_;

    DescriptorSetNotifier serial_notifier_;
    bool serial_available_ = false;
    bool clear_on_reset_ = false;

    QTextDocument serial_document_;

    QString task_msg_;
    unsigned int task_progress_ = 0;
    unsigned int task_total_ = 0;

public:
    Board(tyb_board *board, QObject *parent = nullptr);
    virtual ~Board();

    tyb_board *board() const;

    bool matchesIdentity(const QString &id);

    tyb_board_state state() const;
    uint16_t capabilities() const;

    const tyb_board_model *model() const;
    QString modelName() const;
    QString modelDesc() const;

    QString identity() const;
    QString location() const;
    uint64_t serialNumber() const;

    std::vector<BoardInterfaceInfo> interfaces() const;

    bool isUploadAvailable() const;
    bool isResetAvailable() const;
    bool isRebootAvailable() const;
    bool isSerialAvailable() const;

    void setClearOnReset(bool clear);
    bool clearOnReset() const;

    QTextDocument &serialDocument();
    void appendToSerialDocument(const QString& s);

    void sendSerial(const QByteArray &buf);

    QString runningTask(unsigned int *progress, unsigned int *total) const;

    virtual bool event(QEvent *e) override;

    static QStringList makeCapabilityList(uint16_t capabilities);
    static QString makeCapabilityString(uint16_t capabilities, QString empty_str = QString());

public slots:
    void upload(const QString &filename, bool reset_after = true);
    void reset();
    void reboot();

signals:
    void boardChanged();
    void boardDropped();

    void taskProgress(const Board &board, const QString &msg, size_t progress, size_t total);

    void propertyChanged(const char *name, const QVariant &value);

private slots:
    void serialReceived(ty_descriptor desc);
    void reportTaskProgress(const QString &msg, unsigned int progress, unsigned int total);

private:
    void refreshBoard();

    friend class Manager;
    friend class BoardWorker;
};

class Manager : public QAbstractListModel {
    Q_OBJECT

    tyb_monitor *manager_ = nullptr;
    DescriptorSetNotifier manager_notifier_;

    std::vector<std::shared_ptr<Board>> boards_;

public:
    typedef decltype(boards_)::iterator iterator;
    typedef decltype(boards_)::const_iterator const_iterator;

    Manager(QObject *parent = nullptr)
        : QAbstractListModel(parent) {}
    virtual ~Manager();

    bool start();

    tyb_monitor *manager() const { return manager_; }

    iterator begin() { return boards_.begin(); }
    iterator end() { return boards_.end(); }
    const_iterator cbegin() const { return boards_.cbegin(); }
    const_iterator cend() const { return boards_.cend(); }

    std::vector<std::shared_ptr<Board>> boards();
    std::shared_ptr<Board> board(unsigned int i);
    unsigned int boardCount() const;

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;

    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;

signals:
    void boardAdded(std::shared_ptr<Board> board);

private slots:
    void refreshManager(ty_descriptor desc);
    void updateTaskProgress(const Board &board, const QString &msg, size_t progress, size_t total);

private:
    int handleEvent(tyb_board *board, tyb_monitor_event event);
    void handleAddedEvent(tyb_board *board);
    void handleChangedEvent(tyb_board *board);
    void handleDroppedEvent(tyb_board *board);
};

#endif
