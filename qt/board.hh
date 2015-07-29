/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef BOARD_HH
#define BOARD_HH

#include <QAbstractListModel>
#include <QFuture>
#include <QRunnable>
#include <QTextDocument>
#include <QThread>

#include <memory>
#include <vector>

#include "ty.h"
#include "descriptor_set_notifier.hh"

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
    bool clear_on_reset_ = false;

    QTextDocument serial_document_;

    QFuture<void> running_task_;

public:
    static std::shared_ptr<Board> createBoard(tyb_board *board);
    virtual ~Board();

    std::shared_ptr<Board> getSharedPtr();

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

    void setClearOnReset(bool clear);
    bool clearOnReset() const;

    QTextDocument &serialDocument();
    void appendToSerialDocument(const QString& s);

    QFuture<void> runningTask() const;

    virtual bool event(QEvent *e) override;

    static QStringList makeCapabilityList(uint16_t capabilities);
    static QString makeCapabilityString(uint16_t capabilities, QString empty_str = QString());

    void refreshBoard();

public slots:
    QFuture<void> upload(const QString &filename, bool reset_after = true);
    QFuture<void> reset();
    QFuture<void> reboot();

    QFuture<void> sendSerial(const QByteArray &buf);

signals:
    void boardChanged();
    void boardDropped();

    void propertyChanged(const QByteArray &name, const QVariant &value);

    void taskProgress(unsigned int progress, unsigned int total);

private slots:
    void serialReceived(ty_descriptor desc);

private:
    Board(tyb_board *board, QObject *parent = nullptr);

    QFuture<void> startAsync(std::function<void(BoardTask &)> f);
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

private:
    int handleEvent(tyb_board *board, tyb_monitor_event event);
    void handleAddedEvent(tyb_board *board);
    void handleChangedEvent(tyb_board *board);
    void handleDroppedEvent(tyb_board *board);

    void refreshBoardItem(Board *board);
};

#endif
