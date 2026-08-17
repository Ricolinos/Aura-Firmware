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
#include "lcd.h"

#include "aura_shutdown_screen.h"
#include "apple2026_shell.h"
#include "apple2026_tokens.h"
#include "aura_widgets.h"

void aura_shutdown_screen_draw(void)
{
    int size = A26_ICON_SIZE_SHUTDOWN_ICON;
    int x = (A26_SCREEN_WIDTH - size) / 2;
    int y = (A26_SCREEN_HEIGHT - size) / 2;

    /* Negro/blanco crudos a proposito -- el dueno pidio que esta
     * pantalla NO respete el tema (ver aura_shutdown_screen.h). */
    lcd_set_background(LCD_BLACK);
    lcd_set_foreground(LCD_WHITE);
    lcd_clear_display();

    aura_widgets_draw_icon_ink("poweroff", size, x, y, LCD_WHITE, 256);

    lcd_update();
}
