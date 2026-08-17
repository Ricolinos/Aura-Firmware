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

void aura_photos_draw(aura_nav_t *nav);
void aura_photos_handle_button(aura_nav_t *nav, long button);

void aura_photo_viewer_draw(aura_nav_t *nav);
void aura_photo_viewer_handle_button(aura_nav_t *nav, long button);

#endif /* AURA_PHOTOS_H */
