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
#ifndef AURA_SWITCH_WAIT_H
#define AURA_SWITCH_WAIT_H

#include <stdbool.h>

/*
 * Aura (D-342): predicado puro para la espera de un commit de tagcache en
 * vuelo antes de que aura_firmware_switch_to() apague tagcache y reinicie.
 * Deliberadamente libre de encabezados de Rockbox (kernel.h: current_tick,
 * HZ, sleep) para poder probarse en host desde apps/aura/test. Ver
 * DECISIONS.md D-342 para el diagnostico completo.
 *
 * commit_step: valor de tagcache_get_commit_step() (0 == inactivo, sin
 * escritura de indice en curso -- seguro apagar tagcache y reiniciar).
 *
 * now / deadline: valores de tick comparados exactamente como
 * TIME_BEFORE(now, deadline) en system.h, es decir
 * (long)(now) - (long)(deadline) < 0. Acepta el mismo wraparound con signo
 * que current_tick.
 *
 * Devuelve true si aun hay que seguir esperando (commit en curso y no se ha
 * cumplido el tope); false si ya es seguro proceder (commit terminado o
 * tope agotado).
 */
bool aura_switch_wait_for_commit(int commit_step, long now, long deadline);

#endif /* AURA_SWITCH_WAIT_H */
