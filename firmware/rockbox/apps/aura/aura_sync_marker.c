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
#include "aura_sync_marker.h"

#include <string.h>
#include <stdio.h>

void aura_sync_marker_init(aura_sync_marker_t *out)
{
    out->version = -1;
    out->timestamp[0] = '\0';
    out->music = false;
    out->video = false;
    out->images = false;
    out->attempts = 0;
}

static const char *skip_ws(const char *p)
{
    while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')
        p++;
    return p;
}

/* Busca `"key"` seguida (tras espacios) de ':' y devuelve un puntero al
 * primer caracter del valor, o NULL. Solo considera apariciones que son
 * de verdad una clave (comilla de cierre + ':'), no un valor de texto
 * que casualmente contenga la palabra. */
static const char *find_value(const char *text, const char *key)
{
    size_t klen = strlen(key);
    const char *p = text;

    while ((p = strchr(p, '"')) != NULL)
    {
        const char *q;
        p++;
        if (strncmp(p, key, klen) != 0 || p[klen] != '"')
        {
            /* saltar hasta la comilla de cierre de esta cadena */
            q = strchr(p, '"');
            if (!q)
                return NULL;
            p = q + 1;
            continue;
        }
        q = skip_ws(p + klen + 1);
        if (*q != ':')
        {
            p = p + klen + 1;
            continue;
        }
        return skip_ws(q + 1);
    }
    return NULL;
}

/* -1 si no hay un entero no negativo ahi. */
static int parse_int(const char *v)
{
    int n = 0;
    if (*v < '0' || *v > '9')
        return -1;
    while (*v >= '0' && *v <= '9')
    {
        if (n > 100000)
            return -1;
        n = n * 10 + (*v - '0');
        v++;
    }
    return n;
}

/* 1/0 para true/false, -1 si no es un booleano. */
static int parse_bool(const char *v)
{
    if (!strncmp(v, "true", 4))
        return 1;
    if (!strncmp(v, "false", 5))
        return 0;
    return -1;
}

static void parse_string(const char *v, char *out, size_t outsz)
{
    size_t n = 0;
    out[0] = '\0';
    if (*v != '"')
        return;
    v++;
    while (*v && *v != '"' && n + 1 < outsz)
    {
        if (*v == '\\' && v[1])
            v++; /* escape simple: se copia el caracter escapado */
        out[n++] = *v++;
    }
    out[n] = '\0';
}

aura_sync_marker_status_t aura_sync_marker_parse(const char *text,
                                                 aura_sync_marker_t *out)
{
    const char *v;
    int n;

    aura_sync_marker_init(out);
    if (!text)
        return AURA_SYNC_MARKER_MALFORMED;

    v = skip_ws(text);
    if (*v != '{')
        return AURA_SYNC_MARKER_MALFORMED;

    v = find_value(text, "version");
    if (!v || (n = parse_int(v)) < 0)
    {
        aura_sync_marker_init(out);
        return AURA_SYNC_MARKER_MISSING_VERSION;
    }
    out->version = n;

    v = find_value(text, "timestamp");
    if (v)
        parse_string(v, out->timestamp, sizeof(out->timestamp));

    /* Las tres claves de secciones son unicas en todo el documento
     * (viven dentro de "changes"), asi que una busqueda plana basta y
     * ademas tolera que un Studio futuro las mueva de nivel. */
    v = find_value(text, "music");
    out->music = v && parse_bool(v) == 1;
    v = find_value(text, "video");
    out->video = v && parse_bool(v) == 1;
    v = find_value(text, "images");
    out->images = v && parse_bool(v) == 1;

    v = find_value(text, "attempts");
    if (v && (n = parse_int(v)) >= 0)
        out->attempts = n;

    if (out->version > AURA_SYNC_MARKER_VERSION_SUPPORTED)
        return AURA_SYNC_MARKER_UNSUPPORTED;

    return AURA_SYNC_MARKER_OK;
}

int aura_sync_marker_serialize(const aura_sync_marker_t *m,
                               char *buf, size_t bufsize)
{
    int n = snprintf(buf, bufsize,
                     "{\n"
                     "  \"version\": %d,\n"
                     "  \"timestamp\": \"%s\",\n"
                     "  \"changes\": {\n"
                     "    \"music\": %s,\n"
                     "    \"video\": %s,\n"
                     "    \"images\": %s\n"
                     "  },\n"
                     "  \"attempts\": %d\n"
                     "}\n",
                     m->version < 0 ? AURA_SYNC_MARKER_VERSION_SUPPORTED : m->version,
                     m->timestamp,
                     m->music ? "true" : "false",
                     m->video ? "true" : "false",
                     m->images ? "true" : "false",
                     m->attempts < 0 ? 0 : m->attempts);
    if (n < 0 || (size_t)n >= bufsize)
        return -1;
    return n;
}

bool aura_sync_marker_has_work(const aura_sync_marker_t *m)
{
    return m->music || m->video || m->images;
}
