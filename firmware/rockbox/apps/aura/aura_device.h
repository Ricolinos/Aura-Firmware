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
/* Lectura de /.rockbox/aura/device.cfg (CONTRATO-dispositivo.md v2, D-294):
 * el firmware solo consume `device_name`. Se recarga al arrancar y al
 * volver de la pantalla USB (los dos momentos en que Studio pudo
 * escribirlo), igual que el marcador de sincronizacion (aura_sync.c). El
 * firmware NUNCA escribe este archivo. */
#ifndef AURA_DEVICE_H
#define AURA_DEVICE_H

/* Relee el archivo. Barato (unas lineas); nunca falla de forma visible:
 * sin archivo o sin device_name valido, aura_device_name() devuelve NULL. */
void aura_device_reload(void);

/* Nombre saneado (puntero ESTABLE a un buffer interno -- el panel derecho
 * compara punteros para su debounce), o NULL si no hay nombre. */
const char *aura_device_name(void);

#endif /* AURA_DEVICE_H */
