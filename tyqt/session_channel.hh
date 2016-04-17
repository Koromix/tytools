/*
 * ty, a collection of GUI and command-line tools to manage Teensy devices
 *
 * Distributed under the MIT license (see LICENSE.txt or http://opensource.org/licenses/MIT)
 * Copyright (c) 2015 Niels Martignène <niels.martignene@gmail.com>
 */

#ifndef SESSION_CHANNEL_HH
#define SESSION_CHANNEL_HH

#include <QLockFile>
#include <QLocalServer>
#include <QLocalSocket>

#include <memory>

class SessionPeer : public QObject {
    Q_OBJECT

    std::unique_ptr<QLocalSocket> socket_;
    uint32_t expected_length_ = 0;

public:
    enum CloseReason {
        LocalClose,
        RemoteClose,
        Error
    };

    static std::unique_ptr<SessionPeer> wrapSocket(QLocalSocket *socket);
    static std::unique_ptr<SessionPeer> connectTo(const QString &name);
    ~SessionPeer();

    void close();

    bool isConnected() const { return socket_->state() == QLocalSocket::ConnectedState; }

    void send(const QStringList &arguments);
    void send(const QString &argument) { send(QStringList(argument)); }
    void send(const char *argument) { send(QStringList(argument)); }

signals:
    void received(const QStringList &arguments);
    void closed(SessionPeer::CloseReason reason);

private:
    SessionPeer(QLocalSocket *socket);
    void close(CloseReason reason);

private slots:
    void dataReceived();
};

class SessionChannel : public QObject {
    Q_OBJECT

#ifdef _WIN32
    void *mutex_ = nullptr; // HANDLE
#else
    std::unique_ptr<QLockFile> lock_;
#endif

    QString id_;
    bool locked_ = false;

    std::unique_ptr<QLocalServer> server_;

public:
    SessionChannel(const QString &id, QObject *parent = nullptr);
    SessionChannel(QObject *parent = nullptr)
        : SessionChannel(QString(), parent) {}
    virtual ~SessionChannel();

    void init(const QString &id = QString());

    QString identifier() const { return id_; }

    bool lock();
    void unlock();
    bool isLocked() const { return locked_; }

    bool listen();
    std::unique_ptr<SessionPeer> nextPendingConnection();
    void close();

    std::unique_ptr<SessionPeer> connectToServer();

signals:
    void newConnection();

private:
    QString makeSocketName() const;
};

#endif
