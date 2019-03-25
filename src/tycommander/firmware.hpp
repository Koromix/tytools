/* TyTools - public domain
   Niels Martignène <niels.martignene@protonmail.com>
   https://koromix.dev/tytools

   This software is in the public domain. Where that dedication is not
   recognized, you are granted a perpetual, irrevocable license to copy,
   distribute, and modify this file as you see fit.

   See the LICENSE file for more details. */

#ifndef FIRMWARE_HH
#define FIRMWARE_HH

#include <QString>

#include <memory>

#include "../libty/firmware.h"

class Firmware : public std::enable_shared_from_this<Firmware> {
    ty_firmware *fw_ = nullptr;

public:
    ~Firmware();

    Firmware& operator=(const Firmware &other) = delete;
    Firmware(const Firmware &other) = delete;
    Firmware& operator=(const Firmware &&other) = delete;
    Firmware(const Firmware &&other) = delete;

    static std::shared_ptr<Firmware> load(const QString &filename);

    QString filename() const { return fw_->filename; }
    QString name() const { return fw_->name; }

    ty_firmware *firmware() const { return fw_; }

private:
    Firmware(ty_firmware *fw)
        : fw_(fw) {}
};

#endif
