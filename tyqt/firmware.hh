/*
 * ty, a collection of GUI and command-line tools to manage Teensy devices
 *
 * Distributed under the MIT license (see LICENSE.txt or http://opensource.org/licenses/MIT)
 * Copyright (c) 2015 Niels Martignène <niels.martignene@gmail.com>
 */

#ifndef FIRMWARE_HH
#define FIRMWARE_HH

#include <QString>

#include <memory>

#include "ty/firmware.h"

class Firmware : public std::enable_shared_from_this<Firmware> {
    tyb_firmware *fw_ = nullptr;

public:
    ~Firmware();

    Firmware& operator=(const Firmware &other) = delete;
    Firmware(const Firmware &other) = delete;
    Firmware& operator=(const Firmware &&other) = delete;
    Firmware(const Firmware &&other) = delete;

    static std::shared_ptr<Firmware> load(const QString &filename);

    QString filename() const { return tyb_firmware_get_filename(fw_); }
    QString name() const { return tyb_firmware_get_name(fw_); }
    size_t size() const { return tyb_firmware_get_size(fw_); }

    tyb_firmware *firmware() const { return fw_; }

private:
    Firmware(tyb_firmware *fw)
        : fw_(fw) {}
};

#endif
