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
/* Mezcla de color en espacio RGB 0-255 (PLAN.md T0.3). Modulo puro en
 * C99, sin dependencias de Rockbox -- mismo criterio que aura_motion.c/
 * aura_wheel.c/aura_flow.c: compila igual en el host (apps/aura/test/)
 * que en el firmware. No conoce LCD_RGBPACK ni el formato de pixel del
 * target -- el llamador convierte antes/despues.
 *
 * Consumidor inicial: los dos colores derivados del acento configurable
 * (fundamentos/01-color.md, componentes/selection-summary.md) -- ver
 * aura_accent_light()/aura_accent_dark() en apple2026_shell.h. Tambien
 * sirve para cualquier simulacion de opacidad contra un fondo conocido
 * (mismo principio que a26_shell_blend() del sistema Apple2026, aca en
 * RGB 0-255 en vez de empaquetado nativo, para poder testearse aqui).
 */
#ifndef AURA_COLOR_H
#define AURA_COLOR_H

typedef struct {
    int r, g, b; /* cada uno 0-255 */
} aura_rgb_t;

/* Desempaqueta/empaqueta 0xRRGGBB de 24 bits -- el formato de
 * persistencia (aura.cfg) y de los tokens hex de tokens.json, NO el
 * formato nativo del LCD (que es RGB565 empaquetado via LCD_RGBPACK y
 * varia por target). */
aura_rgb_t aura_color_from_rgb24(unsigned rgb24);
unsigned aura_color_to_rgb24(aura_rgb_t c);

/* Mezcla `c` hacia `toward` en `pct` por ciento (clampado a [0,100]).
 * pct=0 -> c sin cambios; pct=100 -> toward. Interpolacion lineal entera
 * por canal, redondeo al mas cercano. */
aura_rgb_t aura_color_blend_pct(aura_rgb_t c, aura_rgb_t toward, int pct);

#endif /* AURA_COLOR_H */
