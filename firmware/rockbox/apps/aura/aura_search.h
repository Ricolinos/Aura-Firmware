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
/* Pantalla de Busqueda (encargo del dueno del diseno 2026-08-13).
 *
 * Teclado clasico del iPod: una TIRA de caracteres que la rueda
 * recorre, no una cuadricula. La tira se muestra en MAYUSCULAS pero
 * escribe MINUSCULAS (la busqueda no distingue de todos modos). El
 * texto escrito se ve completo arriba, en letra chica, y truncado en la
 * caja de busqueda con puntos suspensivos por delante.
 *
 * Lo escrito PERSISTE al salir con MENU y volver a entrar, mientras el
 * regreso se quede DENTRO de Musica (p.ej. Busqueda -> submenu Musica
 * -> Busqueda). Solo se reinicia cuando la navegacion desanda toda la
 * pila hasta la raiz/menu principal (encargo 2026-08-14) -- ver el
 * gancho centralizado en aura_screens_handle_button() (aura_screens.c)
 * que llama a aura_search_reset() al detectar ese regreso a la raiz.
 */
#ifndef AURA_SEARCH_H
#define AURA_SEARCH_H

#include <stdbool.h>
#include "aura_nav.h"

void aura_search_draw(void);
void aura_search_handle_button(aura_nav_t *nav, long button);

/* Reinicia texto, cursor de la tira y resultados -- llamado SOLO desde
 * el gancho de "regreso a la raiz" en aura_screens.c (Fix 3, encargo
 * 2026-08-14). Reentrar a Busqueda sin haber tocado la raiz no debe
 * perder lo escrito. */
void aura_search_reset(void);

/* La lista de resultados a pantalla completa (misma pantalla de
 * busqueda, ya confirmada con PLAY). */
void aura_search_results_draw(void);
void aura_search_results_handle_button(aura_nav_t *nav, long button);

/* true mientras la pantalla de busqueda esta activa: la puerta de
 * energia la consulta para animar el cursor sin despertar al resto. */
bool aura_search_needs_tick(void);

#endif /* AURA_SEARCH_H */
