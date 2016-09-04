/*
 * ty, a collection of GUI and command-line tools to manage Teensy devices
 *
 * Distributed under the MIT license (see LICENSE.txt or http://opensource.org/licenses/MIT)
 * Copyright (c) 2015 Niels Martignène <niels.martignene@gmail.com>
 */

#ifndef CLIENT_HANDLER_HH
#define CLIENT_HANDLER_HH

#include <QHash>

#include <memory>
#include <vector>

#include "tyqt/session_channel.hpp"
#include "tyqt/task.hpp"

class Board;

class ClientHandler : public QObject {
    Q_OBJECT

    static const QHash<QString, void (ClientHandler::*)(const QStringList &)> commands_;

    std::unique_ptr<SessionPeer> peer_;

    QString working_directory_;
    bool multi_ = false;
    QStringList filters_;

    std::vector<TaskInterface> tasks_;

    unsigned int finished_tasks_ = 0;
    unsigned int error_count_ = 0;

public:
    ClientHandler(std::unique_ptr<SessionPeer> peer, QObject *parent = nullptr);

    void execute(const QStringList &parameters);

signals:
    void closed(SessionPeer::CloseReason reason);

private:
    void setWorkingDirectory(const QStringList &parameters);
    void setMultiSelection(const QStringList &parameters);
    void selectBoard(const QStringList &filters);
    void openMainWindow(const QStringList &parameters);
    void reset(const QStringList &parameters);
    void reboot(const QStringList &parameters);
    void upload(const QStringList &parameters);

    static std::vector<TaskInterface> makeUploadTasks(
        const std::vector<std::shared_ptr<Board>> &boards, const QStringList &filenames);

    std::vector<std::shared_ptr<Board>> selectedBoards();

    void notifyLog(ty_log_level level, const QString &msg);
    void notifyStarted();
    void notifyFinished(bool success);
    void notifyProgress(const QString &action, unsigned int value, unsigned int max);

    void addTask(TaskInterface task);
    void executeTasks();
};

#endif
