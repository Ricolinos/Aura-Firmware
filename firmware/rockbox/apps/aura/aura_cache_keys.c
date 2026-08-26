/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 *
 * Copyright (C) 2026 Ricardo Gómez
 *
 * Aura UI -- capa de interfaz sobre este fork de Rockbox (ver
 * MODIFICATIONS.md, DECISIONS.md D-001/D-002 en la raíz del repositorio).
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This software is distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY
 * KIND, either express or implied.
 *
 ****************************************************************************/
/* D-337/D-338: ver aura_cache_keys.h. */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "aura_cache_keys.h"

bool aura_cache_keys_is_tagcache_file(const char *name)
{
    size_t n = name ? strlen(name) : 0;

    return n > 13 /* "database_" + al menos 1 + ".tcd" */
        && strncmp(name, "database_", 9) == 0
        && strcmp(name + n - 4, ".tcd") == 0;
}

bool aura_cache_keys_stamp_needs_rebuild(const char *recorded, const char *current)
{
    if (recorded == NULL || recorded[0] == '\0')
        return true;
    if (current == NULL)
        return true;
    return strcmp(recorded, current) != 0;
}

int aura_cache_keys_album_name(char *out, size_t outsz,
                               uint32_t path_crc, uint32_t mtime, int size)
{
    int n = snprintf(out, outsz, "a-%08lx-%lu-%d.pfraw",
                     (unsigned long)path_crc, (unsigned long)mtime, size);

    if (n < 0 || (size_t)n >= outsz)
        return -1;
    return n;
}

static bool parse_hex8(const char *s, uint32_t *out)
{
    uint32_t v = 0;
    int i;

    for (i = 0; i < 8; i++)
    {
        char c = s[i];
        int d;

        if (c >= '0' && c <= '9')
            d = c - '0';
        else if (c >= 'a' && c <= 'f')
            d = c - 'a' + 10;
        else
            return false;
        v = (v << 4) | (uint32_t)d;
    }
    *out = v;
    return true;
}

static const char *parse_dec(const char *s, unsigned long *out)
{
    unsigned long v = 0;
    const char *p = s;

    if (*p < '0' || *p > '9')
        return NULL;
    while (*p >= '0' && *p <= '9')
    {
        v = v * 10 + (unsigned long)(*p - '0');
        p++;
    }
    *out = v;
    return p;
}

bool aura_cache_keys_album_parse(const char *name, uint32_t *path_crc,
                                 uint32_t *mtime, int *size)
{
    uint32_t crc;
    unsigned long mt, sz;
    const char *p;

    if (name == NULL || strncmp(name, "a-", 2) != 0)
        return false;
    p = name + 2;
    if (!parse_hex8(p, &crc) || p[8] != '-')
        return false;
    p += 9;
    p = parse_dec(p, &mt);
    if (p == NULL || *p != '-')
        return false;
    p = parse_dec(p + 1, &sz);
    if (p == NULL || strcmp(p, ".pfraw") != 0 || sz == 0 || sz > 4096)
        return false;

    if (path_crc) *path_crc = crc;
    if (mtime)    *mtime = (uint32_t)mt;
    if (size)     *size = (int)sz;
    return true;
}

bool aura_cache_keys_album_parse_name(const char *name)
{
    return aura_cache_keys_album_parse(name, NULL, NULL, NULL);
}
