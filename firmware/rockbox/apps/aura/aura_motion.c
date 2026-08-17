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
#include "aura_motion.h"

int aura_motion_linear(long elapsed, long duration)
{
    if (elapsed <= 0 || duration <= 0)
        return 0;
    if (elapsed >= duration)
        return 256;
    return (int)(elapsed * 256 / duration);
}

/* easeOutBack de Penner (s=1.70158, la constante estandar de la
 * industria para este tipo de curva), 24 pasos, /256. Generada una
 * sola vez fuera de linea (no en el dispositivo: no hay FPU) con:
 *
 *   f(t) = 1 + (s+1)*(t-1)^3 + s*(t-1)^2,  t = i/24
 *
 * y redondeada a punto fijo. El pico (282, ~10% de sobrepaso) cae
 * alrededor del 60% del recorrido -- sobrepaso perceptible pero no "de
 * juguete" (riesgo senalado en PLAN-APPLE2026.md SS3), fiel al resorte
 * de iOS. */
static const int spring_table[25] = {
      0,  47,  89, 126, 158, 186, 209, 229, 245, 257, 267, 274, 278,
    281, 282, 281, 279, 276, 272, 269, 265, 261, 259, 257, 256,
};
#define SPRING_TABLE_STEPS 24 /* indices 0..24 */

int aura_motion_spring(long elapsed, long duration)
{
    long pos, idx, rem;
    int a, b;

    if (elapsed <= 0 || duration <= 0)
        return 0;
    if (elapsed >= duration)
        return 256;

    /* Posicion fraccionaria dentro de la tabla, en 1/256 de paso, para
     * interpolar linealmente entre las dos muestras mas cercanas -- 24
     * muestras alcanzan de sobra para un movimiento de ~380ms a la
     * cadencia de fundidos (20fps son ~7-8 cuadros), pero interpolar
     * evita un escalon perceptible si el llamador pide mas resolucion
     * (p. ej. el simulador a 60fps de host). */
    pos = elapsed * SPRING_TABLE_STEPS * 256 / duration;
    idx = pos / 256;
    rem = pos % 256;

    if (idx >= SPRING_TABLE_STEPS)
        return 256;

    a = spring_table[idx];
    b = spring_table[idx + 1];
    return a + (int)((b - a) * rem / 256);
}

/* t^2: arranca lento, acelera -- sin division salvo la ya hecha dentro
 * de aura_motion_linear() (todo el clamping/casos borde se hereda de
 * ahi: elapsed<=0 o duration<=0 da t=0 => 0; elapsed>=duration da
 * t=256 => 256*256/256=256). */
int aura_motion_ease_in(long elapsed, long duration)
{
    int t = aura_motion_linear(elapsed, duration);
    return (int)((long)t * t / 256);
}

/* 1-(1-t)^2: arranca rapido, desacelera hasta asentarse -- misma
 * herencia de clamping via aura_motion_linear() que ease_in. */
int aura_motion_ease_out(long elapsed, long duration)
{
    int u = 256 - aura_motion_linear(elapsed, duration);
    return (int)(256 - (long)u * u / 256);
}
