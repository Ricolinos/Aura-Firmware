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
#include <string.h>
#include <stdio.h>

#include "lcd.h"
#include "button.h"
#include "kernel.h"
#include "rtc.h"
#include "timefuncs.h"

#include "aura_calendar.h"
#include "apple2026_shell.h"
#include "apple2026_tokens.h"
#include "aura_widgets.h"
#include "aura_lang.h"
#include "aura_screens.h"

/* Rejilla del mes: 7 columnas x 6 semanas bajo la cabecera. */
#define CAL_COLS      7
#define CAL_ROWS      6
/* +34: deja aire para el titulo del mes (y=22, alto 12) y la cabecera
 * de dias (CAL_TOP-12) sin que se toquen. 6 filas x 28 = 168, asi que
 * la rejilla completa termina en 222 y cabe. */
#define CAL_TOP       (A26_LAYOUT_STATUSBAR_HEIGHT + 34)
#define CAL_CELL_W    (A26_SCREEN_WIDTH / CAL_COLS)
#define CAL_CELL_H    27

static int s_year = 0, s_month = 0, s_day = 1; /* month 0-11 */
static bool s_inited = false;

static const char *const MONTHS_ES[12] = {
    "Enero", "Febrero", "Marzo", "Abril", "Mayo", "Junio",
    "Julio", "Agosto", "Septiembre", "Octubre", "Noviembre", "Diciembre",
};
/* Semana que empieza en LUNES, como el calendario en español. */
static const char *const DOW_ES[7] = { "L", "M", "M", "J", "V", "S", "D" };

static int days_in_month(int year, int month)
{
    static const int base[12] = { 31,28,31,30,31,30,31,31,30,31,30,31 };
    bool leap = (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);

    return (month == 1 && leap) ? 29 : base[month];
}

/* Dia de la semana del 1 del mes, 0 = lunes (Zeller adaptado). */
static int first_dow(int year, int month)
{
    int m = month + 1, y = year, k, j, h;

    if (m < 3) { m += 12; y--; }
    k = y % 100;
    j = y / 100;
    h = (1 + (13 * (m + 1)) / 5 + k + k / 4 + j / 4 + 5 * j) % 7; /* 0=sab */
    return (h + 5) % 7; /* -> 0 = lunes */
}

static void ensure_today(void)
{
    struct tm *now;

    if (s_inited)
        return;
    now = get_time();
    if (now)
    {
        s_year = now->tm_year + 1900;
        s_month = now->tm_mon;
        s_day = now->tm_mday;
    }
    else
    {
        s_year = 2026; s_month = 0; s_day = 1;
    }
    s_inited = true;
}

void aura_calendar_draw(void)
{
    char title[32];
    int total, start, i, w, h;

    ensure_today();
    total = days_in_month(s_year, s_month);
    start = first_dow(s_year, s_month);

    a26_shell_clear_screen();
    aura_widgets_draw_status_bar(aura_str(AURA_STR_EXTRAS_CALENDAR));

    /* Mes y año centrados sobre la rejilla. */
    snprintf(title, sizeof(title), "%s %d", MONTHS_ES[s_month], s_year);
    lcd_setfont(a26_font(A26_FONT_STYLE_DS_BOLD_12));
    lcd_set_foreground(a26_color(A26_TEXT_PRIMARY));
    lcd_getstringsize((const unsigned char *)title, &w, &h);
    lcd_putsxy((A26_SCREEN_WIDTH - w) / 2, A26_LAYOUT_STATUSBAR_HEIGHT + 2,
               (const unsigned char *)title);

    /* Cabecera de dias. */
    lcd_setfont(a26_font(A26_FONT_STYLE_DS_REG_8));
    lcd_set_foreground(a26_color(A26_TEXT_TERTIARY));
    for (i = 0; i < CAL_COLS; i++)
    {
        lcd_getstringsize((const unsigned char *)DOW_ES[i], &w, &h);
        lcd_putsxy(i * CAL_CELL_W + (CAL_CELL_W - w) / 2, CAL_TOP - 12,
                   (const unsigned char *)DOW_ES[i]);
    }

    /* Dias. */
    lcd_setfont(a26_font(A26_FONT_STYLE_DS_REG_12));
    for (i = 0; i < total; i++)
    {
        int cell = start + i;
        int col = cell % CAL_COLS;
        int row = cell / CAL_COLS;
        int x = col * CAL_CELL_W;
        int y = CAL_TOP + row * CAL_CELL_H;
        char num[4];
        bool sel = (i + 1 == s_day);

        if (row >= CAL_ROWS)
            break;

        if (sel)
            a26_shell_fill_rounded_rect(x + 3, y, CAL_CELL_W - 6, CAL_CELL_H - 4,
                                         A26_LAYOUT_CORNER_RADIUS_PILL,
                                         a26_color(A26_SELECTION_FILL),
                                         a26_color(A26_SHELL_BG));

        snprintf(num, sizeof(num), "%d", i + 1);
        lcd_set_foreground(sel ? aura_accent() : a26_color(A26_TEXT_PRIMARY));
        lcd_getstringsize((const unsigned char *)num, &w, &h);
        lcd_putsxy(x + (CAL_CELL_W - w) / 2, y + (CAL_CELL_H - 4 - h) / 2,
                   (const unsigned char *)num);
    }
}

static void shift_month(int delta)
{
    int total;

    s_month += delta;
    while (s_month < 0)  { s_month += 12; s_year--; }
    while (s_month > 11) { s_month -= 12; s_year++; }

    total = days_in_month(s_year, s_month);
    if (s_day > total)
        s_day = total;
}

void aura_calendar_handle_button(aura_nav_t *nav, long button)
{
    int total;

    ensure_today();
    total = days_in_month(s_year, s_month);

    switch (button)
    {
    /* La rueda recorre los DIAS; los botones cambian de MES (como el
     * original). */
    case BUTTON_SCROLL_FWD:
        s_day = aura_wheel_advance(s_day - 1, total, 1) + 1;
        break;
    case BUTTON_SCROLL_BACK:
        s_day = aura_wheel_advance(s_day - 1, total, -1) + 1;
        break;
    case BUTTON_RIGHT:
        shift_month(1);
        break;
    case BUTTON_LEFT:
        shift_month(-1);
        break;
    case BUTTON_SELECT:
        aura_nav_push(nav, AURA_SCREEN_EXTRAS_CALENDAR_DAY);
        break;
    case BUTTON_MENU:
        aura_nav_pop(nav);
        break;
    default:
        break;
    }
}

void aura_calendar_day_draw(void)
{
    char title[32];
    const char *msg = aura_str(AURA_STR_CAL_NO_EVENTS);
    int w, h;

    ensure_today();
    snprintf(title, sizeof(title), "%d %s", s_day, MONTHS_ES[s_month]);

    a26_shell_clear_screen();
    aura_widgets_draw_status_bar(title);

    lcd_setfont(a26_font(A26_FONT_STYLE_DS_REG_12));
    lcd_set_foreground(a26_color(A26_TEXT_SECONDARY));
    lcd_getstringsize((const unsigned char *)msg, &w, &h);
    lcd_putsxy((A26_SCREEN_WIDTH - w) / 2,
               (A26_SCREEN_HEIGHT + A26_LAYOUT_STATUSBAR_HEIGHT - h) / 2,
               (const unsigned char *)msg);
}

void aura_calendar_day_handle_button(aura_nav_t *nav, long button)
{
    /* Play/pausa no hace nada aqui (doc del original); MENU regresa. */
    if (button == BUTTON_MENU)
        aura_nav_pop(nav);
}
