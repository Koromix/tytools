/* TyTools - public domain
   Niels Martignène <niels.martignene@protonmail.com>
   https://koromix.dev/tytools

   This software is in the public domain. Where that dedication is not
   recognized, you are granted a perpetual, irrevocable license to copy,
   distribute, and modify this file as you see fit.

   See the LICENSE file for more details. */

#ifndef BOARD_HH
#define BOARD_HH

#include <QFile>
#include <QIcon>
#include <QMutex>
#include <QStringList>
#include <QTextCodec>
#include <QTextDecoder>
#include <QTextDocument>
#include <QThread>
#include <QTimer>

#include <memory>
#include <vector>

#include "../libty/board.h"
#include "database.hpp"
#include "descriptor_notifier.hpp"
#include "firmware.hpp"
#include "../libty/monitor.h"
#include "task.hpp"

class Monitor;

struct BoardInterfaceInfo {
    QString name;
    QString path;

    uint16_t capabilities;
    uint8_t number;
    bool open;
};

class Board : public QObject, public std::enable_shared_from_this<Board> {
    Q_OBJECT

    DatabaseInterface db_;
    DatabaseInterface cache_;

    ty_board *board_;

    ty_board_interface *serial_iface_ = nullptr;
    DescriptorNotifier serial_notifier_;
    QTextCodec *serial_codec_;
    std::unique_ptr<QTextDecoder> serial_decoder_;
    QMutex serial_lock_;
    char serial_buf_[262144];
    size_t serial_buf_len_ = 0;
    QTextDocument serial_document_;
    QFile serial_log_file_;
    bool serial_clear_when_available_ = false;

    QTimer error_timer_;

    QString firmware_;
    bool reset_after_;
    unsigned int serial_rate_ = 0;
    QString serial_codec_name_;
    bool clear_on_reset_;
    bool enable_serial_;
    QString serial_log_dir_;
    size_t serial_log_size_;

    QString status_text_;
    QString status_icon_name_;
    QIcon status_icon_;

    QString status_firmware_;
    QStringList recent_firmwares_;

    ty_pool *pool_ = nullptr;

    TaskInterface task_;
    TaskWatcher task_watcher_;

public:
    virtual ~Board();

    void setDatabase(DatabaseInterface db) { db_ = db; }
    DatabaseInterface database() const { return db_; }
    void setCache(DatabaseInterface cache) { cache_ = cache; }
    DatabaseInterface cache() const { return cache_; }

    ty_board *board() const { return board_; }

    bool matchesTag(const QString &id);

    uint16_t capabilities() const;
    bool hasCapability(ty_board_capability cap) const;

    ty_model model() const;
    QString modelName() const;

    QString tag() const;
    QString id() const;
    QString location() const;
    QString serialNumber() const;
    QString description() const;

    std::vector<BoardInterfaceInfo> interfaces() const;

    bool errorOccured() const { return error_timer_.remainingTime() > 0; }

    QString statusText() const { return status_text_; }
    QIcon statusIcon() const { return status_icon_; }

    QString firmware() const { return firmware_; }
    QStringList recentFirmwares() const { return recent_firmwares_; }
    bool resetAfter() const { return reset_after_; }
    unsigned int serialRate() const { return serial_rate_; }
    QString serialCodecName() const { return serial_codec_name_; }
    QTextCodec *serialCodec() const { return serial_codec_; }
    bool clearOnReset() const { return clear_on_reset_; }
    unsigned int scrollBackLimit() const { return serial_document_.maximumBlockCount(); }
    bool enableSerial() const { return enable_serial_; }
    size_t serialLogSize() const { return serial_log_size_; }
    QString serialLogFilename() const { return serial_log_file_.fileName(); }

    bool serialOpen() const { return serial_iface_; }
    bool serialIsSerial() const;
    QTextDocument &serialDocument() { return serial_document_; }

    static QStringList makeCapabilityList(uint16_t capabilities);
    static QString makeCapabilityString(uint16_t capabilities, QString empty_str = QString());

    TaskInterface upload(const QString &filename = QString());
    TaskInterface upload(const std::vector<std::shared_ptr<Firmware>> &fws);
    TaskInterface upload(const std::vector<std::shared_ptr<Firmware>> &fws, bool reset_after);
    TaskInterface reset();
    TaskInterface reboot();
    TaskInterface sendSerial(const QByteArray &buf);
    TaskInterface sendSerial(const QString &s);
    TaskInterface sendFile(const QString &filename);

    void appendFakeSerialRead(const QString &s);

    TaskInterface task() const { return task_; }
    ty_task_status taskStatus() const { return task_.status(); }

public slots:
    void setTag(const QString &tag);

    void setFirmware(const QString &firmware);
    void clearRecentFirmwares();
    void setResetAfter(bool reset_after);
    void setSerialRate(unsigned int rate);
    void setSerialCodecName(QString codec_name);
    void setClearOnReset(bool clear_on_reset);
    void setScrollBackLimit(unsigned int limit);
    void setEnableSerial(bool enable, bool persist = true);
    void setSerialLogSize(size_t size);

    TaskInterface startUpload(const QString &filename = QString());
    TaskInterface startUpload(const std::vector<std::shared_ptr<Firmware>> &fws);
    TaskInterface startUpload(const std::vector<std::shared_ptr<Firmware>> &fws, bool reset_after);
    TaskInterface startReset();
    TaskInterface startReboot();
    TaskInterface startSendSerial(const QByteArray &buf);
    TaskInterface startSendSerial(const QString &s);
    TaskInterface startSendFile(const QString &filename);

    void notifyLog(ty_log_level level, const QString &msg);

signals:
    void infoChanged();
    void settingsChanged();
    void interfacesChanged();
    void statusChanged();
    void progressChanged();

    void dropped();

private slots:
    void updateStatus();

    void serialReceived(ty_descriptor desc);
    void appendBufferToSerialDocument();

    void notifyFinished(bool success, std::shared_ptr<void> result);

private:
    Board(ty_board *board, QObject *parent = nullptr);
    void loadSettings(Monitor *monitor);
    QString findLogFilename(const QString &id, unsigned int max);

    void setThreadPool(ty_pool *pool) { pool_ = pool; }

    void writeToSerialLog(const char *buf, size_t len);

    void refreshBoard();
    bool updateSerialInterface();
    bool openSerialInterface();
    void closeSerialInterface();
    void updateSerialLogState(bool new_file);

    void addUploadedFirmware(ty_firmware *fw);

    TaskInterface watchTask(TaskInterface task);

    friend class Monitor;
};

#endif
