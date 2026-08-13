/* Contenido y logica de cada pantalla de Aura: que se dibuja y que
 * hace cada boton segun la pantalla activa en la pila de navegacion.
 * aura_main.c solo bombea eventos de boton hacia aca. */
#ifndef AURA_SCREENS_H
#define AURA_SCREENS_H

#include "aura_nav.h"

void aura_screens_draw(aura_nav_t *nav);
void aura_screens_handle_button(aura_nav_t *nav, long button);

/* Avance de seleccion con la dinamica real de la rueda (velocidad ->
 * 1-3 items), acotado a la lista. Compartido por todas las pantallas
 * con lista propia. */
int aura_wheel_advance(int sel, int count, int direction);

#endif /* AURA_SCREENS_H */
