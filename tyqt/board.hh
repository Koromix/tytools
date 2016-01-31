/*
 * ty, a collection of GUI and command-line tools to manage Teensy devices
 *
 * Distributed under the MIT license (see LICENSE.txt or http://opensource.org/licenses/MIT)
 * Copyright (c) 2015 Niels Martignène <niels.martignene@gmail.com>
 */

#ifndef BOARD_HH
#define BOARD_HH

#include <QMutex>
#include <QTextDocument>
#include <QThread>
#include <QTimer>

#include <functional>
#include <memory>
#include <vector>

#include "ty/board.h"
#include "descriptor_notifier.hh"
#include "firmware.hh"
#include "ty/monitor.h"
#include "task.hh"

struct BoardInterfaceInfo {
    QString name;
    QString path;

    uint16_t capabilities;
    uint8_t number;
    bool open;
};

class Board : public QObject, public std::enable_shared_from_this<Board> {
    Q_OBJECT

    Q_PROPERTY(QString tag READ tag WRITE setTag)
    Q_PROPERTY(QString firmware READ firmware WRITE setFirmware STORED false)
    Q_PROPERTY(bool resetAfter READ resetAfter WRITE setResetAfter)
    Q_PROPERTY(bool clearOnReset READ clearOnReset WRITE setClearOnReset)
    Q_PROPERTY(unsigned int scrollBackLimit READ scrollBackLimit WRITE setScrollBackLimit)

    ty_board *board_;

    bool serial_attach_ = true;
    ty_board_interface *serial_iface_ = nullptr;
    DescriptorNotifier serial_notifier_;
    QMutex serial_lock_;
    char serial_buf_[262144];
    size_t serial_buf_len_ = 0;
    QTextDocument serial_document_;

    QTimer error_timer_;

    QString firmware_;
    bool reset_after_ = true;
    bool clear_on_reset_ = false;

    QString firmware_name_;

    TaskInterface running_task_;
    TaskWatcher task_watcher_;
    std::function<void(bool success, std::shared_ptr<void> result)> task_finish_;

public:
    static std::shared_ptr<Board> createBoard(ty_board *board);
    virtual ~Board();

    ty_board *board() const;

    bool matchesTag(const QString &id);

    ty_board_state state() const;
    uint16_t capabilities() const;

    const ty_board_model *model() const;
    QString modelName() const;

    QString id() const;
    QString location() const;
    uint64_t serialNumber() const;

    std::vector<BoardInterfaceInfo> interfaces() const;

    bool isRunning() const;
    bool isUploadAvailable() const;
    bool isResetAvailable() const;
    bool isRebootAvailable() const;
    bool isSerialAvailable() const;
    bool errorOccured() const;

    QString statusIconFileName() const;
    QString firmwareName() const;
    QString statusText() const;

    void setTag(const QString &tag);
    QString tag() const { return ty_board_get_tag(board_); }

    void setFirmware(const QString &firmware);
    QString firmware() const { return firmware_; }

    void setResetAfter(bool reset_after);
    bool resetAfter() const { return reset_after_; }

    void setClearOnReset(bool clear_on_reset);
    bool clearOnReset() const { return clear_on_reset_; }

    void setScrollBackLimit(unsigned int limit);
    unsigned int scrollBackLimit() const { return serial_document_.maximumBlockCount(); }

    QTextDocument &serialDocument();
    void appendToSerialDocument(const QString& s);

    static QStringList makeCapabilityList(uint16_t capabilities);
    static QString makeCapabilityString(uint16_t capabilities, QString empty_str = QString());

    TaskInterface upload(const std::vector<std::shared_ptr<Firmware>> &fws);
    TaskInterface upload(const std::vector<std::shared_ptr<Firmware>> &fws, bool reset_after);
    TaskInterface reset();
    TaskInterface reboot();

    bool attachMonitor();
    void detachMonitor();
    bool isMonitorAttached() const { return serial_iface_; }
    bool autoAttachMonitor() const { return serial_attach_; }

    bool sendSerial(const QByteArray &buf);

    TaskInterface runningTask() const;

signals:
    void boardChanged();
    void boardDropped();
    void taskChanged();

    void settingChanged(const QString &name, const QVariant &value);

public slots:
    void notifyLog(ty_log_level level, const QString &msg);

private slots:
    void serialReceived(ty_descriptor desc);
    void updateSerialDocument();

    void notifyFinished(bool success, std::shared_ptr<void> result);
    void notifyProgress(const QString &action, unsigned int value, unsigned int max);

private:
    Board(ty_board *board, QObject *parent = nullptr);

    void refreshBoard();
    bool openSerialInterface();
    void closeSerialInterface();

    TaskInterface wrapBoardTask(ty_task *task,
                                std::function<void(bool success, std::shared_ptr<void> result)> finish = nullptr);

    friend class Monitor;
};

#endif
