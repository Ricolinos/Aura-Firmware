/* Coverflow: version simplificada para el modo grafico Completo.
 * Reemplaza a la lista plana de Albumes (AURA_SCREEN_MUSIC_ALBUMS /
 * AURA_SCREEN_MUSIC_ALBUMS_BY_ARTIST) solo cuando
 * aura_settings.graphics_mode == AURA_GFX_FULL -- en Ultra/Minimalista
 * esas pantallas siguen siendo la lista de aura_screens.c. Ver D-025.
 */
#ifndef AURA_COVERFLOW_H
#define AURA_COVERFLOW_H

#include "aura_nav.h"

void aura_coverflow_draw(aura_nav_t *nav, aura_screen_id_t screen);
void aura_coverflow_handle_button(aura_nav_t *nav, aura_screen_id_t screen, long button);

#endif /* AURA_COVERFLOW_H */
