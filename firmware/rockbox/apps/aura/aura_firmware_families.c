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
#include "aura_firmware_families.h"

#include <stddef.h>

/* Orden = orden de las filas de Ajustes > Cambiar sistema. Solo se
 * anade al final: el indice es el que las pantallas llevan cableado. */
static const struct {
    const char *dormant_dir;
    aura_str_id_t name;
} siblings[] = {
    { "/.firmware-metro",   AURA_STR_FAMILY_METRO },
    { "/.firmware-moonlit", AURA_STR_FAMILY_MOONLIT },
};

#define SIBLING_COUNT ((int)(sizeof(siblings) / sizeof(siblings[0])))

int aura_firmware_sibling_count(void)
{
    return SIBLING_COUNT;
}

const char *aura_firmware_sibling_dormant_dir(int i)
{
    if (i < 0 || i >= SIBLING_COUNT)
        return NULL;
    return siblings[i].dormant_dir;
}

aura_str_id_t aura_firmware_sibling_name(int i)
{
    if (i < 0 || i >= SIBLING_COUNT)
        return AURA_STR_COUNT;
    return siblings[i].name;
}
