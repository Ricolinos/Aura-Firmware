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
/* Niveles de reduccion de Animaciones/Graficos (PLAN-niveles-fx.md,
 * docs/aura-design-system/sistema/06-niveles-de-fx.md): punto unico de
 * consulta para las decisiones que CRUZAN ambos ajustes, en vez de que
 * cada componente repita a mano la misma combinacion.
 *
 * Regla de precedencia (D-a): Graficos decide QUE existe; Animaciones
 * decide COMO se mueve lo que existe. Cuando Graficos elimina un
 * elemento, Animaciones ya no tiene nada que decidir sobre el.
 *
 * Los 24+ sitios PRE-EXISTENTES que ya comparaban aura_settings.
 * animation_mode a mano (aura_transitions.c, aura_nowplaying.c) NO se
 * tocan aca -- son gates/bifurcaciones binarias triviales (NONE/ALL) sin
 * comportamiento nuevo; forzarlos por este header seria un refactor sin
 * cambio de conducta, no una decision de niveles real. Este archivo cubre
 * SOLO los puntos de decision nuevos que la matriz de niveles introduce. */
#ifndef AURA_FX_H
#define AURA_FX_H

#include <stdbool.h>

#include "aura_settings.h"
#include "apple2026_tokens.h"

/* CoverDrift, Graficos=Minimos (D-d): tope de imagenes RESIDENTES sobre
 * el pool general disponible -- 0 en Ninguno (no se decodifica nada, el
 * llamador dibuja el degradado de acento en su lugar), acotado en
 * Minimos, sin tope en Todos (comportamiento actual, el pool completo
 * disponible por rotacion). `pool_count` es el tamano REAL del pool de la
 * biblioteca (usado tambien, sin pasar por aca, para decidir si CoverDrift
 * llega a montarse -- ese umbral de 3 es independiente de Graficos). */
static inline int aura_fx_coverdrift_pool_cap(int pool_count)
{
    if (aura_settings.graphics_mode == AURA_GFX_NONE)
        return 0;
    if (aura_settings.graphics_mode == AURA_GFX_MINIMAL)
        return (pool_count < AURA_DS_METRICS_COVER_DRIFT_POOL_CAP_MINIMAL)
            ? pool_count : AURA_DS_METRICS_COVER_DRIFT_POOL_CAP_MINIMAL;
    return pool_count;
}

/* Debounce del panel derecho (ms), segun hacia donde apunta la identidad
 * PENDIENTE (D-262/D-266 -- ver render_panel_debounced() en
 * aura_screens.c) y el ajuste de Graficos (D-b: Todos acorta ambos
 * plazos al mismo valor corto, el porque vive en PLAN-niveles-fx.md §3 y
 * en la seccion "Niveles de reduccion" de cada componente). */
static inline long aura_fx_panel_debounce_ms(bool pending_is_coverdrift)
{
    if (aura_settings.graphics_mode == AURA_GFX_ALL)
        return AURA_DS_METRICS_RIGHT_PANEL_DEBOUNCE_SHORT_MS;
    return pending_is_coverdrift
        ? AURA_DS_METRICS_RIGHT_PANEL_DEBOUNCE_MS
        : AURA_DS_METRICS_RIGHT_PANEL_DEBOUNCE_FAST_MS;
}

/* CoverDrift, eje Animaciones (independiente de Graficos, D-a): con
 * Animaciones != Todas, la imagen activa queda QUIETA en el centro (el
 * ciclo de contenido de 7s sigue avanzando -- rotar de imagen es
 * contenido, no animacion, D-a); con Todas conserva el drift de borde a
 * borde de siempre. */
static inline bool aura_fx_coverdrift_motion(void)
{
    return aura_settings.animation_mode == AURA_ANIM_ALL;
}

/* CoverDrift, eje Animaciones: con Ninguna, el cambio de imagen activa es
 * un CORTE instantaneo; en cualquier otro nivel conserva el cross-fade
 * real de siempre. */
static inline bool aura_fx_coverdrift_crossfade(void)
{
    return aura_settings.animation_mode != AURA_ANIM_NONE;
}

/* SelectionSummary, Graficos=Ninguno (D-f/Q1): solo texto, sin tile
 * (icono+degradado+sombra). El panel en si SIGUE existiendo -- Q1 de
 * PLAN-niveles-fx.md, la matriz del dueno manda sobre la regla previa que
 * colapsaba (split) a (full) con Graficos=Ninguno. */
static inline bool aura_fx_ss_show_tile(void)
{
    return aura_settings.graphics_mode != AURA_GFX_NONE;
}

#endif /* AURA_FX_H */
