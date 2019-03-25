/* libhs - public domain
   Niels Martignène <niels.martignene@protonmail.com>
   https://koromix.dev/libhs

   This software is in the public domain. Where that dedication is not
   recognized, you are granted a perpetual, irrevocable license to copy,
   distribute, and modify this file as you see fit.

   See the LICENSE file for more details. */

#ifndef _HS_MATCH_PRIV_H
#define _HS_MATCH_PRIV_H

#include "common_priv.h"
#include "device.h"
#include "match.h"

typedef struct _hs_match_helper {
    hs_match_spec *specs;
    unsigned int specs_count;

    uint32_t types;
} _hs_match_helper;

int _hs_match_helper_init(_hs_match_helper *helper, const hs_match_spec *specs,
                          unsigned int specs_count);
void _hs_match_helper_release(_hs_match_helper *helper);

bool _hs_match_helper_match(const _hs_match_helper *helper, const hs_device *dev,
                            void **rmatch_udata);
bool _hs_match_helper_has_type(const _hs_match_helper *helper, hs_device_type type);

#endif
