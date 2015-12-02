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
#include <QPointer>

#include <memory>

class SessionChannel;

class SessionPeer : public QObject {
    Q_OBJECT

    SessionChannel *channel_;
    QLocalSocket *socket_;

    unsigned int busy_ = 0;

    uint32_t expected_length_ = 0;

public:
    void send(const QStringList &arguments);
    void send(const QString &argument) { send(QStringList(argument)); }
    void send(const char *argument) { send(QStringList(argument)); }

    bool isConnected() const { return socket_->state() == QLocalSocket::ConnectedState; }

private:
    SessionPeer(SessionChannel *channel, QLocalSocket *socket = new QLocalSocket());

    bool connect(const QString &name);

private slots:
    void dataReceived();
    void dropClient();

    friend class SessionChannel;
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

    QLocalServer server_;
    QPointer<SessionPeer> client_;

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
    bool connectToMaster();
    void close();

    void send(const QStringList &arguments);
    void send(const QString &argument) { send(QStringList(argument)); }
    void send(const char *argument) { send(QStringList(argument)); }

signals:
    void received(SessionPeer &peer, const QStringList &arguments);

    void masterClosed();

private:
    QString makeSocketName() const;

private slots:
    void receiveConnection();
};

#endif
