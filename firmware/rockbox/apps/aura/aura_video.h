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
/* Videos: lista de archivos MPEG-1/2 en /Videos (el unico formato que
 * reproduce el dispositivo -- lo genera Aura Studio al sincronizar,
 * ver PLAN.md) y reproduccion delegada al plugin mpegplayer del fork
 * (D-029 en DECISIONS.md: portar un decoder de video propio queda
 * fuera de alcance). */
#ifndef AURA_VIDEO_H
#define AURA_VIDEO_H

#include "aura_nav.h"
#include "aura_media_categories.h"

/* D-316: `screen` decide el filtro de categoria -- AURA_SCREEN_VIDEOS_
 * MOVIES/TVSHOWS/CLIPS filtran por la categoria correspondiente del
 * indice OPCIONAL de Aura Studio (aura_media_categories.h); cualquier
 * otra pantalla (AURA_SCREEN_VIDEOS_ALL) sigue mostrando la lista
 * completa sin filtrar, CERO cambio de comportamiento respecto a antes
 * de D-316. */
void aura_video_draw(aura_nav_t *nav, aura_screen_id_t screen);
void aura_video_handle_button(aura_nav_t *nav, aura_screen_id_t screen, long button);

/* Mismo bug hermano que aura_photos_invalidate() (D-291): sin esto,
 * un sync por USB durante la sesion no se refleja hasta reiniciar. */
void aura_video_invalidate(void);

/* Cuenta real de videos en /Videos (asegura el escaneo si hace falta),
 * mismo criterio que aura_photos_count() -- reemplaza a
 * sync_summary.cfg como fuente del estado vacio de "Todos los
 * videos". */
int aura_video_count(void);

/* D-316: cuenta/nombre de archivo del video filtrado #index dentro de
 * la categoria `cat` -- usado por CoverDrift (aura_screens.c) para
 * construir su pool de video sin duplicar el escaneo/almacenamiento de
 * este modulo. `cat` = AURA_VIDEO_CAT_NONE devuelve la lista completa
 * sin filtrar (mismo criterio que aura_video_draw()/_handle_button()).
 * `aura_video_filtered_filename()` devuelve NULL fuera de rango. */
int aura_video_count_filtered(aura_video_cat_t cat);
const char *aura_video_filtered_filename(aura_video_cat_t cat, int index);

#endif /* AURA_VIDEO_H */
