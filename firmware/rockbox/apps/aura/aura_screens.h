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
/* Contenido y logica de cada pantalla de Aura: que se dibuja y que
 * hace cada boton segun la pantalla activa en la pila de navegacion.
 * aura_main.c solo bombea eventos de boton hacia aca. */
#ifndef AURA_SCREENS_H
#define AURA_SCREENS_H

#include <stdbool.h>

#include "aura_nav.h"

void aura_screens_draw(aura_nav_t *nav);
void aura_screens_handle_button(aura_nav_t *nav, long button);

/* Avance de seleccion con la dinamica real de la rueda (velocidad ->
 * 1-3 items), acotado a la lista. Compartido por todas las pantallas
 * con lista propia. */
int aura_wheel_advance(int sel, int count, int direction);

/* D-262: true mientras el panel derecho tiene algo PENDIENTE sin
 * confirmar todavia -- la seleccion se movio a una fila con identidad
 * distinta a la comprometida y todavia no se cumplen los
 * AURA_DS_METRICS_RIGHT_PANEL_DEBOUNCE_MS (o ya se cumplieron y el
 * fundido esta corriendo). aura_main.c lo usa para pedir cuadros a
 * cadencia media durante esa espera, igual que antes hacia con el
 * temporizador de 3s especifico de CoverDrift (D-254, retirado --
 * este reemplazo es general, cubre CoverDrift Y SelectionSummary/icono
 * normal por igual). */
bool aura_screens_right_panel_pending(void);

/* D-262: true SOLO mientras el fundido real de 600ms del panel derecho
 * esta corriendo (tras cumplirse el plazo de arriba) -- aura_main.c lo
 * usa para pedir cuadros a cadencia FINA en ese tramo, igual que ya
 * hace con aura_coverdrift_animating() para el movimiento ambiental en
 * vivo. */
bool aura_screens_right_panel_fading(void);

/* D-259/D-262: true si CoverDrift estaba realmente MONTADO (comprometido
 * en el panel derecho, no solo pendiente, y con el pool con imagenes
 * suficientes) para la fila con destino `target`, en el ultimo cuadro
 * dibujado -- para que el manejador de SELECT decida que coreografia
 * de transicion usar al entrar a esa pantalla. Sin efectos secundarios
 * (no reinicia ningun temporizador). */
bool aura_screens_coverdrift_active_for(aura_screen_id_t target);

/* D-264: equivalente a lo de arriba pero para la fila "Acerca de" --
 * true si esa fila esta realmente comprometida (no solo pendiente) en
 * el panel derecho en el ultimo cuadro dibujado. El dueno confirmo que
 * el morph de entrada a Acerca de reusa el revelado de CoverDrift
 * (D-259/D-261), asi que necesita la misma senal "estaba comprometido
 * de verdad" pero para una pantalla que no es CoverDrift. */
bool aura_screens_about_reveal_active(void);

#endif /* AURA_SCREENS_H */
