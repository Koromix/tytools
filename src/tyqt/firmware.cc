/*
 * ty, a collection of GUI and command-line tools to manage Teensy devices
 *
 * Distributed under the MIT license (see LICENSE.txt or http://opensource.org/licenses/MIT)
 * Copyright (c) 2015 Niels Martignène <niels.martignene@gmail.com>
 */

#include "firmware.hpp"

using namespace std;

Firmware::~Firmware()
{
    ty_firmware_unref(fw_);
}

shared_ptr<Firmware> Firmware::load(const QString &filename)
{
    // Work around the private constructor for make_shared()
    struct FirmwareSharedEnabler : public Firmware {
        FirmwareSharedEnabler(ty_firmware *fw)
            : Firmware(fw) {}
    };

    ty_firmware *fw;
    int r;

    r = ty_firmware_load(filename.toLocal8Bit().constData(), nullptr, &fw);
    if (r < 0)
        return nullptr;

    return make_shared<FirmwareSharedEnabler>(fw);
}
