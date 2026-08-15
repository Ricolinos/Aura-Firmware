/* Contenido y logica de cada pantalla de Aura: que se dibuja y que
 * hace cada boton segun la pantalla activa en la pila de navegacion.
 * aura_main.c solo bombea eventos de boton hacia aca. */
#ifndef AURA_SCREENS_H
#define AURA_SCREENS_H

#include <stdbool.h>

#include "aura_nav.h"

void aura_screens_draw(aura_nav_t *nav);
void aura_screens_handle_button(aura_nav_t *nav, long button);

/* Avance de seleccion con la dinamica real de la rueda (velocidad ->
 * 1-3 items), acotado a la lista. Compartido por todas las pantallas
 * con lista propia. */
int aura_wheel_advance(int sel, int count, int direction);

/* D-254: true mientras el temporizador de 3s de CoverDrift esta
 * contando (la seleccion se poso sobre una fila calificada pero
 * todavia no se cumplio el plazo) -- aura_main.c lo usa para pedir
 * cuadros a cadencia fina durante esa espera, igual que ya hace con
 * aura_coverdrift_animating() una vez montado. */
bool aura_screens_coverdrift_arming(void);

#endif /* AURA_SCREENS_H */
