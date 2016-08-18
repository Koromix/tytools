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

#include "database.hpp"
#include "descriptor_notifier.hpp"
#include "ty/monitor.h"

class Board;
struct ty_board;
struct ty_pool;

class Monitor : public QAbstractListModel {
    Q_OBJECT

    DatabaseInterface db_;
    DatabaseInterface cache_;

    bool started_ = false;
    ty_monitor *monitor_ = nullptr;
    DescriptorNotifier monitor_notifier_;

    ty_pool *pool_;
    QThread serial_thread_;

    bool default_serial_;
    size_t serial_log_size_;

    std::vector<std::shared_ptr<Board>> boards_;

public:
    typedef decltype(boards_)::iterator iterator;
    typedef decltype(boards_)::const_iterator const_iterator;

    enum Column {
        COLUMN_BOARD,
        COLUMN_STATUS,
        COLUMN_IDENTITY,
        COLUMN_LOCATION,
        COLUMN_SERIAL_NUMBER,
        COLUMN_DESCRIPTION,

        COLUMN_COUNT
    };

    enum CustomRole {
        ROLE_BOARD = Qt::UserRole + 1
    };

    Monitor(QObject *parent = nullptr);
    virtual ~Monitor();

    void setDatabase(DatabaseInterface db) { db_ = db; }
    DatabaseInterface database() const { return db_; }
    void setCache(DatabaseInterface cache) { cache_ = cache; }
    DatabaseInterface cache() const { return cache_; }
    void loadSettings();

    void setMaxTasks(unsigned int max_tasks);
    unsigned int maxTasks() const;
    void setSerialByDefault(bool default_serial);
    bool serialByDefault() const { return default_serial_; }
    void setSerialLogSize(size_t default_size);
    size_t serialLogSize() const { return serial_log_size_; }

    bool start();
    void stop();

    ty_monitor *monitor() const { return monitor_; }

    iterator begin() { return boards_.begin(); }
    iterator end() { return boards_.end(); }
    const_iterator cbegin() const { return boards_.cbegin(); }
    const_iterator cend() const { return boards_.cend(); }

    std::vector<std::shared_ptr<Board>> boards();
    std::shared_ptr<Board> board(unsigned int i);
    unsigned int boardCount() const;

    static std::shared_ptr<Board> boardFromModel(const QAbstractItemModel *model,
                                                 const QModelIndex &index);
    static std::shared_ptr<Board> boardFromModel(const QAbstractItemModel *model, int index)
    {
        return boardFromModel(model, model->index(index, 0));
    }

    std::shared_ptr<Board> find(std::function<bool(Board &board)> filter);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;

    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;
    bool setData(const QModelIndex &index, const QVariant &value, int role) override;

signals:
    void settingsChanged();

    void boardAdded(Board *board);

private slots:
    void refresh(ty_descriptor desc);

private:
    iterator findBoardIterator(ty_board *board);

    static int handleEvent(ty_board *board, ty_monitor_event event, void *udata);
    void handleAddedEvent(ty_board *board);
    void handleChangedEvent(ty_board *board);

    void refreshBoardItem(iterator it);
    void removeBoardItem(iterator it);

    QString findLogFilename(const QString &tpl, unsigned int max);
};

#endif
