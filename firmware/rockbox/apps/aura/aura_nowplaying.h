/* Pantalla "Ahora suena": caratula, titulo/artista/album, progreso,
 * bateria, y letra sincronizada (.lrc) accesible con el boton central. */
#ifndef AURA_NOWPLAYING_H
#define AURA_NOWPLAYING_H

#include "aura_nav.h"

/* Geometria de la inclinacion real de la caratula (T3.1, DECISIONS.md
 * D-099) -- publica porque componentes/cover-flow.md acopla la
 * coreografia `Flip-and-Flow` (T3.2(d)) a esta MISMA geometria
 * exacta ("si se cambia una, se cambia la otra"): la caratula que
 * vuela desde Cover Flow tiene que aterrizar en este angulo/posicion
 * exactos, no una aproximacion recalculada aparte. */
#define AURA_NOWPLAYING_TILT_IANGLE 20     /* 7 grados * 1024/360, redondeado */
/* PFreal, re-derivado (2026-08-12) para el signo de angulo corregido
 * (ver draw_cover_tilted() en aura_nowplaying.c: el signo original
 * dejaba el lado izquierdo corto/retrocedido y el derecho alto/completo,
 * al reves de lo pedido por el dueno del diseno) -- mismo metodo de
 * D-099, busqueda numerica contra aura_flow_begin_projection() para que
 * el borde izquierdo de la proyeccion caiga en ART_X=10 con
 * ART_SIZE=135 y este angulo exacto, verificado por render real. */
#define AURA_NOWPLAYING_TILT_CX     (-85300)

/* True si hay algo cargado para reproducir (pausado o sonando). Si es
 * false, aura_screens.c muestra el estado vacio en su lugar. */
bool aura_nowplaying_active(void);

/* Y de la fila de iconos de modos del ultimo render no-compacto --
 * para el morph de entrada desde Cover Flow (aura_transitions.c). */
int aura_nowplaying_last_mode_row_y(void);

/* Consumido por aura_screens al hacer pop desde el reproductor: true
 * (una sola vez) si la salida vino del Modo 4 (letras, pantalla
 * completa) -- el pop usa el slide de pantalla completa hacia el menu
 * anterior en vez del morph de regreso al coverflow. */
bool aura_nowplaying_take_fullscreen_exit(void);

/* Despliegue inverso del Modo 4 (panel -> layout normal), sincrono --
 * lo encadena aura_screens antes del morph de regreso al carrusel
 * cuando la salida del Modo 4 va hacia el coverflow. */
void aura_nowplaying_unfold_from_lyrics(void);

/* true mientras el Modo 4 (hoja de vidrio) esta en pantalla -- las
 * esquinas derechas de pantalla no se estampan encima (la hoja es
 * cuadrada). */
bool aura_nowplaying_sheet_active(void);

void aura_nowplaying_draw(void);
void aura_nowplaying_handle_button(aura_nav_t *nav, long button);

/* True mientras el overlay de volumen (Fase 17, PLAN-UX.md) sigue
 * visible y necesita un redibujo propio para desaparecer solo, aunque
 * el usuario no vuelva a tocar el clickwheel. */
bool aura_nowplaying_needs_tick(void);

/* True mientras el icono de modo recien activado sigue en pleno resorte
 * (doc "Reproductor - Ahora suena.md" SS5, Fase 30) -- aura_main.c pide
 * la cadencia fina de 20fps mientras dure, mismo patron que
 * aura_widgets_pill_animating(). */

#endif /* AURA_NOWPLAYING_H */
