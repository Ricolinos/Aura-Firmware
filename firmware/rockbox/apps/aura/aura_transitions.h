/* Transiciones entre pantallas, controladas por el ajuste Animaciones
 * (aura_settings.animation_mode) -- no por Graficos, que decide que se
 * dibuja, no que se mueve:
 *   Ninguna -- sin transicion (redibujo instantaneo).
 *   Minimas -- desplazamiento breve indicando adelante/atras.
 *   Todas   -- el mismo desplazamiento con mas cuadros, mas fluido.
 */
#ifndef AURA_TRANSITIONS_H
#define AURA_TRANSITIONS_H

#include <stdint.h>

#include "aura_nav.h"

/* Anima la entrada a la pantalla actual de `nav` desplazandola desde
 * fuera de encuadre. `direction` > 0 = se entro a una pantalla nueva
 * (entra desde la derecha), < 0 = se volvio a la anterior (entra desde
 * la izquierda). No hace nada en modo Ultra minimalista.
 *
 * `width` acota la region del empuje al rango [0, width): en un T1
 * entre dos menus divididos (L1/L2) el empuje solo debe verse en el
 * panel izquierdo (A26_LAYOUT_PANEL_LEFT_WIDTH) -- el panel derecho es
 * una capa aparte que no se mueve y se actualiza con su propio debounce
 * (L3). Para un T3 (cualquier extremo a pantalla completa) pasar
 * A26_SCREEN_WIDTH. Valores fuera de rango caen a ancho completo.
 *
 * Cubre dos de los cuatro patrones de PLAN-UX.md L4: T1 (menu->menu) y
 * T3 (push de pantalla completa) -- ambos son, en la practica, "revela
 * la pantalla nueva desde un borde", la misma animacion con distinto
 * origen segun la direccion. Ver D-058 en DECISIONS.md. */
void aura_transition_slide(aura_nav_t *nav, int direction, int width);

/* T4 (revelado de Coverflow, L4): el contenido nuevo emerge desde
 * ambos bordes hacia el centro en vez de deslizar desde uno solo --
 * distingue visualmente la entrada a Coverflow de una navegacion de
 * lista comun. No hace nada en modo Ultra (Coverflow no existe ahi). */
void aura_transition_coverflow_enter(aura_nav_t *nav);

/* `Flip-and-Flow` (PLAN.md T3.2(d), componentes/cover-flow.md,
 * transiciones/00-vocabulario.md): al elegir una cancion en Cover
 * Flow, la caratula del album (ya decodificada a su tamano final de
 * NowPlaying, AURA_DS_METRICS_NOW_PLAYING_COVER_SIZE) vuela desde su
 * posicion de reposo en el carrusel (angulo 0, centrada, altura
 * `from_y`) hasta la geometria EXACTA de NowPlaying
 * (AURA_NOWPLAYING_TILT_IANGLE/_CX/_Y, aura_nowplaying.h) -- con
 * reflejo durante todo el trayecto. Arranca la reproduccion real
 * (aura_music_play_songs ya debe haberse llamado por el invocador
 * antes de esto -- esta funcion solo anima) y empuja
 * AURA_SCREEN_NOWPLAYING al terminar. No hace nada (deja que el
 * llamador navegue directo) si `album_seek` no tiene caratula real --
 * no hay nada que volar. */
/* `from_y` es el CENTRO vertical de la caratula en el carrusel (los
 * renderers de columnas centran en la linea media desde la correccion
 * de geometria 2026-08-12). Tras el vuelo corre el morph de entrada de
 * now-playing.md (fade de textos, modos desde la derecha, progreso
 * desde abajo, barra en Drop) y hace el aura_nav_push() -- el llamador
 * NO debe aplicar ninguna transicion generica encima. */
void aura_transition_flip_and_flow(aura_nav_t *nav, int32_t album_seek, int from_y);

#endif /* AURA_TRANSITIONS_H */
