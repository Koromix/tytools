/*
 * ty, a collection of GUI and command-line tools to manage Teensy devices
 *
 * Distributed under the MIT license (see LICENSE.txt or http://opensource.org/licenses/MIT)
 * Copyright (c) 2015 Niels Martignène <niels.martignene@gmail.com>
 */

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

    void setPath(const QString &path);
    QString path() const { return dir_.path(); }
    QString absolutePath() const { return dir_.absolutePath(); }

    void update();

    bool isValid() const { return valid_; }
    bool isIntegrated() const { return integrated_; }

    QString arduinoVersion() const { return arduino_version_; }
    bool isArduinoLegacy() const { return arduino_legacy_; }
    QString teensyduinoVersion() const { return teensyduino_version_; }

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
