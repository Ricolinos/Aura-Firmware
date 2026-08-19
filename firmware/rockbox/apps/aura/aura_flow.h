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
/* Nucleo matematico de Music Flow real (Fase 31.1, PLAN-APPLE2026.md).
 *
 * Cosechado de `apps/plugins/pictureflow/pictureflow.c` (D-vease
 * PLAN-APPLE2026.md SS0: "el renderizador de perspectiva por columnas
 * ... el corazon del efecto, ~400 lineas portables casi sin rb->") --
 * NO es el plugin, es su aritmetica de punto fijo y su formula de
 * proyeccion por columnas, portada tal cual y adaptada a la geometria
 * fija de Aura (320x240, un solo target, a diferencia del plugin que
 * generaliza a cualquier LCD_WIDTH/HEIGHT de todo el arbol de Rockbox).
 *
 * La tecnica de reflejo por tabla que el mismo SS0 menciona ("atenuacion
 * precalculada por fila, sin gradiente en tiempo real") NO se porta de
 * nuevo aca -- Aura ya la tiene implementada con su propia convencion
 * documentada (35% de alto, doc SS5.4) en aura_art.c desde la Fase 29
 * (D-077); este modulo se enfoca en lo que todavia no existe: la
 * perspectiva.
 *
 * Modulo puro en C99, sin dependencias de Rockbox -- mismo criterio que
 * aura_nav.c/aura_motion.c/aura_wheel.c, compila igual en el host que
 * en el firmware. NO dibuja nada ni toca el framebuffer: eso es
 * deliberado (Fase 31.1 es "sin tocar hardware, sin conectar a la
 * pantalla real" -- ver DECISIONS D-079). Cuando llegue el momento de
 * blitear pixeles de verdad, ese codigo vive en un modulo de dibujo
 * separado que consume este.
 */
#ifndef AURA_FLOW_H
#define AURA_FLOW_H

/* -- Aritmetica de punto fijo (PFreal de pictureflow.c, portada tal
 * cual: PFREAL_SHIFT=10) ---------------------------------------------- */
#define AURA_FLOW_SHIFT 10
#define AURA_FLOW_ONE   (1 << AURA_FLOW_SHIFT)
#define AURA_FLOW_HALF  (AURA_FLOW_ONE >> 1)

/* angulo en unidades propias: IANGLE_MAX=1024 equivale a una vuelta
 * completa (2*pi) -- mismo mapeo que pictureflow.c, para que un angulo
 * ya calculado en terminos de "eje de rotacion del slide" se pueda usar
 * tal cual. */
#define AURA_FLOW_IANGLE_MAX  1024
#define AURA_FLOW_IANGLE_MASK 1023

int aura_flow_fmul(int a, int b);
int aura_flow_fdiv(int num, int den);
int aura_flow_fsin(int iangle);
int aura_flow_fcos(int iangle);

/* -- Geometria de camara fija para 320x240 (derivada de las macros de
 * pictureflow.c con LCD_WIDTH=320, LCD_HEIGHT=240, aspecto 1:1 -- ver
 * apple2026_tokens.h A26_SCREEN_WIDTH/HEIGHT) ---------------------------- */
#define AURA_FLOW_SCREEN_W      320
#define AURA_FLOW_SCREEN_H      240
#define AURA_FLOW_DISPLAY_W     128  /* ancho de la "franja" de flujo, doc SS0 */
#define AURA_FLOW_CAM_DIST      240
#define AURA_FLOW_CAM_DIST_R    (AURA_FLOW_CAM_DIST << AURA_FLOW_SHIFT)
#define AURA_FLOW_DISPLAY_LEFT_R  (AURA_FLOW_HALF - AURA_FLOW_SCREEN_W * AURA_FLOW_HALF)
#define AURA_FLOW_MAXSLIDE_LEFT_R (AURA_FLOW_HALF - AURA_FLOW_DISPLAY_W * AURA_FLOW_HALF)

/* -- Proyeccion por columnas -------------------------------------------- */

/* Un slide a proyectar: angulo (unidades IANGLE, 0 = de frente a la
 * camara), distancia adicional de camara (0 = posicion de reposo -- SIN
 * escalar por AURA_FLOW_ONE, es un entero simple igual que
 * slide_data.distance en pictureflow.c; aura_flow_begin_projection() lo
 * escala internamente) y posicion horizontal de su centro en punto fijo
 * (coordenadas de pantalla, PFreal). */
typedef struct {
    int angle;
    int distance;
    int cx;
} aura_flow_slide_t;

/* Estado de la recurrencia de proyeccion (formula de pictureflow.c
 * render_slide(), extraida del blit de pixeles): en vez de recalcular
 * la formula de perspectiva completa por columna, xsnum/xsden avanzan
 * con un incremento constante (xsnumi/xsdeni) y una sola division por
 * columna recupera xs -- es una transformacion de Mobius de la columna
 * de pantalla, la misma optimizacion que ya paga su costo en hardware
 * real sin FPU. */
typedef struct {
    int slide_width_px;
    int slide_left;    /* PFreal: borde izquierdo de la fuente */
    int cosr, sinr;
    int zo;             /* PFreal: distancia efectiva de camara para este slide */
    int xsnum, xsden;
    int xsnumi, xsdeni;
    int xs;              /* PFreal: posicion fuente de la columna actual */
    int screen_x;         /* columna de pantalla actual */
    int has_rotation;      /* angle!=0 || zo!=0 -- decide recurrencia vs paso fijo */
} aura_flow_projection_t;

/* Inicializa `proj` en la primera columna de pantalla donde `slide` es
 * visible (proj->screen_x). Si el slide no es visible en absoluto
 * (fuera de rango), proj->screen_x >= AURA_FLOW_SCREEN_W. */
void aura_flow_begin_projection(aura_flow_projection_t *proj,
                                 const aura_flow_slide_t *slide,
                                 int slide_width_px);

/* Avanza a la siguiente columna de pantalla. Devuelve 0 (y no modifica
 * `proj`) si ya no queda fuente visible (la columna fuente actual llego
 * al borde del slide) o si se salio de pantalla -- el llamador corta el
 * bucle de dibujo ahi. */
int aura_flow_advance_column(aura_flow_projection_t *proj);

/* Columna de la imagen fuente (0..slide_width-1) que corresponde a la
 * posicion actual de la proyeccion. */
int aura_flow_source_column(const aura_flow_projection_t *proj);

/* Escala vertical en punto fijo para la columna actual: cuantos
 * "pixeles fuente" avanzar por cada pixel vertical de pantalla al
 * muestrear esa columna (mas alla de AURA_FLOW_ONE = la columna se ve
 * mas chica que su tamano fuente, perspectiva alejandose). */
int aura_flow_vertical_scale(const aura_flow_projection_t *proj);

#endif /* AURA_FLOW_H */
