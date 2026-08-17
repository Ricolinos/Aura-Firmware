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
#include "aura_style_manifest.h"

#include <string.h>
#include <stdlib.h>

/* Copia acotada con NUL garantizado -- este modulo evita strlcpy()
 * (vive en apps/misc.c de Rockbox, no host-includable) a proposito,
 * para compilar identico en el host y en el firmware. */
static void copy_bounded(char *dst, size_t dstsize, const char *src)
{
    size_t len = strlen(src);
    if (len >= dstsize)
        len = dstsize - 1;
    memcpy(dst, src, len);
    dst[len] = '\0';
}

/* "#RRGGBB" o "RRGGBB" -> rgb24; -1 si no parsea. */
static int parse_hex_rgb24(const char *value)
{
    const char *p = value;
    char *end;
    long v;

    if (*p == '#')
        p++;
    v = strtol(p, &end, 16);
    if (end == p || *end != '\0' || v < 0 || v > 0xFFFFFF)
        return -1;
    return (int)v;
}

void aura_style_manifest_init(aura_style_manifest_t *m)
{
    int i;

    memset(m, 0, sizeof(*m));
    m->format = -1;
    for (i = 0; i < AURA_STYLE_ROLE_COUNT; i++)
    {
        m->palette_light[i] = -1;
        m->palette_dark[i] = -1;
    }
    m->category_settings_gray = -1;
    m->category_video = -1;
    m->category_photos = -1;
    m->category_extras_yellow = -1;
}

static const struct { const char *key; aura_style_role_t role; } light_keys[] = {
    { "palette_light_shell_bg",       AURA_STYLE_ROLE_SHELL_BG },
    { "palette_light_text_primary",   AURA_STYLE_ROLE_TEXT_PRIMARY },
    { "palette_light_text_secondary", AURA_STYLE_ROLE_TEXT_SECONDARY },
    { "palette_light_text_tertiary",  AURA_STYLE_ROLE_TEXT_TERTIARY },
    { "palette_light_shell_rail",     AURA_STYLE_ROLE_SHELL_RAIL },
    { "palette_light_progress_fill",  AURA_STYLE_ROLE_PROGRESS_FILL },
    { "palette_light_progress_track", AURA_STYLE_ROLE_PROGRESS_TRACK },
    { "palette_light_selection_fill", AURA_STYLE_ROLE_SELECTION_FILL },
};
static const struct { const char *key; aura_style_role_t role; } dark_keys[] = {
    { "palette_dark_shell_bg",       AURA_STYLE_ROLE_SHELL_BG },
    { "palette_dark_text_primary",   AURA_STYLE_ROLE_TEXT_PRIMARY },
    { "palette_dark_text_secondary", AURA_STYLE_ROLE_TEXT_SECONDARY },
    { "palette_dark_text_tertiary",  AURA_STYLE_ROLE_TEXT_TERTIARY },
    { "palette_dark_shell_rail",     AURA_STYLE_ROLE_SHELL_RAIL },
    { "palette_dark_progress_fill",  AURA_STYLE_ROLE_PROGRESS_FILL },
    { "palette_dark_progress_track", AURA_STYLE_ROLE_PROGRESS_TRACK },
    { "palette_dark_selection_fill", AURA_STYLE_ROLE_SELECTION_FILL },
};

void aura_style_manifest_apply_line(aura_style_manifest_t *m,
                                     const char *name, const char *value)
{
    size_t i;

    if (!strcmp(name, "theme_format"))
    {
        m->format = atoi(value);
        return;
    }
    if (!strcmp(name, "theme_id"))
    {
        copy_bounded(m->id, sizeof(m->id), value);
        m->has_id = true;
        return;
    }
    if (!strcmp(name, "theme_name"))
    {
        copy_bounded(m->name, sizeof(m->name), value);
        m->has_name = true;
        return;
    }
    if (!strcmp(name, "category_settings_gray"))
    {
        m->category_settings_gray = parse_hex_rgb24(value);
        return;
    }
    if (!strcmp(name, "category_video"))
    {
        m->category_video = parse_hex_rgb24(value);
        return;
    }
    if (!strcmp(name, "category_photos"))
    {
        m->category_photos = parse_hex_rgb24(value);
        return;
    }
    if (!strcmp(name, "category_extras_yellow"))
    {
        m->category_extras_yellow = parse_hex_rgb24(value);
        return;
    }

    for (i = 0; i < sizeof(light_keys) / sizeof(light_keys[0]); i++)
        if (!strcmp(name, light_keys[i].key))
        {
            m->palette_light[light_keys[i].role] = parse_hex_rgb24(value);
            return;
        }
    for (i = 0; i < sizeof(dark_keys) / sizeof(dark_keys[0]); i++)
        if (!strcmp(name, dark_keys[i].key))
        {
            m->palette_dark[dark_keys[i].role] = parse_hex_rgb24(value);
            return;
        }

    /* Clave desconocida: theme_author/theme_license/
     * theme_redistributable/requires_firmware_min/accent_default/
     * accent_presets (campos de Aura Studio, el firmware no los usa en
     * v1 -- CONTRATO-formato-tema.md SS8) o de un theme_format futuro.
     * Se ignora en silencio, mismo criterio que aura_settings_load(). */
}

bool aura_style_id_is_valid(const char *id)
{
    size_t len, i;

    if (!id)
        return false;
    len = strlen(id);
    if (len == 0 || len >= AURA_STYLE_ID_LEN)
        return false;
    if (!strcmp(id, "default"))
        return false;
    for (i = 0; i < len; i++)
    {
        char c = id[i];
        bool ok = (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-';
        if (!ok)
            return false;
    }
    return true;
}

bool aura_style_manifest_is_loadable(const aura_style_manifest_t *m)
{
    return m->format >= 0 && m->format <= AURA_STYLE_FORMAT_SUPPORTED;
}
