/* Coverflow: version simplificada para el modo grafico Completo.
 * Reemplaza a la lista plana de Albumes (AURA_SCREEN_MUSIC_ALBUMS /
 * AURA_SCREEN_MUSIC_ALBUMS_BY_ARTIST) solo cuando
 * aura_settings.graphics_mode == AURA_GFX_ALL -- con Graficos en Ninguno/Minimos
 * esas pantallas siguen siendo la lista de aura_screens.c. Ver D-025.
 */
#ifndef AURA_COVERFLOW_H
#define AURA_COVERFLOW_H

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

#endif /* AURA_COVERFLOW_H */
