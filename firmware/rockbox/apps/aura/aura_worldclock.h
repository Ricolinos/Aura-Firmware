/* Reloj internacional de Extras (encargo del dueno del diseno
 * 2026-08-13).
 *
 * Lista de relojes ANALOGICOS con su lugar y su hora en formato de 12
 * horas. El primero es la hora LOCAL y se dibuja con la esfera clara;
 * los demas husos elegidos van con la esfera oscura, como en el
 * original. SELECT abre el menu de Anadir / Editar / Eliminar; anadir
 * elige por continente y despues por ciudad.
 */
#ifndef AURA_WORLDCLOCK_H
#define AURA_WORLDCLOCK_H

#include <stdbool.h>
#include "aura_nav.h"

void aura_worldclock_draw(void);
void aura_worldclock_handle_button(aura_nav_t *nav, long button);

/* Selector de continente y de ciudad (pantallas propias del flujo de
 * anadir/editar). */
void aura_worldclock_regions_draw(void);
void aura_worldclock_regions_handle_button(aura_nav_t *nav, long button);
void aura_worldclock_cities_draw(void);
void aura_worldclock_cities_handle_button(aura_nav_t *nav, long button);

/* El minutero se mueve: la puerta de energia redibuja mientras esta
 * pantalla este activa. */
bool aura_worldclock_needs_tick(void);

#endif /* AURA_WORLDCLOCK_H */
