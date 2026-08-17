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
/* Utilidades de arte compartidas (Fase 29, PLAN-APPLE2026.md, doc de
 * diseno SS5.4): reflejo precalculado parametrico, hoy usado por Cover
 * Flow (aura_coverflow.c) y pensado tambien para Ahora suena (Fase 30).
 * Antes vivia solo dentro de aura_albumart.c, con la fraccion de alto
 * vieja (50%, D-057) -- extraido a un modulo propio para que ambos
 * consumidores compartan la misma implementacion en vez de
 * reimplementarla cada uno. Depende de lcd.h (fb_data, RGB_UNPACK_*):
 * no es un modulo host-testable como aura_nav/motion/wheel, mismo
 * perfil de dependencias que aura_albumart.c.
 */
#ifndef AURA_ART_H
#define AURA_ART_H

#include <stdbool.h>

#include "lcd.h"

/* Alto del reflejo como numerador/100 del lado del cuadrado (doc SS5.4
 * del sistema viejo: "35% alto") -- valor por defecto de los
 * consumidores del sistema Apple2026 viejo (Cover Flow viejo,
 * aura_albumart.c). El sistema nuevo (componentes/now-playing.md,
 * cover-flow.md) pide una proporcion mas sutil (25%,
 * AURA_DS_METRICS_COVER_FLOW_REFLECTION_PCT_OF_SLIDE_HEIGHT, G16) --
 * por eso `height_pct` es un parametro, no una constante fija: cada
 * consumidor pasa la suya en vez de que este modulo elija por todos. */
#define AURA_ART_REFLECTION_HEIGHT_PCT 35

/* Opacidad maxima del reflejo (primera fila, pegada a la caratula) --
 * 40-50% pedido por el dueno del diseno (2026-08-12); se desvanece
 * linealmente a 0% hacia abajo. Compartida por Cover Flow y Ahora
 * suena: mismo material, mismo reflejo. */
#define AURA_ART_REFLECTION_PEAK_PCT 45

int aura_art_reflection_height(int size, int height_pct);

/* Genera el reflejo de `cover` (size x size, fb_data, formato nativo
 * del LCD) en `out` (size x aura_art_reflection_height(size, height_pct),
 * reservado por el llamador -- Aura no hace malloc). Espejo vertical de
 * las primeras filas de la caratula, atenuado hacia `bg_color` a medida
 * que se aleja (mascara height_pct%->0%).
 *
 * `transposed`=false: layout normal, fila contigua (cover[y*size+x],
 * out[y*size+x]) -- consumidores existentes (aura_albumart.c "flat",
 * aura_nowplaying.c). `transposed`=true: layout columna contigua
 * (cover[x*size+y], out[x*refl_h+y]) -- cache .pfraw de Cover Flow
 * (T3.2, componentes/cover-flow.md), para que el render por columnas
 * de aura_flow.c lea memoria contigua sin necesidad de transponer un
 * bitmap ya generado en el layout equivocado. */
void aura_art_generate_reflection(const fb_data *cover, fb_data *out,
                                   int size, int height_pct, unsigned bg_color,
                                   bool transposed);

#endif /* AURA_ART_H */
