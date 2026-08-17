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
/* Cronometro de Extras (encargo del dueno del diseno 2026-08-13).
 *
 * Replica el del firmware original: contador HH:MM:SS:CC con las
 * centesimas mas chicas, SELECT guarda un registro (los tres ultimos
 * visibles bajo el contador, todos conservados), y PLAY/PAUSA detiene y
 * regresa al menu conservando el tiempo, con un icono de pausa que
 * indica que se puede reanudar.
 */
#ifndef AURA_STOPWATCH_H
#define AURA_STOPWATCH_H

#include <stdbool.h>
#include "aura_nav.h"

void aura_stopwatch_draw(void);
void aura_stopwatch_handle_button(aura_nav_t *nav, long button);
/* true mientras el cronometro esta contando -- la puerta de energia lo
 * consulta para redibujar a la cadencia de las centesimas. */
bool aura_stopwatch_running(void);

#endif /* AURA_STOPWATCH_H */
