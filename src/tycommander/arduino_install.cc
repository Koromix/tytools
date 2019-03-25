/* TyTools - public domain
   Niels Martignène <niels.martignene@protonmail.com>
   https://koromix.dev/tytools

   This software is in the public domain. Where that dedication is not
   recognized, you are granted a perpetual, irrevocable license to copy,
   distribute, and modify this file as you see fit.

   See the LICENSE file for more details. */

#ifdef _WIN32
    #include <windows.h>
#endif
#include <QCoreApplication>
#include <QFile>
#include <QSaveFile>
#include <QTextStream>

#include "arduino_install.hpp"
#include "tycommander.hpp"

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
    if (arduino_legacy_)
        return integrateLegacy();

    emit log(tr("Integrate TyCommander to '%1'").arg(QDir::toNativeSeparators(dir_.path())));

    auto filename = arduinoPath("hardware/teensy/avr/platform.txt");
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
            emit log(tr(" + Integrate TyCommander instructions after line %1").arg(i));
            out << QString("\n## TyQt\n"
                           "tools.teensyloader.cmd.path=%1\n").arg(QDir::toNativeSeparators(TyCommander::clientFilePath()));
            out << "tools.teensyloader.upload.params.quiet=--quiet\n"
                   "tools.teensyloader.upload.params.verbose=\n"
                   "recipe.objcopy.tyqt.pattern=\"{compiler.path}{build.toolchain}{build.command.objcopy}\" {compiler.elf2hex.flags} \"{build.path}/{build.project_name}.elf\" \"{build.path}/{build.project_name}.{build.board}.hex\"\n"
                   "tools.teensyloader.upload.pattern=\"{cmd.path}\" upload --autostart --wait --multi"
                   " {upload.verbose} \"{build.path}/{build.project_name}.{build.board}.hex\"\n";
            integrated = true;
        }
    }
    if (!integrated) {
        emit error(tr("Failed to add TyCommander instructions"));
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
    emit log(tr("Remove TyCommander integration from '%1'").arg(QDir::toNativeSeparators(dir_.path())));

    QString filename;
    if (arduino_legacy_) {
        filename = arduinoPath("hardware/teensy/boards.txt");
    } else {
        filename = arduinoPath("hardware/teensy/avr/platform.txt");
    }
    if (!findMarker(filename, "TyQt")) {
        emit error(tr("This installation is not using TyCommander"));
        return false;
    }

    emit log(tr("Copy '%1' to '%2'").arg(nicePath(filename + ".notyqt"), nicePath(filename)));
    if (!safeCopy(filename + ".notyqt", filename))
        return false;

    if (arduino_legacy_) {
#ifdef _WIN32
        emit log(tr("Remove avrdude script '%1'").arg("hardware/tools/tyqt_avrdude.bat"));
        QFile::remove(arduinoPath("hardware/tools/tyqt_avrdude.bat"));
#else
        emit log(tr("Remove avrdude script '%1'").arg("hardware/tools/tyqt_avrdude.sh"));
        QFile::remove(arduinoPath("hardware/tools/tyqt_avrdude.sh"));
#endif
    }

    update();
    return true;
}

bool ArduinoInstallation::integrateLegacy()
{
    emit log(tr("Integrate TyCommander to '%1' (legacy)").arg(QDir::toNativeSeparators(dir_.path())));

    auto filename = arduinoPath("hardware/teensy/boards.txt");
    emit log(tr("Rewrite '%1' (to temporary file)").arg(nicePath(filename)));

    QFile src(filename);
    QSaveFile dest(filename);

    if (!src.open(QIODevice::ReadOnly | QIODevice::Text))
        return reportFileError(src);
    if (!dest.open(QIODevice::WriteOnly | QIODevice::Text))
        return reportFileError(dest);

    QTextStream in(&src), out(&dest);
    QStringList models;
    for (unsigned int i = 1; !in.atEnd(); i++) {
        auto line = in.readLine();

        if (line.contains("TyQt", Qt::CaseInsensitive)) {
            emit error(tr("This installation is already patched"));
            return false;
        }

        if (line.startsWith("teensy") && line.contains("upload.avrdude_wrapper")) {
            models.append(line.section('.', 0, 0));

            emit log(tr(" + Comment out line %1 '%2...'").arg(i).arg(line.left(22)));
            out << "#";
        } else if (line.contains("teensy_post_compile")) {
            emit log(tr(" + Comment out line %1 '%2...'").arg(i).arg(line.left(22)));
            out << "#";
        }
        out << line << "\n";
    }
    if (models.isEmpty()) {
        emit error(tr("Failed to add TyCommander instructions"));
        return false;
    }

    out << "\n## TyQt (legacy Arduino)\n";
    for (auto &model: models) {
        emit log(tr(" + Add TyCommander instructions for '%1'").arg(model));
#ifdef _WIN32
        out << QString("%1.upload.avrdude_wrapper=tyqt_avrdude.bat\n").arg(model);
#else
        out << QString("%1.upload.avrdude_wrapper=tyqt_avrdude.sh\n").arg(model);
#endif
    }

    if (src.error())
        return reportFileError(src);
    src.close();

    if (dest.error())
        return reportFileError(dest);
    if (!dest.flush())
        return reportFileError(dest);

    if (!writeAvrdudeScript())
        return false;

    emit log(tr("Backup '%1' to '%2'").arg(nicePath(filename), nicePath(filename + ".notyqt")));
    if (!safeCopy(filename, filename + ".notyqt"))
        return false;

    emit log(tr("Commit changes to '%1'").arg(nicePath(filename)));
    if (!dest.commit())
        return reportFileError(dest);

    update();
    return true;
}

bool ArduinoInstallation::writeAvrdudeScript()
{
#ifdef _WIN32
    QFile script(arduinoPath("hardware/tools/tyqt_avrdude.bat"));
    emit log(tr("Write avrdude script to '%1'").arg(nicePath(script.fileName())));

    if (!script.open(QIODevice::WriteOnly | QIODevice::Text))
        return reportFileError(script);

    QTextStream script_out(&script);
    script_out << "@echo off\n";
    script_out << QString("\"%1\" avrdude %*\n")
                  .arg(QDir::toNativeSeparators(QCoreApplication::applicationFilePath()));

    if (script.error())
        return reportFileError(script);
    if (!script.flush())
        return reportFileError(script);
#else
    QFile script(arduinoPath("hardware/tools/tyqt_avrdude.sh"));
    emit log(tr("Write avrdude script to '%1'").arg(nicePath(script.fileName())));

    if (!script.open(QIODevice::WriteOnly | QIODevice::Text))
        return reportFileError(script);

    QTextStream script_out(&script);
    script_out << "#!/bin/sh\n";
    script_out << QString("\"%1\" avrdude \"$@\"\n").arg(QCoreApplication::applicationFilePath());

    if (script.error())
        return reportFileError(script);
    if (!script.flush())
        return reportFileError(script);
    script.close();

    if (!script.setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner |
                               QFile::ReadGroup | QFile::ExeGroup | QFile::ReadOther | QFile::ExeOther))
        return reportFileError(script);
#endif

    return true;
}

void ArduinoInstallation::updateState()
{
    valid_ = false;
    integrated_ = false;

    arduino_version_ = "";
    arduino_legacy_ = false;
    teensyduino_version_ = "";

    if (dir_.path().isEmpty() || !dir_.exists())
        return;

    arduino_version_ = readVersion(arduinoPath("lib/version.txt"));
#ifdef __APPLE__
    if (arduino_version_.isEmpty())
        arduino_version_ = readVersion(dir_.filePath("Contents/Resources/Java/lib/version.txt"));
#endif
    if (arduino_version_.isEmpty())
        return;
    arduino_legacy_ = arduino_version_.startsWith("1.0.");
    teensyduino_version_ = readVersion(arduinoPath("lib/teensyduino.txt"));
    if (teensyduino_version_.isEmpty())
        return;

    valid_ = true;
    if (arduino_legacy_) {
        integrated_ = findMarker(arduinoPath("hardware/teensy/boards.txt"), "TyQt");
    } else {
        integrated_ = findMarker(arduinoPath("hardware/teensy/avr/platform.txt"), "TyQt");
    }
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
    QFile file(filename);

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

QString ArduinoInstallation::arduinoPath(const QString &path) const
{
#ifdef __APPLE__
    if (arduino_legacy_) {
        return dir_.filePath("Contents/Resources/Java/" + path);
    } else {
        return dir_.filePath("Contents/Java/" + path);
    }
#else
    return dir_.filePath(path);
#endif
}

QString ArduinoInstallation::nicePath(const QString &path) const
{
    return QDir::toNativeSeparators(dir_.relativeFilePath(path));
}

bool ArduinoInstallation::reportFileError(const QFileDevice &dev)
{
    emit error(QString("%1").arg(dev.errorString()));
    return false;
}
