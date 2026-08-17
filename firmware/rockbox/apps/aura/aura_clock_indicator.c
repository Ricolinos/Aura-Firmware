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

#include "apple2026_shell.h"
#include "apple2026_tokens.h"
#include "aura_statusbar.h"
#include "aura_patterns.h"

#include "aura_clock_indicator.h"

void aura_clock_indicator_draw(int x, int width, int y, int enter_progress_256)
{
    char buf[16];
    int w, h, text_x, draw_y;
    int height = AURA_DS_METRICS_CLOCK_INDICATOR_HEIGHT;

    if (enter_progress_256 <= 0)
        return; /* totalmente fuera de pantalla por arriba, nada que dibujar */

    aura_format_clock(buf, sizeof(buf));

    /* Opacidad simulada 80% (status-bar.md, tabla de tipografia:
     * "--font-statusbar-time" -- ClockIndicator solo vive dentro de
     * StatusBar, doc header de este archivo) via a26_shell_blend()
     * hacia el fondo del shell, mismo mecanismo que sombras/scrollbars. */
    /* Encargo del dueno (2026-08-14, primera pasada): DS_REG_8 (8px) a
     * DS_REG_10 (10px). Encargo del dueno (2026-08-14, segunda pasada,
     * "el texto casi no se nota, 2px mas grande"): DS_REG_10 -> DS_REG_12
     * -- reusa el estilo ya cargado por np_album/np_artist/lyrics, cero
     * costo de fuente nueva. AURA_DS_METRICS_CLOCK_INDICATOR_HEIGHT/
     * MAX_WIDTH se ajustaron en tokens.json al tamano real del glifo. */
    lcd_setfont(a26_font(A26_FONT_STYLE_DS_REG_12));
    lcd_set_foreground(a26_shell_blend(a26_color(A26_SHELL_BG), a26_color(A26_TEXT_PRIMARY),
                                        AURA_DS_OPACITY_STATUSBAR_TIME_PCT * 256 / 100));
    lcd_getstringsize((const unsigned char *)buf, &w, &h);
    if (w > AURA_DS_METRICS_CLOCK_INDICATOR_MAX_WIDTH)
        w = AURA_DS_METRICS_CLOCK_INDICATOR_MAX_WIDTH; /* HH:MM/HH:MM AM nunca deberia excederlo */

    text_x = x + (width - w) / 2;

    /* Drop-and-Lift (T1.1 aura_pattern_lerp): interpola la Y entre
     * "fuera de pantalla por arriba" (-height) y su posicion final
     * (y), con el progreso ya calculado por el llamador (T1.1 dice que
     * este patron es un solo paso lineal aplicado por cada componente
     * a su propio eje -- aca es Y). */
    draw_y = aura_pattern_lerp(y - height, y, enter_progress_256);

    lcd_putsxy(text_x, draw_y, (const unsigned char *)buf);
}
