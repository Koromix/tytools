/*
 * ty, a collection of GUI and command-line tools to manage Teensy devices
 *
 * Distributed under the MIT license (see LICENSE.txt or http://opensource.org/licenses/MIT)
 * Copyright (c) 2015 Niels Martignène <niels.martignene@gmail.com>
 */

#ifndef BOARD_HH
#define BOARD_HH

#include <QAbstractListModel>
#include <QTextDocument>

#include <functional>
#include <memory>
#include <vector>

#include "ty.h"
#include "descriptor_set_notifier.hh"
#include "task.hh"

class Manager;
class Board;

class BoardTask;

struct BoardInterfaceInfo {
    QString desc;
    QString path;

    uint16_t capabilities;
    uint8_t number;
};

class Board : public QObject, public std::enable_shared_from_this<Board> {
    Q_OBJECT

    tyb_board *board_;

    DescriptorSetNotifier serial_notifier_;
    bool serial_available_ = false;
    QTextDocument serial_document_;

    QTimer error_timer_;

    QString firmware_;
    QString firmware_name_;
    bool clear_on_reset_ = false;

    TaskWatcher task_watcher_;
    std::function<void(bool success, std::shared_ptr<void> result)> task_finish_;

public:
    static std::shared_ptr<Board> createBoard(tyb_board *board);
    virtual ~Board();

    tyb_board *board() const;

    bool matchesTag(const QString &id);

    tyb_board_state state() const;
    uint16_t capabilities() const;

    const tyb_board_model *model() const;
    QString modelName() const;

    QString tag() const;
    QString location() const;
    uint64_t serialNumber() const;

    std::vector<BoardInterfaceInfo> interfaces() const;

    bool isUploadAvailable() const;
    bool isResetAvailable() const;
    bool isRebootAvailable() const;
    bool isSerialAvailable() const;

    bool errorOccured() const;

    void setFirmware(const QString &firmware);
    QString firmware() const;
    QString firmwareName() const;
    void setClearOnReset(bool clear);
    bool clearOnReset() const;

    QTextDocument &serialDocument();
    void appendToSerialDocument(const QString& s);

    virtual bool event(QEvent *e) override;

    static QStringList makeCapabilityList(uint16_t capabilities);
    static QString makeCapabilityString(uint16_t capabilities, QString empty_str = QString());

    void refreshBoard();

    TaskInterface upload(const QString &filename, bool reset_after = true);
    TaskInterface reset();
    TaskInterface reboot();

    bool sendSerial(const QByteArray &buf);

    TaskInterface runningTask() const;

signals:
    void boardChanged();
    void boardDropped();

    void propertyChanged(const QByteArray &name, const QVariant &value);

    void taskChanged();

private slots:
    void serialReceived(ty_descriptor desc);

    void notifyLog(ty_log_level level, const QString &msg);
    void notifyFinished(bool success, std::shared_ptr<void> result);
    void notifyProgress(const QString &action, unsigned int value, unsigned int max);

private:
    Board(tyb_board *board, QObject *parent = nullptr);

    TaskInterface wrapBoardTask(ty_task *task,
                                std::function<void(bool success, std::shared_ptr<void> result)> finish = nullptr);
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

    std::shared_ptr<Board> find(std::function<bool(Board &board)> filter);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;

    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;

signals:
    void boardAdded(Board *board);

private slots:
    void refreshManager(ty_descriptor desc);

private:
    int handleEvent(tyb_board *board, tyb_monitor_event event);
    void handleAddedEvent(tyb_board *board);
    void handleChangedEvent(tyb_board *board);
    void handleDroppedEvent(tyb_board *board);

    void refreshBoardItem(Board *board);
};

#endif
