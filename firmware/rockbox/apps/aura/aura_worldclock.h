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

/* Catalogo de husos, compartido con la pantalla de Zona horaria de
 * Ajustes: una sola tabla de ciudades para todo el firmware. */
int aura_worldclock_city_count(void);
const char *aura_worldclock_city_name(int i);
int aura_worldclock_city_utc_quarters(int i);
int aura_worldclock_city_lat10(int i);   /* Zona horaria: latitud*10 */
int aura_worldclock_city_lon10(int i);   /* Zona horaria: longitud*10 */
/* Indice REAL a la ciudad en la posicion `order_pos` del orden por
 * LATITUD descendente (Zona horaria, "se va moviendo en orden de
 * latitud"). */
int aura_worldclock_city_by_lat_order(int order_pos);

#endif /* AURA_WORLDCLOCK_H */
