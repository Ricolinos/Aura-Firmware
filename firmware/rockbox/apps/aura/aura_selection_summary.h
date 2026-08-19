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
/* SelectionSummary (PLAN.md T2.8, componentes/selection-summary.md):
 * estado base/vacio del panel derecho -- icono sobre tile con degradado
 * del acento + hasta dos lineas de texto, cuando no hay contenido mas
 * rico (caratulas, fotos) para mostrar ahi.
 *
 * Alcance real (ver DECISIONS.md D-097/D-108):
 * - Icono ESTATICO sobre el tile de degradado, dos slots de texto con
 *   MarqueeText en overflow, cambio instantaneo de valor (sin
 *   Fade-Slide/Scroll-Slide, tal como pide el documento), sombra de
 *   LeftPanel -- completos y verificados.
 * - Variante DINAMICA del icono (B-04 en BLOCKED.md, ya resuelto: "debe
 *   ser una variante" de este componente, no un componente distinto) --
 *   aura_selection_summary_draw_dynamic() comparte todo el layout
 *   (tile, degradado, sombra, texto) con la version estatica, solo
 *   cambia COMO se dibuja el simbolo. Primer renderer real:
 *   aura_selection_summary_render_analog_clock(). La hoja de
 *   calendario y la pantalla real Ajustes > Fecha y Hora
 *   (componentes/date-editor.md, "casi todo pendiente") siguen sin
 *   construir -- eso es contenido/produccion, no la pregunta de
 *   arquitectura que B-04 resolvia.
 * - Cross-fade con CoverDrift (T2.9) NO conectado -- CoverDrift no
 *   existe todavia. Diferido, no bloqueado: se conecta cuando T2.9
 *   exista, el mecanismo (a26_shell_blend hacia el color ya visible)
 *   ya esta disponible.
 */
#ifndef AURA_SELECTION_SUMMARY_H
#define AURA_SELECTION_SUMMARY_H

#include "aura_category.h"

/* D-281: fondo del panel completo -- por defecto la imagen por acento de
 * D-267 (ACCENT_IMAGE); NEUTRAL_FADE es un degradado horizontal gris a
 * blanco (o su equivalente por tema) calculado en tiempo real, pedido
 * SOLO para "Acerca de" (mas fluido hacia el estado expandido, que vive
 * sobre SHELL_BG plano -- PLAN-about-fixes.md C1/Q3). Parametrizado en
 * vez de un caso especial adentro del componente: cualquier fila puede
 * elegirlo a futuro asignando el campo en panel_identity_t, sin tocar
 * este archivo de nuevo. */
typedef enum {
    AURA_SS_BG_ACCENT_IMAGE = 0,
    AURA_SS_BG_NEUTRAL_FADE,
} aura_ss_background_t;

/* Dibuja en la franja [x, x+width) x [0, A26_SCREEN_HEIGHT) -- todo el
 * espacio vertical disponible del panel derecho (StatusBar en (split)
 * solo vive sobre el panel izquierdo, doc status-bar.md, asi que este
 * componente no necesita dejarle hueco arriba).
 *
 * `icon_name` nunca NULL (siempre hay un icono por item de menu, regla
 * de diseno cerrada en el documento). `category` (D-263) fija el
 * degradado del tile -- el llamador SIEMPRE debe pasar la categoria ya
 * CONGELADA/comprometida del contenido que esta dibujando en este
 * cuadro, nunca una consulta en vivo a la seleccion actual del
 * LeftPanel (ver aura_screens.c: panel_identity_category()) -- de lo
 * contrario el color del tile cambia antes que el icono/texto durante
 * el debounce de D-262, deshaciendo el fundido. `top_text` puede ser
 * NULL (slot superior opcional, SF Pro Bold 16pt, una sola linea --
 * "valor" o titulo corto); `bottom_text` casi siempre presente pero
 * tambien acepta NULL para cubrir el caso limite -- hasta DOS lineas
 * por palabra si no cabe en una (D-263), MarqueeText por linea si ni
 * asi cabe. Ninguno de los dos anima su cambio de VALOR (icono y texto
 * cambian de forma instantanea) -- solo el loop de MarqueeText si el
 * texto no cabe.
 */
void aura_selection_summary_draw(int x, int width,
                                  const char *icon_name,
                                  aura_category_t category,
                                  const char *top_text,
                                  const char *bottom_text);

/* Variante dinamica (B-04 en BLOCKED.md): mismo contrato que
 * aura_selection_summary_draw(), pero en vez de un `icon_name` fijo
 * recibe un renderer que dibuja el simbolo el mismo (reloj analogico,
 * hoja de calendario futura, etc.) dentro de un cuadro de `size`x`size`
 * centrado en (x,y) -- el renderer decide su propio contenido, este
 * componente solo le da el hueco ya centrado sobre el tile. */
typedef void (*aura_selection_summary_icon_renderer_t)(int x, int y, int size);

/* Renderer del slot INFERIOR (D-264, "Acerca de": grafico de
 * almacenamiento en vez de texto) -- mismo espiritu que el renderer de
 * icono de arriba (B-04: "debe ser una variante", no un componente
 * nuevo), pero para el rectangulo de texto de abajo en vez del cuadro
 * de icono. Solo se invoca cuando `bottom_text` es NULL y `bottom_renderer`
 * no lo es -- si AMBOS son NULL, el slot simplemente no se dibuja (igual
 * que hoy). `(x, y, width, height)` es el rectangulo completo reservado
 * -- el renderer decide su propio contenido adentro. */
typedef void (*aura_selection_summary_bottom_renderer_t)(int x, int y, int width, int height);

/* `background` (D-281): AURA_SS_BG_ACCENT_IMAGE en cualquier llamador que
 * no lo necesite distinto (mismo aspecto que aura_selection_summary_draw()). */
void aura_selection_summary_draw_dynamic(int x, int width,
                                          aura_selection_summary_icon_renderer_t renderer,
                                          aura_category_t category,
                                          const char *top_text,
                                          const char *bottom_text,
                                          aura_selection_summary_bottom_renderer_t bottom_renderer,
                                          aura_ss_background_t background);

/* Primer renderer dinamico real (no un stub): reloj analogico con las
 * manecillas apuntando a la hora actual (get_time(), mismo dato real
 * que aura_format_clock() ya usa) -- circulo + manecillas en el mismo
 * blanco constante que el resto del contenido sobre el tile de acento
 * (variante "-selector", G5/T2.2). Trigonometria entera reusada de
 * aura_flow_fsin()/fcos() (T1.1/Fase 31) -- regla dura 7, ninguna
 * tabla de seno nueva. */
void aura_selection_summary_render_analog_clock(int x, int y, int size);

/* Mismo par pending()/animating() que aura_statusbar_title_*() y
 * aura_menu_list_scroll_indicator_*() -- cubre AMBOS slots de texto (o
 * cualquiera de los dos desbordando cuenta). aura_main.c los consulta
 * con la misma cadencia gruesa/fina ya establecida. */
int aura_selection_summary_pending(void);
int aura_selection_summary_animating(void);

/* Geometria real del tile en (split) -- D-278/D-279, para construir el
 * `carry` de aura_transition_shift_and_reveal() sin duplicar la formula
 * de centrado (D-270) en otro archivo. */
void aura_selection_summary_tile_rect_split(int *x, int *y, int *w, int *h);

/* Dibuja el tile (sombra + degradado por categoria + simbolo/renderer
 * centrado) en (x,y), tamano fijo TILE_SIZE (90px). Extraido de
 * aura_selection_summary_draw()/_draw_dynamic() (D-278/D-279) para que
 * otras pantallas (hoy: el estado expandido de "Acerca de") puedan
 * dibujar el MISMO tile fuera del panel derecho -- necesario para que
 * aura_transition_shift_and_reveal() tenga un elemento identico en
 * ambos extremos del viaje. `icon_name` o `renderer`, nunca los dos
 * (mismo contrato que el resto del componente). */
void aura_selection_summary_draw_tile(int x, int y, aura_category_t category,
                                       const char *icon_name,
                                       aura_selection_summary_icon_renderer_t renderer);

/* Degradado VERTICAL de 3 puntos (acento oscuro -> acento -> acento claro,
 * D-292) que este componente usa como fallback de su fondo cuando el
 * acento activo no tiene imagen propia -- exportado (PLAN-niveles-fx.md
 * D-c) para que CoverDrift lo reuse tal cual como fondo de Graficos=
 * Ninguno ("color de acento con un ligero degradado"). Llena
 * [x, x+width) x [0, A26_SCREEN_HEIGHT) -- mismo hueco que el resto de
 * este componente. */
void aura_selection_summary_draw_accent_gradient_background(int x, int width);

#endif /* AURA_SELECTION_SUMMARY_H */
