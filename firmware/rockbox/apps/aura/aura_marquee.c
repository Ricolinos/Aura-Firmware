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
#include "lcd.h"

#include "aura_marquee.h"
#include "aura_patterns.h"
#include "apple2026_tokens.h"

int aura_marquee_draw(int x, int y, int max_width, const char *text, long elapsed_ms)
{
    int w, h;
    struct viewport vp;
    struct viewport *saved;
    int scroll_distance, offset;

    if (!text)
        return 0;

    lcd_getstringsize((const unsigned char *)text, &w, &h);

    if (w <= max_width)
    {
        lcd_putsxy(x, y, (const unsigned char *)text);
        return 0;
    }

    /* Recorte duro al ancho disponible: parte del viewport YA activo
     * (conserva fuente/color/drawmode que el llamador ya configuro con
     * lcd_setfont/lcd_set_foreground), solo acota el rectangulo. */
    vp = *lcd_current_viewport;
    vp.x = x;
    vp.y = y;
    vp.width = max_width;
    vp.height = h;
    saved = lcd_set_viewport(&vp);

    scroll_distance = w + AURA_DS_METRICS_MARQUEE_LOOP_GAP;
    offset = aura_pattern_marquee_offset(elapsed_ms,
        AURA_DS_METRICS_MARQUEE_STATIC_MS, AURA_DS_METRICS_MARQUEE_SCROLL_MS,
        scroll_distance);

    /* Dos copias del texto separadas por scroll_distance: mientras la
     * primera sale por la izquierda, la segunda ya esta entrando por
     * la derecha -- "sin fase de entrada separada" (doc). */
    lcd_putsxy(-offset, 0, (const unsigned char *)text);
    lcd_putsxy(-offset + scroll_distance, 0, (const unsigned char *)text);

    lcd_set_viewport(saved);
    return 1;
}
