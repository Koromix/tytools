/* TyTools - public domain
   Niels Martignène <niels.martignene@protonmail.com>
   https://koromix.dev/tytools

   This software is in the public domain. Where that dedication is not
   recognized, you are granted a perpetual, irrevocable license to copy,
   distribute, and modify this file as you see fit.

   See the LICENSE file for more details. */

#include "common.h"
#include "class_priv.h"
#include "../libhs/device.h"

extern const struct _ty_class_vtable _ty_teensy_class_vtable;
extern const struct _ty_class_vtable _ty_generic_class_vtable;

static const hs_match_spec default_match_specs[] = {
    HS_MATCH_VID_PID(0x16C0, 0x0476, (void *)&_ty_teensy_class_vtable),
    HS_MATCH_VID_PID(0x16C0, 0x0478, (void *)&_ty_teensy_class_vtable),
    HS_MATCH_VID_PID(0x16C0, 0x0482, (void *)&_ty_teensy_class_vtable),
    HS_MATCH_VID_PID(0x16C0, 0x0483, (void *)&_ty_teensy_class_vtable),
    HS_MATCH_VID_PID(0x16C0, 0x0484, (void *)&_ty_teensy_class_vtable),
    HS_MATCH_VID_PID(0x16C0, 0x0485, (void *)&_ty_teensy_class_vtable),
    HS_MATCH_VID_PID(0x16C0, 0x0486, (void *)&_ty_teensy_class_vtable),
    HS_MATCH_VID_PID(0x16C0, 0x0487, (void *)&_ty_teensy_class_vtable),
    HS_MATCH_VID_PID(0x16C0, 0x0488, (void *)&_ty_teensy_class_vtable),
    HS_MATCH_VID_PID(0x16C0, 0x0489, (void *)&_ty_teensy_class_vtable),
    HS_MATCH_VID_PID(0x16C0, 0x048A, (void *)&_ty_teensy_class_vtable),
    HS_MATCH_VID_PID(0x16C0, 0x048B, (void *)&_ty_teensy_class_vtable),
    HS_MATCH_VID_PID(0x16C0, 0x048C, (void *)&_ty_teensy_class_vtable),
    HS_MATCH_VID_PID(0x16C0, 0x04D0, (void *)&_ty_teensy_class_vtable),
    HS_MATCH_VID_PID(0x16C0, 0x04D1, (void *)&_ty_teensy_class_vtable),
    HS_MATCH_VID_PID(0x16C0, 0x04D2, (void *)&_ty_teensy_class_vtable),
    HS_MATCH_VID_PID(0x16C0, 0x04D3, (void *)&_ty_teensy_class_vtable),
    HS_MATCH_VID_PID(0x16C0, 0x04D4, (void *)&_ty_teensy_class_vtable),
    HS_MATCH_VID_PID(0x16C0, 0x04D5, (void *)&_ty_teensy_class_vtable),
    HS_MATCH_VID_PID(0x16C0, 0x04D9, (void *)&_ty_teensy_class_vtable),

    HS_MATCH_TYPE(HS_DEVICE_TYPE_SERIAL, (void *)&_ty_generic_class_vtable)
};

const hs_match_spec *_ty_class_match_specs = default_match_specs;
unsigned int _ty_class_match_specs_count = _HS_COUNTOF(default_match_specs);
