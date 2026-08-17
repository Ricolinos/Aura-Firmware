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

#include "aura_screenlock.h"
#include "apple2026_shell.h"
#include "apple2026_tokens.h"
#include "aura_widgets.h"
#include "aura_lang.h"
#include "aura_screens.h"
#include "aura_settings.h"

#define SL_DIGITS   4
#define SL_BOX_W    34
#define SL_BOX_H    44
#define SL_GAP      A26_SPACING_MD

/* Dos pasadas: escribir la clave y confirmarla. Si la confirmacion no
 * coincide, se vuelve a la primera -- sin mensajes de error tecnicos,
 * el propio reinicio de los digitos es la senal. */
static int  s_digits[SL_DIGITS];
static int  s_first[SL_DIGITS];
static int  s_focus = 0;
static bool s_confirming = false;

/* "Desactivar bloqueo" (Task B, encargo del dueno): confirmacion antes
 * de borrar la clave guardada -- mismo patron que "Restablecer ajustes"
 * (s_reset_confirm_yes en aura_screens.c). Arranca en No (seguro por
 * defecto, nunca se desactiva sin querer). */
static bool s_disable_confirm_yes = false;

static void reset_all(void)
{
    memset(s_digits, 0, sizeof(s_digits));
    memset(s_first, 0, sizeof(s_first));
    s_focus = 0;
    s_confirming = false;
}

static void draw_pin_entry(void)
{
    int total_w = SL_DIGITS * SL_BOX_W + (SL_DIGITS - 1) * SL_GAP;
    int x0 = (A26_SCREEN_WIDTH - total_w) / 2;
    int y = A26_SCREEN_HEIGHT / 2 - SL_BOX_H / 2 + 10;
    /* D-197: candado EN VIVO (screen_lock_active) -- pantalla de
     * desbloqueo, un solo paso, texto distinto del flujo de
     * configuracion (que sigue usando LOCK_SET/LOCK_CONFIRM). */
    const char *hint = aura_settings.screen_lock_active
        ? aura_str(AURA_STR_LOCK_ENTER)
        : aura_str(s_confirming ? AURA_STR_LOCK_CONFIRM : AURA_STR_LOCK_SET);
    int i, w, h;

    a26_shell_clear_screen();
    aura_widgets_draw_status_bar(aura_str(AURA_STR_SETTINGS_SCREENLOCK));

    /* Candado grande arriba, el mismo simbolo del sistema. */
    aura_widgets_draw_icon("lock", A26_ICON_SIZE_PREVIEW,
                            (A26_SCREEN_WIDTH - A26_ICON_SIZE_PREVIEW) / 2,
                            A26_LAYOUT_STATUSBAR_HEIGHT + A26_SPACING_LG);

    lcd_setfont(a26_font(A26_FONT_STYLE_DS_REG_12));
    lcd_set_foreground(a26_color(A26_TEXT_SECONDARY));
    lcd_getstringsize((const unsigned char *)hint, &w, &h);
    lcd_putsxy((A26_SCREEN_WIDTH - w) / 2, y - A26_SPACING_XXL,
               (const unsigned char *)hint);

    for (i = 0; i < SL_DIGITS; i++)
    {
        int x = x0 + i * (SL_BOX_W + SL_GAP);
        char d[2] = { (char)('0' + s_digits[i]), '\0' };
        bool focus = (i == s_focus);

        a26_shell_fill_rounded_rect(x, y, SL_BOX_W, SL_BOX_H,
                                     A26_LAYOUT_CORNER_RADIUS_CARD,
                                     a26_color(A26_SELECTION_FILL),
                                     a26_color(A26_SHELL_BG));
        lcd_setfont(a26_font(A26_FONT_STYLE_TITLE));
        lcd_set_foreground(focus ? aura_accent() : a26_color(A26_TEXT_PRIMARY));
        lcd_getstringsize((const unsigned char *)d, &w, &h);
        lcd_putsxy(x + (SL_BOX_W - w) / 2, y + (SL_BOX_H - h) / 2,
                   (const unsigned char *)d);
    }
}

void aura_screenlock_draw(void)
{
    /* Tres estados posibles (Task B, encargo del dueno):
     *  1) screen_lock_active: candado EN VIVO -- pantalla de desbloqueo
     *     (unico paso alcanzable cuando aura_main.c intercepta el loop
     *     entero; nunca se llega aca navegando desde Ajustes, pero el
     *     mismo dibujo sirve por si alguna vez se redibuja en ese
     *     instante).
     *  2) screen_lock_enabled (sin estar activo ahora mismo): el
     *     usuario entro a "Bloqueo de pantalla" desde Ajustes con el
     *     bloqueo ya armado -- la unica accion posible aca es
     *     "Desactivar" (armar de nuevo exige clave nueva, no tiene
     *     sentido "reconfigurar" una que ya funciona).
     *  3) ninguno de los dos: flujo de "Activar" -- configurar una
     *     clave nueva (dos pasadas). */
    if (!aura_settings.screen_lock_active && aura_settings.screen_lock_enabled)
    {
        aura_widgets_draw_confirm(aura_str(AURA_STR_SCREENLOCK_DISABLE_TITLE),
                                   aura_str(AURA_STR_SCREENLOCK_DISABLE_BODY),
                                   s_disable_confirm_yes);
        return;
    }
    draw_pin_entry();
}

/* Empaqueta los 4 digitos en un numero 0-9999 para compararlos/guardarlos
 * como un solo entero (mismo formato que aura_settings.screen_lock_pin). */
static unsigned digits_to_pin(const int digits[SL_DIGITS])
{
    return (unsigned)(digits[0] * 1000 + digits[1] * 100 + digits[2] * 10 + digits[3]);
}

/* D-197: desbloqueo -- UN solo paso (sin confirmar dos veces, la clave
 * ya existe), compara contra aura_settings.screen_lock_pin. MENU no
 * hace nada: a diferencia de configurar una clave nueva, aca no hay
 * "cancelar" legitimo -- la unica salida es acertar la clave. */
static void handle_unlock_button(long button)
{
    switch (button)
    {
    case BUTTON_SCROLL_FWD:
        s_digits[s_focus] = (s_digits[s_focus] + 1) % 10;
        break;
    case BUTTON_SCROLL_BACK:
        s_digits[s_focus] = (s_digits[s_focus] + 9) % 10;
        break;

    case BUTTON_SELECT:
        if (s_focus < SL_DIGITS - 1)
        {
            s_focus++;
            break;
        }
        if (digits_to_pin(s_digits) == aura_settings.screen_lock_pin)
        {
            /* Clave correcta: se apaga el candado y se guarda de
             * inmediato -- si el usuario apaga el aparato en este
             * instante, debe volver a arrancar YA desbloqueado. */
            aura_settings.screen_lock_active = false;
            aura_settings_save();
            reset_all();
        }
        else
        {
            /* Incorrecta: mismo lenguaje que el flujo de configurar
             * (reiniciar los digitos es la senal, sin texto de error) --
             * se queda en esta pantalla, nunca se sale sola. */
            reset_all();
        }
        break;

    default:
        /* MENU y cualquier otro boton: sin efecto. Ningun otro camino
         * de esta pantalla llama a aura_nav_push/pop en modo
         * desbloqueo -- el aparato entero queda atrapado aca hasta
         * acertar (aura_main.c intercepta el loop principal). */
        break;
    }
}

/* Configurar una clave nueva ("Activar", Task B): dos pasadas, MENU
 * cancela sin guardar. D-197 la hacia guardar YA con el candado
 * encendido de inmediato -- el reporte original del dueno era que
 * configurar una clave "no servia de nada" (nunca se guardaba); ahora
 * el problema es el opuesto (bloqueaba el aparato al instante, sin que
 * el dueno pudiera elegir CUANDO se arma). Task B lo decouple: aca solo
 * se ARMA el bloqueo (`screen_lock_enabled`) -- `screen_lock_active` (el
 * candado EN VIVO) lo prende exclusivamente aura_main() en cada
 * arranque, nunca esta pantalla. */
static void handle_configure_button(aura_nav_t *nav, long button)
{
    switch (button)
    {
    case BUTTON_SCROLL_FWD:
        s_digits[s_focus] = (s_digits[s_focus] + 1) % 10;
        break;
    case BUTTON_SCROLL_BACK:
        s_digits[s_focus] = (s_digits[s_focus] + 9) % 10;
        break;

    case BUTTON_SELECT:
        if (s_focus < SL_DIGITS - 1)
        {
            s_focus++;
            break;
        }
        if (!s_confirming)
        {
            /* Primera pasada completa: pedir confirmacion. */
            memcpy(s_first, s_digits, sizeof(s_first));
            memset(s_digits, 0, sizeof(s_digits));
            s_focus = 0;
            s_confirming = true;
        }
        else if (memcmp(s_first, s_digits, sizeof(s_first)) == 0)
        {
            /* Coincide: se guarda la clave y el bloqueo queda ARMADO
             * (screen_lock_enabled) -- se activara de verdad (candado
             * EN VIVO) recien en el proximo arranque, no ahora mismo. */
            aura_settings.screen_lock_pin = (unsigned short)digits_to_pin(s_first);
            aura_settings.screen_lock_configured = true;
            aura_settings.screen_lock_enabled = true;
            aura_settings_save();
            reset_all();
            aura_nav_pop(nav);
        }
        else
        {
            /* No coincide: vuelve a la primera pasada. */
            reset_all();
        }
        break;

    case BUTTON_MENU:
        /* Restablece y NO configura nada (regla del original). */
        reset_all();
        aura_nav_pop(nav);
        break;

    default:
        break;
    }
}

/* "Desactivar bloqueo" (Task B): confirmar borra la clave guardada Y el
 * armado -- reactivar mas adelante exige volver a pasar por
 * handle_configure_button() desde cero, nunca reusa la clave vieja
 * (encargo explicito del dueno: "hay que volver a configurar el pin si
 * se quiere reactivar"). Mismo patron que handle_reset_confirm() en
 * aura_screens.c. */
static void handle_disable_confirm(aura_nav_t *nav, long button)
{
    switch (button)
    {
    case BUTTON_SCROLL_FWD:
    case BUTTON_SCROLL_BACK:
        s_disable_confirm_yes = !s_disable_confirm_yes;
        break;
    case BUTTON_SELECT:
        if (s_disable_confirm_yes)
        {
            aura_settings.screen_lock_enabled = false;
            aura_settings.screen_lock_configured = false;
            aura_settings.screen_lock_active = false;
            aura_settings.screen_lock_pin = 0;
            aura_settings_save();
        }
        s_disable_confirm_yes = false;
        aura_nav_pop(nav);
        break;
    case BUTTON_MENU:
        s_disable_confirm_yes = false;
        aura_nav_pop(nav);
        break;
    default:
        break;
    }
}

void aura_screenlock_handle_button(aura_nav_t *nav, long button)
{
    if (aura_settings.screen_lock_active)
        handle_unlock_button(button);
    else if (aura_settings.screen_lock_enabled)
        handle_disable_confirm(nav, button);
    else
        handle_configure_button(nav, button);
}
