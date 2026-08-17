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
/* CoverDrift (componentes/cover-drift.md, D-254): reemplazo del panel
 * derecho de SelectionSummary, condicional por fila -- NUNCA convierte
 * una pantalla FULL en SPLIT ni viceversa (esa clasificacion ya esta
 * cerrada, ver DECISIONS.md D-253). Vive exclusivamente en pantallas
 * que YA son SPLIT (Menu raiz, submenus de Musica/Video/Fotos, "Ruta
 * A" de aura_screens.c/draw_menu_screen_v2()) y solo para ciertas
 * filas resaltadas -- cuales exactas es decision del llamador
 * (aura_screens.c), este modulo solo sabe dibujar, no de que pantalla
 * viene.
 *
 * D-254 (encargo directo del dueno del producto, 2026-08-15, tras
 * rechazar por completo un primer intento -- D-251/D-252, revertidos
 * en D-253 -- que convertia pantallas de FULL a SPLIT):
 * - Imagen que LLENA el panel derecho (160x240) y lo EXCEDE a proposito
 *   (AURA_DS_METRICS_COVER_DRIFT_IMAGE_SIZE, cuadrada porque las
 *   caratulas de album lo son) -- el margen de sobrante acota cuanto
 *   puede desplazarse en cualquiera de las 8 direcciones sin revelar
 *   el fondo del panel. Reemplaza el tile chico centrado de D-098.
 * - Umbral de montaje bajado de 10 a 3 imagenes disponibles
 *   (AURA_DS_METRICS_COVER_DRIFT_MIN_IMAGES_TO_ACTIVATE).
 * - El llamador espera 3000ms desde que la seleccion se posa sobre una
 *   fila calificada antes de reemplazar el icono normal (tiempo para
 *   decodificar la primera caratula) -- ese temporizador vive en
 *   aura_screens.c, no aqui (este modulo no sabe de filas/pantallas).
 * - Movimiento a velocidad CONSTANTE (D-254 probo una desaceleracion
 *   real con aura_motion_ease_out(), remapeando el tiempo antes de
 *   pasarlo a aura_pattern_drift_pos() -- el dueno del producto la
 *   probo en el simulador y pidio quitarla, D-255). aura_pattern_drift_pos()
 *   (aura_patterns.c) ya es puramente lineal por su cuenta -- se le pasa
 *   el tiempo real directo, sin ningun remapeo.
 * - Fundido REAL (blend pixel a pixel) entre la imagen saliente y la
 *   entrante durante la ventana de cross-fade -- reemplaza el corte
 *   directo que D-098/D-251 dejaban deliberadamente diferido por falta
 *   de compositor offscreen. Con las dos imagenes garantizadas a cubrir
 *   el panel completo siempre (ver el .c), el blend es directo: por
 *   cada pixel visible del panel, mezcla el pixel correspondiente de
 *   ambas imagenes con a26_shell_blend() (mismo mecanismo que el resto
 *   del sistema usa para blends de un color de tinta, aqui aplicado
 *   pixel real contra pixel real).
 */
#ifndef AURA_COVERDRIFT_H
#define AURA_COVERDRIFT_H

#include "lcd.h"

typedef struct {
    const struct bitmap *bmp; /* NULL = imagen activa todavia sin decodificar (placeholder) */
} aura_coverdrift_image_t;

/* True si hay imagenes suficientes para montar CoverDrift en vez de
 * SelectionSummary (componentes/cover-drift.md, "Montaje"). */
int aura_coverdrift_should_mount(int image_count);

/* D-256: decide si toca avanzar el ciclo (cambiar de imagen activa)
 * ESTE cuadro -- separado de aura_coverdrift_draw() a proposito. El
 * llamador DEBE invocar esta funcion primero, ANTES de decodificar la
 * imagen activa/anterior (ver los getters de indice abajo) y ANTES de
 * llamar a aura_coverdrift_draw() -- si no, el indice nuevo se conoce
 * demasiado tarde para decodificarlo a tiempo, y el cuadro en que el
 * ciclo avanza cae al placeholder solido (bug real corregido en
 * D-256, "flash" de color de acento entre imagenes). `width`/`count`
 * son los mismos que se le van a pasar a aura_coverdrift_draw() en
 * este cuadro. */
void aura_coverdrift_advance_if_due(int width, int count);

/* Dibuja en la franja [x, x+width) x [0, A26_SCREEN_HEIGHT) -- mismo
 * hueco exacto que ocuparia aura_selection_summary_draw() en la misma
 * llamada, nunca los dos a la vez. `images`/`count` son el pool
 * disponible (>=3, verificar con aura_coverdrift_should_mount() antes
 * de llamar); la imagen ACTIVA rota sola entre ellas (una visible a la
 * vez, G10), el resto del pool no necesita estar decodificado -- ver
 * los getters de indice abajo. Requiere haber llamado
 * aura_coverdrift_advance_if_due() antes en el mismo cuadro. */
void aura_coverdrift_draw(int x, int width,
                           const aura_coverdrift_image_t *images, int count);

int aura_coverdrift_pending(void);
int aura_coverdrift_animating(void);

/* D-254: el llamador decodifica bajo demanda SOLO la imagen activa y
 * la anterior (nunca el pool completo -- con hasta 300 caratulas de
 * album posibles y ~168KB por imagen al tamano nuevo, decodificarlas
 * todas de una vez es inviable). Llamar DESPUES de
 * aura_coverdrift_advance_if_due() y ANTES de aura_coverdrift_draw()
 * -- asi el indice que devuelven ya es el vigente para ESTE cuadro, a
 * tiempo de decodificarlo (D-256). Devuelven -1 si CoverDrift todavia
 * no se monto ninguna vez. */
int aura_coverdrift_active_index(void);
int aura_coverdrift_prev_index(void);

#endif /* AURA_COVERDRIFT_H */
