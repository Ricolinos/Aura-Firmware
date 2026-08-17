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
/* Calendario de Extras (encargo del dueno del diseno 2026-08-13):
 * rejilla del mes navegable -- la rueda recorre los dias, los botones
 * de avanzar/retroceder cambian de mes, SELECT entra al dia. */
#ifndef AURA_CALENDAR_H
#define AURA_CALENDAR_H

#include "aura_nav.h"

void aura_calendar_draw(void);
void aura_calendar_handle_button(aura_nav_t *nav, long button);
void aura_calendar_day_draw(void);
void aura_calendar_day_handle_button(aura_nav_t *nav, long button);

#endif /* AURA_CALENDAR_H */
