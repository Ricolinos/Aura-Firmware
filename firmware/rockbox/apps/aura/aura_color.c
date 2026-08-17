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
#include "aura_color.h"

static int clamp_255(int v)
{
    if (v < 0) return 0;
    if (v > 255) return 255;
    return v;
}

static int clamp_pct(int v)
{
    if (v < 0) return 0;
    if (v > 100) return 100;
    return v;
}

aura_rgb_t aura_color_from_rgb24(unsigned rgb24)
{
    aura_rgb_t c;
    c.r = (int)((rgb24 >> 16) & 0xFF);
    c.g = (int)((rgb24 >> 8) & 0xFF);
    c.b = (int)(rgb24 & 0xFF);
    return c;
}

unsigned aura_color_to_rgb24(aura_rgb_t c)
{
    unsigned r = (unsigned)clamp_255(c.r);
    unsigned g = (unsigned)clamp_255(c.g);
    unsigned b = (unsigned)clamp_255(c.b);
    return (r << 16) | (g << 8) | b;
}

static int blend_channel(int from, int to, int pct)
{
    /* Redondeo al mas cercano (+50 antes de dividir /100), no truncado
     * hacia abajo -- para que pct=50 entre dos canales de diferencia
     * impar no se sesgue siempre hacia `from`. */
    int delta = to - from;
    int scaled = delta * pct;
    int rounded = (scaled >= 0) ? (scaled + 50) / 100 : (scaled - 50) / 100;
    return clamp_255(from + rounded);
}

aura_rgb_t aura_color_blend_pct(aura_rgb_t c, aura_rgb_t toward, int pct)
{
    aura_rgb_t out;
    int p = clamp_pct(pct);

    out.r = blend_channel(c.r, toward.r, p);
    out.g = blend_channel(c.g, toward.g, p);
    out.b = blend_channel(c.b, toward.b, p);
    return out;
}
