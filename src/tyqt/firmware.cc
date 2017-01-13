/* Teensy Tools - public domain
   Niels Martignène <niels.martignene@gmail.com>
   https://neodd.com/teensytools

   This software is in the public domain. Where that dedication is not
   recognized, you are granted a perpetual, irrevocable license to copy,
   distribute, and modify this file as you see fit.

   See the LICENSE file for more details. */

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
