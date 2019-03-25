/* TyTools - public domain
   Niels Martignène <niels.martignene@protonmail.com>
   https://koromix.dev/tytools

   This software is in the public domain. Where that dedication is not
   recognized, you are granted a perpetual, irrevocable license to copy,
   distribute, and modify this file as you see fit.

   See the LICENSE file for more details. */

#ifndef MONITOR_HH
#define MONITOR_HH

#include <QAbstractListModel>
#include <QThread>

#include <memory>
#include <vector>

#include "database.hpp"
#include "descriptor_notifier.hpp"
#include "../libty/monitor.h"

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

    bool ignore_generic_;
    bool default_serial_;
    size_t serial_log_size_;
    QString serial_log_dir_;

    std::vector<std::shared_ptr<Board>> boards_;

public:
    typedef decltype(boards_)::iterator iterator;
    typedef decltype(boards_)::const_iterator const_iterator;

    enum Column {
        COLUMN_BOARD,
        COLUMN_MODEL,
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

    unsigned int maxTasks() const;
    bool ignoreGeneric() const { return ignore_generic_; }

    bool serialByDefault() const { return default_serial_; }
    size_t serialLogSize() const { return serial_log_size_; }
    QString serialLogDir() const { return serial_log_dir_; }

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

    std::vector<std::shared_ptr<Board>> find(std::function<bool(Board &board)> filter);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;

    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;
    bool setData(const QModelIndex &index, const QVariant &value, int role) override;

public slots:
    void setMaxTasks(unsigned int max_tasks);
    void setIgnoreGeneric(bool ignore_generic);
    void setSerialByDefault(bool default_serial);
    void setSerialLogSize(size_t default_size);
    void setSerialLogDir(const QString &dir);

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

    void configureBoardDatabase(Board &board);
};

#endif
