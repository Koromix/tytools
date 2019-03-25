/* TyTools - public domain
   Niels Martignène <niels.martignene@protonmail.com>
   https://koromix.dev/tytools

   This software is in the public domain. Where that dedication is not
   recognized, you are granted a perpetual, irrevocable license to copy,
   distribute, and modify this file as you see fit.

   See the LICENSE file for more details. */

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
