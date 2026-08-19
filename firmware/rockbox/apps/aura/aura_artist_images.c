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

#include "file.h"
#include "misc.h"
#include "rbpaths.h"
#include "string-extra.h"

#include "aura_artist_images.h"
#include "aura_artist_images_parse.h"

/* Mismo directorio que aura.cfg/sync_summary.cfg/photo_categories.cfg --
 * sin header compartido para esto en el proyecto (precedente ya
 * establecido, ver aura_media_categories.c). */
#define AURA_DIR            ROCKBOX_DIR "/aura"
#define ARTIST_IMAGES_PATH  AURA_DIR "/artist_images.cfg"
#define ARTISTS_DIR         AURA_DIR "/artists"

/* Contrato §D.3: nombre de artista <= 64 bytes, nombre de archivo
 * <= 128 bytes, hasta 300 entradas (mismos topes que
 * AURA_MUSIC_ITEM_LEN/MAX_ITEMS -- una linea que exceda cualquiera de
 * los dos, o que haga que el indice supere las 300 entradas, se
 * ignora: compatibilidad hacia adelante, mismo criterio que
 * aura_media_categories.c). */
#define ARTIST_NAME_LEN  64
#define ARTIST_FILE_LEN  128
#define MAX_ENTRIES      300

typedef struct {
    char artist[ARTIST_NAME_LEN];
    char file[ARTIST_FILE_LEN];
} artist_entry_t;

static artist_entry_t s_entries[MAX_ENTRIES];
static int s_count = 0;
static bool s_loaded = false;

/* Mismo patron que ensure_video_categories()/ensure_photo_categories()
 * (aura_media_categories.c): open()+read_line(), `s_loaded=true` ANTES
 * de intentar abrir para que un archivo ausente no reintente en cada
 * consulta -- se comporta identico a un indice vacio. */
static void ensure_loaded(void)
{
    int fd;
    char line[ARTIST_NAME_LEN + ARTIST_FILE_LEN + 4];

    if (s_loaded)
        return;
    s_loaded = true;
    s_count = 0;

    fd = open(ARTIST_IMAGES_PATH, O_RDONLY);
    if (fd < 0)
        return;

    while (s_count < MAX_ENTRIES && read_line(fd, line, sizeof(line)) > 0)
    {
        char file[ARTIST_FILE_LEN], artist[ARTIST_NAME_LEN];

        if (!aura_artist_images_parse_line(line, file, sizeof(file), artist, sizeof(artist)))
            continue;

        strlcpy(s_entries[s_count].file, file, ARTIST_FILE_LEN);
        strlcpy(s_entries[s_count].artist, artist, ARTIST_NAME_LEN);
        s_count++;
    }
    close(fd);
}

bool aura_artist_images_lookup(const char *artist_tag, char *path_out, size_t sz)
{
    int i;

    if (!artist_tag || !*artist_tag)
        return false;

    ensure_loaded();
    /* Primera coincidencia gana (contrato §D.3: "valor duplicado -> gana
     * la primera linea") -- un recorrido hacia adelante ya lo cumple
     * solo, sin logica extra. */
    for (i = 0; i < s_count; i++)
    {
        if (!strcmp(s_entries[i].artist, artist_tag))
        {
            snprintf(path_out, sz, "%s/%s", ARTISTS_DIR, s_entries[i].file);
            return true;
        }
    }
    return false;
}

void aura_artist_images_invalidate(void)
{
    s_loaded = false;
}
