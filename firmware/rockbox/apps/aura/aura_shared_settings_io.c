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
/* D-355/D-356: lado IMPURO de /.aura/settings.cfg -- lee/escribe disco,
 * traduce entre el vocabulario del contrato (aura_shared_settings.h,
 * enums propios, estables entre familias) y la representacion interna
 * de Aura (global_settings de Rockbox, aura_settings.h). El parseo y
 * formato de texto en si viven en aura_shared_settings.c (modulo puro,
 * host-testable); este archivo no reimplementa nada de eso.
 *
 * Declarado en aura_sync.h -- mismo dueno de rutas /.aura que el resto
 * (AURA_SHARED_SETTINGS_PATH). Solo aura_sync.c llama a estas dos
 * funciones. */
#include "aura_sync.h"
#include "aura_shared_settings.h"
#include "aura_settings.h"
#include "aura_fsutil.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "settings.h"
#include "backlight.h"
#include "powermgmt.h"

#define SETTINGS_BUF_SIZE 1024

/* -- Contrato -> Aura (aplicar) ------------------------------------------ */

static void apply_screen_lock_require(aura_shared_lock_require_t v)
{
    /* Mismo orden en los dos enums (hold/1min/5min/boot) -- se
     * comprueba en vez de confiar en la coincidencia numerica, para
     * que un reordenamiento futuro de cualquiera de los dos falle en
     * compilacion o en este switch, no en silencio. */
    switch (v)
    {
    case AURA_SHARED_LOCK_REQUIRE_HOLD:
        aura_settings.screen_lock_require = AURA_LOCK_REQUIRE_HOLD; break;
    case AURA_SHARED_LOCK_REQUIRE_1MIN:
        aura_settings.screen_lock_require = AURA_LOCK_REQUIRE_1MIN; break;
    case AURA_SHARED_LOCK_REQUIRE_5MIN:
        aura_settings.screen_lock_require = AURA_LOCK_REQUIRE_5MIN; break;
    case AURA_SHARED_LOCK_REQUIRE_BOOT:
        aura_settings.screen_lock_require = AURA_LOCK_REQUIRE_BOOT; break;
    case AURA_SHARED_LOCK_REQUIRE_COUNT:
    default:
        break; /* ausente/invalido: no tocar */
    }
}

static void apply_replaygain(aura_shared_replaygain_t v)
{
    switch (v)
    {
    case AURA_SHARED_REPLAYGAIN_OFF:
        global_settings.replaygain_settings.type = REPLAYGAIN_OFF; break;
    case AURA_SHARED_REPLAYGAIN_TRACK:
        global_settings.replaygain_settings.type = REPLAYGAIN_TRACK; break;
    case AURA_SHARED_REPLAYGAIN_ALBUM:
        global_settings.replaygain_settings.type = REPLAYGAIN_ALBUM; break;
    case AURA_SHARED_REPLAYGAIN_COUNT:
    default:
        break; /* ausente/invalido: no tocar */
    }
}

static void apply_appearance(aura_shared_appearance_t v)
{
    /* Orden INVERTIDO respecto a aura_theme_id_t (LIGHT=0/DARK=1) --
     * nunca un cast directo. */
    switch (v)
    {
    case AURA_SHARED_APPEARANCE_DARK:
        aura_settings.theme = AURA_THEME_DARK; break;
    case AURA_SHARED_APPEARANCE_LIGHT:
        aura_settings.theme = AURA_THEME_LIGHT; break;
    case AURA_SHARED_APPEARANCE_COUNT:
    default:
        return; /* ausente/invalido: no tocar, y no hay tema que sincronizar */
    }
    aura_settings_sync_rockbox_theme_colors();
}

/* D-357 (Fase 3 de esta misma ronda) amplia aura_lang_t a los seis
 * idiomas del contrato -- hasta entonces, fr/de/ru/it son claves
 * VALIDAS y CONOCIDAS (aura_shared_settings_parse() las reconoce, no
 * las trata como clave desconocida) pero esta funcion todavia no sabe
 * mostrar esos cuatro, asi que las deja sin aplicar -- exactamente el
 * mismo criterio que "valor fuera de rango se ignora clave por clave"
 * (regla 2 del contrato), aunque aqui el valor SI es valido, solo que
 * este firmware en concreto (todavia) no lo soporta. */
static void apply_language(aura_shared_lang_t v)
{
    switch (v)
    {
    case AURA_SHARED_LANG_ES:
        aura_settings.language = AURA_LANG_ES; break;
    case AURA_SHARED_LANG_EN:
        aura_settings.language = AURA_LANG_EN; break;
    case AURA_SHARED_LANG_FR:
    case AURA_SHARED_LANG_DE:
    case AURA_SHARED_LANG_RU:
    case AURA_SHARED_LANG_IT:
    case AURA_SHARED_LANG_COUNT:
    default:
        break;
    }
}

void aura_shared_settings_apply_if_newer(void)
{
    /* D-356: buf+m suman ~1.9 KB, por encima del limite de 1 KB de
     * D-226/D-345 para marcos del hilo de UI -- static en vez de pila.
     * Seguro: esta funcion solo corre en el hilo de UI (llamada desde
     * aura_main_sync_after_disk_handoff()), nunca se reentra ni se
     * anida con aura_shared_settings_write_current() (misma regla). */
    static char buf[SETTINGS_BUF_SIZE];
    static aura_shared_settings_t m;
    int n;

    n = aura_fsutil_read_text(AURA_SHARED_SETTINGS_PATH, buf, sizeof(buf));
    if (n <= 0)
        return; /* no existe, no se pudo leer, o no cabe -- nada que aplicar */

    if (aura_shared_settings_parse(buf, &m) != AURA_SHARED_SETTINGS_OK)
        return; /* regla 5: sin cabecera valida, todo queda local */

    if (m.rev <= aura_settings.shared_rev_applied)
        return; /* ya lo tenemos -- lo mas comun, en cada arranque/USB */

    if (m.screen_lock_enabled >= 0)
        aura_settings.screen_lock_enabled = (m.screen_lock_enabled != 0);
    if (m.screen_lock_pin[0])
        aura_settings.screen_lock_pin = (unsigned short)atoi(m.screen_lock_pin);
    apply_screen_lock_require(m.screen_lock_require);

    if (m.brightness >= MIN_BRIGHTNESS_SETTING && m.brightness <= MAX_BRIGHTNESS_SETTING)
    {
        global_settings.brightness = m.brightness;
        backlight_set_brightness(global_settings.brightness);
    }
    if (m.backlight_timeout != -2)
        global_settings.backlight_timeout = m.backlight_timeout;
    if (m.idle_poweroff >= 0)
    {
        global_settings.poweroff = m.idle_poweroff;
        set_poweroff_timeout(global_settings.poweroff);
    }
    if (m.keyclick >= 0)
        /* 2 = "moderate", mismo default que usa el toggle de Ajustes >
         * Clicker (aura_screens.c) al encenderlo -- ver D-196. */
        global_settings.keyclick = m.keyclick ? 2 : 0;
    if (m.volume_limit != AURA_SHARED_SETTINGS_VOLUME_LIMIT_ABSENT)
        global_settings.volume_limit = m.volume_limit;
    apply_replaygain(m.replaygain);
    apply_language(m.language);
    apply_appearance(m.appearance);

    aura_settings.shared_rev_applied = m.rev;

    /* aura_settings.* (bloqueo/idioma/tema/shared_rev_applied) escribe
     * de inmediato -- aura_settings_save() ya es sincrono. Lo de
     * global_settings.* (brillo/retroilum./apagado/clicker/limite/
     * replaygain) es Rockbox nativo: mismo criterio de D-351,
     * settings_save() diferido + flush forzado ya mismo (regla 2 del
     * contrato: "con settings_save() + flush inmediato"). */
    aura_settings_save();
    aura_settings_core_touched();
    aura_settings_core_flush();
}

/* -- Aura -> Contrato (escribir) ------------------------------------------ */

static aura_shared_lock_require_t shared_lock_require_of(aura_lock_require_t v)
{
    switch (v)
    {
    case AURA_LOCK_REQUIRE_HOLD: return AURA_SHARED_LOCK_REQUIRE_HOLD;
    case AURA_LOCK_REQUIRE_1MIN: return AURA_SHARED_LOCK_REQUIRE_1MIN;
    case AURA_LOCK_REQUIRE_5MIN: return AURA_SHARED_LOCK_REQUIRE_5MIN;
    case AURA_LOCK_REQUIRE_BOOT: return AURA_SHARED_LOCK_REQUIRE_BOOT;
    default: return AURA_SHARED_LOCK_REQUIRE_HOLD;
    }
}

static aura_shared_replaygain_t shared_replaygain_of(int rockbox_type)
{
    switch (rockbox_type)
    {
    case REPLAYGAIN_OFF:   return AURA_SHARED_REPLAYGAIN_OFF;
    case REPLAYGAIN_ALBUM: return AURA_SHARED_REPLAYGAIN_ALBUM;
    case REPLAYGAIN_TRACK: return AURA_SHARED_REPLAYGAIN_TRACK;
    /* REPLAYGAIN_SHUFFLE no tiene equivalente en el contrato (SS D.6
     * solo define off/track/album) -- Aura nunca lo activa por su
     * cuenta (su toggle de Ajustes solo alterna OFF/TRACK), pero si
     * otra familia lo dejara asi, "track" es la lectura mas fiel: en
     * los dos casos hay ganancia REAL aplicandose, a diferencia de
     * "off". */
    default: return AURA_SHARED_REPLAYGAIN_TRACK;
    }
}

static aura_shared_appearance_t shared_appearance_of(aura_theme_id_t v)
{
    return (v == AURA_THEME_DARK) ? AURA_SHARED_APPEARANCE_DARK
                                  : AURA_SHARED_APPEARANCE_LIGHT;
}

static aura_shared_lang_t shared_lang_of(aura_lang_t v)
{
    switch (v)
    {
    case AURA_LANG_ES: return AURA_SHARED_LANG_ES;
    case AURA_LANG_EN: return AURA_SHARED_LANG_EN;
    /* D-357 amplia aura_lang_t; hasta entonces no hay mas casos que
     * mapear desde este lado. */
    default: return AURA_SHARED_LANG_ES;
    }
}

void aura_shared_settings_write_current(void)
{
    /* D-356: readbuf+writebuf+old+m suman ~3.8 KB -- misma razon que
     * aura_shared_settings_apply_if_newer() de arriba: static en vez
     * de pila. Seguro por el mismo motivo (solo hilo de UI, sin
     * reentrada ni anidamiento entre las dos funciones). */
    static char readbuf[SETTINGS_BUF_SIZE];
    static char writebuf[SETTINGS_BUF_SIZE];
    static aura_shared_settings_t old, m;
    int n;
    long next_rev = 1;

    /* Relee el archivo ANTES de reescribirlo -- unico modo de preservar
     * las claves desconocidas (regla 2) y de no pisar un `rev` que otra
     * familia haya escrito mientras tanto (siempre se incrementa desde
     * lo que hay AHORA en disco, nunca desde shared_rev_applied de este
     * firmware -- los dos pueden diferir si otra familia escribio y
     * este firmware todavia no lo aplico). */
    n = aura_fsutil_read_text(AURA_SHARED_SETTINGS_PATH, readbuf, sizeof(readbuf));
    if (n > 0 && aura_shared_settings_parse(readbuf, &old) == AURA_SHARED_SETTINGS_OK)
    {
        if (old.rev > 0)
            next_rev = old.rev + 1;
        m = old; /* copia los `unknown[]` preservados; el resto se pisa abajo */
    }
    else
    {
        aura_shared_settings_init(&old);
        aura_shared_settings_init(&m);
    }

    m.rev = next_rev;
    strcpy(m.updated_by, "aura"); /* 4 letras, sobra en un buffer de 8 */

    m.screen_lock_enabled = aura_settings.screen_lock_enabled ? 1 : 0;
    snprintf(m.screen_lock_pin, sizeof(m.screen_lock_pin), "%04u",
             (unsigned)aura_settings.screen_lock_pin % 10000u);
    m.screen_lock_require = shared_lock_require_of(aura_settings.screen_lock_require);

    m.brightness = global_settings.brightness;
    m.backlight_timeout = global_settings.backlight_timeout;
    m.idle_poweroff = global_settings.poweroff;
    m.keyclick = global_settings.keyclick ? 1 : 0;
    m.volume_limit = global_settings.volume_limit;
    m.replaygain = shared_replaygain_of(global_settings.replaygain_settings.type);
    m.language = shared_lang_of(aura_settings.language);
    m.appearance = shared_appearance_of(aura_settings.theme);

    n = aura_shared_settings_serialize(&m, writebuf, sizeof(writebuf));
    if (n < 0)
        return; /* no debería pasar con 13 claves conocidas + 8 desconocidas
                  * como mucho en un buffer de 1 KB, pero si pasara, no
                  * escribir un archivo truncado es mejor que escribirlo */

    if (aura_fsutil_write_all_atomic(AURA_SHARED_SETTINGS_PATH, writebuf, (size_t)n))
    {
        /* Ya lo tiene: es quien lo escribio. Se persiste ya mismo --
         * si no, un reinicio antes del proximo aura_settings_save() de
         * otra pantalla dejaria el aura.cfg en disco con un
         * shared_rev_applied viejo, y el proximo arranque volveria a
         * "aplicarse a si mismo" su propio ultimo estado (inofensivo,
         * pero un ciclo de escritura de mas). */
        aura_settings.shared_rev_applied = m.rev;
        aura_settings_save();
    }
}
