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
/* Alarmas de Extras (encargo del dueno del diseno 2026-08-13):
 * lista de alarmas + editor con Alarma (on/off), Hora (con reloj
 * analogico que se mueve con la configuracion), Repetir, Sonido,
 * Etiqueta y Eliminar. */
#ifndef AURA_ALARMS_H
#define AURA_ALARMS_H

#include "aura_nav.h"

void aura_alarms_draw(void);
void aura_alarms_handle_button(aura_nav_t *nav, long button);
void aura_alarm_edit_draw(void);
void aura_alarm_edit_handle_button(aura_nav_t *nav, long button);
void aura_alarm_time_draw(void);
void aura_alarm_time_handle_button(aura_nav_t *nav, long button);
void aura_alarm_choice_draw(void);
void aura_alarm_choice_handle_button(aura_nav_t *nav, long button);

#endif /* AURA_ALARMS_H */
