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
/* Fotos de artista (D-322/PLAN-biblioteca-medios-v2.md, contrato v6/v7
 * §D.3) -- parte PURA (C99, sin Rockbox, testeable en host) de
 * aura_artist_images.c: parsea UNA linea de artist_images.cfg. Formato
 * INVERTIDO respecto a video_categories.cfg/photo_categories.cfg a
 * proposito (ver aura_media_categories.c): "archivo.jpg: nombre de
 * artista" -- el nombre de archivo va primero porque es FAT-seguro
 * (nunca trae ':'); el nombre de artista, que si puede traerlo (p.ej.
 * "Panic! At The Disco: Live"), va como valor.
 *
 * No reusa settings_parseline() (apps/misc.c) -- esa funcion vive en el
 * arbol real de Rockbox y no es enlazable en un test de host sin
 * arrastrar el resto de esa dependencia. Esta reimplementacion replica
 * su mismo contrato (separa en el PRIMER ':', un '#' inicial descarta la
 * linea entera, recorta espacios de ambos extremos) para el caso
 * particular de este archivo. */
#ifndef AURA_ARTIST_IMAGES_PARSE_H
#define AURA_ARTIST_IMAGES_PARSE_H

#include <stdbool.h>
#include <stddef.h>

/* Parsea `line` ("archivo.jpg: Nombre De Artista") en `file_out`/
 * `artist_out` (buffers de al menos `fsz`/`asz` bytes, siempre
 * NUL-terminados al volver, incluso si la funcion devuelve false).
 * `false` si la linea esta vacia, empieza con '#' (tras recortar
 * espacios iniciales), no trae ':', o el nombre de archivo o el de
 * artista quedan vacios tras recortar espacios. Trunca (sin partir a
 * mitad de una secuencia UTF-8 multibyte no se garantiza aqui -- el
 * contrato limita ambos campos a ASCII/UTF-8 corto, ver CONTRATO-
 * firmware-studio.md §D.3) si el valor real excede el buffer. */
bool aura_artist_images_parse_line(const char *line, char *file_out, size_t fsz,
                                    char *artist_out, size_t asz);

#endif /* AURA_ARTIST_IMAGES_PARSE_H */
