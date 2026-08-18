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
#include "aura_device_name.h"

size_t aura_device_name_sanitize(const char *in, char *out, size_t outsz)
{
    size_t n = 0;
    int pending_space = 0;
    size_t limit;

    if (!out || outsz == 0)
        return 0;
    out[0] = '\0';
    if (!in)
        return 0;

    limit = outsz - 1;
    if (limit > AURA_DEVICE_NAME_MAX_BYTES)
        limit = AURA_DEVICE_NAME_MAX_BYTES;

    /* saltar espacios iniciales */
    while (*in == ' ' || *in == '\t')
        in++;

    while (*in)
    {
        unsigned char c = (unsigned char)*in;
        size_t seq;

        if (c == ' ' || c == '\t')
        {
            pending_space = (n > 0); /* nunca al inicio; el final se descarta abajo */
            in++;
            continue;
        }
        if (c < 0x20 || c == 0x7F)
        {
            in++;
            continue;
        }
        /* largo de la secuencia UTF-8 que empieza en c */
        if (c < 0x80)       seq = 1;
        else if (c < 0xE0)  seq = 2;
        else if (c < 0xF0)  seq = 3;
        else                seq = 4;

        if (n + (pending_space ? 1 : 0) + seq > limit)
            break;

        if (pending_space)
        {
            out[n++] = ' ';
            pending_space = 0;
        }
        while (seq-- && *in)
            out[n++] = *in++;
    }
    out[n] = '\0';
    return n;
}
