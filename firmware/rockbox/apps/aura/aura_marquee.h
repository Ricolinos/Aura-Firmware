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
/* MarqueeText (PLAN.md T2.1, componentes/marquee-text.md): texto
 * reutilizable que implementa el patron Marquee Loop
 * (transiciones/00-vocabulario.md) -- se activa SOLO si el texto no
 * cabe en el ancho disponible; si cabe, se dibuja estatico, sin ningun
 * comportamiento de este modulo. Usado por SelectionSummary (T2.8) y
 * DynamicTitle (T2.6).
 */
#ifndef AURA_MARQUEE_H
#define AURA_MARQUEE_H

/* Dibuja `text` en (x, y), acotado a `max_width` px. Usa la fuente y el
 * color de foreground YA configurados por el llamador (lcd_setfont/
 * lcd_set_foreground) -- este modulo no los toca. `elapsed_ms` es el
 * tiempo transcurrido desde que ESTE texto empezo a mostrarse (el
 * llamador lo calcula y lo reinicia cuando el texto cambia -- mismo
 * patron que el resto del sistema, esta funcion es sin estado propio).
 *
 * Simplificacion documentada (D-091): el documento pide un difuminado
 * de 4px en cada borde durante el loop -- lograrlo de verdad exige
 * compositar alfa por pixel contra el texto ya renderizado, y este LCD
 * no lo expone sin una capa de renderizado a buffer offscreen dedicada
 * (fuera de alcance de este componente). El corte en los bordes es
 * limpio (clip duro via viewport), no difuminado. */
/* Devuelve 1 si el texto no cabia y se activo el patron Marquee Loop
 * (el llamador necesita seguir pidiendo cuadros mientras esto sea
 * cierto -- ver aura_pattern_marquee_offset() para saber si el tramo
 * actual es el estatico o el de movimiento), 0 si cupo y se dibujo
 * estatico (nada que animar). */
int aura_marquee_draw(int x, int y, int max_width, const char *text, long elapsed_ms);

#endif /* AURA_MARQUEE_H */
