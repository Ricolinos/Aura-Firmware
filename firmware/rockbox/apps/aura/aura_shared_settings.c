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
#include "aura_shared_settings.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* Copia acotada con NUL garantizado -- mismo motivo que
 * aura_style_manifest.c: evita strlcpy() (apps/misc.c de Rockbox, no
 * host-includable) para compilar identico en host y firmware. */
static void copy_bounded(char *dst, size_t dstsize, const char *src, size_t srclen)
{
    size_t len = srclen;
    if (len >= dstsize)
        len = dstsize - 1;
    memcpy(dst, src, len);
    dst[len] = '\0';
}

void aura_shared_settings_init(aura_shared_settings_t *out)
{
    memset(out, 0, sizeof(*out));
    out->rev = -1;
    out->screen_lock_enabled = -1;
    out->screen_lock_require = AURA_SHARED_LOCK_REQUIRE_COUNT;
    out->brightness = -1;
    out->backlight_timeout = -2;
    out->idle_poweroff = -1;
    out->keyclick = -1;
    out->volume_limit = AURA_SHARED_SETTINGS_VOLUME_LIMIT_ABSENT;
    out->replaygain = AURA_SHARED_REPLAYGAIN_COUNT;
    out->language = AURA_SHARED_LANG_COUNT;
    out->appearance = AURA_SHARED_APPEARANCE_COUNT;
    out->unknown_count = 0;
}

/* -- Tablas nombre<->enum, en el orden del contrato ---------------------- */

static const char *const lock_require_names[AURA_SHARED_LOCK_REQUIRE_COUNT] = {
    [AURA_SHARED_LOCK_REQUIRE_HOLD]  = "hold",
    [AURA_SHARED_LOCK_REQUIRE_1MIN]  = "1min",
    [AURA_SHARED_LOCK_REQUIRE_5MIN]  = "5min",
    [AURA_SHARED_LOCK_REQUIRE_BOOT]  = "boot",
};
static const char *const replaygain_names[AURA_SHARED_REPLAYGAIN_COUNT] = {
    [AURA_SHARED_REPLAYGAIN_OFF]   = "off",
    [AURA_SHARED_REPLAYGAIN_TRACK] = "track",
    [AURA_SHARED_REPLAYGAIN_ALBUM] = "album",
};
static const char *const lang_names[AURA_SHARED_LANG_COUNT] = {
    [AURA_SHARED_LANG_ES] = "es",
    [AURA_SHARED_LANG_EN] = "en",
    [AURA_SHARED_LANG_FR] = "fr",
    [AURA_SHARED_LANG_DE] = "de",
    [AURA_SHARED_LANG_RU] = "ru",
    [AURA_SHARED_LANG_IT] = "it",
};
static const char *const appearance_names[AURA_SHARED_APPEARANCE_COUNT] = {
    [AURA_SHARED_APPEARANCE_DARK]  = "dark",
    [AURA_SHARED_APPEARANCE_LIGHT] = "light",
};

const char *aura_shared_lock_require_name(aura_shared_lock_require_t v)
{
    /* Mismo cast que D-345/D-352: el enum sube a unsigned en el target,
     * asi que `v >= 0` avisa -Wtype-limits sin este cast. */
    return ((unsigned)v < AURA_SHARED_LOCK_REQUIRE_COUNT) ? lock_require_names[v] : "";
}
const char *aura_shared_replaygain_name(aura_shared_replaygain_t v)
{
    return ((unsigned)v < AURA_SHARED_REPLAYGAIN_COUNT) ? replaygain_names[v] : "";
}
const char *aura_shared_lang_name(aura_shared_lang_t v)
{
    return ((unsigned)v < AURA_SHARED_LANG_COUNT) ? lang_names[v] : "";
}
const char *aura_shared_appearance_name(aura_shared_appearance_t v)
{
    return ((unsigned)v < AURA_SHARED_APPEARANCE_COUNT) ? appearance_names[v] : "";
}

/* value NUL-terminado, comparado exacto contra `names[0..count)`.
 * Devuelve `count` (el sentinela "ausente/invalido") si no matchea
 * ninguno. */
static int name_to_index(const char *value, const char *const *names, int count)
{
    int i;
    for (i = 0; i < count; i++)
        if (!strcmp(value, names[i]))
            return i;
    return count;
}

/* -1 si `value` no es un entero (opcionalmente con signo) valido, o si
 * se sale de [lo, hi]. */
static long parse_int_range(const char *value, long lo, long hi, bool *ok)
{
    char *end;
    long v;

    *ok = false;
    if (!*value)
        return 0;
    v = strtol(value, &end, 10);
    if (*end != '\0')
        return 0;
    if (v < lo || v > hi)
        return 0;
    *ok = true;
    return v;
}

/* -- Parseo linea a linea ------------------------------------------------ */

/* Divide `line` (sin el salto de linea) en nombre/valor sobre el primer
 * ": " -- mismo formato que settings_parseline() de Rockbox, pero este
 * modulo no puede usar esa funcion (vive en apps/misc.c, no
 * host-includable) asi que se reimplementa la version minima que
 * necesita este archivo: "clave: valor", sin espacios alrededor de la
 * clave, un solo espacio obligatorio tras los dos puntos. Devuelve
 * false si la linea no trae ": " (se ignora entera -- ni siquiera
 * cuenta como clave desconocida, es basura, no una clave con valor
 * vacio). */
static bool split_line(const char *line, size_t linelen,
                       const char **name, size_t *namelen,
                       const char **value, size_t *valuelen)
{
    size_t i;
    for (i = 0; i + 1 < linelen; i++)
    {
        if (line[i] == ':' && line[i + 1] == ' ')
        {
            *name = line;
            *namelen = i;
            *value = line + i + 2;
            *valuelen = linelen - i - 2;
            return true;
        }
    }
    return false;
}

static bool name_is(const char *name, size_t namelen, const char *lit)
{
    return namelen == strlen(lit) && !memcmp(name, lit, namelen);
}

static void apply_line(aura_shared_settings_t *out,
                       const char *name, size_t namelen,
                       const char *value, size_t valuelen)
{
    /* Copia el valor a un buffer NUL-terminado acotado -- ninguna clave
     * conocida necesita mas de AURA_SHARED_SETTINGS_UNKNOWN_VALUE_LEN
     * (64) bytes; si algun dia una clave desconocida trae un valor mas
     * largo, se trunca (mismo criterio que copy_bounded en todo el
     * arbol: degradar, no reventar). */
    char v[AURA_SHARED_SETTINGS_UNKNOWN_VALUE_LEN];
    bool ok;
    long n;

    copy_bounded(v, sizeof(v), value, valuelen);

    if (name_is(name, namelen, "rev"))
    {
        n = parse_int_range(v, 1, 2147483647L, &ok);
        out->rev = ok ? n : -1;
        return;
    }
    if (name_is(name, namelen, "updated_by"))
    {
        copy_bounded(out->updated_by, sizeof(out->updated_by), value, valuelen);
        return;
    }
    if (name_is(name, namelen, "screen_lock_enabled"))
    {
        n = parse_int_range(v, 0, 1, &ok);
        out->screen_lock_enabled = ok ? (int)n : -1;
        return;
    }
    if (name_is(name, namelen, "screen_lock_pin"))
    {
        size_t i;
        bool all_digits = (valuelen == 4);
        for (i = 0; all_digits && i < valuelen; i++)
            if (value[i] < '0' || value[i] > '9')
                all_digits = false;
        if (all_digits)
            copy_bounded(out->screen_lock_pin, sizeof(out->screen_lock_pin), value, valuelen);
        else
            out->screen_lock_pin[0] = '\0';
        return;
    }
    if (name_is(name, namelen, "screen_lock_require"))
    {
        out->screen_lock_require = (aura_shared_lock_require_t)
            name_to_index(v, lock_require_names, AURA_SHARED_LOCK_REQUIRE_COUNT);
        return;
    }
    if (name_is(name, namelen, "brightness"))
    {
        n = parse_int_range(v, 1, AURA_SHARED_SETTINGS_BRIGHTNESS_MAX, &ok);
        out->brightness = ok ? (int)n : -1;
        return;
    }
    if (name_is(name, namelen, "backlight_timeout"))
    {
        n = parse_int_range(v, -1, AURA_SHARED_SETTINGS_BACKLIGHT_TIMEOUT_MAX, &ok);
        out->backlight_timeout = ok ? (int)n : -2;
        return;
    }
    if (name_is(name, namelen, "idle_poweroff"))
    {
        n = parse_int_range(v, 0, AURA_SHARED_SETTINGS_IDLE_POWEROFF_MAX, &ok);
        out->idle_poweroff = ok ? (int)n : -1;
        return;
    }
    if (name_is(name, namelen, "keyclick"))
    {
        n = parse_int_range(v, 0, 1, &ok);
        out->keyclick = ok ? (int)n : -1;
        return;
    }
    if (name_is(name, namelen, "volume_limit"))
    {
        n = parse_int_range(v, AURA_SHARED_SETTINGS_VOLUME_LIMIT_MIN,
                            AURA_SHARED_SETTINGS_VOLUME_LIMIT_MAX, &ok);
        out->volume_limit = ok ? (int)n : AURA_SHARED_SETTINGS_VOLUME_LIMIT_ABSENT;
        return;
    }
    if (name_is(name, namelen, "replaygain"))
    {
        out->replaygain = (aura_shared_replaygain_t)
            name_to_index(v, replaygain_names, AURA_SHARED_REPLAYGAIN_COUNT);
        return;
    }
    if (name_is(name, namelen, "language"))
    {
        out->language = (aura_shared_lang_t)
            name_to_index(v, lang_names, AURA_SHARED_LANG_COUNT);
        return;
    }
    if (name_is(name, namelen, "appearance"))
    {
        out->appearance = (aura_shared_appearance_t)
            name_to_index(v, appearance_names, AURA_SHARED_APPEARANCE_COUNT);
        return;
    }

    /* Clave desconocida: preservarla verbatim (regla 2 del contrato),
     * con presupuesto acotado -- mismo criterio de degradacion que el
     * GC de cachés en todo el árbol (D-338 y sucesores): despues del
     * cupo se descarta en silencio, no se aborta el parseo. */
    if (out->unknown_count < AURA_SHARED_SETTINGS_MAX_UNKNOWN)
    {
        aura_shared_settings_unknown_t *u = &out->unknown[out->unknown_count];
        copy_bounded(u->key, sizeof(u->key), name, namelen);
        copy_bounded(u->value, sizeof(u->value), value, valuelen);
        out->unknown_count++;
    }
}

aura_shared_settings_status_t aura_shared_settings_parse(const char *text,
                                                          aura_shared_settings_t *out)
{
    const char *p = text;
    size_t header_len = strlen(AURA_SHARED_SETTINGS_HEADER);

    aura_shared_settings_init(out);

    /* Regla 5: la PRIMERA linea debe ser exactamente la cabecera (hasta
     * el salto de linea, o el fin del texto si el archivo es solo la
     * cabecera). Cualquier otra cosa -- vacio, cabecera de otra
     * version, texto suelto -- rechaza el archivo entero. */
    if (strncmp(p, AURA_SHARED_SETTINGS_HEADER, header_len) != 0)
        return AURA_SHARED_SETTINGS_NO_HEADER;
    p += header_len;
    if (*p != '\0' && *p != '\n' && *p != '\r')
        return AURA_SHARED_SETTINGS_NO_HEADER; /* p.ej. "# aura-shared-settings v10" */

    while (*p)
    {
        const char *line_start;
        size_t linelen = 0;
        const char *name, *value;
        size_t namelen, valuelen;

        while (*p == '\n' || *p == '\r')
            p++;
        if (!*p)
            break;

        line_start = p;
        while (p[linelen] && p[linelen] != '\n' && p[linelen] != '\r')
            linelen++;
        p += linelen;

        if (split_line(line_start, linelen, &name, &namelen, &value, &valuelen))
            apply_line(out, name, namelen, value, valuelen);
        /* Una linea sin ": " (en blanco, comentario suelto) se ignora
         * -- nunca aborta el resto del archivo. */
    }

    return AURA_SHARED_SETTINGS_OK;
}

int aura_shared_settings_serialize(const aura_shared_settings_t *m,
                                   char *buf, size_t bufsize)
{
    int n;
    int i;
    size_t off = 0;

    n = snprintf(buf, bufsize,
                "%s\n"
                "rev: %ld\n"
                "updated_by: %s\n"
                "screen_lock_enabled: %d\n"
                "screen_lock_pin: %s\n"
                "screen_lock_require: %s\n"
                "brightness: %d\n"
                "backlight_timeout: %d\n"
                "idle_poweroff: %d\n"
                "keyclick: %d\n"
                "volume_limit: %d\n"
                "replaygain: %s\n"
                "language: %s\n"
                "appearance: %s\n",
                AURA_SHARED_SETTINGS_HEADER,
                m->rev,
                m->updated_by,
                m->screen_lock_enabled,
                m->screen_lock_pin,
                aura_shared_lock_require_name(m->screen_lock_require),
                m->brightness,
                m->backlight_timeout,
                m->idle_poweroff,
                m->keyclick,
                m->volume_limit,
                aura_shared_replaygain_name(m->replaygain),
                aura_shared_lang_name(m->language),
                aura_shared_appearance_name(m->appearance));
    if (n < 0 || (size_t)n >= bufsize)
        return -1;
    off = (size_t)n;

    for (i = 0; i < m->unknown_count; i++)
    {
        n = snprintf(buf + off, bufsize - off, "%s: %s\n",
                    m->unknown[i].key, m->unknown[i].value);
        if (n < 0 || (size_t)n >= bufsize - off)
            return -1;
        off += (size_t)n;
    }

    return (int)off;
}
