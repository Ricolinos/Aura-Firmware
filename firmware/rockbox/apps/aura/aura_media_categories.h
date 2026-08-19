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
/* Indice OPCIONAL de categoria por archivo para /Videos y /Photos
 * (encargo del dueno del producto, 2026-08-18 -- ver DECISIONS.md D-316,
 * CONTRATO-firmware-studio.md SS-CAT).
 *
 * `/Videos` y `/Photos` en el dispositivo son y siguen siendo PLANOS a
 * proposito (D-192, DECISIONS-ARCHIVE.md: "la categoria es SOLO
 * organizacion dentro de Aura Studio, nunca cambia donde se sincroniza
 * en el iPod") -- este modulo no reorganiza nada, solo lee un indice
 * OPCIONAL, generado por Aura Studio (que ya calcula esta categoria por
 * archivo para su propio catalogo local, biblioteca.json, campo
 * `category`) junto a los datos propios de Aura, con el mismo formato
 * `nombre: valor` por linea que ya usa sync_summary.cfg (settings_
 * parseline() + read_line(), aura_manifest.c) -- ninguna dependencia de
 * un parser JSON real.
 *
 * Ausencia total del archivo (Studio todavia no lo escribe) es un caso
 * SOPORTADO, no un error: toda consulta de categoria devuelve "sin
 * categoria" y las pantallas que filtran por categoria simplemente se
 * ven vacias -- mismo mensaje que "todavia no hay contenido de este
 * tipo", degradacion honesta sin ningun estado especial que mantener. */
#ifndef AURA_MEDIA_CATEGORIES_H
#define AURA_MEDIA_CATEGORIES_H

typedef enum {
    AURA_VIDEO_CAT_NONE = 0, /* sin entrada en el indice (o indice ausente) */
    AURA_VIDEO_CAT_MOVIE,
    AURA_VIDEO_CAT_SERIES,
    AURA_VIDEO_CAT_CLIP,
} aura_video_cat_t;

typedef enum {
    AURA_PHOTO_CAT_NONE = 0,
    AURA_PHOTO_CAT_PHOTO,
    AURA_PHOTO_CAT_IMAGE,
    AURA_PHOTO_CAT_AI,
} aura_photo_cat_t;

/* Categoria del archivo `filename` (nombre exacto tal como aparece en
 * /Videos o /Photos, con extension) segun el indice cargado -- carga
 * bajo demanda en la primera consulta, cacheado hasta la proxima
 * invalidacion. AURA_*_CAT_NONE si el archivo no aparece en el indice o
 * el indice no existe. */
aura_video_cat_t aura_media_categories_video_lookup(const char *filename);
aura_photo_cat_t aura_media_categories_photo_lookup(const char *filename);

/* Fuerza un re-escaneo la proxima vez que se consulte una categoria --
 * mismo criterio y mismos puntos de llamada que aura_video_invalidate()/
 * aura_photos_invalidate() (D-291): un sync por USB durante la sesion no
 * debe requerir reiniciar para que el indice nuevo se refleje. */
void aura_media_categories_invalidate(void);

#endif /* AURA_MEDIA_CATEGORIES_H */
