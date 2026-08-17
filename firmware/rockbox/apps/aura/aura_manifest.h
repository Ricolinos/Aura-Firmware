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
/* Fase 24 (PLAN-UX.md): contadores/bytes por tipo que Aura Studio deja
 * en el dispositivo en cada sync, para que "Acerca de" pueda mostrar
 * cuanto hay realmente sincronizado. Ver aura_manifest.c y
 * CatalogSummaryWriter (Aura Studio, Services/CatalogSummary.swift).
 */
#ifndef AURA_MANIFEST_H
#define AURA_MANIFEST_H

#include <stdbool.h>

typedef struct {
    int music_count;
    long long music_bytes;
    int video_count;
    long long video_bytes;
    int photo_count;
    long long photo_bytes;
    int playlist_count;
    /* D-283 (PLAN-about-fixes.md E2/Q6): conteos por categoria dentro de
     * video/foto, para el Estado 2 de "Acerca de" -- Aura Studio ya
     * clasifica cada item al importar (Rockbox no tiene base de datos
     * de video ni parser EXIF, no puede hacerlo por si solo). Un
     * manifiesto escrito ANTES de esta sesion nunca tuvo estas lineas --
     * has_video_categories/has_photo_categories distinguen eso de
     * "el conteo real es cero", para que el firmware pueda mostrar
     * "sincroniza para ver el detalle" en vez de un 0 enganoso. */
    int video_movies_count;
    int video_series_count;
    int video_clips_count;
    bool has_video_categories;
    int photo_images_count;
    int photo_photos_count;
    int photo_ai_count;
    bool has_photo_categories;
} aura_manifest_t;

/* Lee el resumen que Aura Studio escribe en cada sync a *out. Devuelve
 * false si el archivo todavia no existe (el dispositivo nunca se
 * sincronizo desde Studio) -- *out queda en cero en ese caso, nunca sin
 * inicializar. */
bool aura_manifest_load(aura_manifest_t *out);

#endif /* AURA_MANIFEST_H */
