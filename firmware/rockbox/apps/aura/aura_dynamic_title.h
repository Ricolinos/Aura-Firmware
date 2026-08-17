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
/* DynamicTitle (PLAN.md T2.6, componentes/dynamic-title.md): texto de
 * StatusBar que muestra el menu o seccion actual. Usa MarqueeText
 * (T2.1) en overflow; anima el cambio de VALOR con Fade-Slide
 * (horizontal, cambio de menu) o Scroll-Slide (vertical, cambio de
 * seccion) -- ambos son el patron "un paso lineal" de
 * transiciones/00-vocabulario.md, aplicados aca con aura_motion_linear()
 * + aura_pattern_lerp() (T1.1), sin matematica nueva.
 */
#ifndef AURA_DYNAMIC_TITLE_H
#define AURA_DYNAMIC_TITLE_H

typedef enum {
    AURA_TITLE_TRANSITION_NONE = 0,   /* mismo texto, sin transicion */
    AURA_TITLE_TRANSITION_FADE_SLIDE, /* cambio de menu, horizontal */
    AURA_TITLE_TRANSITION_SCROLL_SLIDE, /* cambio de seccion, vertical */
} aura_title_transition_t;

/* Dibuja `text` en (x,y) acotado a `max_width` (60/80/120 segun estado
 * y visibilidad del reloj, ver AURA_DS_METRICS_DYNAMIC_TITLE_*) --
 * si no cabe, activa MarqueeText (T2.1) automaticamente.
 *
 * `prev_text` es el valor ANTERIOR (NULL si no hay transicion en
 * curso); `transition` que patron aplicar; `progress_256` el avance
 * [0,256] de esa transicion (el llamador ya lo calculo con
 * aura_motion_linear(), mismo criterio que el resto del sistema).
 * `forward` es la direccion: para FADE_SLIDE, 1 = profundizar (nuevo
 * entra desde la derecha), 0 = regresar (desde la izquierda) -- doc
 * confirmado. Para SCROLL_SLIDE, 1 = scroll hacia abajo (nuevo entra
 * desde abajo), 0 = hacia arriba (doc confirmado). Con
 * transition=NONE, `prev_text`/`forward` se ignoran y se dibuja
 * `text` directo (estatico o marquee segun corresponda). */
void aura_dynamic_title_draw(int x, int y, int max_width,
                              const char *text, const char *prev_text,
                              aura_title_transition_t transition,
                              int progress_256, int forward,
                              long marquee_elapsed_ms);

#endif /* AURA_DYNAMIC_TITLE_H */
