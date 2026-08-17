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

void aura_video_draw(aura_nav_t *nav);
void aura_video_handle_button(aura_nav_t *nav, long button);

#endif /* AURA_VIDEO_H */
