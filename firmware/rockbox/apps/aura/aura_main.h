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
/* Punto de entrada de Aura UI. Reemplaza a root_menu() al final de
 * apps/main.c: a partir de aqui la UI de Rockbox (arbol de archivos,
 * menu raiz, WPS) queda completamente inalcanzable para el usuario. */
#ifndef AURA_MAIN_H
#define AURA_MAIN_H

#include "gcc_extensions.h"

void aura_main(void) NORETURN_ATTR;

/* Velocidad angular (grados/seg) del ultimo BUTTON_SCROLL_FWD/BACK
 * procesado -- 0 para cualquier otro boton o para eventos sinteticos sin
 * dato real (doc SS7, aura_wheel.h la consume). */
long aura_main_wheel_velocity(void);

/* Traga los BUTTON_REPEAT de `raw` hasta que llegue su BUTTON_REL --
 * lo llama aura_screens.c despues de una navegacion con transicion:
 * las transiciones sincronas bloquean cientos de ms sin leer botones,
 * y los repeats del boton sostenido se acumulan en la cola; como
 * "repeat = pulsacion nueva" (D-022), cada uno disparaba OTRA
 * navegacion al terminar (bug real: salir del reproductor encadenaba
 * pops hasta el menu principal, 2026-08-12). Una pulsacion (aunque se
 * sostenga) = una navegacion. */
void aura_main_swallow_repeats(long raw);

/* True si el ultimo boton entregado por el bucle principal venia de un
 * BUTTON_REPEAT (boton sostenido) y no de una pulsacion fresca -- el
 * reproductor lo usa para distinguir "tap = pista anterior/siguiente"
 * de "mantener = adelantar/atrasar la cancion" (encargo 2026-08-12)
 * sin romper la normalizacion global de D-022. */
bool aura_main_last_was_repeat(void);

/* Gesto de "mantener presionado" (vocabulario de botones de Aura,
 * B-02 en BLOCKED.md -- pieza de infraestructura general, no atada a
 * un boton en particular). Bit 0x40000000 -- fuera del rango real de
 * BUTTON_* (button.h llega hasta BUTTON_REDRAW=0x20000000) y del par
 * BUTTON_REL/BUTTON_REPEAT que next_button() ya consume internamente,
 * asi que puede combinarse con cualquier codigo de boton real sin
 * colisionar. aura_screens_handle_button() (y cualquier pantalla) lo
 * recibe como `boton | AURA_BUTTON_HOLD` -- una vez, en el instante
 * exacto en que la pulsacion cruza el umbral real de "hold" del
 * driver (BUTTON_REPEAT, ~300ms, mismo convenio que
 * apps/action.c/apps/keymaps -- no un temporizador propio de Aura).
 * No hay un evento de "soltar" un hold: el patron de uso es "hacer
 * algo una vez al cruzar el umbral", igual que MENU-mantenido/
 * SELECT-mantenido en el resto de Rockbox. Solo BUTTON_SELECT
 * dispara esto hoy (el unico caso real, ClockIndicator) -- extender a
 * otro boton es agregarlo a la lista de `is_hold_button()` en
 * aura_main.c, no rediseñar nada. */
#define AURA_BUTTON_HOLD 0x40000000L

#endif /* AURA_MAIN_H */
