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
#include "dir.h"
#include "kernel.h"
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
    uint32_t mtime; /* D-341: del archivo en ARTISTS_DIR, 0 si no estaba */
} artist_entry_t;

static artist_entry_t s_entries[MAX_ENTRIES];
static int s_count = 0;
static bool s_loaded = false;
static bool s_loading = false; /* D-341: ver ensure_loaded() */

/* D-341: una sola pasada de ARTISTS_DIR para anotar el mtime de cada
 * archivo del indice (clave de la maestra compartida). O(n) una vez al
 * cargar, en vez de un escaneo de directorio por fila visible. */
static void annotate_mtimes(void)
{
    DIR *d = opendir(ARTISTS_DIR);
    struct DIRENT *entry;

    if (!d)
        return;
    while ((entry = readdir(d)) != NULL)
    {
        struct dirinfo info;
        int i;

        for (i = 0; i < s_count; i++)
        {
            if (!strcmp(s_entries[i].file, entry->d_name))
            {
                info = dir_get_info(d, entry);
                s_entries[i].mtime = (uint32_t)info.mtime;
            }
        }
    }
    closedir(d);
}

/* Mismo patron que ensure_video_categories()/ensure_photo_categories()
 * (aura_media_categories.c): open()+read_line(), `s_loaded=true` ANTES
 * de intentar abrir para que un archivo ausente no reintente en cada
 * consulta -- se comporta identico a un indice vacio. */
/* D-341: el constructor en segundo plano tambien consulta el indice
 * desde su hilo. El planificador de Rockbox es cooperativo, pero
 * open()/read_line() ceden en el I/O de disco: `s_loading` evita que
 * un segundo llamador vea `s_loaded` y una lista a medias -- espera a
 * que el primero termine. */
static void ensure_loaded(void)
{
    int fd;
    char line[ARTIST_NAME_LEN + ARTIST_FILE_LEN + 4];

    while (s_loading)
        sleep(1);
    if (s_loaded)
        return;
    s_loading = true;
    s_count = 0;

    fd = open(ARTIST_IMAGES_PATH, O_RDONLY);
    if (fd < 0)
    {
        s_loaded = true;
        s_loading = false;
        return;
    }

    while (s_count < MAX_ENTRIES && read_line(fd, line, sizeof(line)) > 0)
    {
        char file[ARTIST_FILE_LEN], artist[ARTIST_NAME_LEN];

        if (!aura_artist_images_parse_line(line, file, sizeof(file), artist, sizeof(artist)))
            continue;

        strlcpy(s_entries[s_count].file, file, ARTIST_FILE_LEN);
        strlcpy(s_entries[s_count].artist, artist, ARTIST_NAME_LEN);
        s_entries[s_count].mtime = 0;
        s_count++;
    }
    close(fd);
    annotate_mtimes();
    s_loaded = true;
    s_loading = false;
}

bool aura_artist_images_lookup_mtime(const char *artist_tag, char *path_out, size_t sz,
                                     uint32_t *mtime_out)
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
            if (mtime_out)
                *mtime_out = s_entries[i].mtime;
            return true;
        }
    }
    return false;
}

bool aura_artist_images_lookup(const char *artist_tag, char *path_out, size_t sz)
{
    return aura_artist_images_lookup_mtime(artist_tag, path_out, sz, NULL);
}

int aura_artist_images_count(void)
{
    ensure_loaded();
    return s_count;
}

bool aura_artist_images_entry(int i, char *path_out, size_t sz, uint32_t *mtime_out)
{
    ensure_loaded();
    if (i < 0 || i >= s_count)
        return false;
    snprintf(path_out, sz, "%s/%s", ARTISTS_DIR, s_entries[i].file);
    if (mtime_out)
        *mtime_out = s_entries[i].mtime;
    return true;
}

void aura_artist_images_invalidate(void)
{
    s_loaded = false;
}
