/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <QCoreApplication>
#include <QDir>

#include <unistd.h>
#ifdef _WIN32
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
#endif

#include "ty.h"
#include "session_channel.hh"

#ifdef _WIN32
typedef BOOL WINAPI ProcessIdToSessionId_func(DWORD dwProcessId, DWORD *pSessionId);

static ProcessIdToSessionId_func *ProcessIdToSessionId_;
#endif

using namespace std;

#ifdef _WIN32

TY_INIT()
{
    HMODULE h = GetModuleHandle("kernel32.dll");
    assert(h);

    ProcessIdToSessionId_ = reinterpret_cast<ProcessIdToSessionId_func *>(GetProcAddress(h, "ProcessIdToSessionId"));
}

#endif

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
    if (!id_.isEmpty()) {
        close();
        unlock();
    }

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

        locked_ = lock_->tryLock(100);
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
    if (id_.isEmpty() || !locked_)
        return false;

    if (server_.isListening())
        return true;

    QString socketName = makeSocketName();

    QLocalServer::removeServer(socketName);

    server_.setSocketOptions(QLocalServer::UserAccessOption);
    connect(&server_, &QLocalServer::newConnection, this, &SessionChannel::receiveConnection);

    return server_.listen(socketName);
}

bool SessionChannel::connectToMaster()
{
    if (id_.isEmpty() || locked_)
        return false;

    if (client_) {
        if (client_->isConnected())
            return true;
    } else {
        client_ = new SessionPeer(this);
    }

    return client_->connect(makeSocketName());
}

void SessionChannel::close()
{
    server_.close();
    delete client_;
}

void SessionChannel::send(const QStringList &arguments)
{
    if (!client_)
        return;

    client_->send(arguments);
}

void SessionChannel::receiveConnection()
{
    QLocalSocket *socket = server_.nextPendingConnection();
    if (!socket)
        return;

    new SessionPeer(this, socket);
}

SessionPeer::SessionPeer(SessionChannel *channel, QLocalSocket *socket)
    : QObject(channel), channel_(channel), socket_(socket)
{
    socket->setParent(this);

    QObject::connect(socket, &QLocalSocket::readyRead, this, &SessionPeer::dataReceived);
    QObject::connect(socket, &QLocalSocket::disconnected, this, &SessionPeer::dropClient);
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

bool SessionPeer::connect(const QString &name)
{
    socket_->connectToServer(name);
    return socket_->waitForConnected(1000);
}

void SessionPeer::dataReceived()
{
    if (socket_->state() != QLocalSocket::ConnectedState)
        return;

    /* Any slot may run an event loop (for example with QDialog::exec()), so we have to delay
     * deletion of this peer if it's disconnected. */
    busy_++;

    while (true) {
        if (!expected_length_) {
            if (socket_->bytesAvailable() < static_cast<qint64>(sizeof(expected_length_)))
                break;

            qint64 read = socket_->read(reinterpret_cast<char *>(&expected_length_), sizeof(expected_length_));
            if (read < static_cast<qint64>(sizeof(expected_length_))) {
                dropClient();
                break;
            }
        }

        // Easier to let the OS handle the buffer, I won't use very big messages anyway
        if (socket_->bytesAvailable() < expected_length_)
            break;

        QByteArray buf = socket_->read(static_cast<qint64>(expected_length_));
        expected_length_ = 0;

        QDataStream stream(buf);
        QStringList arguments;
        stream >> arguments;

        emit channel_->received(*this, arguments);
    }

    busy_--;

    if (!busy_ && socket_->state() != QLocalSocket::ConnectedState)
        deleteLater();
}

void SessionPeer::dropClient()
{
    socket_->close();
    if (!busy_)
        deleteLater();
}
