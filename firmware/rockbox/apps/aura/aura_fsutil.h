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
/* Utilidades de disco compartidas por los modulos de Aura que escriben en
 * el iPod (temas, cache de caratulas, marcador de sincronizacion). Nada
 * de aqui pasa por apps/fileop.c: esa ruta esta atada al navegador de
 * archivos de Rockbox y pide confirmacion con SU PROPIO dialogo, cromo
 * que Aura no usa nunca (CLAUDE.md). Solo remove()/rmdir()/opendir()/
 * readdir() -- sin UI. */
#ifndef AURA_FSUTIL_H
#define AURA_FSUTIL_H

#include <stdbool.h>
#include <stddef.h>

/* Borra un arbol completo (archivos y subdirectorios) y al final el
 * propio directorio. true si TODO se pudo borrar. false si el
 * directorio no existe. */
bool aura_fsutil_remove_tree(const char *path);

/* Borra solo el CONTENIDO de un directorio (recursivo), conservando el
 * directorio en si. true si todo se pudo borrar; false tambien si el
 * directorio no existe (no crea nada). */
bool aura_fsutil_clear_dir(const char *path);

/* Lee un archivo de texto completo en `buf` (NUL-terminado). Devuelve
 * los bytes leidos, -1 si no se pudo abrir, o -2 si no cabe (el
 * contenido se descarta: un archivo mas grande que el buffer no es el
 * que esperabamos). */
int aura_fsutil_read_text(const char *path, char *buf, size_t bufsize);

/* Escribe `len` bytes en `path` (crea/trunca). true si se escribio todo. */
bool aura_fsutil_write_all(const char *path, const char *data, size_t len);

#endif /* AURA_FSUTIL_H */
