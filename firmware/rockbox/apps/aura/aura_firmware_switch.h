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
/* D-327 (CONTRATO-firmware-studio.md v10) / D-333 (v14): varios
 * firmwares instalados a la vez y cambio entre ellos POR RENOMBRE.
 *
 * El arbol activo es siempre /.rockbox (lo unico que el bootloader,
 * compartido por todas las familias, sabe arrancar). Cada hermana duerme
 * completa y con sus propios ajustes como /.firmware-<familia> (Metro en
 * /.firmware-metro, moonlit.aura en /.firmware-moonlit -- tabla en
 * aura_firmware_families.c); cuando otra esta activa, Aura duerme como
 * /.firmware-aura. Cambiar son dos renombres en FAT mas reiniciar --
 * nunca copiar ni borrar. La misma secuencia la ejecuta Aura Studio
 * desde el Mac (ST-056) y cada hermana desde sus ajustes (M-090/M-093,
 * D-047 de moonlit). */
#ifndef AURA_FIRMWARE_SWITCH_H
#define AURA_FIRMWARE_SWITCH_H

#include <stdbool.h>

#include "aura_firmware_families.h"

/* Hay un /.firmware-<hermana i> que despertar. Sin el, la fila de
 * Ajustes > Cambiar sistema explica como instalarlo y no hace nada.
 * false si i esta fuera de rango. */
bool aura_firmware_sibling_installed(int i);

/* Ejecuta el cambio a la hermana i (contrato v10, en este orden y sin
 * nada en medio):
 *   1. guarda todo lo de Aura (aura.cfg, config.cfg, cola de tagcache) y
 *      fuerza el vaciado a disco -- tras el renombre /.rockbox es el arbol
 *      de la ENTRANTE y cualquier escritura tardia caeria alli;
 *   2. /.rockbox -> /.firmware-aura (saliente primero: el peor corte deja
 *      un dormido entero; Studio repara al conectar);
 *   3. /.firmware-<hermana> -> /.rockbox (si falla, se deshace el 2);
 *   4. /rockbox.ipod (raiz, respaldo del bootloader) := el del entrante;
 *   5. /.aura/sync-pending.json con music=true (solo si la biblioteca
 *      cambio desde que la entrante construyo su base, D-329);
 *   6. reinicio EN SECO (system_reboot) -- nunca el apagado normal, que
 *      volveria a guardar los ajustes de Aura... en el arbol de la otra.
 * Solo vuelve (false) si no pudo y el firmware sigue siendo Aura. Si ya
 * existe /.firmware-aura (Studio garantiza que no) aborta sin borrar.
 * Los demas dormidos no se tocan. */
bool aura_firmware_switch_to(int i);

#endif /* AURA_FIRMWARE_SWITCH_H */
