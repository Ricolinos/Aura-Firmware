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
#include <stdio.h>

#include "lcd.h"
#include "powermgmt.h"
#include "power.h"
#include "timefuncs.h"
#include "settings.h"

#include "aura_statusbar.h"

const char *aura_battery_icon_name(void)
{
#if CONFIG_CHARGING
    if (charging_state())
        return "battery-charging";
#endif

    int level = battery_level();
    if (level < 0)
        return "battery-medium"; /* desconocido: ni alarmar ni mentir con "full" */
    if (level >= 60)
        return "battery-full";
    if (level >= 20)
        return "battery-medium";
    return "battery-low";
}

/* HH:MM, reusando el ajuste real de Rockbox (global_settings.timeformat,
 * D-021: mismo criterio que el resto del proyecto, ningun formato propio
 * inventado). Fecha/hora todavia no tienen pantalla de ajustes propia en
 * Aura (Temporiz. luz/reposo si, Fecha y hora sigue diferida -- D-060),
 * asi que "Reloj 24 horas" hoy solo se puede cambiar indirectamente via
 * el ajuste crudo de Rockbox; cuando esa pantalla se construya, este
 * reloj ya la respeta sin cambios. */
void aura_format_clock(char *buf, size_t bufsz)
{
    struct tm *now = get_time();
    int hour = now->tm_hour;

    if (global_settings.timeformat == 1) /* 12 horas */
    {
        int hour12 = hour % 12;
        if (hour12 == 0)
            hour12 = 12;
        snprintf(buf, bufsz, "%d:%02d %s", hour12, now->tm_min,
                  (hour < 12) ? "AM" : "PM");
    }
    else
    {
        snprintf(buf, bufsz, "%02d:%02d", hour, now->tm_min);
    }
}
