/* libhs - public domain
   Niels Martignène <niels.martignene@gmail.com>
   https://neodd.com/libhs

   This software is in the public domain. Where that dedication is not
   recognized, you are granted a perpetual, irrevocable license to copy,
   distribute, and modify this file as you see fit.

   See the LICENSE file for more details. */

#ifndef _HS_FILTER_PRIV_H
#define _HS_FILTER_PRIV_H

#include "common_priv.h"
#include "device.h"
#include "match.h"

typedef struct _hs_filter {
    hs_match *matches;
    unsigned int count;

    uint32_t types;
} _hs_filter;

int _hs_filter_init(_hs_filter *filter, const hs_match *matches, unsigned int count);
void _hs_filter_release(_hs_filter *filter);

bool _hs_filter_match_device(const _hs_filter *filter, const hs_device *dev);
bool _hs_filter_has_type(const _hs_filter *filter, hs_device_type type);

#endif
