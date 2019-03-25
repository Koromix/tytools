/* TyTools - public domain
   Niels Martignène <niels.martignene@protonmail.com>
   https://koromix.dev/tytools

   This software is in the public domain. Where that dedication is not
   recognized, you are granted a perpetual, irrevocable license to copy,
   distribute, and modify this file as you see fit.

   See the LICENSE file for more details. */

#ifndef ARDUINO_INSTALL_HH
#define ARDUINO_INSTALL_HH

#include <QDir>
#include <QFileDevice>

class ArduinoInstallation: public QObject {
    Q_OBJECT

    QDir dir_;

    bool valid_;
    bool integrated_;

    QString arduino_version_;
    bool arduino_legacy_;
    QString teensyduino_version_;

public:
    ArduinoInstallation(const QString &path = QString());

    QString path() const { return dir_.path(); }
    QString absolutePath() const { return dir_.absolutePath(); }

    bool isValid() const { return valid_; }
    bool isIntegrated() const { return integrated_; }

    QString arduinoVersion() const { return arduino_version_; }
    bool isArduinoLegacy() const { return arduino_legacy_; }
    QString teensyduinoVersion() const { return teensyduino_version_; }

public slots:
    void setPath(const QString &path);
    void update();

    bool integrate();
    bool restore();

signals:
    void changed();

    void log(const QString &msg);
    void error(const QString &msg);

private:
    bool integrateLegacy();
    bool writeAvrdudeScript();
    bool restoreLegacy();

    void updateState();

    bool safeCopy(const QString &filename, const QString &new_filename);
    QString readVersion(const QString &filename);
    bool findMarker(const QString &filename, const QString &marker);

    QString arduinoPath(const QString &path) const;
    QString nicePath(const QString &path) const;

    bool reportFileError(const QFileDevice &file);
};

#endif
