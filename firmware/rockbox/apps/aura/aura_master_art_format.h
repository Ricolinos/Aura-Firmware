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
/* D-340/D-341 (contrato v16, "Cache maestra compartida de imagenes"):
 * logica PURA -- sin I/O, sin Rockbox -- del formato maestro que
 * comparten las tres familias (Aura, Metro-Aura, moonlit.aura), para el
 * test host test_master_art.c.
 *
 *  - Nombre de archivo por tipo: a-/r-/p-<crc32 8 hex>.<mtime>.art y el
 *    marcador negativo con la misma clave y extension .none (0 bytes).
 *  - Cabecera de 16 bytes little-endian: magic 'MAST', ancho, alto,
 *    flags = 0, reservado = 0; despues width*height pixeles RGB565 LE
 *    fila-contigua, cuadrada, sin esquinas, sin tema, sin reflejo.
 *  - Derivacion al cargar a RAM: reduccion por caja entera (130 -> 120,
 *    130 -> 48, 80 -> 48), recorte centrado (fill-and-center-crop) y la
 *    caja del segundo decode cuando la fuente no es cuadrada. La
 *    transposicion y las esquinas/circulo siguen en aura_art.c (dependen
 *    de fb_data/tema). */
#ifndef AURA_MASTER_ART_FORMAT_H
#define AURA_MASTER_ART_FORMAT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "aura_cache_keys.h" /* aura_cache_album_key_t: (crc, mtime) */

#define AURA_MASTER_ART_MAGIC        0x5453414Du /* 'MAST' visto LE: 4D 41 53 54 */
#define AURA_MASTER_ART_HEADER_SIZE  16

/* Lados canonicos del contrato v16. */
#define AURA_MASTER_ART_ALBUM_SIZE   130
#define AURA_MASTER_ART_ARTIST_SIZE  130
#define AURA_MASTER_ART_PHOTO_SIZE   80

typedef enum {
    AURA_MASTER_ART_ALBUM  = 'a',
    AURA_MASTER_ART_ARTIST = 'r',
    AURA_MASTER_ART_PHOTO  = 'p',
} aura_master_art_kind_t;

/* Clave estable (crc32 de la ruta del archivo representativo + mtime de
 * ese archivo). Mismo par que la clave de album de D-338. */
typedef aura_cache_album_key_t aura_master_art_key_t;

/* Lado canonico del tipo (130/130/80). */
int aura_master_art_size_for(aura_master_art_kind_t kind);

/* Nombre de archivo (sin directorio): <k>-<crc 8 hex>.<mtime>.art, o
 * .none si `negative`. Caracteres escritos o <0 si no cabe. */
int aura_master_art_name(char *out, size_t outsz, aura_master_art_kind_t kind,
                         const aura_master_art_key_t *key, bool negative);

/* Inverso. Salidas opcionales. Cualquier otro nombre devuelve false. */
bool aura_master_art_parse_name(const char *name, aura_master_art_kind_t *kind,
                                aura_master_art_key_t *key, bool *negative);

/* GC: true si `name` es una entrada (.art o .none) del tipo `kind` cuya
 * clave no esta en `keys`. Nombres ajenos y de otro tipo: false. */
bool aura_master_art_is_orphan(const char *name, aura_master_art_kind_t kind,
                               const aura_master_art_key_t *keys, int count);

/* Cabecera de 16 bytes LE. */
void aura_master_art_header_pack(unsigned char hdr[AURA_MASTER_ART_HEADER_SIZE],
                                 unsigned width, unsigned height);
/* false si magic/flags/reservado no cuadran o el lado es 0. */
bool aura_master_art_header_parse(const unsigned char hdr[AURA_MASTER_ART_HEADER_SIZE],
                                  unsigned *width, unsigned *height);

/* Reduccion por caja (promedio de area, aritmetica entera) de un
 * cuadrado RGB565 fila-contigua src_size -> dst_size (dst_size <=
 * src_size; iguales = copia). Cada pixel destino promedia el bloque
 * [x*src/dst, (x+1)*src/dst) x [y*src/dst, (y+1)*src/dst). */
void aura_master_art_downscale_box(const uint16_t *src, int src_size,
                                   uint16_t *dst, int dst_size);

/* Recorte centrado de size x size sobre un bitmap w x h fila-contigua
 * (w, h >= size). */
void aura_master_art_center_crop(const uint16_t *src, int w, int h,
                                 uint16_t *dst, int size);

/* Fill-and-center-crop en dos decodes: el primero ajusta la imagen
 * DENTRO de size x size (keep-aspect, el decodificador de Rockbox no
 * "llena"), dando `w` x `h` con max(w,h) == size. Si ya es cuadrada no
 * hace falta nada mas (false). Si no, devuelve true y la caja del
 * segundo decode para que el lado MENOR quede en `size` (el mayor crece
 * en proporcion); center_crop() recorta despues. Proporciones mas
 * extremas que max_ratio_x8/8 (p.ej. 4:1 = 32) devuelven false y se
 * deja la version centrada sobre fondo: no vale decodificar un panorama
 * enorme para quedarse con un recorte que no representa nada. */
bool aura_master_art_fill_box(int w, int h, int size, int max_ratio_x8,
                              int *box_w, int *box_h);

/* Compone `src` (w x h, fila-contigua) centrado sobre un tile size x
 * size relleno con `bg` -- para fuentes que no pudieron llenarse. */
void aura_master_art_center_on_tile(const uint16_t *src, int w, int h,
                                    uint16_t *dst, int size, uint16_t bg);

#endif /* AURA_MASTER_ART_FORMAT_H */
