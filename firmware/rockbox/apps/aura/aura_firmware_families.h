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
/* D-333: registro puro de las familias hermanas de Aura (contrato v14,
 * §A bis). Sin I/O, sin dependencias de Rockbox -- lo consume
 * aura_firmware_switch.c (que si toca el disco) y el test host
 * test_firmware_families.c. Una familia nueva es una fila mas aqui y su
 * pantalla de confirmacion en aura_screens.c; nada mas. */
#ifndef AURA_FIRMWARE_FAMILIES_H
#define AURA_FIRMWARE_FAMILIES_H

#include "aura_lang.h"

/* Arbol dormido de ESTA familia (contrato v14 §A bis: /.firmware-<familia>). */
#define AURA_FIRMWARE_OWN_DORMANT "/.firmware-aura"

/* Cuantas familias hermanas conoce este firmware (hoy 2: Metro y
 * moonlit.aura). Los indices 0..count-1 son estables y son los que usan
 * las pantallas AURA_SCREEN_SETTINGS_SWITCH_TO_*. */
int aura_firmware_sibling_count(void);

/* Arbol dormido de la hermana i ("/.firmware-metro", ...). NULL si i
 * esta fuera de rango. */
const char *aura_firmware_sibling_dormant_dir(int i);

/* Nombre de cara al usuario de la hermana i (AURA_STR_FAMILY_*).
 * AURA_STR_COUNT (que aura_str() traduce a "") si i esta fuera de rango. */
aura_str_id_t aura_firmware_sibling_name(int i);

#endif /* AURA_FIRMWARE_FAMILIES_H */
