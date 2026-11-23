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
#include <stdlib.h>

#include "lcd.h"
#include "tick.h"

#include "apple2026_shell.h"
#include "apple2026_tokens.h"
#include "aura_patterns.h"

#include "aura_coverdrift.h"
#include "aura_fx.h"

#define IMAGE_SIZE   AURA_DS_METRICS_COVER_DRIFT_IMAGE_SIZE
#define CYCLE_MS     AURA_DS_METRICS_COVER_DRIFT_MOVE_DURATION_MS
#define CROSSFADE_MS AURA_DS_METRICS_COVER_DRIFT_CROSSFADE_MS
#define ANGLE_COUNT  AURA_DS_METRICS_COVER_DRIFT_DIRECTIONS_DEG_COUNT

/* D-364: muestreo SUB-PIXEL (interpolacion bilineal) del movimiento de
 * deriva. A 40px en 7000ms la deriva avanza ~0.29px por cuadro a 20fps:
 * con muestreo entero eso se ve como saltos de 1px cada ~3.5 cuadros
 * (peor en diagonal, ver D-257). Con muestreo bilineal el desplazamiento
 * fraccionario se interpola pixel a pixel (3 blends por pixel del panel,
 * ~115k blends por cuadro -- unas 3 veces el costo de un cross-fade, que
 * ya corre sin problema en hardware). Solo se usa cuando
 * aura_fx_coverdrift_motion() esta activo (Animaciones=Todas); poner en
 * 0 para volver al muestreo entero (lcd_bitmap_part) si el costo
 * resultara excesivo en bateria/rendimiento. */
#define AURA_COVERDRIFT_SUBPIXEL 1

static const int ANGLES_DEG[ANGLE_COUNT] = AURA_DS_METRICS_COVER_DRIFT_DIRECTIONS_DEG_VALUES;

static int s_index = -1;
static int s_prev_index = -1;
static int s_angle_idx = -1;
static int s_distance = 0;
static long s_since = 0;
static int s_count_last = 0;

/* D-257: redondeo simetrico (no truncamiento hacia cero) de un valor
 * Q?.8 (pixeles*256) al pixel entero mas cercano -- el unico punto
 * donde dx/dy pierden precision, a proposito, al final, para ambos
 * ejes desde la misma fuente (ver aura_coverdrift_draw()). */
static int round_to_nearest_q8(int v256)
{
    return (v256 >= 0) ? (v256 + 128) / 256 : -((-v256 + 128) / 256);
}

/* Distancia de arranque de cada movimiento (D-256, encargo del dueno
 * del producto: "recorran de borde a borde" -- movimiento mucho mas
 * pronunciado). Antes (D-098/D-254) variaba entre 40%-100% del margen
 * disponible; ahora siempre usa el margen COMPLETO -- cada movimiento
 * arranca en el borde extremo de la imagen y viaja hasta el centro,
 * la excursion maxima que el area de sobrante permite sin revelar el
 * fondo del panel (ver max_distance en aura_coverdrift_draw()). */
static int pick_distance(int max_distance)
{
    return max_distance;
}

static int pick_angle_idx_excluding(int prev_idx)
{
    int idx = rand() % ANGLE_COUNT;
    if (idx == prev_idx)
        idx = (idx + 1) % ANGLE_COUNT;
    return idx;
}

static void advance_cycle(int count, int max_distance)
{
    s_prev_index = s_index;
    s_index = (s_index < 0) ? 0 : (s_index + 1) % count;
    s_angle_idx = pick_angle_idx_excluding(s_angle_idx);
    s_distance = pick_distance(max_distance);
    s_since = current_tick;
}

int aura_coverdrift_should_mount(int image_count)
{
    return image_count >= AURA_DS_METRICS_COVER_DRIFT_MIN_IMAGES_TO_ACTIVATE;
}

int aura_coverdrift_pending(void)
{
    return s_index >= 0;
}

int aura_coverdrift_animating(void)
{
    /* D-364: con Animaciones == Todas el movimiento es continuo durante
     * TODO el ciclo (nunca hay un tramo realmente estatico salvo el
     * instante exacto en que llega al centro). Con Animaciones != Todas
     * la imagen activa queda QUIETA en el centro (D-a) -- ahi el unico
     * tramo que de verdad anima pixel a pixel es la ventana del
     * cross-fade; fuera de ella basta la cadencia gruesa de
     * aura_coverdrift_pending() (aura_main.c) para que el reloj del
     * ciclo avance y la rotacion de imagen (contenido, no animacion)
     * ocurra a tiempo. */
    if (s_index < 0)
        return 0;

    if (!aura_fx_coverdrift_motion())
        return aura_fx_coverdrift_crossfade() && s_prev_index >= 0
               && (current_tick - s_since) * 1000L / HZ < CROSSFADE_MS;

    return 1;
}

/* D-364: true solo mientras la imagen ANTERIOR participa del cross-fade
 * vigente. Fuera de esa ventana (o con el fundido apagado en la
 * configuracion global) ya nadie la dibuja, y su slot de decodificacion
 * queda libre para pre-decodificar la siguiente imagen del ciclo (ver
 * ensure_drift_albums_decoded() en aura_screens.c). */
int aura_coverdrift_prev_needed(void)
{
    return s_index >= 0 && s_prev_index >= 0
           && aura_fx_coverdrift_crossfade()
           && (current_tick - s_since) * 1000L / HZ < CROSSFADE_MS;
}

int aura_coverdrift_active_index(void)
{
    return s_index;
}

int aura_coverdrift_prev_index(void)
{
    return s_prev_index;
}

/* Placeholder solido cuando `bmp` es NULL -- la imagen activa/anterior
 * todavia no termino de decodificarse (el llamador decodifica bajo
 * demanda, ver aura_coverdrift.h). Paleta sintetica solo para
 * distinguir imagenes a simple vista, NO es una decision de diseno
 * real -- desaparece en cuanto el llamador entrega el bitmap real,
 * normalmente en 1 cuadro. Llena el panel completo (no un tile chico),
 * mismo criterio de recorte que draw_image_at() para bitmaps reales. */
static unsigned placeholder_color(int index)
{
    switch (index % 4)
    {
        case 0: return aura_accent();
        case 1: return aura_accent_light();
        case 2: return aura_accent_dark();
        default: return a26_shell_blend(aura_accent(), a26_color(A26_SHELL_BG), 128);
    }
}

/* D-254: dibuja UNA imagen (ya sea placeholder o bitmap real) llenando
 * el panel [px, px+pw) x [0, A26_SCREEN_HEIGHT), recortada a partir del
 * desplazamiento de deriva (dx, dy) desde el centro del panel. La
 * imagen (IMAGE_SIZE x IMAGE_SIZE, mayor que el panel en ambos ejes)
 * SIEMPRE cubre el panel completo por construccion -- max_distance en
 * aura_coverdrift_draw() esta acotado exactamente para garantizar esto
 * (ver comentario ahi), asi que nunca hace falta clampear ni revela
 * fondo. `margin_x`/`margin_y` son el sobrante de la imagen sobre el
 * panel en cada eje quieto (dx=dy=0); el origen de muestreo dentro de
 * la imagen es simplemente ese margen desplazado por (dx, dy). */
static void sample_offsets(int margin_x, int margin_y, int dx, int dy,
                            int *src_x, int *src_y)
{
    *src_x = margin_x - dx;
    *src_y = margin_y - dy;
}

static void draw_image_solid(const aura_coverdrift_image_t *images, int index,
                              int px, int pw, int margin_x, int margin_y,
                              int dx, int dy)
{
    const struct bitmap *bmp = images[index].bmp;

    if (bmp)
    {
        int src_x, src_y;
        sample_offsets(margin_x, margin_y, dx, dy, &src_x, &src_y);
        lcd_bitmap_part((const fb_data *)bmp->data, src_x, src_y, IMAGE_SIZE,
                         px, 0, pw, A26_SCREEN_HEIGHT);
        return;
    }

    a26_shell_fill_rounded_rect(px, 0, pw, A26_SCREEN_HEIGHT, 0,
                                 placeholder_color(index), a26_color(A26_SHELL_BG));
}

#if AURA_COVERDRIFT_SUBPIXEL
/* D-365: variante INLINE de a26_shell_blend() para el bucle bilineal.
 * La version de biblioteca (funcion EXTERNA, ~115k llamadas por cuadro
 * entre las 3 mezclas por pixel de 38.4k pixeles) no alcanzaba los
 * 20fps en el ARM de 80MHz -- el dueno reporto el movimiento "como a
 * 5fps" en hardware. Misma matematica (delta por canal, t en [0,256]),
 * pero inline y sin los cortes de alpha<=0/>=256 (las fracciones
 * sub-pixel estan en [1,255] en los caminos que la llaman; con t=0 los
 * llamadores usan la via entera). */
static inline unsigned blend_fast(unsigned from, unsigned to, int t)
{
    int r = RGB_UNPACK_RED(from)
        + (((RGB_UNPACK_RED(to)   - RGB_UNPACK_RED(from))   * t) >> 8);
    int g = RGB_UNPACK_GREEN(from)
        + (((RGB_UNPACK_GREEN(to) - RGB_UNPACK_GREEN(from)) * t) >> 8);
    int b = RGB_UNPACK_BLUE(from)
        + (((RGB_UNPACK_BLUE(to)  - RGB_UNPACK_BLUE(from))  * t) >> 8);
    return LCD_RGBPACK(r, g, b);
}

/* D-364: variante de draw_image_solid() con muestreo SUB-PIXEL
 * (interpolacion bilineal). sx256/sy256 son el origen de muestreo dentro
 * de la imagen en pixeles*256 (equivalente sub-pixel de los src_x/src_y
 * enteros que sample_offsets() produce). Por cada pixel del panel mezcla
 * los 4 vecinos de la imagen fuente: primero horizontal en las dos filas
 * vecinas, luego vertical entre ambos resultados.
 *
 * La deriva esta acotada por construccion a los margenes de sobrante de
 * la imagen (ver aura_coverdrift_draw()), asi que la ventana entera de
 * muestreo siempre cae dentro de la imagen EXCEPTO el vecino +1 que la
 * interpolacion pide en el borde extremo: para ese caso se clampa el
 * origen un pixel atras y se cae a muestreo entero en ese eje (la
 * diferencia visual de <1px en el borde del recorrido es imperceptible
 * frente al riesgo de leer fuera del buffer).
 *
 * D-365: bucle especializado por eje -- de las 8 direcciones de deriva,
 * 4 son puramente horizontales/verticales (fraccion 0 en un eje) y con
 * un solo blend por pixel se dibujan; solo las 4 diagonales pagan las 3
 * mezclas completas. */
static void draw_image_solid_subpixel(const struct bitmap *bmp,
                                       int px, int pw,
                                       int sx256, int sy256)
{
    int sx = sx256 >> 8;
    int fx = sx256 & 255;
    int sy = sy256 >> 8;
    int fy = sy256 & 255;
    int row, col;

    if (sx < 0) { sx = 0; fx = 0; }
    if (sy < 0) { sy = 0; fy = 0; }
    if (sx + pw + 1 > IMAGE_SIZE) { sx = IMAGE_SIZE - pw - 1; fx = 0; }
    if (sy + A26_SCREEN_HEIGHT + 1 > IMAGE_SIZE)
    {
        sy = IMAGE_SIZE - A26_SCREEN_HEIGHT - 1;
        fy = 0;
    }

    /* Sin parte fraccionaria en ningun eje, la interpolacion se reduce a
     * una copia entera -- la via rapida de hardware de siempre. */
    if (fx == 0 && fy == 0)
    {
        lcd_bitmap_part((const fb_data *)bmp->data, sx, sy, IMAGE_SIZE,
                         px, 0, pw, A26_SCREEN_HEIGHT);
        return;
    }

    for (row = 0; row < A26_SCREEN_HEIGHT; row++)
    {
        const fb_data *r0 = (const fb_data *)bmp->data
            + (sy + row) * IMAGE_SIZE + sx;
        fb_data *dst = FBADDR(px, row);

        if (fy == 0)
        {
            /* Movimiento puramente horizontal (0/180 grados). */
            for (col = 0; col < pw; col++)
                dst[col] = blend_fast(r0[col], r0[col + 1], fx);
        }
        else if (fx == 0)
        {
            /* Movimiento puramente vertical (90/270 grados). */
            const fb_data *r1 = r0 + IMAGE_SIZE;

            for (col = 0; col < pw; col++)
                dst[col] = blend_fast(r0[col], r1[col], fy);
        }
        else
        {
            /* Diagonal: bilineal completo, 3 mezclas por pixel. */
            const fb_data *r1 = r0 + IMAGE_SIZE;

            for (col = 0; col < pw; col++)
            {
                unsigned top = blend_fast(r0[col], r0[col + 1], fx);
                unsigned bot = blend_fast(r1[col], r1[col + 1], fx);
                dst[col] = blend_fast(top, bot, fy);
            }
        }
    }
}
#endif /* AURA_COVERDRIFT_SUBPIXEL */

/* D-254: fundido real pixel a pixel entre la imagen SALIENTE (quieta,
 * en el centro exacto -- termino su ciclo ahi, D-089/G10) y la
 * ENTRANTE (moviendose, en (dx,dy)) -- ambas garantizado que cubren el
 * panel completo (mismo invariante que draw_image_solid()), asi que
 * cada pixel del panel tiene una muestra valida de las dos imagenes
 * sin necesidad de clampear. Costo real: A26_SCREEN_HEIGHT*pw pixeles
 * (38400 con el panel de 160x240) por cuadro, solo durante la ventana
 * de CROSSFADE_MS (~600ms desde D-256, unos 12 cuadros a la cadencia de animacion
 * de 20fps que ya usa este modulo) -- comparable en orden de magnitud
 * a los compositores de mascara de icono ya existentes en
 * aura_widgets.c (draw_icon_mask_2()), que recorren pixel a pixel sin
 * problema de rendimiento observado a esa misma cadencia. */
static void draw_crossfade(const aura_coverdrift_image_t *images,
                            int px, int pw, int margin_x, int margin_y,
                            int dx, int dy, int alpha_256)
{
    const struct bitmap *bmp_out = images[s_prev_index].bmp;
    const struct bitmap *bmp_in = images[s_index].bmp;
    int out_src_x, out_src_y, in_src_x, in_src_y;
    int row, col;

    if (!bmp_out || !bmp_in)
    {
        /* Alguna de las dos todavia no decodifico -- no hay como
         * mezclar pixel real, se muestra solo la entrante (o el
         * placeholder si tampoco esa esta lista) sin fundido este
         * cuadro. Degradacion de un solo cuadro, imperceptible (mismo
         * criterio que el resto del sistema). */
        draw_image_solid(images, s_index, px, pw, margin_x, margin_y, dx, dy);
        return;
    }

    sample_offsets(margin_x, margin_y, 0, 0, &out_src_x, &out_src_y);
    sample_offsets(margin_x, margin_y, dx, dy, &in_src_x, &in_src_y);

    for (row = 0; row < A26_SCREEN_HEIGHT; row++)
    {
        const fb_data *out_row = (const fb_data *)bmp_out->data
            + (out_src_y + row) * IMAGE_SIZE + out_src_x;
        const fb_data *in_row = (const fb_data *)bmp_in->data
            + (in_src_y + row) * IMAGE_SIZE + in_src_x;
        fb_data *dst = FBADDR(px, row);

        for (col = 0; col < pw; col++)
            dst[col] = a26_shell_blend(out_row[col], in_row[col], alpha_256);
    }
}

/* D-256 (correccion de un bug real, encargo del dueno del producto:
 * "en lugar de hacer el fundido... aparece un flash color rosa entre
 * imagen e imagen"): antes, advance_cycle() (el avance de s_index) se
 * disparaba DENTRO de aura_coverdrift_draw(), en el mismo cuadro en que
 * el llamador (aura_screens.c) ya habia decodificado la imagen activa
 * ANTES de llamar a draw() -- en el cuadro exacto en que el ciclo
 * avanzaba, el llamador todavia no sabia el nuevo indice (se entera
 * recien DENTRO de este draw(), demasiado tarde para decodificarlo a
 * tiempo). images[s_index].bmp quedaba NULL ese cuadro -> cae al
 * placeholder solido (color de acento, rosa/rojo) -> exactamente el
 * "flash" reportado, una vez por cada cambio de imagen (cada 7s).
 *
 * Arreglo: separar "decidir si toca avanzar" de "dibujar". El llamador
 * ahora invoca esta funcion PRIMERO (antes de decodificar), para que
 * aura_coverdrift_active_index() ya refleje el indice nuevo A TIEMPO
 * de decodificarlo antes de que aura_coverdrift_draw() lo necesite. */
void aura_coverdrift_advance_if_due(int width, int count)
{
    int margin_x = (IMAGE_SIZE - width) / 2;
    int margin_y = (IMAGE_SIZE - A26_SCREEN_HEIGHT) / 2;
    int max_distance = margin_x < margin_y ? margin_x : margin_y;
    long elapsed_ms;

    if (max_distance < 0)
        max_distance = 0;

    if (count != s_count_last || s_index < 0 || s_index >= count)
    {
        s_count_last = count;
        s_index = -1;
        advance_cycle(count, max_distance);
        return;
    }

    elapsed_ms = (current_tick - s_since) * 1000L / HZ;
    if (elapsed_ms >= CYCLE_MS)
        advance_cycle(count, max_distance);
}

/* D-365: salta la imagen activa de inmediato -- el llamador la invoca
 * cuando la activa resulto NO tener caratula decodificable (archivo
 * ausente/corrupto, album sin arte), para que CoverDrift solo muestre
 * imagenes reales y nunca el placeholder solido. Misma mecanica que el
 * avance normal de ciclo (nuevo angulo/distancia, reloj reiniciado,
 * la saltada queda como "anterior" -- como su bitmap es NULL, el
 * cross-fade de ese tramo se degrada a mostrar la entrante directo,
 * ver draw_crossfade()). */
void aura_coverdrift_skip_current(int width, int count)
{
    int margin_x = (IMAGE_SIZE - width) / 2;
    int margin_y = (IMAGE_SIZE - A26_SCREEN_HEIGHT) / 2;
    int max_distance = margin_x < margin_y ? margin_x : margin_y;

    if (max_distance < 0)
        max_distance = 0;

    if (count <= 0 || s_index < 0)
        return;

    advance_cycle(count, max_distance);
}

void aura_coverdrift_draw(int x, int width,
                           const aura_coverdrift_image_t *images, int count)
{
    /* D-254: margen de sobrante de la imagen (IMAGE_SIZE x IMAGE_SIZE)
     * sobre el panel en cada eje -- puede ser negativo si el panel
     * fuera mayor que la imagen en ese eje (no pasa con las
     * dimensiones reales de este sistema, pero se acota a 0 por
     * seguridad: sin margen, sin deriva posible en ese eje). La
     * distancia maxima de deriva es el MENOR de los dos margenes: eso
     * garantiza que, en cualquiera de las 8 direcciones (incluidas las
     * diagonales, donde la distancia se reparte entre ambos ejes), la
     * imagen nunca deja de cubrir el panel completo -- ver
     * draw_image_solid()/draw_crossfade(). */
    int margin_x = (IMAGE_SIZE - width) / 2;
    int margin_y = (IMAGE_SIZE - A26_SCREEN_HEIGHT) / 2;
    long elapsed_ms;
    aura_pattern_point_hp_t pos_hp;
    int dx, dy;
    (void)count;

    /* D-277: sin linea de separador en la ultima columna del LeftPanel
     * (fundamentos/04-bordes.md, D-274/C11: la separacion la da solo la
     * sombra); ver la nota equivalente en aura_selection_summary.c. */

    if (margin_x < 0) margin_x = 0;
    if (margin_y < 0) margin_y = 0;

    /* D-256: el avance de ciclo ya se resolvio en
     * aura_coverdrift_advance_if_due() (el llamador la invoca antes de
     * decodificar, ver aura_coverdrift.h) -- aca solo queda leer el
     * tiempo transcurrido del ciclo YA vigente y dibujar. */
    if (s_index < 0)
        return;

    elapsed_ms = (current_tick - s_since) * 1000L / HZ;
    if (elapsed_ms > CYCLE_MS)
        elapsed_ms = CYCLE_MS;

    /* D-255 (correccion del dueno del producto, 2026-08-15): de vuelta a
     * velocidad CONSTANTE -- D-254 habia agregado una desaceleracion
     * real (ease_out_effective_ms(), remapeando el tiempo antes de
     * pasarlo aca) que el dueno probo y pidio quitar. aura_pattern_drift_pos_hp()
     * (aura_patterns.c) ya es puramente lineal por su cuenta -- se le
     * pasa elapsed_ms directo, sin ningun remapeo de tiempo.
     *
     * D-257 (correccion del dueno del producto: "el movimiento en
     * diagonal se ve escalonado"): se usa la variante de alta precision
     * (dx256/dy256, pixeles*256 sin truncar) en vez de la version
     * entera -- dx/dy se truncan a pixel UNA sola vez, aca mismo, con
     * round_to_nearest_q8() (ver arriba), en vez de la cadena de DOS
     * truncamientos independientes que aura_pattern_drift_pos() hacia
     * antes internamente. Con eso, los dos ejes redondean desde la
     * MISMA fuente de precision, en el mismo instante -- ya no pueden
     * desincronizarse entre si (la causa real del aspecto
     * "escalonado" a velocidad lenta: 8 direcciones sobre apenas
     * ~40px en 7s son bastante menos de 1px por cuadro). */
    /* PLAN-niveles-fx.md D-a: Animaciones decide COMO se mueve lo que
     * Graficos ya decidio que existe -- con Animaciones != Todas, la
     * imagen activa queda QUIETA en el centro (distancia 0); el reloj
     * del ciclo (elapsed_ms/CYCLE_MS) sigue avanzando igual, es lo que
     * gobierna CUANDO rota de imagen (D-a: la rotacion es contenido, no
     * animacion) y, si toca, el cross-fade de abajo. */
    pos_hp = aura_pattern_drift_pos_hp(ANGLES_DEG[s_angle_idx],
                                        aura_fx_coverdrift_motion() ? s_distance : 0,
                                        elapsed_ms, CYCLE_MS);
    dx = round_to_nearest_q8(pos_hp.dx256);
    dy = round_to_nearest_q8(pos_hp.dy256);

    /* D-a: con Animaciones=Ninguna el cambio de imagen es un CORTE --
     * aura_fx_coverdrift_crossfade() es la unica puerta nueva, el resto
     * de la logica (ventana de CROSSFADE_MS, imagen anterior valida) no
     * cambia. */
    if (aura_fx_coverdrift_crossfade() && elapsed_ms < CROSSFADE_MS && s_prev_index >= 0)
    {
        int alpha = (int)(elapsed_ms * 256 / CROSSFADE_MS);
        draw_crossfade(images, x, width, margin_x, margin_y, dx, dy, alpha);
    }
    else
    {
#if AURA_COVERDRIFT_SUBPIXEL
        const struct bitmap *bmp = images[s_index].bmp;

        /* D-364: movimiento con muestreo sub-pixel (bilineal) -- elimina
         * los saltos de 1px del muestreo entero a velocidades de deriva
         * menores que 1px/cuadro. El cross-fade sigue con muestreo
         * entero (su propia mezcla de alpha ya difumina el paso de 1px,
         * y solo dura CROSSFADE_MS); sin bitmap real o con el movimiento
         * apagado, la via entera de siempre. */
        if (bmp && aura_fx_coverdrift_motion())
        {
            draw_image_solid_subpixel(bmp, x, width,
                                       (margin_x << 8) - pos_hp.dx256,
                                       (margin_y << 8) - pos_hp.dy256);
        }
        else
#endif
        {
            draw_image_solid(images, s_index, x, width, margin_x, margin_y,
                              dx, dy);
        }
    }

    /* D-258 (encargo del dueno del producto: "la sombra paralela que
     * proyecta el LeftPanel sobre CoverDrift"): la sombra YA se
     * dibujaba (aura_shell_draw_left_panel_shadow(), arriba de este
     * archivo desde D-254) pero ANTES del contenido -- la caratula, que
     * ahora llena el panel completo, la tapaba por completo cada
     * cuadro. Se mueve al final, y se cambia a la variante de
     * compositing real (aura_shell_draw_left_panel_shadow_over_content(),
     * apple2026_shell.c) que oscurece el pixel YA dibujado en vez de
     * mezclar contra un fondo plano fijo -- la variante original esta
     * documentada para "contenido rico" como una aproximacion que no
     * aplica aca. */
    aura_shell_draw_left_panel_shadow_over_content(x, 0, A26_SCREEN_HEIGHT);
}
