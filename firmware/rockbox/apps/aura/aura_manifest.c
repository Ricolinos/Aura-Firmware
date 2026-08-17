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

#include "aura_manifest.h"

#define AURA_DIR       ROCKBOX_DIR "/aura"
#define MANIFEST_PATH  AURA_DIR "/sync_summary.cfg"

/* No se usa atoll/strtoll (no disponibles en todos los targets de este
 * arbol) -- los valores siempre son enteros decimales no negativos
 * escritos por CatalogSummaryWriter (Aura Studio), asi que un parser
 * propio de unas pocas lineas alcanza sin depender de libc extendida. */
static long long parse_i64(const char *s)
{
    long long value = 0;

    while (*s >= '0' && *s <= '9')
    {
        value = value * 10 + (*s - '0');
        s++;
    }
    return value;
}

bool aura_manifest_load(aura_manifest_t *out)
{
    int fd;
    char line[64];

    memset(out, 0, sizeof(*out));

    fd = open(MANIFEST_PATH, O_RDONLY);
    if (fd < 0)
        return false;

    while (read_line(fd, line, sizeof(line)) > 0)
    {
        char *name, *value;
        if (!settings_parseline(line, &name, &value))
            continue;

        if (!strcmp(name, "music_count"))
            out->music_count = (int)parse_i64(value);
        else if (!strcmp(name, "music_bytes"))
            out->music_bytes = parse_i64(value);
        else if (!strcmp(name, "video_count"))
            out->video_count = (int)parse_i64(value);
        else if (!strcmp(name, "video_bytes"))
            out->video_bytes = parse_i64(value);
        else if (!strcmp(name, "photo_count"))
            out->photo_count = (int)parse_i64(value);
        else if (!strcmp(name, "photo_bytes"))
            out->photo_bytes = parse_i64(value);
        else if (!strcmp(name, "playlist_count"))
            out->playlist_count = (int)parse_i64(value);
        else if (!strcmp(name, "video_movies_count"))
        { out->video_movies_count = (int)parse_i64(value); out->has_video_categories = true; }
        else if (!strcmp(name, "video_series_count"))
        { out->video_series_count = (int)parse_i64(value); out->has_video_categories = true; }
        else if (!strcmp(name, "video_clips_count"))
        { out->video_clips_count = (int)parse_i64(value); out->has_video_categories = true; }
        else if (!strcmp(name, "photo_images_count"))
        { out->photo_images_count = (int)parse_i64(value); out->has_photo_categories = true; }
        else if (!strcmp(name, "photo_photos_count"))
        { out->photo_photos_count = (int)parse_i64(value); out->has_photo_categories = true; }
        else if (!strcmp(name, "photo_ai_count"))
        { out->photo_ai_count = (int)parse_i64(value); out->has_photo_categories = true; }
    }
    close(fd);

    return true;
}
