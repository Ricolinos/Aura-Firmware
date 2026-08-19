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

#include "aura_artist_images_parse.h"

static const char *skip_ws(const char *p)
{
    while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')
        p++;
    return p;
}

bool aura_artist_images_parse_line(const char *line, char *file_out, size_t fsz,
                                    char *artist_out, size_t asz)
{
    const char *p, *colon;
    const char *file_start, *file_end;
    const char *artist_start, *artist_end;
    size_t file_len, artist_len;

    if (!file_out || fsz == 0 || !artist_out || asz == 0)
        return false;
    file_out[0] = '\0';
    artist_out[0] = '\0';
    if (!line)
        return false;

    p = skip_ws(line);
    if (*p == '\0' || *p == '#')
        return false;

    colon = strchr(p, ':');
    if (!colon)
        return false;

    /* Nombre de archivo: entre `p` y `colon`, recortado por la derecha
     * (la izquierda ya la recorto skip_ws de arriba). */
    file_start = p;
    file_end = colon;
    while (file_end > file_start && (file_end[-1] == ' ' || file_end[-1] == '\t'))
        file_end--;
    if (file_end == file_start)
        return false;

    /* Nombre de artista: resto de la linea tras ':', recortado en ambos
     * extremos -- puede traer ':' propio (no se vuelve a buscar). */
    artist_start = skip_ws(colon + 1);
    artist_end = artist_start + strlen(artist_start);
    while (artist_end > artist_start
           && (artist_end[-1] == ' ' || artist_end[-1] == '\t'
               || artist_end[-1] == '\r' || artist_end[-1] == '\n'))
        artist_end--;
    if (artist_end == artist_start)
        return false;

    file_len = (size_t)(file_end - file_start);
    if (file_len >= fsz)
        file_len = fsz - 1;
    memcpy(file_out, file_start, file_len);
    file_out[file_len] = '\0';

    artist_len = (size_t)(artist_end - artist_start);
    if (artist_len >= asz)
        artist_len = asz - 1;
    memcpy(artist_out, artist_start, artist_len);
    artist_out[artist_len] = '\0';

    return true;
}
