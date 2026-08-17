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
#include <stdbool.h>

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
 * origen segun la direccion. Ver D-058 en DECISIONS.md.
 *
 * `cover_drift_was_active` (D-261, generalizado de la variante que
 * D-259 construyo SOLO para aura_transition_coverflow_enter()): true
 * si CoverDrift estaba mostrandose en el panel derecho del origen en
 * el instante exacto en que se disparo esta transicion (ver
 * aura_screens_coverdrift_active_for()). Cuando es true Y `width` es
 * A26_SCREEN_WIDTH (destino a pantalla completa -- el UNICO caso donde
 * esta coreografia tiene sentido), LeftPanel Y el panel derecho (con
 * la caratula de CoverDrift) salen CADA UNO hacia su propio borde,
 * revelando el destino ya renderizado, quieto, detras de ambos desde
 * el primer cuadro -- igual que la entrada a Cover Flow con CoverDrift
 * (D-259). Si `cover_drift_was_active` es true pero `width` no es
 * pantalla completa (por ejemplo un T1 split->split), se ignora con
 * seguridad y corre el push clasico de siempre -- el contrato real
 * (el llamador solo debe pasar `true` cuando el origen era SPLIT y
 * CoverDrift genuinamente estaba montado ahi) es responsabilidad de
 * quien llama, pero esta funcion nunca confia ciegamente en eso.
 * `false` reproduce EXACTAMENTE el comportamiento de siempre, sin
 * ningun cambio -- todos los llamadores existentes antes de D-261
 * pasan `false`. */
void aura_transition_slide(aura_nav_t *nav, int direction, int width,
                            bool cover_drift_was_active);

/* T4 (revelado de Coverflow, L4): el contenido nuevo emerge desde
 * ambos bordes hacia el centro en vez de deslizar desde uno solo --
 * distingue visualmente la entrada a Coverflow de una navegacion de
 * lista comun. No hace nada en modo Ultra (Coverflow no existe ahi).
 *
 * `cover_drift_was_active` (D-259, prueba acotada SOLO a esta entrada
 * -- encargo del dueno del producto): true si CoverDrift estaba
 * mostrandose en el panel derecho del submenu de Musica en el instante
 * exacto en que se disparo esta transicion (ver
 * aura_screens_coverdrift_active_for()). Cuando es true, LeftPanel Y el
 * panel derecho (con la caratula de CoverDrift) salen CADA UNO hacia su
 * propio borde -- izquierdo a la izquierda, derecho a la derecha --
 * revelando el Coverflow ya renderizado, quieto, detras de ambos desde
 * el primer cuadro. Cuando es false, coreografia identica a la de
 * siempre (sin cambios): LeftPanel sale por la izquierda, Coverflow
 * entra desde el borde derecho. */
void aura_transition_coverflow_enter(aura_nav_t *nav, bool cover_drift_was_active);

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
/* Vuelo Cover Flow -> reproductor (rehecho 2026-08-12): arranca del
 * reverso crecido (geometria publica en aura_coverflow.h), media
 * vuelta continua con las laterales saliendo hacia sus bordes, y al
 * aterrizar corre el morph de entrada de now-playing.md y hace el
 * aura_nav_push() -- el llamador NO aplica transicion generica encima. */
void aura_transition_flip_and_flow(aura_nav_t *nav, int32_t album_seek);

/* Regreso reproductor -> Cover Flow: morph de posicion/geometria de la
 * caratula de frente (sin giro), laterales y titulo entrando desde los
 * bordes. Llamar DESPUES del pop (nav ya apunta al Cover Flow); deja
 * el carrusel en reposo. */
void aura_transition_flow_return(aura_nav_t *nav);

/* `Shift-and-Reveal` (D-278, transiciones/00-vocabulario.md): un
 * elemento del panel derecho (icono/tile) NUNCA sale de pantalla -- se
 * reposiciona hacia la izquierda mientras LeftPanel sale empujado (su
 * comportamiento estandar) y el resto de la pantalla FULL-CARRY destino
 * aparece en el hueco liberado a la derecha. Primer consumidor: "Acerca
 * de" (D-279); disenada para que DateEditor la reuse despues sin
 * reescribirla -- solo cambia `carry`.
 *
 * `carry` describe el rect del elemento persistente en AMBOS extremos,
 * siempre en el mismo orden sin importar `direction`:
 *   from_* = posicion/tamano del elemento en la pantalla dividida (split)
 *   to_*   = posicion/tamano del MISMO elemento en la pantalla completa
 * `direction` > 0: split -> full (entrar), el elemento viaja from -> to.
 * `direction` < 0: full -> split (Menu, inversa exacta), el elemento
 * viaja to -> from. Sin escalado: `from_w/h` y `to_w/h` deben coincidir
 * en la primera version del consumidor (si algun dia hace falta que el
 * elemento cambie de tamano durante el viaje, es una extension a
 * documentar aca, no algo que hoy se intente adivinar). */
typedef struct {
    int from_x, from_y, from_w, from_h;
    int to_x,   to_y,   to_w,   to_h;
} aura_shift_rect_t;

void aura_transition_shift_and_reveal(aura_nav_t *nav, int direction,
                                      const aura_shift_rect_t *carry);

/* `Fade-Slide` de una REGION (D-283, PLAN-about-fixes.md Q9): cambio de
 * pagina DENTRO de una misma pantalla FULL (Estados 1-3 de "Acerca de")
 * -- el vocabulario documenta `Fade-Slide` para DynamicTitle (pantalla
 * completa fija, solo el texto cambia); aca se adapta a una region
 * arbitraria porque el resto de la pantalla (StatusBar, el tile
 * persistente de D-283 2/4, los puntos de paginacion) no cambia entre
 * paginas y no debe animarse. Llamar DESPUES de actualizar el estado de
 * navegacion (`s_about_page` u equivalente) que decide que dibuja
 * `aura_screens_draw()` -- mismo contrato que las demas transiciones de
 * este archivo: prerrenderizan el DESTINO, no el origen.
 *
 * `x,y,w,h`: rect de la region a animar, en coordenadas de pantalla.
 * `direction` > 0 (profundizar, SELECT/RIGHT): el contenido saliente se
 * desliza a la izquierda con fade-out: el entrante llega desde la
 * derecha con fade-in. `direction` < 0 (LEFT): al reves. */
void aura_transition_fade_slide_region(aura_nav_t *nav, int x, int y, int w, int h,
                                       int direction);

#endif /* AURA_TRANSITIONS_H */
