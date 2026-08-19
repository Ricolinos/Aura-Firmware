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
/* Carga de caratulas por album (no por pista actual -- eso lo resuelve
 * aura_nowplaying.c solo), con cache en disco `.pfraw` (PLAN.md
 * T3.2(a), componentes/music-flow.md) -- unico consumidor real:
 * Music Flow (aura_musicflow.c).
 *
 * El bitmap queda TRANSPUESTO en memoria (columna contigua, no fila)
 * con las esquinas ya redondeadas horneadas -- mismo formato conceptual
 * que el cache de apps/plugins/pictureflow/pictureflow.c (regla dura 7:
 * extender, no reimplementar), personalizado con el enmascarado de
 * esquinas que pictureflow.c no tiene (ver aura_albumart.c). El
 * consumidor (draw_slide_perspective en aura_musicflow.c) lee este
 * layout directo -- ningun otro modulo debe asumir fila-contigua aqui.
 */
#ifndef AURA_ALBUMART_H
#define AURA_ALBUMART_H

#include <stdbool.h>
#include <stdint.h>

#include "lcd.h" /* fb_data */

typedef struct {
    int size;
    int radius;                     /* esquina redondeada a hornear/verificar contra el cache */
    unsigned char *cover_data;      /* size*size, TRANSPUESTO, formato nativo del LCD */
    unsigned char *reflection_data; /* size*aura_art_reflection_height(size,...), tambien transpuesto */
    bool valid;
} aura_albumart_t;

/* Busca una pista cualquiera del album identificado por `album_seek`
 * (el result_seek de tag_album que ya se tenga de una busqueda previa,
 * ver aura_music.h) y le busca caratula con la misma logica que usa
 * Ahora Suena (find_albumart).
 *
 * Primero intenta el cache `.pfraw` en disco (clave: album_seek + size
 * + radius -- cambiar cualquiera de los dos invalida el cache de ese
 * album sin tocar los demas). Si no existe, decodifica el JPEG/BMP
 * real, transpone, hornea las esquinas y ESCRIBE el cache para la
 * proxima vez -- "cero decodificacion JPEG durante la animacion"
 * (doc) solo aplica una vez cada caratula ya paso por aqui.
 *
 * Devuelve false (out->valid queda en false) si no hay caratula o no
 * hay ninguna pista en ese album. El llamador es dueno de la memoria:
 * antes de llamar debe fijar out->size, out->radius y
 * out->cover_data/out->reflection_data apuntando a buffers de al menos
 * size*size*FB_DATA_SZ y size*aura_art_reflection_height(size,...)*FB_DATA_SZ
 * bytes respectivamente. */
bool aura_albumart_load_for_album(int32_t album_seek, aura_albumart_t *out);

/* Caratula "Default" para un album sin arte real (imagen de referencia
 * del dueno del diseno: nota musical gris sobre tile gris claro plano).
 * Mismo formato (transpuesto, esquinas horneadas al `out->radius` ya
 * fijado, reflejo generado) que una caratula real -- el consumidor
 * (draw_slide_perspective) no necesita distinguir entre los dos casos.
 * `out->valid` siempre queda en true. */
void aura_albumart_load_default(aura_albumart_t *out);

/* Solo el tile default (fondo gris + nota), sin esquinas ni reflejo --
 * para consumidores con su propio pipeline de enmascarado/reflejo
 * (Ahora suena usa layout fila-contigua, `transposed`=false). */
void aura_albumart_default_tile(fb_data *buf, int size, bool transposed);

/* D-224: chequeo liviano de cache en disco para `album_seek` a
 * `size`/`radius` -- valida el header .pfraw (mismo criterio que el
 * primer paso de aura_albumart_load_for_album()) SIN leer el payload de
 * pixeles ni generar reflejo. Usado por el paso de precarga
 * (aura_music.c) para saltar rapido, en un arranque posterior, los
 * albumes que ya pasaron por aqui -- aura_albumart_load_for_album()
 * seguiria siendo correcto sin este chequeo (el acierto de cache ya es
 * su camino rapido), esto solo evita tambien el open+read+reflejo de
 * cada uno cuando la precarga entera ya esta al dia. */
bool aura_albumart_is_cached(int32_t album_seek, int size, int radius);

/* Portada de PLAYLIST (encargo del dueno, 2026-08-14: "quiero que las
 * playlists tengan una imagen... la lista deberia verse como la lista
 * de albumes"). A diferencia de aura_albumart_load_for_album(), no hay
 * ningun seek de tagcache -- las playlists son archivos `.m3u8` en el
 * directorio de catalogo (catalog_get_directory(), playlist_catalog.h),
 * no entradas de la base de datos. `playlist_filename` es el nombre de
 * archivo CRUDO con extension tal cual lo devuelve
 * aura_music_list_playlists() (p.ej. "Roadtrip 2026.m3u8"); esta
 * funcion busca el sidecar "<mismo nombre sin extension>.jpg" en ese
 * mismo directorio -- Aura Studio siempre lo deja ahi (imagen elegida
 * por el usuario, o un colage/tile default generado si no eligio
 * ninguna, LibrarySync.swift). Mismo cache `.pfraw` en disco que
 * aura_albumart_load_for_album() (clave: el nombre de playlist en vez
 * de un album_seek), mismo formato de salida (transpuesto, esquinas
 * horneadas, reflejo) -- el llamador no necesita distinguir entre los
 * dos casos. Devuelve false (out->valid en false) si no hay sidecar;
 * el llamador cae a aura_albumart_load_default() como con cualquier
 * album sin caratula. */
bool aura_playlist_art_load(const char *playlist_filename, aura_albumart_t *out);

#endif /* AURA_ALBUMART_H */
