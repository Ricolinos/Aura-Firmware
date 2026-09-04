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
/* D-337/D-338/D-339 (contrato v15): logica PURA de las claves de cache que
 * comparten los tres firmwares -- sin I/O, sin Rockbox, para el test
 * host test_cache_keys.c.
 *
 *  - Que archivos del arbol son la base tagcache (database_*.tcd) y
 *    viajan al directorio compartido AURA_SHARED_DB_DIR (aura_sync.h).
 *  - El sello db_stamp.txt: cuando obliga a reconstruir.
 *  - La clave estable de caratula de album:
 *        a-<crc32 ruta pista>-<mtime pista>-<lado>.pfraw
 *    (nunca el seek de tagcache, que cambia en cada reconstruccion).
 *  - El marcador negativo a-<crc>-<mtime>.none (D-339) y la decision
 *    del GC de huerfanas sobre ambos. */
#ifndef AURA_CACHE_KEYS_H
#define AURA_CACHE_KEYS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* true para database_*.tcd (idx, N, tmp). No incluye database.ignore ni
 * database_commit.ignore, que no son la base. */
bool aura_cache_keys_is_tagcache_file(const char *name);

/* true si hay que reconstruir: sin sello anotado (NULL/vacio) o distinto
 * al sello vigente de la biblioteca. */
bool aura_cache_keys_stamp_needs_rebuild(const char *recorded, const char *current);

/* Nombre de archivo (sin directorio) de la caratula de album. Devuelve
 * los caracteres escritos, o <0 si no cabe. */
int aura_cache_keys_album_name(char *out, size_t outsz,
                               uint32_t path_crc, uint32_t mtime, int size);

/* Inverso: reconoce un nombre a-<8hex>-<mtime>-<lado>.pfraw. Cualquier
 * otro nombre (pl-*, ar-*, los <seek>-<lado>.pfraw de antes de D-338)
 * devuelve false. Los punteros de salida son opcionales. */
bool aura_cache_keys_album_parse(const char *name, uint32_t *path_crc,
                                 uint32_t *mtime, int *size);

/* D-339: marcador NEGATIVO -- el album no tiene caratula resoluble (ni
 * cover.jpg/folder.jpg, ni JPEG embebido, o el JPEG lo rechazo el
 * decodificador). Archivo de 0 bytes a-<8hex>-<mtime>.none junto a los
 * .pfraw, con la MISMA clave estable de D-338 pero SIN lado: "no hay
 * arte" es un hecho del album, no de un tamano de tile. Mientras exista,
 * ningun consumidor vuelve a buscar ni decodificar; como la clave lleva
 * el mtime de la pista, una pista reescrita por un sync lo deja huerfano
 * (GC) y se reintenta sola. Limitacion documentada (misma hipotesis
 * abierta que D-338): un cover.jpg nuevo SIN tocar la pista no cambia
 * la clave y el .none sobrevive. */
int aura_cache_keys_album_none_name(char *out, size_t outsz,
                                    uint32_t path_crc, uint32_t mtime);

/* Reconoce cualquier entrada de album con clave estable: .pfraw
 * (`negative`=false, `size`=lado) o .none (`negative`=true, `size`=0).
 * Salidas opcionales. */
bool aura_cache_keys_album_parse_any(const char *name, uint32_t *path_crc,
                                     uint32_t *mtime, int *size, bool *negative);

/* Forma de un solo argumento del parse (ambas extensiones), para usar
 * como filtro (aura_fsutil_clear_dir_except()): lo que sobrevive a una
 * reconstruccion es todo lo que lleva clave estable, .none incluido. */
bool aura_cache_keys_album_parse_name(const char *name);

/* Clave estable de un album (D-338). aura_albumart.h la reexporta como
 * aura_albumart_key_t. */
typedef struct {
    uint32_t path_crc;
    uint32_t mtime;
} aura_cache_album_key_t;

/* D-339: "resuelto" para el pre-pase del precache y para el chequeo
 * liviano de cache: hay .pfraw valido O hay marcador .none. */
bool aura_cache_keys_album_resolved(bool pfraw_valid, bool none_present);

/* D-338/D-339: decision pura del GC de cfcache. true si `name` es una
 * entrada de album que ya no corresponde a ninguna clave vigente:
 *  - a-*.pfraw o a-*.none cuya (crc, mtime) no esta en `keys`, o
 *  - <seek>-<lado>.pfraw de antes de D-338 (empieza por digito).
 * pl-*, ar-* y cualquier otro nombre devuelven false: no son de este GC. */
/* D-350 (contrato v18): el <mtime> de la clave de album es
 * max(mtime de la pista representativa, mtime del cover.jpg hermano si
 * existe). Sin esto, una caratula reescrita SIN tocar la pista deja la
 * clave igual y la maestra vieja sobrevive para siempre -- la hipotesis
 * (a) de D-338/M-096/D-055. Pura aritmetica, separada del stat para que
 * la decision se pueda probar en host. */
uint32_t aura_cache_keys_album_mtime(uint32_t track_mtime,
                                     bool cover_present, uint32_t cover_mtime);

/* D-350: "<directorio de la pista>/cover.jpg" a partir de la ruta de la
 * pista. false si no cabe en `out` o si `track_path` no trae directorio
 * (una ruta sin '/' no puede tener un hermano). El nombre es exactamente
 * `cover.jpg`: es lo que el contrato v18 le exige a Studio, y la unica
 * de las variantes que busca find_albumart() que Studio escribe. */
bool aura_cache_keys_sibling_cover(const char *track_path, char *out, size_t outsz);

bool aura_cache_keys_album_is_orphan(const char *name,
                                     const aura_cache_album_key_t *keys, int count);

#endif /* AURA_CACHE_KEYS_H */
