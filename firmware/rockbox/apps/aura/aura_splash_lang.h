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
/* Fase 14 (PLAN-UX.md) / D-055: reescritura de los mensajes de
 * apps/gui/splash.c que vienen del arbol de Rockbox que Aura no
 * controla (playlist, plugin loader, tagcache, apagado por bateria) al
 * tono y wording de Aura. Ver DECISIONS.md. */
#ifndef AURA_SPLASH_LANG_H
#define AURA_SPLASH_LANG_H

#include <stddef.h>

/* Reescribe `buf` in-place si su contenido coincide (completo o por
 * prefijo) con uno de los mensajes conocidos de Rockbox, en el idioma
 * activo de Aura. Si no hay match, deja `buf` intacto -- mostrar el
 * texto real (aunque no tenga el tono de Aura) es preferible a
 * reemplazarlo por un generico que esconda informacion de diagnostico
 * que Aura no anticipo. */
void aura_splash_translate(char *buf, size_t bufsz);

#endif /* AURA_SPLASH_LANG_H */
