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

#include "aura_alarms.h"
#include "apple2026_shell.h"
#include "apple2026_tokens.h"
#include "aura_widgets.h"
#include "aura_lang.h"
#include "aura_screens.h"
#include "aura_flow.h"

/* -- Modelo --------------------------------------------------------------- */

#define AL_MAX 4

typedef struct {
    bool  enabled;
    int   hour;       /* 0-23 */
    int   minute;
    int   repeat;     /* indice en AL_REPEAT */
    int   sound;      /* indice en AL_SOUND */
    int   label;      /* indice en AL_LABELS */
} alarm_t;

static const char *const AL_REPEAT[] = {
    "Una vez", "Cada día", "Entre semana", "Fin de semana",
    "Cada semana", "Cada mes", "Cada año",
};
#define AL_REPEAT_N ((int)(sizeof(AL_REPEAT) / sizeof(AL_REPEAT[0])))

static const char *const AL_SOUND[] = { "Ninguna", "Bip" };
#define AL_SOUND_N ((int)(sizeof(AL_SOUND) / sizeof(AL_SOUND[0])))

/* Las 23 etiquetas del original. */
static const char *const AL_LABELS[] = {
    "Despertador", "Trabajo", "Clase", "Alarma", "Cita",
    "Llamada telefónica", "Desayuno", "Comida", "Cena", "Importante",
    "Receta", "Medicamento", "Devolver llamada", "Entrega", "Recogida",
    "Reunión", "Recordatorio", "Compromiso", "Aniversario", "Cumpleaños",
    "Festividad", "Ir a casa", "Fiesta",
};
#define AL_LABELS_N ((int)(sizeof(AL_LABELS) / sizeof(AL_LABELS[0])))

static alarm_t s_alarms[AL_MAX];
static int s_count = 0;
static int s_sel = 0;        /* fila de la lista */
static int s_edit = -1;      /* alarma en edicion */
static alarm_t s_draft;      /* copia en edicion */
static int s_row = 0;        /* fila del editor */
static int s_choice_kind = 0; /* 0 repetir, 1 sonido, 2 etiqueta */
static int s_choice_sel = 0;
static int s_time_field = 0; /* 0 hora, 1 minuto */

/* Filas del editor (el orden del original). */
enum {
    AL_ROW_ENABLED = 0, AL_ROW_TIME, AL_ROW_REPEAT,
    AL_ROW_SOUND, AL_ROW_LABEL, AL_ROW_DELETE, AL_ROW_COUNT,
};

/* -- Lista ---------------------------------------------------------------- */

void aura_alarms_draw(void)
{
    static aura_list_item_t items[AL_MAX + 1];
    static char rows[AL_MAX][32];
    int i, n = 0;

    for (i = 0; i < s_count; i++)
    {
        int h12 = s_alarms[i].hour % 12;
        if (h12 == 0)
            h12 = 12;
        snprintf(rows[i], sizeof(rows[i]), "%d:%02d %s  %s",
                 h12, s_alarms[i].minute, s_alarms[i].hour < 12 ? "AM" : "PM",
                 AL_LABELS[s_alarms[i].label]);
        items[n].label = rows[i];
        items[n].icon_name = "alarm";
        items[n].checked = 0;
        items[n].toggle = s_alarms[i].enabled ? 1 : 0;
        n++;
    }
    /* Ultima fila: crear una alarma nueva. */
    items[n].label = aura_str(AURA_STR_ALARM_NEW);
    items[n].icon_name = "alarm";
    items[n].checked = 0;
    items[n].toggle = -1;
    items[n].dimmed = 0;
    n++;

    aura_widgets_draw_list(aura_str(AURA_STR_EXTRAS_ALARMS), items, n, s_sel);
}

void aura_alarms_handle_button(aura_nav_t *nav, long button)
{
    int total = s_count + 1;

    switch (button)
    {
    case BUTTON_SCROLL_FWD:  s_sel = aura_wheel_advance(s_sel, total, 1); break;
    case BUTTON_SCROLL_BACK: s_sel = aura_wheel_advance(s_sel, total, -1); break;
    case BUTTON_SELECT:
        s_row = 0;
        if (s_sel < s_count)
        {
            s_edit = s_sel;
            s_draft = s_alarms[s_sel];
        }
        else if (s_count < AL_MAX)
        {
            /* Alarma nueva con los valores por defecto del original. */
            s_edit = -1;
            s_draft.enabled = true;
            s_draft.hour = 7;
            s_draft.minute = 0;
            s_draft.repeat = 0;
            s_draft.sound = 1;
            s_draft.label = 0;
        }
        else
            break;
        aura_nav_push(nav, AURA_SCREEN_EXTRAS_ALARM_EDIT);
        break;
    case BUTTON_MENU: aura_nav_pop(nav); break;
    default: break;
    }
}

/* -- Editor --------------------------------------------------------------- */

void aura_alarm_edit_draw(void)
{
    static aura_list_item_t items[AL_ROW_COUNT];
    static char time_buf[16];
    int h12 = s_draft.hour % 12;

    if (h12 == 0)
        h12 = 12;
    snprintf(time_buf, sizeof(time_buf), "%d:%02d %s", h12, s_draft.minute,
             s_draft.hour < 12 ? "AM" : "PM");

    items[AL_ROW_ENABLED].label = aura_str(AURA_STR_EXTRAS_ALARMS);
    items[AL_ROW_ENABLED].toggle = s_draft.enabled ? 1 : 0;
    items[AL_ROW_TIME].label   = time_buf;
    items[AL_ROW_TIME].toggle  = -1;
    items[AL_ROW_REPEAT].label = AL_REPEAT[s_draft.repeat];
    items[AL_ROW_REPEAT].toggle = -1;
    items[AL_ROW_SOUND].label  = AL_SOUND[s_draft.sound];
    items[AL_ROW_SOUND].toggle = -1;
    items[AL_ROW_LABEL].label  = AL_LABELS[s_draft.label];
    items[AL_ROW_LABEL].toggle = -1;
    items[AL_ROW_DELETE].label = aura_str(AURA_STR_WC_DELETE);
    items[AL_ROW_DELETE].toggle = -1;

    for (int i = 0; i < AL_ROW_COUNT; i++)
    {
        items[i].icon_name = NULL;
        items[i].checked = 0;
    }

    aura_widgets_draw_list(aura_str(AURA_STR_EXTRAS_ALARMS), items,
                            AL_ROW_COUNT, s_row);
}

static void commit_draft(void)
{
    if (s_edit >= 0)
        s_alarms[s_edit] = s_draft;
    else if (s_count < AL_MAX)
        s_alarms[s_count++] = s_draft;
}

void aura_alarm_edit_handle_button(aura_nav_t *nav, long button)
{
    switch (button)
    {
    case BUTTON_SCROLL_FWD:  s_row = aura_wheel_advance(s_row, AL_ROW_COUNT, 1); break;
    case BUTTON_SCROLL_BACK: s_row = aura_wheel_advance(s_row, AL_ROW_COUNT, -1); break;
    case BUTTON_SELECT:
        switch (s_row)
        {
        case AL_ROW_ENABLED:
            s_draft.enabled = !s_draft.enabled;
            break;
        case AL_ROW_TIME:
            s_time_field = 0;
            aura_nav_push(nav, AURA_SCREEN_EXTRAS_ALARM_TIME);
            break;
        case AL_ROW_REPEAT:
            s_choice_kind = 0; s_choice_sel = s_draft.repeat;
            aura_nav_push(nav, AURA_SCREEN_EXTRAS_ALARM_CHOICE);
            break;
        case AL_ROW_SOUND:
            s_choice_kind = 1; s_choice_sel = s_draft.sound;
            aura_nav_push(nav, AURA_SCREEN_EXTRAS_ALARM_CHOICE);
            break;
        case AL_ROW_LABEL:
            s_choice_kind = 2; s_choice_sel = s_draft.label;
            aura_nav_push(nav, AURA_SCREEN_EXTRAS_ALARM_CHOICE);
            break;
        case AL_ROW_DELETE:
            if (s_edit >= 0)
            {
                int k;
                for (k = s_edit; k < s_count - 1; k++)
                    s_alarms[k] = s_alarms[k + 1];
                s_count--;
                if (s_sel > s_count)
                    s_sel = s_count;
            }
            aura_nav_pop(nav);
            break;
        default: break;
        }
        break;
    case BUTTON_MENU:
        /* Salir del editor GUARDA: la alarma es el borrador que se
         * estuvo viendo, no hay un "aceptar" aparte en el original. */
        commit_draft();
        aura_nav_pop(nav);
        break;
    default: break;
    }
}

/* -- Editor de hora, con reloj analogico en vivo -------------------------- */

#define AL_DIAL_R 42

void aura_alarm_time_draw(void)
{
    int cx = A26_SCREEN_WIDTH / 2;
    int cy = A26_LAYOUT_STATUSBAR_HEIGHT + 14 + AL_DIAL_R;
    char buf[16];
    int h12 = s_draft.hour % 12;
    int i, w, h, dx, dy;

    if (h12 == 0)
        h12 = 12;

    a26_shell_clear_screen();
    aura_widgets_draw_status_bar(aura_str(AURA_STR_EXTRAS_ALARMS));

    /* Esfera: la manecilla se mueve EN VIVO con la configuracion, que
     * es lo que hace legible el ajuste (encargo 2026-08-13). */
    for (dy = -AL_DIAL_R; dy <= AL_DIAL_R; dy++)
    {
        int half = (int)a26_shell_isqrt256(
            (unsigned)(AL_DIAL_R * AL_DIAL_R - dy * dy)) / 256;
        lcd_set_foreground(a26_color(A26_SELECTION_FILL));
        lcd_hline(cx - half, cx + half, cy + dy);
    }
    lcd_set_foreground(a26_color(A26_TEXT_TERTIARY));
    for (i = 0; i < AURA_FLOW_IANGLE_MAX; i += 8)
    {
        dx = aura_flow_fsin(i) * AL_DIAL_R / AURA_FLOW_ONE;
        dy = -aura_flow_fcos(i) * AL_DIAL_R / AURA_FLOW_ONE;
        lcd_drawpixel(cx + dx, cy + dy);
    }
    /* Horaria y minutera. */
    lcd_set_foreground(a26_color(A26_TEXT_PRIMARY));
    dx = aura_flow_fsin(((s_draft.hour % 12) * 60 + s_draft.minute)
                        * AURA_FLOW_IANGLE_MAX / 720) * (AL_DIAL_R - 18) / AURA_FLOW_ONE;
    dy = -aura_flow_fcos(((s_draft.hour % 12) * 60 + s_draft.minute)
                         * AURA_FLOW_IANGLE_MAX / 720) * (AL_DIAL_R - 18) / AURA_FLOW_ONE;
    lcd_drawline(cx, cy, cx + dx, cy + dy);
    lcd_set_foreground(aura_accent());
    dx = aura_flow_fsin(s_draft.minute * AURA_FLOW_IANGLE_MAX / 60)
         * (AL_DIAL_R - 9) / AURA_FLOW_ONE;
    dy = -aura_flow_fcos(s_draft.minute * AURA_FLOW_IANGLE_MAX / 60)
         * (AL_DIAL_R - 9) / AURA_FLOW_ONE;
    lcd_drawline(cx, cy, cx + dx, cy + dy);

    /* Hora numerica: el campo en edicion, en acento. */
    snprintf(buf, sizeof(buf), "%d:%02d %s", h12, s_draft.minute,
             s_draft.hour < 12 ? "AM" : "PM");
    lcd_setfont(a26_font(A26_FONT_STYLE_TITLE));
    lcd_set_foreground(aura_accent());
    lcd_getstringsize((const unsigned char *)buf, &w, &h);
    lcd_putsxy((A26_SCREEN_WIDTH - w) / 2, cy + AL_DIAL_R + A26_SPACING_LG,
               (const unsigned char *)buf);

    lcd_setfont(a26_font(A26_FONT_STYLE_DS_REG_10));
    lcd_set_foreground(a26_color(A26_TEXT_SECONDARY));
    {
        const char *hint = aura_str(s_time_field == 0 ? AURA_STR_ALARM_HOUR
                                                       : AURA_STR_ALARM_MINUTE);
        lcd_getstringsize((const unsigned char *)hint, &w, &h);
        lcd_putsxy((A26_SCREEN_WIDTH - w) / 2,
                   cy + AL_DIAL_R + A26_SPACING_LG + 24,
                   (const unsigned char *)hint);
    }
}

void aura_alarm_time_handle_button(aura_nav_t *nav, long button)
{
    switch (button)
    {
    case BUTTON_SCROLL_FWD:
        if (s_time_field == 0) s_draft.hour = (s_draft.hour + 1) % 24;
        else                   s_draft.minute = (s_draft.minute + 1) % 60;
        break;
    case BUTTON_SCROLL_BACK:
        if (s_time_field == 0) s_draft.hour = (s_draft.hour + 23) % 24;
        else                   s_draft.minute = (s_draft.minute + 59) % 60;
        break;
    case BUTTON_SELECT:
        if (s_time_field == 0)
            s_time_field = 1;
        else
            aura_nav_pop(nav);
        break;
    case BUTTON_MENU: aura_nav_pop(nav); break;
    default: break;
    }
}

/* -- Listas de eleccion del editor ---------------------------------------- */

static int choice_count(void)
{
    if (s_choice_kind == 0) return AL_REPEAT_N;
    if (s_choice_kind == 1) return AL_SOUND_N;
    return AL_LABELS_N;
}

static const char *choice_label(int i)
{
    if (s_choice_kind == 0) return AL_REPEAT[i];
    if (s_choice_kind == 1) return AL_SOUND[i];
    return AL_LABELS[i];
}

void aura_alarm_choice_draw(void)
{
    static aura_list_item_t items[AL_LABELS_N];
    int n = choice_count();
    int i;

    for (i = 0; i < n; i++)
    {
        items[i].label = choice_label(i);
        items[i].icon_name = NULL;
        items[i].toggle = -1;
        items[i].dimmed = 0;
        items[i].checked = (i == s_choice_sel);
    }
    aura_widgets_draw_list(aura_str(AURA_STR_EXTRAS_ALARMS), items, n,
                            s_choice_sel);
}

void aura_alarm_choice_handle_button(aura_nav_t *nav, long button)
{
    int n = choice_count();

    switch (button)
    {
    case BUTTON_SCROLL_FWD:  s_choice_sel = aura_wheel_advance(s_choice_sel, n, 1); break;
    case BUTTON_SCROLL_BACK: s_choice_sel = aura_wheel_advance(s_choice_sel, n, -1); break;
    case BUTTON_SELECT:
        if (s_choice_kind == 0)      s_draft.repeat = s_choice_sel;
        else if (s_choice_kind == 1) s_draft.sound = s_choice_sel;
        else                         s_draft.label = s_choice_sel;
        aura_nav_pop(nav);
        break;
    case BUTTON_MENU: aura_nav_pop(nav); break;
    default: break;
    }
}
