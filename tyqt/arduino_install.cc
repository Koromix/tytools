/*
 * ty, a collection of GUI and command-line tools to manage Teensy devices
 *
 * Distributed under the MIT license (see LICENSE.txt or http://opensource.org/licenses/MIT)
 * Copyright (c) 2015 Niels Martignène <niels.martignene@gmail.com>
 */

#ifdef _WIN32
    #include <windows.h>
#endif
#include <QCoreApplication>
#include <QFile>
#include <QSaveFile>
#include <QTextStream>

#include "arduino_install.hh"
#include "tyqt.hh"

using namespace std;

ArduinoInstallation::ArduinoInstallation(const QString &path)
{
    setPath(path);
}

void ArduinoInstallation::setPath(const QString &path)
{
    dir_.setPath(path);
    update();
}

void ArduinoInstallation::update()
{
    updateState();
    emit changed();
}

bool ArduinoInstallation::integrate()
{
    emit log(tr("Integrate TyQt to '%1'").arg(QDir::toNativeSeparators(dir_.path())));

    auto filename = dir_.filePath("hardware/teensy/avr/platform.txt");
    emit log(tr("Rewrite '%1' (to temporary file)").arg(nicePath(filename)));

    QFile src(filename);
    QSaveFile dest(filename);

    if (!src.open(QIODevice::ReadOnly | QIODevice::Text))
        return reportFileError(src);
    if (!dest.open(QIODevice::WriteOnly | QIODevice::Text))
        return reportFileError(dest);

    QTextStream in(&src), out(&dest);
    bool integrated = false;
    for (unsigned int i = 1; !in.atEnd(); i++) {
        auto line = in.readLine();

        if (line.contains("TyQt", Qt::CaseInsensitive)) {
            emit error(tr("This installation is already patched"));
            return false;
        }

        if (line.startsWith("tools.teensyloader") || line.contains("teensy_post_compile")) {
            emit log(tr(" + Comment out line %1 '%2...'").arg(i).arg(line.left(22)));
            out << "#";
        }
        out << line << "\n";

        if (line.startsWith("tools.teensyloader.upload.pattern") && !integrated) {
            emit log(tr(" + Integrate TyQt instructions after line %1").arg(i));
            out << QString("\n## TyQt\n"
                           "tools.teensyloader.cmd.path=%1\n").arg(QDir::toNativeSeparators(tyQt->clientFilePath()));
            out << "tools.teensyloader.upload.params.quiet=--quiet\n"
                   "tools.teensyloader.upload.params.verbose=\n"
                   "tools.teensyloader.upload.pattern=\"{cmd.path}\" upload --autostart --wait"
                   " --board=@{serial.port} --usbtype {build.usbtype} \"{build.path}/{build.project_name}.hex\"\n";
            integrated = true;
        }
    }
    if (!integrated) {
        emit error(tr("Failed to add TyQt instructions"));
        return false;
    }

    if (src.error())
        return reportFileError(src);
    src.close();

    if (dest.error())
        return reportFileError(dest);
    if (!dest.flush())
        return reportFileError(dest);

    emit log(tr("Backup '%1' to '%2'").arg(nicePath(filename), nicePath(filename + ".notyqt")));
    if (!safeCopy(filename, filename + ".notyqt"))
        return false;

    emit log(tr("Commit changes to '%1'").arg(nicePath(filename)));
    if (!dest.commit())
        return reportFileError(dest);

    update();
    return true;
}

bool ArduinoInstallation::restore()
{
    emit log(tr("Remove TyQt integration from '%1'").arg(QDir::toNativeSeparators(dir_.path())));

    auto filename = dir_.filePath("hardware/teensy/avr/platform.txt");
    if (!findMarker(filename, "TyQt")) {
        emit error(tr("This installation is not using TyQt"));
        return false;
    }

    emit log(tr("Copy '%1' to '%2'").arg(nicePath(filename + ".notyqt"), nicePath(filename)));
    if (!safeCopy(filename + ".notyqt", filename))
        return false;

    update();
    return true;
}

void ArduinoInstallation::updateState()
{
    valid_ = false;
    integrated_ = false;
    arduino_version_ = "";
    teensyduino_version_ = "";

    if (dir_.path().isEmpty() || !dir_.exists())
        return;

    arduino_version_ = readVersion("lib/version.txt");
    if (arduino_version_.isEmpty())
        return;
    teensyduino_version_ = readVersion("lib/teensyduino.txt");
    if (teensyduino_version_.isEmpty())
        return;

    valid_ = true;
    integrated_ = findMarker(dir_.filePath("hardware/teensy/avr/platform.txt"), "TyQt");
}

bool ArduinoInstallation::safeCopy(const QString &filename, const QString &new_filename)
{
    QFile src(filename);
    QSaveFile dest(new_filename);

    if (!src.open(QIODevice::ReadOnly))
        return reportFileError(src);
    if (!dest.open(QIODevice::WriteOnly))
        return reportFileError(dest);

    /* Qt provides no way to do an atomic rename outside of QSaveFile, and no way to
       to copy a QFile to a QSaveFile.  */
    char buf[8192];
    int64_t len;
    while ((len = src.read(buf, sizeof(buf))) > 0) {
        if (dest.write(buf, len) < len)
            return reportFileError(dest);
    }

    if (src.error())
        return reportFileError(src);

    if (!dest.commit())
        return reportFileError(dest);

    return true;
}

QString ArduinoInstallation::readVersion(const QString &filename)
{
    QFile file(dir_.filePath(filename));

    if (!file.exists())
        return "";
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return "";

    return QTextStream(&file).readLine(32);
}

bool ArduinoInstallation::findMarker(const QString &filename, const QString &marker)
{
    QFile file(filename);

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return false;

    QTextStream in(&file);
    while (!in.atEnd()) {
        if (in.readLine().contains(marker, Qt::CaseInsensitive))
            return true;
    }

    return false;
}

QString ArduinoInstallation::nicePath(const QString &path)
{
    return QDir::toNativeSeparators(dir_.relativeFilePath(path));
}

bool ArduinoInstallation::reportFileError(const QFileDevice &dev)
{
    emit error(QString("%1").arg(dev.errorString()));
    return false;
}
