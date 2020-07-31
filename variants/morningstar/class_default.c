/* TyTools - public domain
   Niels Martignène <niels.martignene@protonmail.com>
   https://koromix.dev/tytools

   This software is in the public domain. Where that dedication is not
   recognized, you are granted a perpetual, irrevocable license to copy,
   distribute, and modify this file as you see fit.

   See the LICENSE file for more details. */

#include "../../src/libty/common_priv.h"
#include "../../src/libty/class_priv.h"
#include "../../src/libhs/device.h"

extern const struct _ty_class_vtable _ty_teensy_class_vtable;
extern const struct _ty_class_vtable _ty_generic_class_vtable;

static const hs_match_spec default_match_specs[] = {
    HS_MATCH_VID_PID(0x16C0, 0x0485, (void *)&_ty_teensy_class_vtable), // USB MIDI
    HS_MATCH_VID_PID(0x16C0, 0x0478, (void *)&_ty_teensy_class_vtable), // Halfkay
    HS_MATCH_VID_PID(0x331b, 0x01,   (void *)&_ty_teensy_class_vtable),
    HS_MATCH_VID_PID(0x331b, 0x02,   (void *)&_ty_teensy_class_vtable),
    HS_MATCH_VID_PID(0x331b, 0x03,   (void *)&_ty_teensy_class_vtable),
    HS_MATCH_VID_PID(0x331b, 0x04,   (void *)&_ty_teensy_class_vtable),
    HS_MATCH_VID_PID(0x331b, 0x05,   (void *)&_ty_teensy_class_vtable),
    HS_MATCH_VID_PID(0x331b, 0x06,   (void *)&_ty_teensy_class_vtable),
    HS_MATCH_VID_PID(0x331b, 0x07,   (void *)&_ty_teensy_class_vtable),
    HS_MATCH_VID_PID(0x331b, 0x08,   (void *)&_ty_teensy_class_vtable),
    HS_MATCH_VID_PID(0x331b, 0x09,   (void *)&_ty_teensy_class_vtable),
    HS_MATCH_VID_PID(0x331b, 0x0A,   (void *)&_ty_teensy_class_vtable),
    HS_MATCH_VID_PID(0x331b, 0x0B,   (void *)&_ty_teensy_class_vtable),
    HS_MATCH_VID_PID(0x331b, 0x0C,   (void *)&_ty_teensy_class_vtable),
    HS_MATCH_VID_PID(0x331b, 0x0D,   (void *)&_ty_teensy_class_vtable),
    HS_MATCH_VID_PID(0x331b, 0x0E,   (void *)&_ty_teensy_class_vtable),
    HS_MATCH_VID_PID(0x331b, 0x0F,   (void *)&_ty_teensy_class_vtable),
    HS_MATCH_VID_PID(0x331b, 0x10,   (void *)&_ty_teensy_class_vtable),
    HS_MATCH_VID_PID(0x331b, 0x11,   (void *)&_ty_teensy_class_vtable),
    HS_MATCH_VID_PID(0x331b, 0x12,   (void *)&_ty_teensy_class_vtable),
    HS_MATCH_VID_PID(0x331b, 0x13,   (void *)&_ty_teensy_class_vtable),
    HS_MATCH_VID_PID(0x331b, 0x14,   (void *)&_ty_teensy_class_vtable),
    HS_MATCH_VID_PID(0x331b, 0x15,   (void *)&_ty_teensy_class_vtable),
    HS_MATCH_VID_PID(0x331b, 0x16,   (void *)&_ty_teensy_class_vtable),
    HS_MATCH_VID_PID(0x331b, 0x17,   (void *)&_ty_teensy_class_vtable),
    HS_MATCH_VID_PID(0x331b, 0x18,   (void *)&_ty_teensy_class_vtable),
    HS_MATCH_VID_PID(0x331b, 0x19,   (void *)&_ty_teensy_class_vtable),
    HS_MATCH_VID_PID(0x331b, 0x1A,   (void *)&_ty_teensy_class_vtable),
    HS_MATCH_VID_PID(0x331b, 0x1B,   (void *)&_ty_teensy_class_vtable),
    HS_MATCH_VID_PID(0x331b, 0x1C,   (void *)&_ty_teensy_class_vtable),
    HS_MATCH_VID_PID(0x331b, 0x1D,   (void *)&_ty_teensy_class_vtable),
    HS_MATCH_VID_PID(0x331b, 0x1E,   (void *)&_ty_teensy_class_vtable),
    HS_MATCH_VID_PID(0x331b, 0x1F,   (void *)&_ty_teensy_class_vtable),

    HS_MATCH_TYPE(HS_DEVICE_TYPE_SERIAL, (void *)&_ty_generic_class_vtable)
};

const hs_match_spec *_ty_class_match_specs = default_match_specs;
unsigned int _ty_class_match_specs_count = TY_COUNTOF(default_match_specs);
