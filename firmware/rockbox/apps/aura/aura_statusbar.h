/* Barra de estado propia de Aura (Fase 13, PLAN-UX.md, L5): reemplaza a
 * la barra de estado clasica de Rockbox (que Fase 12/D-051 ya
 * desactivo por completo via STATUSBAR_OFF). Bateria siempre visible a
 * la derecha; icono de reproduccion (o el candado de Hold, que le gana
 * el lugar) a su izquierda; titulo alineado a la izquierda en menus o
 * centrado en pantallas con interfaz propia. */
#ifndef AURA_STATUSBAR_H
#define AURA_STATUSBAR_H

#include <stddef.h>

/* HH:MM, reusando global_settings.timeformat (D-021) -- ningun formato
 * propio. Compartida con aura_clock_indicator.c (T2.5) para no duplicar
 * la logica de 12/24h en dos lugares. */
void aura_format_clock(char *buf, size_t bufsz);

/* Dibuja la franja [x, x+width) x [0, A26_LAYOUT_STATUSBAR_HEIGHT).
 * `title` puede ser NULL (sin texto, solo bateria/reproduccion/hold).
 * `centered` = 0 alinea a la izquierda (menus), 1 centra (pantallas
 * con interfaz propia: Ahora suena, reloj, etc.). No llama a
 * lcd_update(). */
void aura_statusbar_draw(int x, int width, const char *title, int centered);

/* True mientras el titulo actual siga desbordando (activo el patron
 * Marquee Loop, T2.1), sin importar en que tramo del ciclo este --
 * aura_main.c la consulta con cadencia GRUESA (mismo criterio que
 * aura_widgets_scrollbar_pending()) para no perderse la frontera entre
 * el tramo estatico y el de movimiento. False si el titulo actual cupo
 * sin activar el patron. */
int aura_statusbar_title_pending(void);

/* True solo durante el tramo de MOVIMIENTO del ciclo -- cadencia FINA
 * (mismo criterio que aura_widgets_scrollbar_animating()). */
int aura_statusbar_title_animating(void);

#endif /* AURA_STATUSBAR_H */
