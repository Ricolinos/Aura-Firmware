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
#include <stdio.h>
#include <string.h>

#include "lcd.h"
#include "button.h"
#include "kernel.h"

#include "aura_stopwatch.h"
#include "apple2026_shell.h"
#include "apple2026_tokens.h"
#include "aura_widgets.h"
#include "aura_lang.h"

/* Registros: se conservan todos los que caben; bajo el contador solo se
 * muestran los TRES ultimos, como el original. */
#define SW_MAX_LAPS     32
#define SW_VISIBLE_LAPS 3

static bool s_running = false;
static long s_start_tick = 0;   /* tick en que arranco el tramo actual */
static long s_accum_ms = 0;     /* tiempo acumulado de tramos anteriores */
static long s_laps[SW_MAX_LAPS];
static int  s_lap_count = 0;

bool aura_stopwatch_running(void)
{
    return s_running;
}

static long elapsed_ms(void)
{
    if (!s_running)
        return s_accum_ms;
    return s_accum_ms + (current_tick - s_start_tick) * 1000L / HZ;
}

/* HH:MM:SS y centesimas por separado: el original dibuja las centesimas
 * mas chicas, asi que no pueden ir en la misma cadena. */
static void format_split(long ms, char *main_buf, size_t main_sz,
                          char *cs_buf, size_t cs_sz)
{
    long total_s = ms / 1000;
    /* Acotado a 99 h: mas alla el formato dejaria de ser HH:MM:SS y el
     * compilador tendria razon en avisar de truncamiento. */
    long h = total_s / 3600;

    if (h > 99)
        h = 99;
    snprintf(main_buf, main_sz, "%02d:%02d:%02d",
             (int)h, (int)((total_s / 60) % 60), (int)(total_s % 60));
    snprintf(cs_buf, cs_sz, "%02d", (int)((ms % 1000) / 10));
}

void aura_stopwatch_draw(void)
{
    char main_buf[16], cs_buf[8];
    int mw, mh, cw, ch, x, y, i;

    a26_shell_clear_screen();
    aura_widgets_draw_status_bar(aura_str(AURA_STR_EXTRAS_STOPWATCH));

    format_split(elapsed_ms(), main_buf, sizeof(main_buf), cs_buf, sizeof(cs_buf));

    /* Contador centrado en el tercio superior del area util. */
    lcd_setfont(a26_font(A26_FONT_STYLE_TITLE));
    lcd_getstringsize((const unsigned char *)main_buf, &mw, &mh);
    lcd_setfont(a26_font(A26_FONT_STYLE_DS_BOLD_14));
    lcd_getstringsize((const unsigned char *)cs_buf, &cw, &ch);

    x = (A26_SCREEN_WIDTH - (mw + A26_SPACING_XS + cw)) / 2;
    y = A26_LAYOUT_STATUSBAR_HEIGHT + A26_SPACING_XXL;

    lcd_setfont(a26_font(A26_FONT_STYLE_TITLE));
    lcd_set_foreground(s_running ? aura_accent() : a26_color(A26_TEXT_PRIMARY));
    lcd_putsxy(x, y, (const unsigned char *)main_buf);

    /* Centesimas alineadas por la BASE del contador grande, no por su
     * tope: si no, "flotan" sobre los dos puntos. */
    lcd_setfont(a26_font(A26_FONT_STYLE_DS_BOLD_14));
    lcd_set_foreground(a26_color(A26_TEXT_SECONDARY));
    lcd_putsxy(x + mw + A26_SPACING_XS, y + mh - ch,
               (const unsigned char *)cs_buf);

    /* Icono de estado: en pausa se indica que se puede reanudar. */
    if (!s_running && (s_accum_ms > 0 || s_lap_count > 0))
        aura_widgets_draw_icon("pause-fill", A26_ICON_SIZE_TRANSPORT,
                                (A26_SCREEN_WIDTH - A26_ICON_SIZE_TRANSPORT) / 2,
                                y + mh + A26_SPACING_MD);

    /* Los tres registros mas recientes, del mas nuevo al mas viejo. */
    lcd_setfont(a26_font(A26_FONT_STYLE_DS_REG_12));
    y = y + mh + A26_SPACING_XXL + A26_SPACING_LG;
    for (i = 0; i < SW_VISIBLE_LAPS && i < s_lap_count; i++)
    {
        char row[48];
        int idx = s_lap_count - 1 - i;
        int w, h;

        format_split(s_laps[idx], main_buf, sizeof(main_buf), cs_buf, sizeof(cs_buf));
        snprintf(row, sizeof(row), "%d.  %s.%s", idx + 1, main_buf, cs_buf);
        lcd_set_foreground(i == 0 ? a26_color(A26_TEXT_PRIMARY)
                                   : a26_color(A26_TEXT_SECONDARY));
        lcd_getstringsize((const unsigned char *)row, &w, &h);
        lcd_putsxy((A26_SCREEN_WIDTH - w) / 2, y, (const unsigned char *)row);
        y += h + A26_SPACING_XS;
    }
}

void aura_stopwatch_handle_button(aura_nav_t *nav, long button)
{
    switch (button)
    {
    case BUTTON_SELECT:
        if (!s_running)
        {
            /* Arranca (o reanuda tras una pausa). */
            s_start_tick = current_tick;
            s_running = true;
        }
        else if (s_lap_count < SW_MAX_LAPS)
        {
            /* Registro: el contador NO se detiene, solo se marca. */
            s_laps[s_lap_count++] = elapsed_ms();
        }
        break;

    case BUTTON_PLAY:
        /* Detiene conservando el tiempo -- reanudable. */
        if (s_running)
        {
            s_accum_ms = elapsed_ms();
            s_running = false;
        }
        break;

    case BUTTON_MENU:
        if (s_running)
        {
            s_accum_ms = elapsed_ms();
            s_running = false;
        }
        aura_nav_pop(nav);
        break;

    default:
        break;
    }
}
