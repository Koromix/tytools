/* libhs - public domain
    Niels Martignène <niels.martignene@protonmail.com>
    https://koromix.dev/libhs

    This software is in the public domain. Where that dedication is not
    recognized, you are granted a perpetual, irrevocable license to copy,
    distribute, and modify this file as you see fit.

    See the LICENSE file for more details. */

#include "common_priv.h"
#ifndef _WIN32
    #include <sys/stat.h>
#endif
#include "match_priv.h"

int hs_match_parse(const char *str, hs_match_spec *rspec)
{
    unsigned int vid = 0, pid = 0, type = 0;

    str += strspn(str, " ");
    if (str[0]) {
        char type_buf[16];
        int r;

        r = sscanf(str, "%04x:%04x/%15s", &vid, &pid, type_buf);
        if (r < 2)
            return hs_error(HS_ERROR_PARSE, "Malformed device match string '%s'", str);

        if (r == 3) {
            for (unsigned int i = 1; i < _HS_COUNTOF(hs_device_type_strings); i++) {
                if (!strcmp(hs_device_type_strings[i], type_buf))
                    type = i;
            }
            if (!type)
                return hs_error(HS_ERROR_PARSE, "Unknown device type '%s' in match string '%s'",
                                type_buf, str);
        }
    }

    memset(rspec, 0, sizeof(*rspec));
    rspec->vid = (uint16_t)vid;
    rspec->pid = (uint16_t)pid;
    rspec->type = type;
    return 0;
}

int _hs_match_helper_init(_hs_match_helper *helper, const hs_match_spec *specs,
                          unsigned int specs_count)
{
    if (!specs) {
        helper->specs = NULL;
        helper->specs_count = 0;
        helper->types = UINT32_MAX;

        return 0;
    }

    // I promise to be careful
    helper->specs = specs_count ? (hs_match_spec *)specs : NULL;
    helper->specs_count = specs_count;

    helper->types = 0;
    for (unsigned int i = 0; i < specs_count; i++) {
        if (!specs[i].type) {
            helper->types = UINT32_MAX;
            break;
        }

        helper->types |= (uint32_t)(1 << specs[i].type);
    }

    return 0;
}

void _hs_match_helper_release(_hs_match_helper *helper)
{
    _HS_UNUSED(helper);
}

static bool test_spec(const hs_match_spec *spec, const hs_device *dev)
{
    if (spec->type && dev->type != (hs_device_type)spec->type)
        return false;
    if (spec->vid && dev->vid != spec->vid)
        return false;
    if (spec->pid && dev->pid != spec->pid)
        return false;

    return true;
}

bool _hs_match_helper_match(const _hs_match_helper *helper, const hs_device *dev,
                            void **rmatch_udata)
{
    // Do the fast checks first
    if (!_hs_match_helper_has_type(helper, dev->type))
        return false;
    if (!helper->specs_count) {
        if (rmatch_udata)
            *rmatch_udata = NULL;
        return true;
    }

    for (unsigned int i = 0; i < helper->specs_count; i++) {
        if (test_spec(&helper->specs[i], dev)) {
            if (rmatch_udata)
                *rmatch_udata = helper->specs[i].udata;
            return true;
        }
    }

    return false;
}

bool _hs_match_helper_has_type(const _hs_match_helper *helper, hs_device_type type)
{
    return helper->types & (uint32_t)(1 << type);
}
