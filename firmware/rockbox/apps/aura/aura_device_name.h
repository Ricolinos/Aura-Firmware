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
/* Nombre del dispositivo (D-294) -- CONTRATO-dispositivo.md v2. Aura Studio
 * escribe /.rockbox/aura/device.cfg (device_id, device_name, device_owner,
 * ...); el firmware SOLO lee `device_name` y lo muestra en el slot
 * superior de "Acerca de" en vez del literal "Mi iPod" (contrato SS E).
 * Este modulo es la parte pura (C99, sin Rockbox, testeable en host):
 * saneo del valor tal como puede venir del archivo. La lectura del disco
 * vive en aura_device.c. */
#ifndef AURA_DEVICE_NAME_H
#define AURA_DEVICE_NAME_H

#include <stddef.h>

/* Contrato SS C: <= 32 caracteres y <= 48 bytes UTF-8. 48 + NUL. */
#define AURA_DEVICE_NAME_MAX_BYTES 48
#define AURA_DEVICE_NAME_BUF       (AURA_DEVICE_NAME_MAX_BYTES + 1)

/* Copia `in` a `out` (de al menos AURA_DEVICE_NAME_BUF bytes) aplicando
 * las reglas del contrato del lado que le toca al firmware: recorta
 * espacios en los extremos, colapsa espacios internos, descarta
 * caracteres de control (< 0x20 y 0x7F), y trunca a 48 bytes SIN partir
 * una secuencia UTF-8. Devuelve la longitud en bytes del resultado; 0 si
 * queda vacio (el llamador cae al literal "Mi iPod"). */
size_t aura_device_name_sanitize(const char *in, char *out, size_t outsz);

#endif /* AURA_DEVICE_NAME_H */
