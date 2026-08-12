/* Coverflow: version simplificada para el modo grafico Completo.
 * Reemplaza a la lista plana de Albumes (AURA_SCREEN_MUSIC_ALBUMS /
 * AURA_SCREEN_MUSIC_ALBUMS_BY_ARTIST) solo cuando
 * aura_settings.graphics_mode == AURA_GFX_ALL -- con Graficos en Ninguno/Minimos
 * esas pantallas siguen siendo la lista de aura_screens.c. Ver D-025.
 */
#ifndef AURA_COVERFLOW_H
#define AURA_COVERFLOW_H

#include <stdint.h>

#include "aura_nav.h"

void aura_coverflow_draw(aura_nav_t *nav, aura_screen_id_t screen);
void aura_coverflow_handle_button(aura_nav_t *nav, aura_screen_id_t screen, long button);

/* Estados idle/scrolling (PLAN.md T3.2(b), componentes/cover-flow.md).
 * Movimiento continuo mientras "scrolling" (sin tramo estatico real,
 * mismo criterio que CoverDrift/T2.9) -- pending() y animating()
 * coinciden, expuestos ambos por consistencia con el resto de la
 * puerta de energia en aura_main.c. */
int aura_coverflow_pending(void);
int aura_coverflow_animating(void);

/* -- Coreografia de vuelo hacia/desde el reproductor (2026-08-12) ------
 * Geometria publica del reverso crecido: la transicion de vuelo
 * (aura_transitions.c) arranca/aterriza exactamente aqui. Requiere
 * apple2026_tokens.h incluido antes (mismo contrato que el resto del
 * proyecto). */
#define AURA_COVERFLOW_BACK_SIZE 200
#define AURA_COVERFLOW_BACK_X ((A26_SCREEN_WIDTH - AURA_COVERFLOW_BACK_SIZE) / 2)
#define AURA_COVERFLOW_BACK_Y (A26_LAYOUT_STATUSBAR_HEIGHT         + (A26_SCREEN_HEIGHT - A26_LAYOUT_STATUSBAR_HEIGHT - AURA_COVERFLOW_BACK_SIZE) / 2)

/* Fondo de un cuadro del vuelo de IDA: pantalla limpia + barra +
 * laterales deslizandose hacia SU borde (las de la derecha salen por
 * la derecha, las de la izquierda por la izquierda) -- `out_t256`
 * 0..256, 256 = ya fuera de pantalla. La tapa central NO se dibuja
 * (esa la anima el vuelo). */
void aura_coverflow_draw_exit_frame(int out_t256);

/* Fondo de un cuadro del vuelo de REGRESO: laterales entrando desde
 * los bordes + titulo/artista subiendo desde el borde inferior --
 * `in_t256` 0..256, 256 = todo asentado. */
void aura_coverflow_draw_return_frame(int in_t256);

/* Album objetivo actual (seek de tagcache) -- la transicion de regreso
 * recarga su caratula con el. */
int32_t aura_coverflow_current_album_seek(void);

/* Deja el carrusel en reposo (IDLE) mostrando la caratula de frente --
 * lo llama la transicion de regreso al aterrizar: el usuario vuelve al
 * carrusel, no a la lista abierta. */
void aura_coverflow_settle_idle(void);

#endif /* AURA_COVERFLOW_H */
