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
#include "apple2026_shell.h"
#include "apple2026_tokens.h"
#include "aura_widgets.h"

#include "aura_selector.h"

void aura_selector_draw(int x, int y, int w, int h, aura_selector_indicator_t indicator)
{
    unsigned bg = a26_color(A26_SHELL_BG);

    /* Pastilla GRIS (A26_SELECTION_FILL), contenido en acento --
     * correccion directa del dueno del diseno (2026-08-12): "el color
     * de acento sobre el elemento seleccionado aplica al texto y al
     * icono, no al seleccionador; el seleccionador debe ser de un
     * color gris". La primera lectura de selector.md ("usa
     * --color-accent") habia pintado la pastilla misma de acento --
     * el acento del documento se refiere al CONTENIDO del item
     * seleccionado. El documento fuente queda por actualizar por su
     * dueno (este repo no toca docs/aura-design-system/). */
    a26_shell_fill_rounded_rect(x, y, w, h, AURA_DS_METRICS_SELECTOR_CORNER_RADIUS,
                                 a26_color(A26_SELECTION_FILL), bg);

    if (indicator == AURA_SELECTOR_INDICATOR_CHEVRON)
    {
        int icon_size = AURA_DS_METRICS_SELECTOR_INDICATOR_MAX_HEIGHT;
        int icon_x = x + w - AURA_DS_METRICS_SELECTOR_INDICATOR_GAP_FROM_EDGE - icon_size;
        int icon_y = y + (h - icon_size) / 2;

        /* Acento, como el resto del contenido del item seleccionado
         * (texto/icono) -- la variante "-on" resuelve la tinta contra
         * el acento vigente en runtime. */
        aura_widgets_draw_icon_selected("chevron-right", icon_size, icon_x, icon_y);
    }
}
