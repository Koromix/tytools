/* TyTools - public domain
   Niels Martignène <niels.martignene@protonmail.com>
   https://koromix.dev/tytools

   This software is in the public domain. Where that dedication is not
   recognized, you are granted a perpetual, irrevocable license to copy,
   distribute, and modify this file as you see fit.

   See the LICENSE file for more details. */

#include <QCoreApplication>
#include <QDataStream>
#include <QDir>

#ifdef _WIN32
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
#else
    #include <unistd.h>
#endif

#include "session_channel.hpp"

#ifdef _WIN32
typedef BOOL WINAPI ProcessIdToSessionId_func(DWORD dwProcessId, DWORD *pSessionId);

static ProcessIdToSessionId_func *ProcessIdToSessionId_ =
    reinterpret_cast<ProcessIdToSessionId_func *>(GetProcAddress(GetModuleHandle("kernel32.dll"),
                                                                 "ProcessIdToSessionId"));
#endif

using namespace std;

SessionChannel::SessionChannel(const QString &id, QObject *parent)
    : QObject(parent)
{
    if (!id.isEmpty())
        init(id);
}

SessionChannel::~SessionChannel()
{
    close();
    unlock();
}

void SessionChannel::init(const QString &id)
{
    close();
    unlock();

    if (!id.isEmpty()) {
        id_ = id;
    } else {
        id_ = QCoreApplication::applicationName();
    }
}

#ifdef _WIN32

bool SessionChannel::lock()
{
    if (id_.isEmpty())
        return false;

    if (!mutex_) {
        QString mutexName = QString("Local\\") + id_;

        mutex_ = CreateMutexW(NULL, FALSE, reinterpret_cast<LPCWSTR>(mutexName.utf16()));
        locked_ = mutex_ && (GetLastError() == ERROR_SUCCESS);
    }

    return locked_;
}

void SessionChannel::unlock()
{
    if (mutex_)
        CloseHandle(mutex_);
    mutex_ = nullptr;

    locked_ = false;
}

QString SessionChannel::makeSocketName() const
{
    QString socketName = id_;

    if (ProcessIdToSessionId_) {
        DWORD session = 0;
        ProcessIdToSessionId_(GetCurrentProcessId(), &session);

        socketName += '-' + QString::number(session);
    }

    return socketName;
}

#else

bool SessionChannel::lock()
{
    if (id_.isEmpty())
        return false;

    if (!lock_) {
        QString lockName = QString("%1/%2-%3.lock").arg(QDir::tempPath(), id_, QString::number(getuid()));

        lock_ = unique_ptr<QLockFile>(new QLockFile(lockName));
        lock_->setStaleLockTime(0);

        locked_ = lock_->tryLock(5);
    }

    return locked_;
}

void SessionChannel::unlock()
{
    if (lock_ && locked_)
        lock_->unlock();
    lock_ = nullptr;

    locked_ = false;
}

QString SessionChannel::makeSocketName() const
{
    return QString("%1-%2").arg(id_, QString::number(getuid()));
}

#endif

bool SessionChannel::listen()
{
    if (!lock())
        return false;
    if (server_ && server_->isListening())
        return true;

    auto socket_name = makeSocketName();

    QLocalServer::removeServer(socket_name);

    if (!server_) {
        server_ = unique_ptr<QLocalServer>(new QLocalServer());
        connect(server_.get(), &QLocalServer::newConnection, this, &SessionChannel::newConnection);
    }
    server_->setSocketOptions(QLocalServer::UserAccessOption);

    return server_->listen(socket_name);
}

unique_ptr<SessionPeer> SessionChannel::nextPendingConnection()
{
    auto socket = server_->nextPendingConnection();
    if (!socket)
        return nullptr;
    return SessionPeer::wrapSocket(socket);
}

void SessionChannel::close()
{
    server_.reset();
}

unique_ptr<SessionPeer> SessionChannel::connectToServer()
{
    if (id_.isEmpty())
        return nullptr;
    if (locked_)
        return nullptr;

    return SessionPeer::connectTo(makeSocketName());
}

unique_ptr<SessionPeer> SessionPeer::wrapSocket(QLocalSocket *socket)
{
    if (!socket)
        return nullptr;

    return unique_ptr<SessionPeer>(new SessionPeer(socket));
}

unique_ptr<SessionPeer> SessionPeer::connectTo(const QString &name)
{
    auto socket = unique_ptr<QLocalSocket>(new QLocalSocket());

    socket->connectToServer(name);
    if (!socket->waitForConnected(1000))
        return nullptr;

    return unique_ptr<SessionPeer>(new SessionPeer(socket.release()));
}

SessionPeer::SessionPeer(QLocalSocket *socket)
    : socket_(socket)
{
    socket_->setParent(nullptr);

    QObject::connect(socket, &QLocalSocket::readyRead, this, &SessionPeer::dataReceived);
    QObject::connect(socket, &QLocalSocket::disconnected, this, [=]() {
        close(RemoteClose);
    });
    QObject::connect(socket, static_cast<void(QLocalSocket::*)(QLocalSocket::LocalSocketError)>(&QLocalSocket::error),
                     this, [=]() {
        close(Error);
    });
}

SessionPeer::~SessionPeer()
{
    close();
}

void SessionPeer::close()
{
    close(LocalClose);
}

void SessionPeer::send(const QStringList &arguments)
{
    if (socket_->state() != QLocalSocket::ConnectedState)
        return;

    QByteArray buf;
    QDataStream stream(&buf, QIODevice::WriteOnly);
    stream << arguments;

    uint32_t length = buf.size();
    socket_->write(reinterpret_cast<char *>(&length), sizeof(length));
    socket_->write(buf);
}

void SessionPeer::dataReceived()
{
    if (socket_->state() != QLocalSocket::ConnectedState)
        return;

    while (true) {
        // Get the length first (first 8 bytes)
        if (!expected_length_) {
            if (socket_->bytesAvailable() < static_cast<qint64>(sizeof(expected_length_)))
                break;

            qint64 read = socket_->read(reinterpret_cast<char *>(&expected_length_), sizeof(expected_length_));
            if (read < static_cast<qint64>(sizeof(expected_length_))) {
                close(Error);
                break;
            }
        }
        // Easier to let Qt/OS handle the buffer, I won't use very big messages anyway
        if (socket_->bytesAvailable() < expected_length_)
            break;

        auto buf = socket_->read(static_cast<qint64>(expected_length_));
        expected_length_ = 0;

        QDataStream stream(buf);
        QStringList arguments;
        stream >> arguments;
        emit received(arguments);
    }
}

void SessionPeer::close(CloseReason reason)
{
    if (!isConnected())
        return;

    socket_->close();
    emit closed(reason);
}
