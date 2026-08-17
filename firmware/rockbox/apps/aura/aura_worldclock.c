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
#include "string-extra.h"

#include "aura_worldclock.h"
#include "apple2026_shell.h"
#include "apple2026_tokens.h"
#include "aura_widgets.h"
#include "aura_lang.h"
#include "aura_settings.h"
#include "aura_screens.h"
#include "aura_flow.h"

/* -- Catalogo de husos ----------------------------------------------------
 *
 * Ciudad + desplazamiento respecto de UTC en CUARTOS de hora (asi
 * caben los husos a media hora y a 45 minutos sin coma flotante). No
 * hay horario de verano: el original tampoco lo resolvia por ciudad, y
 * fingir que si seria peor que no tenerlo. */
typedef struct {
    const char *city;
    int8_t region;      /* indice en WC_REGIONS */
    int16_t utc_q;      /* cuartos de hora respecto de UTC */
    int16_t lat10;       /* latitud en DECIMAS de grado, +N/-S (Zona horaria) */
    int16_t lon10;       /* longitud en DECIMAS de grado, +E/-W */
} wc_city_t;

static const char *const WC_REGIONS[] = {
    "África", "América del Norte", "América del Sur", "Asia",
    "Atlántico", "Europa", "Oceanía", "Pacífico",
};
#define WC_REGION_COUNT ((int)(sizeof(WC_REGIONS) / sizeof(WC_REGIONS[0])))

static const wc_city_t WC_CITIES[] = {
    /* África -- { ciudad, region, UTC en cuartos de hora, lat*10, lon*10 } */
    { "El Cairo",        0,  8,  300,  312 }, { "Johannesburgo", 0,  8, -262,  280 },
    { "Lagos",           0,  4,   65,   34 }, { "Nairobi",       0, 12,  -13,  368 },
    /* América del Norte */
    { "Ciudad de México", 1, -24,  194, -991 }, { "Nueva York",   1, -20,  407, -740 },
    { "Chicago",         1, -24,  419, -876 }, { "Denver",       1, -28,  397, -1050 },
    { "Los Ángeles",     1, -32,  341, -1182 }, { "Toronto",      1, -20,  437, -794 },
    { "Vancouver",       1, -32,  493, -1231 }, { "La Habana",    1, -20,  231, -824 },
    /* América del Sur */
    { "Bogotá",          2, -20,   47, -741 }, { "Lima",         2, -20, -120, -770 },
    { "Santiago",        2, -16, -334, -707 }, { "Buenos Aires", 2, -12, -346, -584 },
    { "São Paulo",       2, -12, -236, -466 }, { "Caracas",      2, -16,  105, -669 },
    /* Asia */
    { "Tokio",           3,  36,  357, 1397 }, { "Pekín",        3,  32,  399, 1164 },
    { "Hong Kong",       3,  32,  223, 1142 }, { "Singapur",     3,  32,   13, 1038 },
    { "Bangkok",         3,  28,  138, 1005 }, { "Nueva Delhi",  3,  22,  286,  772 },
    { "Dubái",           3,  16,  252,  553 }, { "Seúl",         3,  36,  376, 1270 },
    /* Atlántico */
    { "Azores",          4,  -4,  377, -257 }, { "Reikiavik",    4,   0,  641, -219 },
    /* Europa */
    { "Londres",         5,   0,  515,  -1 }, { "Madrid",       5,   4,  404,  -37 },
    { "París",           5,   4,  489,   23 }, { "Berlín",       5,   4,  525,  134 },
    { "Roma",            5,   4,  419,  125 }, { "Lisboa",       5,   0,  387,  -91 },
    { "Atenas",          5,   8,  380,  237 }, { "Moscú",        5,  12,  558,  376 },
    /* Oceanía */
    { "Sídney",          6,  40, -339, 1512 }, { "Melbourne",    6,  40, -378, 1449 },
    { "Auckland",        6,  48, -368, 1748 },
    /* Pacífico */
    { "Honolulu",        7, -40,  213, -1578 }, { "Fiyi",         7,  48, -181, 1784 },
};
#define WC_CITY_COUNT ((int)(sizeof(WC_CITIES) / sizeof(WC_CITIES[0])))

int aura_worldclock_city_count(void) { return WC_CITY_COUNT; }
const char *aura_worldclock_city_name(int i)
{
    return (i >= 0 && i < WC_CITY_COUNT) ? WC_CITIES[i].city : "";
}
int aura_worldclock_city_utc_quarters(int i)
{
    return (i >= 0 && i < WC_CITY_COUNT) ? WC_CITIES[i].utc_q : 0;
}
int aura_worldclock_city_lat10(int i)
{
    return (i >= 0 && i < WC_CITY_COUNT) ? WC_CITIES[i].lat10 : 0;
}
int aura_worldclock_city_lon10(int i)
{
    return (i >= 0 && i < WC_CITY_COUNT) ? WC_CITIES[i].lon10 : 0;
}

/* Orden por LATITUD descendente (encargo del original: "al scrollear
 * con el click wheel, se va moviendo en orden de latitud, para elegir
 * exactamente el pais"), calculado una sola vez. Devuelve el indice
 * REAL a WC_CITIES que corresponde a la posicion `order_pos` en ese
 * orden. */
int aura_worldclock_city_by_lat_order(int order_pos)
{
    static int8_t order[WC_CITY_COUNT];
    static bool ready = false;

    if (!ready)
    {
        int i, a, b;
        for (i = 0; i < WC_CITY_COUNT; i++)
            order[i] = (int8_t)i;
        for (a = 1; a < WC_CITY_COUNT; a++)
        {
            int8_t key = order[a];
            int16_t key_lat = WC_CITIES[key].lat10;
            b = a - 1;
            while (b >= 0 && WC_CITIES[order[b]].lat10 < key_lat)
            {
                order[b + 1] = order[b];
                b--;
            }
            order[b + 1] = key;
        }
        ready = true;
    }
    if (order_pos < 0 || order_pos >= WC_CITY_COUNT)
        return 0;
    return order[order_pos];
}

/* -- Estado --------------------------------------------------------------- */

#define WC_MAX_CLOCKS 4
static int8_t s_clocks[WC_MAX_CLOCKS] = { 0 }; /* indices a WC_CITIES */
static int s_clock_count = 0;
static int s_sel = 0;              /* fila seleccionada (0 = local) */
static int s_region_sel = 0;
static int s_city_sel = 0;
static int s_edit_slot = -1;       /* >=0 = se esta editando ese reloj */

/* Menu flotante de SELECT. */
static bool s_menu_open = false;
static int  s_menu_sel = 0;
#define WC_MENU_ROWS 3

bool aura_worldclock_needs_tick(void)
{
    return true; /* el minutero avanza solo */
}

/* -- Geometria ------------------------------------------------------------ */

#define WC_ROW_H     52
#define WC_TOP       (A26_LAYOUT_STATUSBAR_HEIGHT + A26_SPACING_SM)
#define WC_DIAL_R    20
#define WC_DIAL_X    (A26_SPACING_XXL + WC_DIAL_R)

/* -- Reloj analogico -------------------------------------------------------
 *
 * Manecillas con la LUT de senos de punto fijo de aura_flow (IANGLE
 * 1024 = vuelta completa), sin coma flotante ni tabla nueva. El angulo
 * arranca en las 12 y avanza en sentido horario: iangle = 256 + giro
 * medido hacia la derecha, que en coordenadas de pantalla (y hacia
 * abajo) equivale a x = cos, y = -sin del angulo desde las 12. */
static void draw_hand(int cx, int cy, int iangle, int len, unsigned color)
{
    int dx = aura_flow_fsin(iangle) * len / AURA_FLOW_ONE;
    int dy = -aura_flow_fcos(iangle) * len / AURA_FLOW_ONE;

    lcd_set_foreground(color);
    lcd_drawline(cx, cy, cx + dx, cy + dy);
}

static void draw_dial(int cx, int cy, int hour24, int minute, bool local)
{
    /* Esfera clara = hora LOCAL; oscura = los demas husos (encargo
     * 2026-08-13, replicando el original). */
    unsigned face = local ? a26_color(A26_SHELL_BG) : a26_color(A26_TEXT_PRIMARY);
    unsigned ink  = local ? a26_color(A26_TEXT_PRIMARY) : a26_color(A26_SHELL_BG);
    unsigned rim  = local ? a26_color(A26_TEXT_PRIMARY) : a26_color(A26_TEXT_PRIMARY);
    int r, dx, dy;

    /* Esfera: disco relleno por filas, con el borde antialiasado del
     * mismo mecanismo de cobertura que el resto del sistema. */
    for (dy = -WC_DIAL_R; dy <= WC_DIAL_R; dy++)
    {
        int half = (int)a26_shell_isqrt256(
            (unsigned)(WC_DIAL_R * WC_DIAL_R - dy * dy)) / 256;
        lcd_set_foreground(face);
        lcd_hline(cx - half, cx + half, cy + dy);
    }
    /* Aro */
    lcd_set_foreground(rim);
    for (r = 0; r < AURA_FLOW_IANGLE_MAX; r += 8)
    {
        dx = aura_flow_fsin(r) * WC_DIAL_R / AURA_FLOW_ONE;
        dy = -aura_flow_fcos(r) * WC_DIAL_R / AURA_FLOW_ONE;
        lcd_drawpixel(cx + dx, cy + dy);
    }

    /* Marcas de las 12, 3, 6 y 9 */
    for (r = 0; r < 4; r++)
    {
        int a = r * (AURA_FLOW_IANGLE_MAX / 4);
        int x0 = cx + aura_flow_fsin(a) * (WC_DIAL_R - 4) / AURA_FLOW_ONE;
        int y0 = cy - aura_flow_fcos(a) * (WC_DIAL_R - 4) / AURA_FLOW_ONE;
        lcd_set_foreground(ink);
        lcd_drawpixel(x0, y0);
    }

    /* Horaria y minutera: 1024 unidades = 12 h para la horaria (la
     * hora avanza con los minutos, no a saltos), 1024 = 60 min para la
     * minutera. */
    draw_hand(cx, cy, ((hour24 % 12) * 60 + minute) * AURA_FLOW_IANGLE_MAX / 720,
              WC_DIAL_R - 9, ink);
    draw_hand(cx, cy, minute * AURA_FLOW_IANGLE_MAX / 60, WC_DIAL_R - 5, ink);
}

/* -- Hora por huso -------------------------------------------------------- */

static void city_time(int city_idx, int *hour24, int *minute)
{
    struct tm *now = get_time();
    int local_q = aura_settings.tz_local_quarters;
    long mins;

    if (!now)
    {
        *hour24 = 0;
        *minute = 0;
        return;
    }
    mins = now->tm_hour * 60 + now->tm_min;
    if (city_idx >= 0)
        mins += (WC_CITIES[city_idx].utc_q - local_q) * 15;

    mins %= 24 * 60;
    if (mins < 0)
        mins += 24 * 60;
    *hour24 = (int)(mins / 60);
    *minute = (int)(mins % 60);
}

static void format_12h(int hour24, int minute, char *out, size_t outsz)
{
    int h12 = hour24 % 12;

    if (h12 == 0)
        h12 = 12;
    snprintf(out, outsz, "%d:%02d %s", h12, minute, hour24 < 12 ? "AM" : "PM");
}

/* -- Dibujo --------------------------------------------------------------- */

static void draw_menu_overlay(void)
{
    static const aura_str_id_t rows[WC_MENU_ROWS] = {
        AURA_STR_WC_ADD, AURA_STR_WC_EDIT, AURA_STR_WC_DELETE,
    };
    int box_w = 150;
    int box_h = WC_MENU_ROWS * 24 + 2 * A26_SPACING_SM;
    int box_x = (A26_SCREEN_WIDTH - box_w) / 2;
    int box_y = (A26_SCREEN_HEIGHT - box_h) / 2;
    int i;

    /* Panel flotante, nunca pantalla completa (Principio 3). */
    a26_shell_fill_rounded_rect(box_x, box_y, box_w, box_h,
                                 A26_LAYOUT_CORNER_RADIUS_CARD,
                                 a26_color(A26_SELECTION_FILL),
                                 a26_color(A26_SHELL_BG));

    lcd_setfont(a26_font(A26_FONT_STYLE_DS_REG_12));
    for (i = 0; i < WC_MENU_ROWS; i++)
    {
        int y = box_y + A26_SPACING_SM + i * 24;

        if (i == s_menu_sel)
            a26_shell_fill_rounded_rect(box_x + A26_SPACING_SM, y,
                                         box_w - 2 * A26_SPACING_SM, 22,
                                         A26_LAYOUT_CORNER_RADIUS_PILL,
                                         a26_color(A26_SHELL_BG),
                                         a26_color(A26_SELECTION_FILL));
        lcd_set_foreground(i == s_menu_sel ? aura_accent()
                                            : a26_color(A26_TEXT_PRIMARY));
        lcd_putsxy(box_x + A26_SPACING_LG, y + 5,
                   (const unsigned char *)aura_str(rows[i]));
    }
}

void aura_worldclock_draw(void)
{
    int i, total = s_clock_count + 1; /* +1 = la hora local */

    a26_shell_clear_screen();
    aura_widgets_draw_status_bar(aura_str(AURA_STR_EXTRAS_CLOCKS));

    for (i = 0; i < total; i++)
    {
        int y = WC_TOP + i * WC_ROW_H;
        int cy = y + WC_ROW_H / 2 - 2;
        bool local = (i == 0);
        int city = local ? -1 : s_clocks[i - 1];
        const char *name = local ? aura_str(AURA_STR_WC_LOCAL)
                                  : WC_CITIES[city].city;
        char clock_txt[16];
        int hour24, minute, w, h;

        if (y + WC_ROW_H > A26_SCREEN_HEIGHT)
            break;

        if (i == s_sel && !s_menu_open)
            a26_shell_fill_rounded_rect(A26_SPACING_SM, y,
                                         A26_SCREEN_WIDTH - 2 * A26_SPACING_SM,
                                         WC_ROW_H - 4,
                                         A26_LAYOUT_CORNER_RADIUS_CARD,
                                         a26_color(A26_SELECTION_FILL),
                                         a26_color(A26_SHELL_BG));

        city_time(city, &hour24, &minute);
        draw_dial(WC_DIAL_X, cy, hour24, minute, local);

        lcd_setfont(a26_font(A26_FONT_STYLE_DS_BOLD_12));
        lcd_set_foreground(i == s_sel ? aura_accent() : a26_color(A26_TEXT_PRIMARY));
        lcd_putsxy(WC_DIAL_X + WC_DIAL_R + A26_SPACING_LG, cy - 12,
                   (const unsigned char *)name);

        format_12h(hour24, minute, clock_txt, sizeof(clock_txt));
        lcd_setfont(a26_font(A26_FONT_STYLE_DS_REG_12));
        lcd_set_foreground(a26_color(A26_TEXT_SECONDARY));
        lcd_getstringsize((const unsigned char *)clock_txt, &w, &h);
        lcd_putsxy(WC_DIAL_X + WC_DIAL_R + A26_SPACING_LG, cy + 2,
                   (const unsigned char *)clock_txt);
    }

    if (s_menu_open)
        draw_menu_overlay();
}

/* -- Interaccion ---------------------------------------------------------- */

static void add_or_replace_city(int city_idx)
{
    if (s_edit_slot >= 0 && s_edit_slot < s_clock_count)
        s_clocks[s_edit_slot] = (int8_t)city_idx;
    else if (s_clock_count < WC_MAX_CLOCKS)
        s_clocks[s_clock_count++] = (int8_t)city_idx;
    s_edit_slot = -1;
}

void aura_worldclock_handle_button(aura_nav_t *nav, long button)
{
    int total = s_clock_count + 1;

    if (s_menu_open)
    {
        switch (button)
        {
        case BUTTON_SCROLL_FWD:
            s_menu_sel = aura_wheel_advance(s_menu_sel, WC_MENU_ROWS, 1);
            break;
        case BUTTON_SCROLL_BACK:
            s_menu_sel = aura_wheel_advance(s_menu_sel, WC_MENU_ROWS, -1);
            break;
        case BUTTON_SELECT:
            s_menu_open = false;
            if (s_menu_sel == 0)             /* Anadir */
            {
                s_edit_slot = -1;
                aura_nav_push(nav, AURA_SCREEN_EXTRAS_CLOCK_REGIONS);
            }
            else if (s_menu_sel == 1)        /* Editar */
            {
                if (s_sel > 0)
                {
                    s_edit_slot = s_sel - 1;
                    aura_nav_push(nav, AURA_SCREEN_EXTRAS_CLOCK_REGIONS);
                }
            }
            else if (s_sel > 0)              /* Eliminar */
            {
                int k = s_sel - 1;
                for (; k < s_clock_count - 1; k++)
                    s_clocks[k] = s_clocks[k + 1];
                s_clock_count--;
                if (s_sel > s_clock_count)
                    s_sel = s_clock_count;
            }
            break;
        case BUTTON_MENU:
            s_menu_open = false;
            break;
        default:
            break;
        }
        return;
    }

    switch (button)
    {
    case BUTTON_SCROLL_FWD:
        s_sel = aura_wheel_advance(s_sel, total, 1);
        break;
    case BUTTON_SCROLL_BACK:
        s_sel = aura_wheel_advance(s_sel, total, -1);
        break;
    case BUTTON_SELECT:
        s_menu_open = true;
        s_menu_sel = 0;
        break;
    case BUTTON_MENU:
        aura_nav_pop(nav);
        break;
    default:
        break;
    }
}

/* -- Selector de continente ----------------------------------------------- */

void aura_worldclock_regions_draw(void)
{
    static aura_list_item_t items[WC_REGION_COUNT];
    int i;

    for (i = 0; i < WC_REGION_COUNT; i++)
    {
        items[i].label = WC_REGIONS[i];
        items[i].icon_name = NULL;
        items[i].checked = 0;
        items[i].toggle = -1;
        items[i].dimmed = 0;
    }
    aura_widgets_draw_list(aura_str(AURA_STR_WC_ADD), items,
                            WC_REGION_COUNT, s_region_sel);
}

void aura_worldclock_regions_handle_button(aura_nav_t *nav, long button)
{
    switch (button)
    {
    case BUTTON_SCROLL_FWD:
        s_region_sel = aura_wheel_advance(s_region_sel, WC_REGION_COUNT, 1);
        break;
    case BUTTON_SCROLL_BACK:
        s_region_sel = aura_wheel_advance(s_region_sel, WC_REGION_COUNT, -1);
        break;
    case BUTTON_SELECT:
        s_city_sel = 0;
        aura_nav_push(nav, AURA_SCREEN_EXTRAS_CLOCK_CITIES);
        break;
    case BUTTON_MENU:
        aura_nav_pop(nav);
        break;
    default:
        break;
    }
}

/* -- Selector de ciudad --------------------------------------------------- */

static int cities_of_region(int region, int *out_idx, int max)
{
    int i, n = 0;

    for (i = 0; i < WC_CITY_COUNT && n < max; i++)
        if (WC_CITIES[i].region == region)
            out_idx[n++] = i;
    return n;
}

void aura_worldclock_cities_draw(void)
{
    static aura_list_item_t items[WC_CITY_COUNT];
    int idx[WC_CITY_COUNT];
    int n = cities_of_region(s_region_sel, idx, WC_CITY_COUNT);
    int i;

    for (i = 0; i < n; i++)
    {
        items[i].label = WC_CITIES[idx[i]].city;
        items[i].icon_name = NULL;
        items[i].checked = 0;
        items[i].toggle = -1;
        items[i].dimmed = 0;
    }
    aura_widgets_draw_list(WC_REGIONS[s_region_sel], items, n, s_city_sel);
}

void aura_worldclock_cities_handle_button(aura_nav_t *nav, long button)
{
    int idx[WC_CITY_COUNT];
    int n = cities_of_region(s_region_sel, idx, WC_CITY_COUNT);

    switch (button)
    {
    case BUTTON_SCROLL_FWD:
        s_city_sel = aura_wheel_advance(s_city_sel, n, 1);
        break;
    case BUTTON_SCROLL_BACK:
        s_city_sel = aura_wheel_advance(s_city_sel, n, -1);
        break;
    case BUTTON_SELECT:
        if (n > 0)
        {
            add_or_replace_city(idx[s_city_sel]);
            /* Vuelve a la lista de relojes: dos niveles de un golpe,
             * el gesto termino. */
            aura_nav_pop(nav);
            aura_nav_pop(nav);
        }
        break;
    case BUTTON_MENU:
        aura_nav_pop(nav);
        break;
    default:
        break;
    }
}
