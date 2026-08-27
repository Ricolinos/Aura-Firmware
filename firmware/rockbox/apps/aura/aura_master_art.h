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
/* D-340/D-341 (contrato v16): cache MAESTRA compartida de imagenes bajo
 * /.aura/art/{albums,artists,photos}/ (AURA_SHARED_ART_*_DIR, aura_sync.h).
 *
 * La maestra es la unica fuente que se decodifica de un JPEG/BMP: una
 * sola vez, por el firmware activo, sin tema ni esquinas ni reflejo,
 * cuadrada (fill-and-center-crop), RGB565 LE fila-contigua. Las cachés
 * privadas de cada familia (cfcache y photocache, extension .pfraw, en Aura)
 * pasan a ser L2 regenerable: se DERIVAN de la maestra al cargar a RAM
 * (transponer, reducir por caja, esquinas del tema, reflejo) -- nunca
 * por cuadro. Regla dura: jamas se decodifica JPEG si la maestra existe,
 * y siempre se escribe la maestra cuando se decodifica.
 *
 * Este modulo es la parte con I/O y Rockbox; la logica pura (nombres,
 * cabecera, derivacion) vive en aura_master_art_format.c (test host).
 *
 * Hilos (D-341): el decodificador de Rockbox NO tiene estado global en
 * el build del core (apps/recorder/jpeg_load.c: `struct jpeg` vive
 * DENTRO del buffer que le pasa el llamador; `static struct jpeg jpeg`
 * solo existe bajo JPEG_FROM_MEM, plugins), pero SI cede la CPU a mitad
 * de una imagen (yield() en su bucle de filas y en resize.c), asi que
 * lo unico que hay que serializar entre el hilo de UI y el constructor
 * en segundo plano es el SCRATCH de decodificacion que comparten. Ese
 * scratch es de este modulo y solo se toca con el candado tomado
 * (mutex recursivo de Rockbox). */
#ifndef AURA_MASTER_ART_H
#define AURA_MASTER_ART_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "lcd.h" /* fb_data */
#include "aura_master_art_format.h"

/* Clave estable de un archivo de imagen/pista: crc32 de su ruta
 * completa + mtime (tal como lo reporta tagcache o dir_get_info()). */
void aura_master_art_key_from_path(const char *path, uint32_t mtime,
                                   aura_master_art_key_t *key);

/* Candado del scratch de decodificacion (recursivo). Todo camino que
 * decodifique JPEG/BMP al scratch compartido lo toma; los consumidores
 * con buffer propio (Ahora suena, visor de fotos, Movie Flow, CoverDrift)
 * no lo necesitan. */
void aura_master_art_decode_lock(void);
void aura_master_art_decode_unlock(void);
/* Scratch compartido (dimensionado para CoverDrift a 320 px con margen
 * x2, ver .c). Solo valido con el candado tomado. */
unsigned char *aura_master_art_scratch(size_t *size);

/* Estado en disco de la clave para ese tipo. */
bool aura_master_art_present(aura_master_art_kind_t kind, const aura_master_art_key_t *key);
bool aura_master_art_none_present(aura_master_art_kind_t kind, const aura_master_art_key_t *key);
/* present || none: nada que decodificar para esta clave. */
bool aura_master_art_resolved(aura_master_art_kind_t kind, const aura_master_art_key_t *key);

/* Lee la maestra a `out` (size_for(kind)^2 pixeles, fila-contigua).
 * false si no existe, la cabecera no cuadra con el tipo, o el archivo
 * esta truncado (se trata como ausente: el constructor la reescribe). */
bool aura_master_art_read(aura_master_art_kind_t kind, const aura_master_art_key_t *key,
                          fb_data *out);
/* Escribe la maestra desde `flat` (size_for(kind)^2) y borra el .none de
 * esa clave si sobrevivio uno. */
bool aura_master_art_write(aura_master_art_kind_t kind, const aura_master_art_key_t *key,
                           const fb_data *flat);
/* Marcador negativo compartido (0 bytes): la fuente no tiene imagen
 * resoluble. Solo por un veredicto definitivo, nunca por un fallo
 * transitorio de disco. */
void aura_master_art_write_none(aura_master_art_kind_t kind, const aura_master_art_key_t *key);
void aura_master_art_remove_none(aura_master_art_kind_t kind, const aura_master_art_key_t *key);

/* GC con presupuesto sobre el subdirectorio del tipo: borra .art/.none
 * cuya clave no esta en `keys` (fuentes que se fueron, archivos
 * reescritos por un sync). Nunca borra una clave viva. */
#define AURA_MASTER_ART_GC_BUDGET 64
void aura_master_art_gc(aura_master_art_kind_t kind, const aura_master_art_key_t *keys, int count);

/* Decodifica `path` (JPEG o .bmp; con clip_len > 0, el JPEG embebido en
 * [clip_off, clip_off+clip_len) de ese archivo) a un cuadrado size x
 * size fila-contigua en `out_flat` con fill-and-center-crop (dos
 * decodes si la fuente no es cuadrada; con proporcion mas extrema que
 * 4:1, o si el segundo decode falla, centra la version ajustada sobre
 * el color promedio de la propia imagen -- sin tema). Toma el candado.
 * false si el decodificador rechaza el archivo o no puede abrirlo. */
bool aura_master_art_decode_fill(const char *path, int clip_off, unsigned long clip_len,
                                 int size, fb_data *out_flat);

#endif /* AURA_MASTER_ART_H */
