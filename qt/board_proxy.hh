/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef BOARD_PROXY_HH
#define BOARD_PROXY_HH

#include <QAbstractListModel>
#include <QTextDocument>
#include <QThread>

#include <memory>
#include <vector>

#include "ty.h"
#include "descriptor_set_notifier.hh"

class BoardManagerProxy;
class BoardProxy;

struct BoardInterfaceInfo {
    QString desc;
    QString path;

    uint16_t capabilities;
    uint8_t number;
};

class BoardCommand;
class BoardProxyWorker : public QObject
{
    Q_OBJECT

    BoardCommand *running_task_ = nullptr;

private:
    BoardProxyWorker(QObject *parent = nullptr)
        : QObject(parent) {}

    void reportTaskProgress(unsigned int progress, unsigned int total);
    void reportTaskProgress() { reportTaskProgress(0, 0); }

signals:
    void taskProgress(const QString &msg, unsigned int progress, unsigned int total);

protected:
    virtual void customEvent(QEvent *ev) override;

    friend class BoardProxy;
};

class BoardProxy : public QObject {
    Q_OBJECT

    QThread *thread_;
    BoardProxyWorker *worker_;

    ty_board *board_;

    DescriptorSetNotifier serial_notifier_;
    bool serial_available_ = false;

    QTextDocument serial_document_;

    QString task_msg_;
    unsigned int task_progress_ = 0;
    unsigned int task_total_ = 0;

public:
    BoardProxy(ty_board *board, QObject *parent = nullptr);
    virtual ~BoardProxy();

    ty_board *board() const;

    bool matchesIdentity(const QString &id);

    ty_board_state state() const;
    uint16_t capabilities() const;

    const ty_board_model *model() const;
    QString modelName() const;

    QString identity() const;
    QString location() const;
    uint64_t serialNumber() const;

    std::vector<BoardInterfaceInfo> interfaces() const;

    bool isUploadAvailable() const;
    bool isResetAvailable() const;
    bool isRebootAvailable() const;
    bool isSerialAvailable() const;

    QTextDocument &serialDocument();
    void appendToSerialDocument(const QString& s);

    void sendSerial(const QByteArray &buf);

    QString runningTask(unsigned int *progress, unsigned int *total) const;

    static QStringList makeCapabilityList(uint16_t capabilities);
    static QString makeCapabilityString(uint16_t capabilities, QString empty_str = QString());

public slots:
    void upload(const QString &filename, bool reset_after = true);
    void reset();
    void reboot();

signals:
    void boardChanged();
    void boardDropped();

    void taskProgress(const BoardProxy &board, const QString &msg, size_t progress, size_t total);

private slots:
    void serialReceived(ty_descriptor desc);
    void reportTaskProgress(const QString &msg, unsigned int progress, unsigned int total);

private:
    void refreshBoard();

    friend class BoardManagerProxy;
    friend class BoardProxyWorker;
};

class BoardManagerProxy : public QAbstractListModel {
    Q_OBJECT

    ty_board_manager *manager_ = nullptr;
    DescriptorSetNotifier manager_notifier_;

    std::vector<std::shared_ptr<BoardProxy>> boards_;

public:
    typedef decltype(boards_)::iterator iterator;
    typedef decltype(boards_)::const_iterator const_iterator;

    BoardManagerProxy(QObject *parent = nullptr)
        : QAbstractListModel(parent) {}
    virtual ~BoardManagerProxy();

    bool start();

    ty_board_manager *manager() const { return manager_; }

    iterator begin() { return boards_.begin(); }
    iterator end() { return boards_.end(); }
    const_iterator cbegin() const { return boards_.cbegin(); }
    const_iterator cend() const { return boards_.cend(); }

    std::shared_ptr<BoardProxy> board(size_t i);
    size_t boardCount() const;

    virtual int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    virtual QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    virtual Qt::ItemFlags flags(const QModelIndex &index) const override;

signals:
    void boardAdded(std::shared_ptr<BoardProxy> board);

private slots:
    void refreshManager(ty_descriptor desc);
    void updateTaskProgress(const BoardProxy &board, const QString &msg, size_t progress, size_t total);

private:
    int handleEvent(ty_board *board, ty_board_event event);
    void handleAddedEvent(ty_board *board);
    void handleChangedEvent(ty_board *board);
    void handleDroppedEvent(ty_board *board);
};

#endif
