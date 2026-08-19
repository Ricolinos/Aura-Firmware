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
/* Indice OPCIONAL de fotos de artista (D-322/PLAN-biblioteca-medios-v2.md,
 * CONTRATO-firmware-studio.md §D.3) -- calcado de aura_media_categories.c
 * (I/O: `.rockbox/aura/artist_images.cfg`, carga bajo demanda, cacheado
 * hasta invalidar), con dos diferencias de forma reales:
 *
 *  - El formato del ARCHIVO va INVERTIDO ("archivo.jpg: Nombre de
 *    Artista", nunca al reves) -- ver aura_artist_images_parse.h. El
 *    parser de UNA linea vive aparte (C99 puro, host-testeable); este
 *    modulo solo hace I/O y la busqueda lineal.
 *  - Varias LINEAS pueden apuntar al mismo archivo (un artista con
 *    variantes de tag: "The 1975" / "The 1975 feat. X"), asi que la
 *    busqueda es por ARTISTA (valor), no por archivo (clave) -- lo
 *    opuesto de aura_media_categories_video_lookup().
 *
 * Ausencia total del archivo/directorio es un caso SOPORTADO: toda
 * consulta devuelve false, el llamador cae al placeholder circular. */
#ifndef AURA_ARTIST_IMAGES_H
#define AURA_ARTIST_IMAGES_H

#include <stdbool.h>
#include <stddef.h>

/* Ruta completa (`.rockbox/aura/artists/<archivo>.jpg`) de la foto del
 * artista `artist_tag` (string UTF-8 EXACTO del tag `artist` de
 * tagcache -- comparacion `strcmp`, sin normalizar, ver D-322) en
 * `path_out` (al menos `sz` bytes). Carga el indice bajo demanda en la
 * primera consulta, cacheado hasta `aura_artist_images_invalidate()`.
 * `false` si el indice no existe, `artist_tag` no aparece en ninguna
 * linea, o el indice esta vacio. */
bool aura_artist_images_lookup(const char *artist_tag, char *path_out, size_t sz);

/* Fuerza un re-escaneo la proxima vez que se consulte -- mismos puntos
 * de llamada que aura_media_categories_invalidate() (fin de un sync de
 * musica, entrada a la seccion Artistas). */
void aura_artist_images_invalidate(void);

#endif /* AURA_ARTIST_IMAGES_H */
