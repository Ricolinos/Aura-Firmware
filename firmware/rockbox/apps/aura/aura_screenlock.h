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
/* Bloqueo de pantalla GLOBAL, en Ajustes (encargo del dueno del diseno
 * 2026-08-13, reubicado desde Extras el 2026-08-14): candado y una
 * clave de 4 digitos que se configura con la rueda; despues de
 * escribirla, pide confirmarla. MENU restablece y cancela; SELECT al
 * final la establece.
 *
 * Tres estados segun aura_settings (screen_lock_active/_enabled), no
 * dos:
 *  - screen_lock_active: "desbloquear" (un solo paso, compara contra la
 *    clave guardada). aura_main.c llama a este mismo par de funciones
 *    directamente en este modo, interceptando el loop principal ANTES
 *    de aura_screens_draw()/aura_screens_handle_button() -- por eso el
 *    bloqueo alcanza a todo el aparato y no solo a esta pantalla.
 *    aura_main.c es tambien el UNICO lugar que enciende este estado (en
 *    cada arranque, a partir de screen_lock_enabled) -- este archivo ya
 *    no lo hace.
 *  - screen_lock_enabled (sin estar activo): "Desactivar" -- confirmar
 *    borra la clave guardada y el armado; reactivar exige configurar
 *    una clave nueva desde cero.
 *  - ninguno de los dos: "Activar" -- configurar una clave nueva (flujo
 *    de arriba). Al coincidir arma screen_lock_enabled; NO enciende
 *    screen_lock_active de inmediato (eso lo decide aura_main.c en el
 *    proximo arranque). */
#ifndef AURA_SCREENLOCK_H
#define AURA_SCREENLOCK_H

#include "aura_nav.h"

/* D-351: que va a hacer la pantalla de codigo. Antes se INFERIA del
 * estado (`enabled` sin `active` => desactivar), lo que dejaba sin
 * lugar a "Cambiar codigo": con el bloqueo armado, la unica pantalla
 * alcanzable era la de desactivar. El submenu de Bloqueo lo dice
 * explicitamente al entrar. */
typedef enum {
    AURA_SCREENLOCK_MODE_SET = 0, /* activar: clave nueva, dos pasadas */
    AURA_SCREENLOCK_MODE_CHANGE,  /* cambiar: igual, pero ya estaba armado */
    AURA_SCREENLOCK_MODE_REMOVE,  /* quitar: confirmacion y borrado */
} aura_screenlock_mode_t;

void aura_screenlock_begin(aura_screenlock_mode_t mode);

void aura_screenlock_draw(void);

/* D-351: pantalla de bloqueo EN REPOSO -- la que se ve mientras el
 * interruptor Hold esta puesto y el bloqueo esta armado. Mismo lenguaje
 * que la de desbloqueo (candado grande, barra de estado con reloj,
 * bateria y el propio candado) pero SIN cajas de digitos: con Hold
 * puesto no hay nada que teclear, la rueda esta muerta por hardware.
 * El codigo se pide al QUITAR el Hold, y solo segun
 * aura_settings.screen_lock_require. */
void aura_screenlock_draw_resting(void);
void aura_screenlock_handle_button(aura_nav_t *nav, long button);

#endif /* AURA_SCREENLOCK_H */
