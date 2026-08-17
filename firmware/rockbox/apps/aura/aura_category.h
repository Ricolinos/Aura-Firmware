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
/* Categoria de icono por seccion de nivel superior del Menu principal
 * (encargo del dueno 2026-08-14, "fixed per-menu-category colors
 * cascading to submenu icons"): Musica/Ajustes/Video/Fotos/Extras cada
 * una con su propio color, que las pantallas descendientes de esa
 * seccion HEREDAN sin importar la profundidad de navegacion -- Ajustes ->
 * Pantalla -> Brillo sigue siendo "categoria Ajustes" tres niveles abajo.
 *
 * Modulo puro en C99, sin dependencias de Rockbox (mismo criterio que
 * aura_nav.c/aura_color.c/aura_motion.c/aura_wheel.c/aura_flow.c):
 * compila igual en el host (apps/aura/test/) que en el firmware. Solo
 * conoce `aura_screen_id_t` (aura_nav.h) -- ninguna resolucion de color
 * real vive aca, eso es aura_category_gradient() en apple2026_shell.h
 * (necesita aura_accent()/aura_settings, que SI son especificos de
 * Rockbox). */
#ifndef AURA_CATEGORY_H
#define AURA_CATEGORY_H

#include "aura_nav.h"

typedef enum {
    /* Sin seccion resuelta (pila en AURA_SCREEN_ROOT sin seleccion
     * valida todavia, o cualquier pantalla no mapeada) -- los
     * resolvedores de color en apple2026_shell.c tratan esto igual que
     * AURA_CATEGORY_MUSIC (degradado del acento, el comportamiento de
     * SIEMPRE antes de este encargo): "fallar abierto" a lo ya conocido,
     * nunca a un color inventado. */
    AURA_CATEGORY_NONE = 0,
    AURA_CATEGORY_MUSIC,
    AURA_CATEGORY_SETTINGS,
    AURA_CATEGORY_VIDEO,
    AURA_CATEGORY_PHOTOS,
    AURA_CATEGORY_EXTRAS,
} aura_category_t;

/* Mapeo pantalla -> categoria de su seccion de nivel superior. Cubre
 * TODO aura_screen_id_t (switch exhaustivo, sin `default` salvo para
 * AURA_SCREEN_ROOT/AURA_SCREEN_COUNT) -- agregar una pantalla nueva al
 * arbol sin agregarla aca cae en AURA_CATEGORY_NONE (fallback seguro,
 * nunca un crash ni un color al azar). */
aura_category_t aura_category_for_screen(aura_screen_id_t screen);

/* Categoria de la pantalla activa, recalculada una vez por cuadro desde
 * aura_screens_draw() (unico punto de entrada de dibujo, aura_main.c) --
 * el mismo patron de estado global "una sola pantalla visible a la vez"
 * que ya usa aura_widgets.c (s_panel_pending_icon, aura_widgets_split_active).
 * Consumida por apple2026_shell.c (tile de SelectionSummary) y
 * aura_widgets.c (variante "-on" de iconos, degradado de categoria en
 * vez de acento plano). */
void aura_category_set_current(aura_category_t cat);
aura_category_t aura_category_current(void);

#endif /* AURA_CATEGORY_H */
