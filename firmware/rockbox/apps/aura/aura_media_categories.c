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

#include "file.h"
#include "misc.h"
#include "rbpaths.h"
#include "settings.h"
#include "string-extra.h"

#include "aura_media_categories.h"

/* Mismo directorio que sync_summary.cfg/cfcache/photocache -- sin header
 * compartido para esto en el proyecto, cada archivo redefine AURA_DIR
 * igual (precedente ya establecido, ver aura_manifest.c/aura_photos.c). */
#define AURA_DIR                 ROCKBOX_DIR "/aura"
#define VIDEO_CATEGORIES_PATH    AURA_DIR "/video_categories.cfg"
#define PHOTO_CATEGORIES_PATH    AURA_DIR "/photo_categories.cfg"

/* Contrato SS-CAT (CONTRATO-firmware-studio.md): nombre de archivo tal
 * como aparece en /Videos o /Photos, <= 95 bytes UTF-8 + '\0' -- mismo
 * limite que VIDEO_NAME_LEN/PHOTO_NAME_LEN (aura_video.c/aura_photos.c). */
#define NAME_LEN 96

/* Topes de entradas cargadas -- mismos valores que MAX_VIDEOS/MAX_PHOTOS
 * (aura_video.c/aura_photos.c): una entrada del indice para un archivo
 * que ni siquiera se llego a LISTAR (mas alla del tope de esas listas)
 * no tiene con que emparejarse, asi que cargar mas no serviria de nada. */
#define MAX_VIDEO_ENTRIES 100
#define MAX_PHOTO_ENTRIES 500

typedef struct {
    char name[NAME_LEN];
    unsigned char cat;
} cat_entry_t;

static cat_entry_t s_video_cats[MAX_VIDEO_ENTRIES];
static int s_video_cat_count = 0;
static bool s_video_loaded = false;

static cat_entry_t s_photo_cats[MAX_PHOTO_ENTRIES];
static int s_photo_cat_count = 0;
static bool s_photo_loaded = false;

static aura_video_cat_t parse_video_code(const char *v)
{
    if (!strcmp(v, "movie"))  return AURA_VIDEO_CAT_MOVIE;
    if (!strcmp(v, "series")) return AURA_VIDEO_CAT_SERIES;
    if (!strcmp(v, "clip"))   return AURA_VIDEO_CAT_CLIP;
    return AURA_VIDEO_CAT_NONE;
}

static aura_photo_cat_t parse_photo_code(const char *v)
{
    if (!strcmp(v, "photo")) return AURA_PHOTO_CAT_PHOTO;
    if (!strcmp(v, "image")) return AURA_PHOTO_CAT_IMAGE;
    if (!strcmp(v, "ai"))    return AURA_PHOTO_CAT_AI;
    return AURA_PHOTO_CAT_NONE;
}

/* Mismo patron exacto que aura_manifest_load() (aura_manifest.c):
 * open() + read_line() + settings_parseline() ("nombre: valor" por
 * linea, '#' inicial descarta la linea completa) -- ningun parser JSON
 * real, el formato de sync_summary.cfg ya establece el precedente para
 * archivos que Aura Studio escribe para que el firmware los lea. Aca el
 * "nombre" es el nombre de archivo (unico dentro de /Videos o /Photos,
 * ambos planos) y el "valor" es el codigo corto de categoria. Un archivo
 * ausente (Studio todavia no lo escribe) deja *_loaded=true con count=0
 * -- se comporta identico a un indice vacio, sin reintentar en cada
 * consulta. */
static void ensure_video_categories(void)
{
    int fd;
    char line[NAME_LEN + 16];

    if (s_video_loaded)
        return;
    s_video_loaded = true;
    s_video_cat_count = 0;

    fd = open(VIDEO_CATEGORIES_PATH, O_RDONLY);
    if (fd < 0)
        return;

    while (s_video_cat_count < MAX_VIDEO_ENTRIES
           && read_line(fd, line, sizeof(line)) > 0)
    {
        char *name, *value;
        aura_video_cat_t cat;

        if (!settings_parseline(line, &name, &value))
            continue;
        cat = parse_video_code(value);
        if (cat == AURA_VIDEO_CAT_NONE)
            continue;

        strlcpy(s_video_cats[s_video_cat_count].name, name, NAME_LEN);
        s_video_cats[s_video_cat_count].cat = (unsigned char)cat;
        s_video_cat_count++;
    }
    close(fd);
}

static void ensure_photo_categories(void)
{
    int fd;
    char line[NAME_LEN + 16];

    if (s_photo_loaded)
        return;
    s_photo_loaded = true;
    s_photo_cat_count = 0;

    fd = open(PHOTO_CATEGORIES_PATH, O_RDONLY);
    if (fd < 0)
        return;

    while (s_photo_cat_count < MAX_PHOTO_ENTRIES
           && read_line(fd, line, sizeof(line)) > 0)
    {
        char *name, *value;
        aura_photo_cat_t cat;

        if (!settings_parseline(line, &name, &value))
            continue;
        cat = parse_photo_code(value);
        if (cat == AURA_PHOTO_CAT_NONE)
            continue;

        strlcpy(s_photo_cats[s_photo_cat_count].name, name, NAME_LEN);
        s_photo_cats[s_photo_cat_count].cat = (unsigned char)cat;
        s_photo_cat_count++;
    }
    close(fd);
}

aura_video_cat_t aura_media_categories_video_lookup(const char *filename)
{
    int i;

    ensure_video_categories();
    for (i = 0; i < s_video_cat_count; i++)
        if (!strcmp(s_video_cats[i].name, filename))
            return (aura_video_cat_t)s_video_cats[i].cat;
    return AURA_VIDEO_CAT_NONE;
}

aura_photo_cat_t aura_media_categories_photo_lookup(const char *filename)
{
    int i;

    ensure_photo_categories();
    for (i = 0; i < s_photo_cat_count; i++)
        if (!strcmp(s_photo_cats[i].name, filename))
            return (aura_photo_cat_t)s_photo_cats[i].cat;
    return AURA_PHOTO_CAT_NONE;
}

void aura_media_categories_invalidate(void)
{
    s_video_loaded = false;
    s_photo_loaded = false;
}
