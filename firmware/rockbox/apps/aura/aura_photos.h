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
/* Fotos: lista de imagenes en /Photos (JPG y BMP se decodifican; PNG y
 * GIF se listan pero muestran "formato no soportado" -- sus decoders
 * solo existen dentro del plugin imageviewer, no como funcion de app
 * reutilizable; portarlos queda fuera de alcance de esta fase, ver
 * D-028 en DECISIONS.md) y un visor de pantalla completa. */
#ifndef AURA_PHOTOS_H
#define AURA_PHOTOS_H

#include "aura_nav.h"
#include "aura_media_categories.h"

/* D-316: `screen` decide el filtro de categoria -- AURA_SCREEN_PHOTOS_
 * PHOTO/IMAGE/AI filtran por la categoria correspondiente del indice
 * OPCIONAL de Aura Studio (aura_media_categories.h); cualquier otra
 * pantalla (AURA_SCREEN_PHOTOS_ALL) sigue mostrando la lista completa
 * sin filtrar, CERO cambio de comportamiento respecto a antes de D-316. */
void aura_photos_draw(aura_nav_t *nav, aura_screen_id_t screen);
void aura_photos_handle_button(aura_nav_t *nav, aura_screen_id_t screen, long button);

void aura_photo_viewer_draw(aura_nav_t *nav);
void aura_photo_viewer_handle_button(aura_nav_t *nav, long button);

/* Fuerza un re-escaneo de /Photos la proxima vez que se dibuje la
 * lista -- sin esto, un sync por USB durante la sesion no se refleja
 * hasta reiniciar (D-291). Se llama al entrar a la pantalla desde el
 * menu, no en cada frame. */
void aura_photos_invalidate(void);

/* Cuenta real de imagenes listables en /Photos (asegura el escaneo si
 * hace falta) -- reemplaza a sync_summary.cfg como fuente del estado
 * vacio del panel derecho del menu (D-291), que podia desincronizarse
 * de lo que en verdad hay en el disco. */
int aura_photos_count(void);

/* D-316: cuenta/nombre de archivo de la foto filtrada #index dentro de
 * la categoria `cat` -- usado por CoverDrift (aura_screens.c) para
 * construir su pool de fotos sin duplicar el escaneo/almacenamiento de
 * este modulo. `cat` = AURA_PHOTO_CAT_NONE devuelve la lista completa
 * sin filtrar. `aura_photos_filtered_filename()` devuelve NULL fuera de
 * rango. */
int aura_photos_count_filtered(aura_photo_cat_t cat);
const char *aura_photos_filtered_filename(aura_photo_cat_t cat, int index);

#endif /* AURA_PHOTOS_H */
