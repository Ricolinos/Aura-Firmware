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
/* Pantalla de apagado (encargo del dueno, 2026-08-14): "al apagar el
 * dispositivo, deberia mostrarse al centro el icono de apagado, en una
 * pantalla negra (icono blanco) independientemente del tema claro u
 * oscuro". Excepcion deliberada al sistema de temas -- por eso no usa
 * a26_color()/a26_shell_clear_screen() como el resto de las pantallas,
 * sino LCD_BLACK/LCD_WHITE crudos. aura_main.c la dibuja UNA vez al
 * interceptar SYS_POWEROFF, antes de dejar que default_event_handler()
 * siga con el apagado real de Rockbox (que ya no dibuja su propio
 * splash de texto -- ver aura_settings_apply_core_defaults(),
 * show_shutdown_message). */
#ifndef AURA_SHUTDOWN_SCREEN_H
#define AURA_SHUTDOWN_SCREEN_H

void aura_shutdown_screen_draw(void);

#endif
