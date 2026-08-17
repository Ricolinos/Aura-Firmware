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
#include "aura_flow.h"

int aura_flow_fmul(int a, int b)
{
    return (int)(((long long)a * (long long)b) >> AURA_FLOW_SHIFT);
}

/* D-219 (encargo del dueno, 2026-08-14: "analiza el plugin de
 * pictureflow... compara como lo tenemos configurado nosotros"):
 * cuando este modulo se escribio (D-079), todavia no estaba conectado
 * a pantalla ("este modulo no corre en tiempo real todavia... la
 * version portable importa mas que la mas rapida", comentario
 * original) -- pero aura_coverflow_draw() SI lo llama en tiempo real
 * hoy, una vez por columna de pantalla de CADA tapa visible (hasta
 * ~9), en CADA cuadro mientras el carrusel se desliza. El bucle de
 * hasta 30 iteraciones de corrimiento+comparacion que habia aca es
 * exactamente lo que pictureflow.c evita con su propio allowed_shift()
 * (linea 803 de pictureflow.c): un solo conteo de ceros a la izquierda
 * (clz), que en ARMv5+ es la instruccion CLZ nativa -- UNA instruccion
 * en vez de hasta 30 corrimientos con rama. Portado ahora que el
 * "tiempo real" que el comentario original anticipaba ya llego. */
static int clz32(unsigned v)
{
    return __builtin_clz(v);
}

/* IMPORTANTE: NO es clz(uval)-1 como en pictureflow.c -- esta funcion
 * preserva a proposito el resultado EXACTO del bucle que reemplaza
 * (una unidad de corrimiento menos que la formula de pictureflow, un
 * desvio preexistente de Aura sin relacion con el rendimiento) para
 * que el cambio sea una optimizacion de VELOCIDAD pura, cero cambio de
 * comportamiento numerico -- verificado a mano contra el bucle
 * original para varios valores de `uval` antes de portar esto, y
 * `make -C apps/aura/test test` (test_flow.c) sigue pasando identico. */
static int allowed_shift(int val)
{
    unsigned uval = (unsigned)(val ^ (val >> 31));

    if (uval == 0)
        return 30;

    return clz32(uval) - 2;
}

int aura_flow_fdiv(int num, int den)
{
    int shift = allowed_shift(num);
    if (shift > AURA_FLOW_SHIFT)
        shift = AURA_FLOW_SHIFT;
    if (shift < 0)
        shift = 0;

    num <<= shift;
    den >>= (AURA_FLOW_SHIFT - shift);
    if (den == 0)
        return 0; /* fuera de rango del original tambien -- evita division por cero */
    return num / den;
}

/* fmuln de pictureflow.c: multiplicacion preescalada, para casos donde
 * se conoce de antemano cuantos bits bajos van a quedar vacios (evita
 * desbordar el intermedio de 32 bits sin necesitar 64 bits). Portada tal
 * cual, con los mismos corrimientos (SHIFT-2, 0) que usa render_slide()
 * para el termino de compensacion de inclinacion. */
static int fmuln(int a, int b, int ps1, int ps2)
{
    return (int)((long)(a >> ps1) * (long)(b >> ps2)) >> (AURA_FLOW_SHIFT - ps1 - ps2);
}

/* Tabla de seno de pictureflow.c (33 muestras, un cuarto de vuelta cada
 * 8 entradas -- IANGLE_MAX=1024 por vuelta completa). Portada tal cual,
 * NO regenerada: son las mismas constantes que ya probo el firmware
 * original en este hardware. */
static const short sin_tab[] = {
        0,   100,   200,   297,   392,   483,   569,   650,
      724,   792,   851,   903,   946,   980,  1004,  1019,
     1024,  1019,  1004,   980,   946,   903,   851,   792,
      724,   650,   569,   483,   392,   297,   200,   100,
        0,  -100,  -200,  -297,  -392,  -483,  -569,  -650,
     -724,  -792,  -851,  -903,  -946,  -980, -1004, -1019,
    -1024, -1019, -1004,  -980,  -946,  -903,  -851,  -792,
     -724,  -650,  -569,  -483,  -392,  -297,  -200,  -100,
        0
};

int aura_flow_fsin(int iangle)
{
    int i, p, q, g;

    iangle &= AURA_FLOW_IANGLE_MASK;
    i = iangle >> 4;
    p = sin_tab[i];
    q = sin_tab[i + 1];
    g = q - p;
    return p + g * (iangle - i * 16) / 16;
}

int aura_flow_fcos(int iangle)
{
    return aura_flow_fsin(iangle + (AURA_FLOW_IANGLE_MAX >> 2));
}

void aura_flow_begin_projection(aura_flow_projection_t *proj,
                                 const aura_flow_slide_t *slide,
                                 int slide_width_px)
{
    int cosr = aura_flow_fcos(slide->angle);
    int sinr = aura_flow_fsin(slide->angle);
    int abs_sinr = sinr < 0 ? -sinr : sinr;
    /* zo real de render_slide() (pictureflow.c): "PFREAL_ONE*distance +
     * CAM_DIST_R*100/zoom - CAM_DIST_R - fmuln(MAXSLIDE_LEFT_R,
     * fabs(sinr), SHIFT-2, 0)". Aura fija zoom=100 siempre (geometria
     * fija, ver aura_flow.h), asi que el termino del medio se cancela
     * (CAM_DIST_R*100/100 - CAM_DIST_R = 0) y queda afuera de la formula
     * -- no es una omision, es la misma simplificacion que ya declara el
     * header para el resto de las constantes de camara. Sin este
     * termino de compensacion, los slides inclinados (angle != 0)
     * proyectan mal: es lo que empuja la cara tilteada hacia atras para
     * que no quede recortada contra la camara. */
    int zo = AURA_FLOW_ONE * slide->distance
           - fmuln(AURA_FLOW_MAXSLIDE_LEFT_R, abs_sinr, AURA_FLOW_SHIFT - 2, 0);
    int slide_left = -slide_width_px * AURA_FLOW_HALF + AURA_FLOW_HALF;
    int xs = slide_left;
    int xp, xi;

    proj->slide_width_px = slide_width_px;
    proj->slide_left = slide_left;
    proj->cosr = cosr;
    proj->sinr = sinr;
    proj->zo = zo;
    proj->has_rotation = (slide->angle != 0) || (zo != 0);

    /* Columna de pantalla proyectada del borde izquierdo de la fuente
     * (formula de camara de pictureflow.c, portada tal cual). */
    xp = aura_flow_fdiv(AURA_FLOW_CAM_DIST * (slide->cx + aura_flow_fmul(xs, cosr)),
                         (AURA_FLOW_CAM_DIST_R + zo + aura_flow_fmul(xs, sinr)));

    if (xp < AURA_FLOW_DISPLAY_LEFT_R)
        xp = AURA_FLOW_DISPLAY_LEFT_R;

    /* Redondea hacia arriba a la primera columna entera de pantalla,
     * igual que el original ("Since we're finding the screen position
     * of the left edge of the slide, we round up"). */
    xi = (xp - AURA_FLOW_DISPLAY_LEFT_R + AURA_FLOW_ONE - 1) >> AURA_FLOW_SHIFT;
    xp = AURA_FLOW_DISPLAY_LEFT_R + xi * AURA_FLOW_ONE;

    if (xi >= AURA_FLOW_SCREEN_W)
    {
        proj->screen_x = AURA_FLOW_SCREEN_W; /* nada visible */
        return;
    }

    /* xs real en esa primera columna, mas los incrementos constantes de
     * la recurrencia de Mobius que aura_flow_advance_column() reusa
     * columna a columna sin volver a resolver la formula completa. */
    proj->xsnum = AURA_FLOW_CAM_DIST * (slide->cx - xp)
                - aura_flow_fmul(xp, zo);
    proj->xsden = aura_flow_fmul(xp, sinr) - AURA_FLOW_CAM_DIST * cosr;
    proj->xs = aura_flow_fdiv(proj->xsnum, proj->xsden);

    proj->xsnumi = -AURA_FLOW_CAM_DIST_R - zo;
    proj->xsdeni = sinr;
    proj->screen_x = xi;
}

int aura_flow_advance_column(aura_flow_projection_t *proj)
{
    int column;

    if (proj->screen_x >= AURA_FLOW_SCREEN_W)
        return 0;

    if (proj->has_rotation)
    {
        proj->xsnum += proj->xsnumi;
        proj->xsden += proj->xsdeni;
        proj->xs = aura_flow_fdiv(proj->xsnum, proj->xsden);
    }
    else
    {
        proj->xs += AURA_FLOW_ONE;
    }
    proj->screen_x++;

    if (proj->screen_x >= AURA_FLOW_SCREEN_W)
        return 0;

    column = (proj->xs - proj->slide_left) / AURA_FLOW_ONE;
    return column >= 0 && column < proj->slide_width_px;
}

int aura_flow_source_column(const aura_flow_projection_t *proj)
{
    int column = (proj->xs - proj->slide_left) / AURA_FLOW_ONE;
    if (column < 0)
        column = 0;
    if (column >= proj->slide_width_px)
        column = proj->slide_width_px - 1;
    return column;
}

int aura_flow_vertical_scale(const aura_flow_projection_t *proj)
{
    int dy = (AURA_FLOW_CAM_DIST_R + proj->zo + aura_flow_fmul(proj->xs, proj->sinr))
           / AURA_FLOW_CAM_DIST;
    return dy > 0 ? dy : 1;
}
