/* Contenido y logica de cada pantalla de Aura: que se dibuja y que
 * hace cada boton segun la pantalla activa en la pila de navegacion.
 * aura_main.c solo bombea eventos de boton hacia aca. */
#ifndef AURA_SCREENS_H
#define AURA_SCREENS_H

#include "aura_nav.h"

void aura_screens_draw(aura_nav_t *nav);
void aura_screens_handle_button(aura_nav_t *nav, long button);

#endif /* AURA_SCREENS_H */
