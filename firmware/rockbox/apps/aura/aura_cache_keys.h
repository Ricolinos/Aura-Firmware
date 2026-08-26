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
/* D-337/D-338 (contrato v15): logica PURA de las claves de cache que
 * comparten los tres firmwares -- sin I/O, sin Rockbox, para el test
 * host test_cache_keys.c.
 *
 *  - Que archivos del arbol son la base tagcache (database_*.tcd) y
 *    viajan al directorio compartido AURA_SHARED_DB_DIR (aura_sync.h).
 *  - El sello db_stamp.txt: cuando obliga a reconstruir.
 *  - La clave estable de caratula de album:
 *        a-<crc32 ruta pista>-<mtime pista>-<lado>.pfraw
 *    (nunca el seek de tagcache, que cambia en cada reconstruccion). */
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

/* Forma de un solo argumento del parse, para usar como filtro
 * (aura_fsutil_clear_dir_except()). */
bool aura_cache_keys_album_parse_name(const char *name);

#endif /* AURA_CACHE_KEYS_H */
