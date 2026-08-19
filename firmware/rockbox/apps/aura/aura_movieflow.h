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
/* Movie Flow (D-318, encargo del dueno 2026-08-18): "copia casi exacta"
 * de Music Flow (aura_musicflow.c) para la seccion de Video -- mismo
 * motor de proyeccion por columnas (aura_flow.c), mismo mecanismo de
 * flip, pero:
 *
 * - Formato de tapa RECTANGULAR 3:4 (MVF_COVER_W x MVF_COVER_H), no
 *   cuadrado -- proporcion de cartel de pelicula, no de caratula de
 *   album.
 * - Fuente de datos propia: NO tagcache -- el pool combinado de
 *   Peliculas + Series (ver drift_pool_t/AURA_DRIFT_POOL_VIDEO_ALL en
 *   aura_screens.c para el precedente de "peliculas+series, nunca
 *   videoclips") que aura_video.c ya filtra por categoria (D-316). Las
 *   series se agrupan por temporada via el nombre de archivo (patron
 *   "SxxEyy", D-318) -- un slide de Movie Flow por PELICULA o por
 *   TEMPORADA, nunca por episodio individual.
 * - Una PELICULA se reproduce al instante con SELECT -- sin el flip a
 *   lista (no hay nada que listar). Una TEMPORADA si flipea, mostrando
 *   sus episodios como lista de texto -- MISMO panel/geometria que el
 *   reverso de Music Flow (reusa el patron, no un componente nuevo,
 *   decision del dueno).
 * - La entrada al reproductor NUNCA es Flip-and-Flow -- es un fundido a
 *   negro simple y despues plugin_load(mpegplayer) directo (mismo
 *   mecanismo que ya usa aura_video.c, no la pantalla NowPlaying de
 *   Musica).
 */
#ifndef AURA_MOVIEFLOW_H
#define AURA_MOVIEFLOW_H

#include "aura_nav.h"

void aura_movieflow_draw(aura_nav_t *nav, aura_screen_id_t screen);
void aura_movieflow_handle_button(aura_nav_t *nav, aura_screen_id_t screen, long button);

/* Mismo criterio que aura_musicflow_pending()/_animating() -- movimiento
 * continuo mientras "scrolling", pending()==animating(). */
int aura_movieflow_pending(void);
int aura_movieflow_animating(void);

/* Fuerza un re-escaneo de /Videos (peliculas+temporadas) la proxima vez
 * que se dibuje -- mismo criterio que aura_video_invalidate() (D-291),
 * llamado en los mismos puntos. */
void aura_movieflow_invalidate(void);

#endif /* AURA_MOVIEFLOW_H */
