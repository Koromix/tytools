/*
 * ty, a collection of GUI and command-line tools to manage Teensy devices
 *
 * Distributed under the MIT license (see LICENSE.txt or http://opensource.org/licenses/MIT)
 * Copyright (c) 2015 Niels Martignène <niels.martignene@gmail.com>
 */

#ifndef MONITOR_HH
#define MONITOR_HH

#include <QAbstractListModel>
#include <QThread>

#include <memory>
#include <vector>

#include "descriptor_notifier.hh"
#include "ty/monitor.h"

class Board;
class Database;
struct tyb_board;

class Manager : public QAbstractListModel {
    Q_OBJECT

    Database *db_ = nullptr;

    tyb_monitor *manager_ = nullptr;
    DescriptorNotifier manager_notifier_;

    QThread serial_thread_;

    std::vector<std::shared_ptr<Board>> boards_;

public:
    typedef decltype(boards_)::iterator iterator;
    typedef decltype(boards_)::const_iterator const_iterator;

    Manager(QObject *parent = nullptr)
        : QAbstractListModel(parent) {}
    virtual ~Manager();

    void setDatabase(Database *db) { db_ = db; }
    Database *database() const { return db_; }

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
    bool setData(const QModelIndex &index, const QVariant &value, int role) override;

signals:
    void boardAdded(Board *board);

private slots:
    void refreshManager(ty_descriptor desc);

private:
    iterator findBoardIterator(tyb_board *board);

    int handleEvent(tyb_board *board, tyb_monitor_event event);
    void handleAddedEvent(tyb_board *board);
    void handleChangedEvent(tyb_board *board);

    void refreshBoardItem(iterator it);

    void saveBoardSetting(const Board &board, const QString &key, const QVariant &value);
    void restoreBoardSettings(Board &board);
};

#endif
