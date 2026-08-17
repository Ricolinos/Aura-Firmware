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
/* Helpers compartidos de estado del dispositivo (reloj formateado,
 * icono de bateria). La barra de estado VIEJA que vivia aqui (Fase 13,
 * PLAN-UX.md) se retiro en la auditoria 2026-08-12: StatusBar v2
 * (aura_status_bar_v2.c, componentes/status-bar.md) es la unica barra
 * del sistema desde entonces -- este modulo conserva solo las piezas
 * que v2 y ClockIndicator ya consumian de aca. */
#ifndef AURA_STATUSBAR_H
#define AURA_STATUSBAR_H

#include <stddef.h>

/* HH:MM, reusando global_settings.timeformat (D-021) -- ningun formato
 * propio. Compartida con aura_clock_indicator.c (T2.5) para no duplicar
 * la logica de 12/24h en dos lugares. */
void aura_format_clock(char *buf, size_t bufsz);

/* Nombre del icono de bateria segun carga/nivel real -- compartida con
 * aura_status_bar_v2.c (T2.7) por el mismo motivo. */
const char *aura_battery_icon_name(void);

#endif /* AURA_STATUSBAR_H */
