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
/* ScrollIndicator (PLAN.md T2.4, componentes/scroll-indicator.md):
 * barra de desplazamiento con tamano FIJO (no proporcional al
 * contenido), capsula, que solo aparece si la lista tiene mas de
 * AURA_DS_METRICS_SCROLL_INDICATOR_MIN_ITEMS_TO_SHOW items. Vive en el
 * padding derecho de LeftPanel/MenuList, en el reverso de Music Flow,
 * en las listas de albumes/playlists y -- desde D-275 -- tambien en
 * las listas de contenido a pantalla completa (aura_widgets_draw_list),
 * que antes tenian su propio scrollbar del sistema viejo (D-073).
 */
#ifndef AURA_SCROLL_INDICATOR_H
#define AURA_SCROLL_INDICATOR_H

/* Dibuja el indicador si corresponde (no-op si count <= mostrar_desde).
 * `x` es el borde derecho del carril (el indicador ocupa
 * AURA_DS_METRICS_SCROLL_INDICATOR_THICKNESS px terminando en `x`).
 * `track_y`/`track_h` acotan el carril completo. `selected`/`count`
 * describen la posicion del ITEM seleccionado dentro de la lista --
 * el pulgar es proporcional a selected/(count-1), no a la ventana
 * visible (D-275): el iPod Classic mueve el pulgar con cada item,
 * mientras que la ventana de MenuList se queda quieta durante los
 * primeros y ultimos items (el "estatico" que reporto el dueno).
 * `idle_elapsed_ms` es el tiempo desde el ULTIMO movimiento de
 * seleccion -- el llamador lo reinicia a 0 cada vez que la seleccion
 * cambia; controla el fundido Fade-on-Idle (entrada 150ms /
 * persistencia 1.5s / salida 500ms).
 *
 * Deslizamiento (D-275): el pulgar NO salta a su posicion objetivo --
 * se desliza en linea recta durante AURA_DS_METRICS_SCROLL_INDICATOR_
 * SLIDE_MS (patron "redirigir sin salto" de aura_pattern_lerp(): si el
 * objetivo cambia a mitad del recorrido, arranca desde donde iba). El
 * estado vive dentro del componente (una sola lista visible a la vez,
 * mismo patron que el debounce del panel derecho): se reinicia sin
 * animar cuando cambia el carril (x/track_y/track_h/count -- otra
 * pantalla u otra lista) o cuando el indicador estaba invisible (no
 * "vuela" desde una posicion que el usuario nunca vio).
 *
 * `bg`/`ink`: fondo real debajo del carril y tinta del pulgar -- el
 * fundido se simula mezclando ink hacia bg, asi que ambos deben ser los
 * colores REALES del contexto. */
void aura_scroll_indicator_draw(int x, int track_y, int track_h,
                                 int selected, int count,
                                 long idle_elapsed_ms,
                                 unsigned bg, unsigned ink);

/* Bombeo de redibujo (mismo par pending()/animating() que usa el bucle
 * principal para el resto de animaciones, D-074/D-091). El llamador
 * pasa su propio idle_elapsed_ms; el componente suma su estado interno
 * de deslizamiento, que el llamador no conoce.
 * pending(): true durante TODA la ventana entrada+persistencia+salida o
 * mientras el pulgar se desliza -- cadencia gruesa (HZ/4).
 * animating(): true SOLO en los tramos que cambian cuadro a cuadro
 * (fundidos y deslizamiento) -- cadencia fina (HZ/20). */
int aura_scroll_indicator_pending(long idle_elapsed_ms);
int aura_scroll_indicator_animating(long idle_elapsed_ms);

#endif /* AURA_SCROLL_INDICATOR_H */
