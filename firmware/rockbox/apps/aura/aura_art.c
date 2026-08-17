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
#include "aura_art.h"

int aura_art_reflection_height(int size, int height_pct)
{
    return size * height_pct / 100;
}

void aura_art_generate_reflection(const fb_data *cover, fb_data *out,
                                   int size, int height_pct, unsigned bg_color,
                                   bool transposed)
{
    int refl_h = aura_art_reflection_height(size, height_pct);
    int bg_r = RGB_UNPACK_RED(bg_color);
    int bg_g = RGB_UNPACK_GREEN(bg_color);
    int bg_b = RGB_UNPACK_BLUE(bg_color);
    int y, x;

    /* `transposed` (PLAN.md T3.2(a), componentes/cover-flow.md: cache
     * .pfraw guarda las caratulas TRANSPUESTAS -- columna contigua en
     * memoria, igual que pictureflow.c, regla dura 7) -- el espejo
     * vertical + atenuacion es una operacion por indices LOGICOS
     * (fila/columna), el layout de memoria solo cambia COMO se
     * calculan los offsets, no la formula. Nunca se transpone un
     * bitmap ya generado: se genera directo en el layout que hace
     * falta. */
    for (y = 0; y < refl_h; y++)
    {
        /* Fila y del reflejo = fila (size-1-y) de la caratula, invertida
         * verticalmente, difuminada hacia el fondo segun se aleja del
         * borde. */
        int source_row = size - 1 - y;
        /* Pico de opacidad del reflejo: 45% (correccion del dueno del
         * diseno 2026-08-12, "el reflejo es muy visible, debe tener una
         * opacidad menor, del 40-50%") -- antes arrancaba en espejo
         * nitido (100%) y solo se desvanecia hacia abajo. */
        int peak = 255 * AURA_ART_REFLECTION_PEAK_PCT / 100;
        int fade = peak - (peak * y) / refl_h;

        for (x = 0; x < size; x++)
        {
            unsigned px = transposed ? cover[(size_t)x * size + source_row]
                                      : cover[(size_t)source_row * size + x];
            int r = RGB_UNPACK_RED(px);
            int g = RGB_UNPACK_GREEN(px);
            int b = RGB_UNPACK_BLUE(px);
            fb_data result;

            r = bg_r + ((r - bg_r) * fade) / 255;
            g = bg_g + ((g - bg_g) * fade) / 255;
            b = bg_b + ((b - bg_b) * fade) / 255;
            result = LCD_RGBPACK(r, g, b);

            if (transposed)
                out[(size_t)x * refl_h + y] = result;
            else
                out[(size_t)y * size + x] = result;
        }
    }
}
